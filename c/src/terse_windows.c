#include "terse_platform.h"

#include <errno.h>
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

	HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
	if (is_console_handle(input_handle)) {
		DWORD mode;
		if (GetConsoleMode(input_handle, &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
			SetConsoleMode(input_handle, mode);
		}
	}

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
	if (!buffer || capacity == 0) {
		return 0;
	}

	HANDLE input_handle = get_console_handle(input_fd);
	HANDLE output_handle = get_console_handle(output_fd);

	if (!is_console_handle(input_handle) || !is_console_handle(output_handle)) {
		return 0;
	}

	// Save current console mode
	DWORD original_input_mode, original_output_mode;
	if (!GetConsoleMode(input_handle, &original_input_mode) ||
	    !GetConsoleMode(output_handle, &original_output_mode)) {
		return 0;
	}

	// Set raw mode for probing
	DWORD raw_input_mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
	DWORD raw_output_mode = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
	if (!SetConsoleMode(input_handle, raw_input_mode) ||
	    !SetConsoleMode(output_handle, raw_output_mode)) {
		return 0;
	}

	// Send Secondary DA request: ESC [ > 0 c
	const char request[] = "\x1b[>0c";
	DWORD written;
	if (!WriteFile(output_handle, request, sizeof(request) - 1, &written, NULL)) {
		SetConsoleMode(input_handle, original_input_mode);
		SetConsoleMode(output_handle, original_output_mode);
		return 0;
	}

	// Read response with timeout
	size_t length = read_bytes_with_timeout(input_fd, buffer, capacity, 200);

	// Restore console modes
	SetConsoleMode(input_handle, original_input_mode);
	SetConsoleMode(output_handle, original_output_mode);

	return length;
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
