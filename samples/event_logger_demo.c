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
	static char buffer[32];
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

static void log_event(const terse_event_t *event)
{
	switch (event->type) {
	case TERSE_EVENT_CHAR:
	{
		char utf8[5] = { 0 };
		size_t len = encode_utf8(event->data.ch.scalar, utf8);
		utf8[len] = '\0';
		printf("CHAR scalar=U+%04X text='%s' width=%d mods=%s\r\n",
			(unsigned int)event->data.ch.scalar,
			utf8,
			event->data.ch.width,
			mod_string(event->data.ch.mods));
		break;
	}
	case TERSE_EVENT_ENTER:
		printf("ENTER mods=%s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_BACKSPACE:
		printf("BACKSPACE mods=%s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_TAB:
		printf("TAB mods=%s\r\n", mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_HOME:
	case TERSE_EVENT_END:
	case TERSE_EVENT_PAGE_UP:
	case TERSE_EVENT_PAGE_DOWN:
	case TERSE_EVENT_INSERT:
	case TERSE_EVENT_DELETE:
	case TERSE_EVENT_ARROW_UP:
	case TERSE_EVENT_ARROW_DOWN:
	case TERSE_EVENT_ARROW_LEFT:
	case TERSE_EVENT_ARROW_RIGHT:
		printf("%s mods=%s\r\n",
			(event->type == TERSE_EVENT_HOME) ? "HOME" :
			(event->type == TERSE_EVENT_END) ? "END" :
			(event->type == TERSE_EVENT_PAGE_UP) ? "PAGE_UP" :
			(event->type == TERSE_EVENT_PAGE_DOWN) ? "PAGE_DOWN" :
			(event->type == TERSE_EVENT_INSERT) ? "INSERT" :
			(event->type == TERSE_EVENT_DELETE) ? "DELETE" :
			(event->type == TERSE_EVENT_ARROW_UP) ? "ARROW_UP" :
			(event->type == TERSE_EVENT_ARROW_DOWN) ? "ARROW_DOWN" :
			(event->type == TERSE_EVENT_ARROW_LEFT) ? "ARROW_LEFT" : "ARROW_RIGHT",
			mod_string(event->data.key.mods));
		break;
	case TERSE_EVENT_FUNCTION:
		printf("FUNCTION F%d mods=%s\r\n",
			 event->data.function.number,
			 mod_string(event->data.function.mods));
		break;
	case TERSE_EVENT_MOUSE_DOWN:
	case TERSE_EVENT_MOUSE_UP:
	case TERSE_EVENT_MOUSE_MOVE:
	case TERSE_EVENT_MOUSE_SCROLL:
		printf("MOUSE %s button=%d row=%d col=%d mods=%s\r\n",
			(event->type == TERSE_EVENT_MOUSE_DOWN) ? "DOWN" :
			(event->type == TERSE_EVENT_MOUSE_UP) ? "UP" :
			(event->type == TERSE_EVENT_MOUSE_MOVE) ? "MOVE" : "SCROLL",
			event->data.mouse.button,
			event->data.mouse.row,
			event->data.mouse.col,
			mod_string(event->data.mouse.mods));
		break;
	case TERSE_EVENT_PASTE_BEGIN:
		printf("PASTE_BEGIN\r\n");
		break;
	case TERSE_EVENT_PASTE_END:
		printf("PASTE_END\r\n");
		break;
	case TERSE_EVENT_RESIZE:
		printf("RESIZE rows=%d cols=%d\r\n",
			event->data.resize.rows,
			event->data.resize.cols);
		break;
	case TERSE_EVENT_RAW_SEQUENCE:
	{
		printf("RAW bytes=");
		for (size_t i = 0; i < (size_t)event->data.raw.length; ++i) {
			printf("%s%02X", (i == 0) ? "" : " ", event->data.raw.bytes[i]);
		}
		printf("\r\n");
		break;
	}
	default:
		printf("UNKNOWN type=%d\r\n", event->type);
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

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\r\n");
		return 1;
	}
	unsigned int keyboard_supported = terse_keyboard_get_supported(handle);
	if (keyboard_supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		(void)terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS);
	}
	if (keyboard_supported & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		(void)terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL);
	}

	printf("Event logger demo. Press Ctrl+C to exit.\r\n");
	fflush(stdout);

	terse_state_t saved_state;
	(void)terse_capture_state(handle, &saved_state);

	while (1) {
		terse_event_t event;
		int rc = terse_read_event(handle, -1, &event);
		if (rc == TERSE_EVENT_NONE) {
			continue;
		}
		if (rc < 0) {
			terse_error_info_t info = terse_get_last_error(handle);
			fprintf(stderr, "read_event failed: category=%d code=%d\r\n", info.category, info.code);
			break;
		}
		log_event(&event);
		if (event.type == TERSE_EVENT_CHAR &&
		    (event.data.ch.mods & TERSE_MOD_CTRL) &&
		    (event.data.ch.mods & (TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_META)) == 0 &&
		    (event.data.ch.scalar == 'C' || event.data.ch.scalar == 'c')) {
			printf("Ctrl+C detected, exiting.\r\n");
			break;
		}
	}

	(void)terse_restore_state(handle, &saved_state);
	terse_close(handle);
	return 0;
}
