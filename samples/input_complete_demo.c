#include "terse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static const char *mod_string(int mods)
{
	static char buffer[64];
	buffer[0] = '\0';
	int first = 1;
	if (mods & TERSE_MOD_SHIFT) {
		strcat(buffer, first ? "Shift" : "+Shift");
		first = 0;
	}
	if (mods & TERSE_MOD_CTRL) {
		strcat(buffer, first ? "Ctrl" : "+Ctrl");
		first = 0;
	}
	if (mods & TERSE_MOD_ALT) {
		strcat(buffer, first ? "Alt" : "+Alt");
		first = 0;
	}
	if (mods & TERSE_MOD_META) {
		strcat(buffer, first ? "Meta" : "+Meta");
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

static void display_event(const terse_event_t *event)
{
	switch (event->type) {
	case TERSE_EVENT_CHAR:
	{
		char utf8[5] = { 0 };
		size_t len = encode_utf8(event->data.ch.scalar, utf8);
		utf8[len] = '\0';
		if (event->data.ch.mods) {
			printf("[CHAR] '%s' (U+%04X) Modifiers: %s\r\n",
				utf8,
				(unsigned int)event->data.ch.scalar,
				mod_string(event->data.ch.mods));
		} else {
			printf("[CHAR] '%s' (U+%04X)\r\n",
				utf8,
				(unsigned int)event->data.ch.scalar);
		}
		break;
	}
	case TERSE_EVENT_ENTER:
		if (event->data.key.mods) {
			printf("[ENTER] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		} else {
			printf("[ENTER]\r\n");
		}
		break;
	case TERSE_EVENT_BACKSPACE:
		if (event->data.key.mods) {
			printf("[BACKSPACE] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		} else {
			printf("[BACKSPACE]\r\n");
		}
		break;
	case TERSE_EVENT_TAB:
		if (event->data.key.mods) {
			printf("[TAB] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		} else {
			printf("[TAB]\r\n");
		}
		break;
	case TERSE_EVENT_ARROW_UP:
		printf("[ARROW UP] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_ARROW_DOWN:
		printf("[ARROW DOWN] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_ARROW_LEFT:
		printf("[ARROW LEFT] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_ARROW_RIGHT:
		printf("[ARROW RIGHT] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_HOME:
		printf("[HOME] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_END:
		printf("[END] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_PAGE_UP:
		printf("[PAGE UP] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_PAGE_DOWN:
		printf("[PAGE DOWN] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_INSERT:
		printf("[INSERT] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_DELETE:
		printf("[DELETE] Modifiers: %s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_FUNCTION:
		printf("[FUNCTION KEY] F%d Modifiers: %s\r\n",
			event->data.function.number,
			mod_string(event->data.function.mods));
		break;
	case TERSE_EVENT_RESIZE:
		printf("[RESIZE] %d rows x %d cols\r\n",
			event->data.resize.rows,
			event->data.resize.cols);
		break;
	case TERSE_EVENT_RAW_SEQUENCE:
	{
		printf("[RAW SEQUENCE] %zu bytes: ", event->data.raw.length);
		for (size_t i = 0; i < event->data.raw.length; ++i) {
			printf("%02X ", event->data.raw.bytes[i]);
		}
		printf("\r\n");
		break;
	}
	default:
		printf("[UNKNOWN EVENT] type=%d\r\n", event->type);
		break;
	}
	fflush(stdout);
}

int main(void)
{
	if (install_raw_terminal() != 0) {
		fprintf(stderr, "Failed to set raw mode.\r\n");
		return 1;
	}

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P2, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\r\n");
		return 1;
	}

	/* Enable enhanced keyboard protocols if supported */
	unsigned int keyboard_supported = terse_keyboard_get_supported(handle);
	if (keyboard_supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		(void)terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS);
	}
	if (keyboard_supported & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		(void)terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL);
	}

	terse_show_cursor(handle, 0);

	printf("========================================\r\n");
	printf("  Complete Input Events Demonstration\r\n");
	printf("========================================\r\n\r\n");
	printf("This demo displays all keyboard input events.\r\n\r\n");
	printf("Try the following:\r\n");
	printf("  - Function keys: F1-F12\r\n");
	printf("  - Navigation: Home, End, PageUp, PageDown, Insert, Delete\r\n");
	printf("  - Arrow keys: Up, Down, Left, Right\r\n");
	printf("  - Special keys: Enter, Tab, Backspace\r\n");
	printf("  - Modifier combinations: Shift, Ctrl, Alt, Meta\r\n");
	printf("  - Regular characters with modifiers\r\n\r\n");
	printf("Press ESC to exit.\r\n");
	printf("========================================\r\n\r\n");
	fflush(stdout);

	while (1) {
		terse_event_t event;
		int rc = terse_read_event(handle, -1, &event);
		if (rc == TERSE_EVENT_NONE) {
			continue;
		}
		if (rc < 0) {
			terse_error_t err = terse_get_last_error(handle);
			fprintf(stderr, "read_event failed: %d\r\n", err);
			break;
		}

		display_event(&event);

		/* Exit on ESC key */
		if (event.type == TERSE_EVENT_CHAR &&
		    event.data.ch.scalar == 0x1B &&
		    event.data.ch.mods == 0) {
			printf("\r\nESC pressed. Exiting...\r\n");
			break;
		}
	}

	terse_show_cursor(handle, 1);
	terse_close(handle);
	return 0;
}
