#include "terse_platform.h"
#include "terse_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

// Windows handle mapping for stdin/stdout file descriptors
// fd 0 = stdin, fd 1 = stdout, fd 2 = stderr
static HANDLE
get_console_handle(int fd)
{
	DWORD std_handle;
	switch (fd) {
	case 0:
		std_handle = STD_INPUT_HANDLE;
		break;
	case 1:
		std_handle = STD_OUTPUT_HANDLE;
		break;
	case 2:
		std_handle = STD_ERROR_HANDLE;
		break;
	default:
		return INVALID_HANDLE_VALUE;
	}
	return GetStdHandle(std_handle);
}

static int
is_console_handle(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		return 0;
	}
	DWORD mode;
	return GetConsoleMode(handle, &mode);
}

// Read bytes from console with timeout support
static size_t
read_bytes_with_timeout(int fd, unsigned char *buffer, size_t capacity, int timeout_ms)
{
	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		return 0;
	}

	size_t total = 0;
	const int slice = 25;
	int remaining = timeout_ms;

	// Save and set console mode for raw input
	DWORD original_mode;
	if (!GetConsoleMode(handle, &original_mode)) {
		return 0;
	}
	DWORD raw_mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
	SetConsoleMode(handle, raw_mode);

	while (total < capacity) {
		int wait = slice;
		DWORD wait_timeout;
		if (timeout_ms < 0) {
			wait_timeout = INFINITE;
		} else {
			if (remaining <= 0) {
				break;
			}
			if (remaining < slice) {
				wait = remaining;
			}
			wait_timeout = (DWORD)wait;
		}

		DWORD result = WaitForSingleObject(handle, wait_timeout);
		if (result == WAIT_TIMEOUT) {
			if (timeout_ms >= 0) {
				remaining -= wait;
			}
			continue;
		}
		if (result != WAIT_OBJECT_0) {
			break;
		}

		DWORD read_count;
		if (!ReadFile(handle, buffer + total, (DWORD)(capacity - total), &read_count, NULL)) {
			break;
		}
		if (read_count == 0) {
			break;
		}
		total += read_count;
		if (timeout_ms >= 0) {
			remaining -= wait;
		}
	}

	SetConsoleMode(handle, original_mode);
	return total;
}

terse_options_t
terse_platform_default_options(void)
{
	terse_options_t options = {
		.input_fd = 0,  // stdin
		.output_fd = 1, // stdout
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	// Enable virtual terminal processing on Windows 10+
	HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (is_console_handle(output_handle)) {
		DWORD mode;
		if (GetConsoleMode(output_handle, &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(output_handle, mode);
		}
	}

	// NOTE: Do NOT enable ENABLE_VIRTUAL_TERMINAL_INPUT here
	// It breaks ReadConsoleInput() modifier key detection (vk=0, dwControlKeyState=0)
	// Applications should set console mode before calling terse_open() if needed

	return options;
}

terse_size_t
terse_platform_query_fd_size(int fd)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};

	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		return size;
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(handle, &csbi)) {
		size.rows = (unsigned short)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
		size.cols = (unsigned short)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
		size.known = 1;
	}

	return size;
}

size_t
terse_platform_probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity)
{
	// Windows Console does not respond to Secondary DA (ESC [ > 0 c)
	// and the probe sequence would be displayed as raw text.
	// Since Windows Console is a well-known environment, we skip the probe.
	(void)input_fd;
	(void)output_fd;
	(void)buffer;
	(void)capacity;
	return 0;
}

terse_error_t
terse_platform_query_cursor_position(int input_fd, int output_fd, int *out_row, int *out_col)
{
	if (!out_row || !out_col) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	HANDLE input_handle = get_console_handle(input_fd);
	HANDLE output_handle = get_console_handle(output_fd);

	if (!is_console_handle(input_handle) || !is_console_handle(output_handle)) {
		return TERSE_ERR_NOT_TTY;
	}

	// Save current console modes
	DWORD original_input_mode, original_output_mode;
	if (!GetConsoleMode(input_handle, &original_input_mode) ||
	    !GetConsoleMode(output_handle, &original_output_mode)) {
		return TERSE_ERR_IO;
	}

	// Set raw mode
	DWORD raw_input_mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
	DWORD raw_output_mode = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
	if (!SetConsoleMode(input_handle, raw_input_mode) ||
	    !SetConsoleMode(output_handle, raw_output_mode)) {
		return TERSE_ERR_IO;
	}

	// Send CPR (Cursor Position Report) request: CSI 6 n
	const char request[] = "\x1b[6n";
	DWORD written;
	if (!WriteFile(output_handle, request, sizeof(request) - 1, &written, NULL)) {
		SetConsoleMode(input_handle, original_input_mode);
		SetConsoleMode(output_handle, original_output_mode);
		return TERSE_ERR_IO;
	}

	// Read response: CSI row ; col R
	unsigned char buffer[32];
	size_t length = 0;
	const int timeout_ms = 200;
	const int slice = 25;
	int remaining = timeout_ms;

	while (length < sizeof(buffer)) {
		int wait = (remaining < slice) ? remaining : slice;
		DWORD result = WaitForSingleObject(input_handle, (DWORD)wait);

		if (result == WAIT_TIMEOUT) {
			remaining -= wait;
			if (remaining <= 0) {
				break;
			}
			continue;
		}
		if (result != WAIT_OBJECT_0) {
			SetConsoleMode(input_handle, original_input_mode);
			SetConsoleMode(output_handle, original_output_mode);
			return TERSE_ERR_IO;
		}

		DWORD read_count;
		if (!ReadFile(input_handle, buffer + length, (DWORD)(sizeof(buffer) - length), &read_count, NULL)) {
			break;
		}
		if (read_count == 0) {
			break;
		}
		length += read_count;
		remaining -= wait;

		// Check if we have a complete response (ends with 'R')
		if (length > 0 && buffer[length - 1] == 'R') {
			break;
		}
	}

	// Restore console modes
	SetConsoleMode(input_handle, original_input_mode);
	SetConsoleMode(output_handle, original_output_mode);

	// Parse response: ESC [ row ; col R
	if (length < 6 || buffer[0] != 0x1b || buffer[1] != '[' || buffer[length - 1] != 'R') {
		return TERSE_ERR_PROTOCOL;
	}

	// Parse row and col
	int row = 0, col = 0;
	size_t i = 2;
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		row = row * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != ';') {
		return TERSE_ERR_PROTOCOL;
	}
	i++; // skip ';'
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		col = col * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != 'R') {
		return TERSE_ERR_PROTOCOL;
	}

	// Terminal returns 1-based coordinates, convert to 0-based
	*out_row = row - 1;
	*out_col = col - 1;
	return TERSE_OK;
}

terse_error_t
terse_platform_wait_for_input(int fd, int timeout_ms)
{
	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		errno = EBADF;
		return TERSE_ERR_INVALID_HANDLE;
	}

	DWORD wait_timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
	DWORD result = WaitForSingleObject(handle, wait_timeout);

	if (result == WAIT_OBJECT_0) {
		return 1; // Input available
	} else if (result == WAIT_TIMEOUT) {
		return 0; // Timeout, no input
	} else {
		errno = EIO;
		return TERSE_ERR_IO;
	}
}

ssize_t
terse_platform_read_byte(int fd, unsigned char *out)
{
	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		errno = EBADF;
		return -EBADF;
	}

	DWORD read_count;
	if (!ReadFile(handle, out, 1, &read_count, NULL)) {
		DWORD error = GetLastError();
		errno = EIO;
		return -EIO;
	}

	return (ssize_t)read_count;
}

size_t
terse_platform_drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		return 1; // Return current length (buffer[0] already has ESC)
	}

	size_t len = 1;
	while (len < max) {
		DWORD result = WaitForSingleObject(handle, 10); // 10ms timeout
		if (result == WAIT_TIMEOUT) {
			break;
		}
		if (result != WAIT_OBJECT_0) {
			break;
		}

		DWORD read_count;
		if (!ReadFile(handle, buffer + len, 1, &read_count, NULL)) {
			break;
		}
		if (read_count == 0) {
			break;
		}
		len += read_count;

		if (len >= 3) {
			unsigned char c = buffer[len - 1];
			// Special case: ESC [ [ needs one more byte (Linux console compatibility)
			if (len == 3 && buffer[1] == '[' && buffer[2] == '[') {
				// Continue reading
			} else if (c >= '@' && c <= '~') {
				break;
			}
		}
	}
	return len;
}

terse_error_t
terse_platform_write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	HANDLE handle = get_console_handle(fd);
	if (!is_console_handle(handle)) {
		errno = EBADF;
		return TERSE_ERR_INVALID_HANDLE;
	}

	while (len > 0) {
		DWORD written;
		if (!WriteFile(handle, bytes, (DWORD)len, &written, NULL)) {
			DWORD error = GetLastError();
			errno = EIO;
			return TERSE_ERR_IO;
		}
		if (written == 0) {
			errno = EPIPE;
			return TERSE_ERR_IO;
		}
		bytes += written;
		len -= written;
	}

	return TERSE_OK;
}

/* Convert Windows control key state to terse modifier flags */
static int
convert_control_key_state(DWORD dwControlKeyState)
{
	int mods = 0;

	if (dwControlKeyState & SHIFT_PRESSED) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
		mods |= TERSE_MOD_CTRL;
	}
	if (dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
		mods |= TERSE_MOD_ALT;
	}

	return mods;
}

/* Convert KEY_EVENT_RECORD to terse_event_t */
static terse_error_t
convert_key_event(const KEY_EVENT_RECORD *ker, terse_event_t *out_event)
{
	/* Only process key down events */
	if (!ker->bKeyDown) {
		return TERSE_ERR_NO_EVENT;
	}

	int mods = convert_control_key_state(ker->dwControlKeyState);
	WCHAR wch = ker->uChar.UnicodeChar;
	WORD vk = ker->wVirtualKeyCode;

	/* Handle special keys */
	switch (vk) {
	case VK_RETURN:
		out_event->type = TERSE_EVENT_ENTER;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_BACK:
		out_event->type = TERSE_EVENT_BACKSPACE;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_TAB:
		out_event->type = TERSE_EVENT_TAB;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_ESCAPE:
		out_event->type = TERSE_EVENT_CHAR;
		out_event->data.ch.scalar = 0x1B;
		out_event->data.ch.width = 0;
		out_event->data.ch.mods = mods;
		return TERSE_OK;

	case VK_UP:
		out_event->type = TERSE_EVENT_ARROW_UP;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_DOWN:
		out_event->type = TERSE_EVENT_ARROW_DOWN;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_LEFT:
		out_event->type = TERSE_EVENT_ARROW_LEFT;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_RIGHT:
		out_event->type = TERSE_EVENT_ARROW_RIGHT;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_HOME:
		out_event->type = TERSE_EVENT_HOME;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_END:
		out_event->type = TERSE_EVENT_END;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_PRIOR: /* Page Up */
		out_event->type = TERSE_EVENT_PAGE_UP;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_NEXT: /* Page Down */
		out_event->type = TERSE_EVENT_PAGE_DOWN;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_INSERT:
		out_event->type = TERSE_EVENT_INSERT;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_DELETE:
		out_event->type = TERSE_EVENT_DELETE;
		out_event->data.key.mods = mods;
		return TERSE_OK;

	case VK_F1: case VK_F2: case VK_F3: case VK_F4:
	case VK_F5: case VK_F6: case VK_F7: case VK_F8:
	case VK_F9: case VK_F10: case VK_F11: case VK_F12:
		out_event->type = TERSE_EVENT_FUNCTION;
		out_event->data.function.number = (vk - VK_F1 + 1);
		out_event->data.function.mods = mods;
		return TERSE_OK;

	default:
		break;
	}

	/* Handle character input */
	if (wch != 0) {
		out_event->type = TERSE_EVENT_CHAR;

		/* Map Ctrl+letter control characters back to uppercase letters
		 * Windows returns 0x01-0x1A for Ctrl+A through Ctrl+Z
		 * Temporary fix: detect control characters and force TERSE_MOD_CTRL
		 */
		if (wch >= 0x01 && wch <= 0x1A) {
			out_event->data.ch.scalar = (unsigned int)('A' + (wch - 1));
			out_event->data.ch.mods = mods | TERSE_MOD_CTRL;
		} else {
			out_event->data.ch.scalar = (unsigned int)wch;
			out_event->data.ch.mods = mods;
		}

		/* Determine character width (simple heuristic) */
		if (wch < 0x80) {
			out_event->data.ch.width = 1;
		} else if (wch >= 0x1100 &&
		           ((wch >= 0x1100 && wch <= 0x115F) || /* Hangul Jamo */
		            (wch >= 0x2E80 && wch <= 0x9FFF) || /* CJK */
		            (wch >= 0xAC00 && wch <= 0xD7AF) || /* Hangul Syllables */
		            (wch >= 0xF900 && wch <= 0xFAFF) || /* CJK Compatibility */
		            (wch >= 0xFE10 && wch <= 0xFE19) || /* Vertical forms */
		            (wch >= 0xFE30 && wch <= 0xFE6F) || /* CJK Compatibility Forms */
		            (wch >= 0xFF00 && wch <= 0xFF60) || /* Fullwidth Forms */
		            (wch >= 0xFFE0 && wch <= 0xFFE6))) { /* Fullwidth Forms */
			out_event->data.ch.width = 2;
		} else {
			out_event->data.ch.width = 1;
		}

		return TERSE_OK;
	}

	/* Ignore other key events (modifier keys pressed alone, etc.) */
	return TERSE_ERR_NO_EVENT;
}

terse_error_t
terse_platform_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	if (!handle || !out_event) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	int input_fd = handle->options.input_fd;
	HANDLE input_handle = get_console_handle(input_fd);

	if (!is_console_handle(input_handle)) {
		return TERSE_ERR_NOT_TTY;
	}

	/* Wait for input with timeout */
	DWORD wait_timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
	DWORD result = WaitForSingleObject(input_handle, wait_timeout);

	if (result == WAIT_TIMEOUT) {
		return TERSE_ERR_NO_EVENT;
	}
	if (result != WAIT_OBJECT_0) {
		return TERSE_ERR_IO;
	}

	/* Read console input event */
	INPUT_RECORD input_record;
	DWORD events_read;

	/* Explicitly use ReadConsoleInputW to ensure Unicode handling */
	if (!ReadConsoleInputW(input_handle, &input_record, 1, &events_read)) {
		return TERSE_ERR_IO;
	}

	if (events_read == 0) {
		return TERSE_ERR_NO_EVENT;
	}

	/* Process event based on type */
	switch (input_record.EventType) {
	case KEY_EVENT:
		return convert_key_event(&input_record.Event.KeyEvent, out_event);

	case WINDOW_BUFFER_SIZE_EVENT:
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(input_handle, &csbi)) {
			out_event->type = TERSE_EVENT_RESIZE;
			out_event->data.resize.rows = (csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
			out_event->data.resize.cols = (csbi.srWindow.Right - csbi.srWindow.Left + 1);
			return TERSE_OK;
		}
		return TERSE_ERR_NO_EVENT;
	}

	case MOUSE_EVENT:
		/* TODO: Implement mouse event conversion */
		return TERSE_ERR_NO_EVENT;

	case FOCUS_EVENT:
	case MENU_EVENT:
		/* Ignore these events */
		return TERSE_ERR_NO_EVENT;

	default:
		return TERSE_ERR_NO_EVENT;
	}
}

terse_error_t
terse_platform_move_to_fast(terse_handle_t handle, int row, int col)
{
	(void)handle;
	(void)row;
	(void)col;
	/* Windows uses standard escape sequences or VT API, no fast path available */
	return TERSE_ERR_NOT_SUPPORTED;
}

terse_error_t
terse_platform_clear_screen_fast(terse_handle_t handle, terse_clear_mode_t mode)
{
	(void)handle;
	(void)mode;
	/* Windows uses standard escape sequences or VT API, no fast path available */
	return TERSE_ERR_NOT_SUPPORTED;
}
