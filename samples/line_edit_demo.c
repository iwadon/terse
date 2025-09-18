#include "terse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define BUFFER_CAPACITY 1024

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

static void print_error(terse_handle_t handle, const char *label)
{
	terse_error_info_t err = terse_get_last_error(handle);
	fprintf(stderr, "%s failed: category=%d code=%d\n", label, err.category, err.code);
}

static void render_line(terse_handle_t handle, int row, const char *prompt, const char *buffer, size_t cursor_pos)
{
	if (terse_move_to(handle, row, 1) < 0) {
		print_error(handle, "move_to");
		return;
	}
	if (terse_clear_line(handle, TERSE_CLEAR_AFTER) < 0) {
		print_error(handle, "clear_line");
	}
	terse_style_t bold = terse_style_default();
	bold.effects = TERSE_STYLE_BOLD;
	if (terse_set_style(handle, &bold) < 0) {
		print_error(handle, "set_style");
	}
	if (terse_write_text(handle, prompt) < 0) {
		print_error(handle, "write_text");
	}
	terse_style_t reset = terse_style_default();
	if (terse_set_style(handle, &reset) < 0) {
		print_error(handle, "reset_style");
	}
	if (terse_write_text(handle, buffer) < 0) {
		print_error(handle, "write_text");
	}
	int col = (int)strlen(prompt) + (int)cursor_pos + 1;
	if (terse_move_to(handle, row, col) < 0) {
		print_error(handle, "move_to");
	}
}

static void line_edit_loop(terse_handle_t handle)
{
	char buffer[BUFFER_CAPACITY];
	size_t length = 0;
	size_t cursor = 0;
	buffer[0] = '\0';

	const char *prompt = "edit> ";
	render_line(handle, 2, prompt, buffer, cursor);

	while (1) {
		terse_event_t event;
		int rc = terse_read_event(handle, -1, &event);
		if (rc == TERSE_EVENT_NONE) {
			continue;
		}
		if (rc < 0) {
			print_error(handle, "read_event");
			break;
		}

		if (event.type == TERSE_EVENT_CHAR) {
			unsigned int ch = event.data.ch.scalar;
			if ((event.data.ch.mods & TERSE_MOD_CTRL) != 0) {
				if (ch == 'C') {
					break;
				}
				continue;
			}
			if (isprint((int)ch) && length + 1 < BUFFER_CAPACITY) {
				memmove(buffer + cursor + 1, buffer + cursor, length - cursor + 1);
				buffer[cursor] = (char)ch;
				cursor++;
				length++;
			}
		} else if (event.type == TERSE_EVENT_BACKSPACE) {
			if (cursor > 0) {
				memmove(buffer + cursor - 1, buffer + cursor, length - cursor + 1);
				cursor--;
				length--;
			}
		} else if (event.type == TERSE_EVENT_ARROW_LEFT) {
			if (cursor > 0) {
				cursor--;
			}
		} else if (event.type == TERSE_EVENT_ARROW_RIGHT) {
			if (cursor < length) {
				cursor++;
			}
		} else if (event.type == TERSE_EVENT_ENTER) {
			break;
		}

		render_line(handle, 2, prompt, buffer, cursor);
	}

	if (terse_move_to(handle, 4, 1) < 0) {
		print_error(handle, "move_to");
	}
	if (terse_clear_line(handle, TERSE_CLEAR_AFTER) < 0) {
		print_error(handle, "clear_line");
	}
	if (terse_write_text(handle, "You entered: ") < 0) {
		print_error(handle, "write_text");
	}
	if (terse_write_text(handle, buffer) < 0) {
		print_error(handle, "write_text");
	}
	if (terse_write_text(handle, "\n") < 0) {
		print_error(handle, "write_text");
	}
}

int main(void)
{
	if (install_raw_terminal() != 0) {
		fprintf(stderr, "Failed to enable raw mode.\n");
		return 1;
	}

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\n");
		return 1;
	}

	terse_state_t saved_state;
	if (terse_capture_state(handle, &saved_state) < 0) {
		print_error(handle, "capture_state");
	}

	if (terse_show_cursor(handle, 1) < 0) {
		print_error(handle, "show_cursor");
	}
	if (terse_clear_screen(handle, TERSE_CLEAR_ALL) < 0) {
		print_error(handle, "clear_screen");
	}
	if (terse_move_to(handle, 1, 1) < 0) {
		print_error(handle, "move_to");
	}
	if (terse_write_text(handle, "Line Editing Demo (press Enter to finish, Ctrl+C to abort)\n") < 0) {
		print_error(handle, "write_text");
	}

	line_edit_loop(handle);

	if (terse_restore_state(handle, &saved_state) < 0) {
		print_error(handle, "restore_state");
	}
	terse_close(handle);
	return 0;
}
