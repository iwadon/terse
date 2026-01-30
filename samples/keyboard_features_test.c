/*
 * Keyboard features detection test
 */
#include "terse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static DWORD g_original_input_mode = 0;
static DWORD g_original_output_mode = 0;
static UINT g_original_cp = 0;
static UINT g_original_output_cp = 0;
static int g_raw_installed = 0;

static void restore_terminal(void)
{
	if (g_raw_installed) {
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleMode(hStdin, g_original_input_mode);
		SetConsoleMode(hStdout, g_original_output_mode);
		if (g_original_cp != 0) {
			SetConsoleCP(g_original_cp);
		}
		if (g_original_output_cp != 0) {
			SetConsoleOutputCP(g_original_output_cp);
		}
		g_raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetStdHandle failed\n");
		return -1;
	}
	g_original_cp = GetConsoleCP();
	g_original_output_cp = GetConsoleOutputCP();
	if (!SetConsoleCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleCP(65001) failed\n");
	}
	if (!SetConsoleOutputCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleOutputCP(65001) failed\n");
	}
	if (!GetConsoleMode(hStdin, &g_original_input_mode)) {
		fprintf(stderr, "GetConsoleMode(input) failed\n");
		return -1;
	}
	if (!GetConsoleMode(hStdout, &g_original_output_mode)) {
		fprintf(stderr, "GetConsoleMode(output) failed\n");
		return -1;
	}
	DWORD dwInputMode = g_original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
	dwInputMode = (dwInputMode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
	if (!SetConsoleMode(hStdin, dwInputMode)) {
		fprintf(stderr, "SetConsoleMode(input) failed\n");
		return -1;
	}
	DWORD dwOutputMode = g_original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hStdout, dwOutputMode)) {
		fprintf(stderr, "SetConsoleMode(output) failed\n");
		return -1;
	}
	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}
#else /* POSIX */
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static int g_raw_installed = 0;

static void restore_terminal(void)
{
	if (g_raw_installed) {
		(void)tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
		g_raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &g_original_termios) != 0) {
		return -1;
	}
	struct termios raw = g_original_termios;
	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_cflag |= CS8;
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
		return -1;
	}
	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}
#endif /* _WIN32 */

static const char *mod_string(int mods)
{
	static char buffer[64];
	buffer[0] = '\0';
	int first = 1;
	if (mods & TERSE_MOD_SHIFT) {
		strcat(buffer, first ? "Shift" : "|Shift");
		first = 0;
	}
	if (mods & TERSE_MOD_CTRL) {
		strcat(buffer, first ? "Ctrl" : "|Ctrl");
		first = 0;
	}
	if (mods & TERSE_MOD_ALT) {
		strcat(buffer, first ? "Alt" : "|Alt");
		first = 0;
	}
	if (mods & TERSE_MOD_META) {
		strcat(buffer, first ? "Meta" : "|Meta");
		first = 0;
	}
	if (buffer[0] == '\0') {
		strcpy(buffer, "(none)");
	}
	return buffer;
}

static size_t encode_utf8(unsigned int scalar, char *dest)
{
	if (scalar <= 0x7f) {
		dest[0] = (char)scalar;
		return 1;
	}
	if (scalar <= 0x7ff) {
		dest[0] = (char)(0xc0 | ((scalar >> 6) & 0x1f));
		dest[1] = (char)(0x80 | (scalar & 0x3f));
		return 2;
	}
	if (scalar <= 0xffff) {
		dest[0] = (char)(0xe0 | ((scalar >> 12) & 0x0f));
		dest[1] = (char)(0x80 | ((scalar >> 6) & 0x3f));
		dest[2] = (char)(0x80 | (scalar & 0x3f));
		return 3;
	}
	if (scalar <= 0x10ffff) {
		dest[0] = (char)(0xf0 | ((scalar >> 18) & 0x07));
		dest[1] = (char)(0x80 | ((scalar >> 12) & 0x3f));
		dest[2] = (char)(0x80 | ((scalar >> 6) & 0x3f));
		dest[3] = (char)(0x80 | (scalar & 0x3f));
		return 4;
	}
	dest[0] = '?';
	return 1;
}

int main(void)
{
	if (install_raw_terminal() != 0) {
		fprintf(stderr, "Failed to set raw mode.\r\n");
		return 1;
	}

#ifdef _WIN32
	terse_options_t options = {
		.input_fd = 0,
		.output_fd = 1,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
#else
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
#endif
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\r\n");
		return 1;
	}

	printf("=== Keyboard Features Test ===\r\n\r\n");

	// Check what features are supported
	unsigned int supported = terse_keyboard_get_supported(handle);
	printf("Supported keyboard features:\r\n");
	if (supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		printf("  - MODIFY_OTHER_KEYS\r\n");
	}
	if (supported & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		printf("  - KITTY_PROTOCOL\r\n");
	}
	if (supported == 0) {
		printf("  (none)\r\n");
	}
	printf("\r\n");

	// Check what features are currently enabled
	unsigned int enabled = terse_keyboard_get_enabled(handle);
	printf("Currently enabled keyboard features:\r\n");
	if (enabled & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		printf("  - MODIFY_OTHER_KEYS\r\n");
	}
	if (enabled & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		printf("  - KITTY_PROTOCOL\r\n");
	}
	if (enabled == 0) {
		printf("  (none)\r\n");
	}
	printf("\r\n");

	// Try to enable Kitty protocol if supported
	if (supported & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		printf("Attempting to enable Kitty Keyboard Protocol...\r\n");
		int rc = terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL);
		if (rc == 0) {
			printf("  Success!\r\n");
		} else {
			printf("  Failed with error code: %d\r\n", rc);
		}
		printf("\r\n");
	}

	// Try to enable ModifyOtherKeys if supported
	if (supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		printf("Attempting to enable ModifyOtherKeys...\r\n");
		int rc = terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS);
		if (rc == 0) {
			printf("  Success!\r\n");
		} else {
			printf("  Failed with error code: %d\r\n", rc);
		}
		printf("\r\n");
	}

	printf("Now test with keyboard input:\r\n");
	printf("Press 'b', 'Shift+B', 'Ctrl+B', 'Ctrl+Shift+B'\r\n");
	printf("Press Ctrl+C to exit.\r\n\r\n");

	while (1) {
		terse_event_t event;
		int rc = terse_read_event(handle, -1, &event);
		if (rc != 0) {
			continue;
		}

		if (event.type == TERSE_EVENT_CHAR) {
			char utf8[5] = { 0 };
			size_t len = encode_utf8(event.data.ch.scalar, utf8);
			utf8[len] = '\0';
			printf("CHAR scalar=U+%04X text='%s' mods=%s\r\n",
			       (unsigned int)event.data.ch.scalar,
			       utf8,
			       mod_string(event.data.ch.mods));

			// Check for Ctrl+C to exit
			if ((event.data.ch.mods & TERSE_MOD_CTRL) &&
			    (event.data.ch.scalar == 'C' || event.data.ch.scalar == 'c')) {
				printf("Exiting.\r\n");
				break;
			}
		} else if (event.type == TERSE_EVENT_ARROW_UP ||
		           event.type == TERSE_EVENT_ARROW_DOWN ||
		           event.type == TERSE_EVENT_ARROW_LEFT ||
		           event.type == TERSE_EVENT_ARROW_RIGHT) {
			printf("ARROW_%s mods=%s\r\n",
			       (event.type == TERSE_EVENT_ARROW_UP) ? "UP" : (event.type == TERSE_EVENT_ARROW_DOWN) ? "DOWN"
			                                                 : (event.type == TERSE_EVENT_ARROW_LEFT)   ? "LEFT"
			                                                                                            : "RIGHT",
			       mod_string(event.data.key.mods));
		}
	}

	terse_close(handle);
	return 0;
}
