#include "terse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define BUFFER_CAPACITY 1024
#define UTF8_BUFFER_CAPACITY (BUFFER_CAPACITY * 4 + 1)

static struct termios g_original_termios;
static int g_raw_installed = 0;

typedef struct glyph {
	unsigned int scalar;
	int width;
} glyph_t;

typedef struct line_buffer {
	glyph_t glyphs[BUFFER_CAPACITY];
	size_t length; // glyph count
	size_t cursor; // glyph index
} line_buffer_t;

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

static size_t glyphs_to_utf8(const glyph_t *glyphs, size_t count, char *out, size_t capacity)
{
	size_t written = 0;
	for (size_t i = 0; i < count; ++i) {
		char temp[4] = { 0 };
		size_t len = encode_utf8(glyphs[i].scalar, temp);
		if (written + len >= capacity) {
			break;
		}
		memcpy(out + written, temp, len);
		written += len;
	}
	if (capacity > 0) {
		out[written < (capacity - 1) ? written : (capacity - 1)] = '\0';
	}
	return written;
}

static int glyphs_display_width(const glyph_t *glyphs, size_t count)
{
	int width = 0;
	for (size_t i = 0; i < count; ++i) {
		if (glyphs[i].width > 0) {
			width += glyphs[i].width;
		}
	}
	return width;
}

static void render_line(terse_handle_t handle, int row, const char *prompt, const line_buffer_t *line)
{
	char utf8_buffer[UTF8_BUFFER_CAPACITY];
	glyphs_to_utf8(line->glyphs, line->length, utf8_buffer, sizeof(utf8_buffer));

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
	if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
		print_error(handle, "reset_style");
	}
	if (terse_write_text(handle, utf8_buffer) < 0) {
		print_error(handle, "write_text");
	}
	int col = (int)strlen(prompt) + glyphs_display_width(line->glyphs, line->cursor) + 1;
	if (terse_move_to(handle, row, col) < 0) {
		print_error(handle, "move_to");
	}
}

static void line_edit_loop(terse_handle_t handle)
{
	line_buffer_t line = { 0 };
	char utf8_snapshot[UTF8_BUFFER_CAPACITY];

	const char *prompt = "edit> ";
	render_line(handle, 2, prompt, &line);

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
			if (line.length < BUFFER_CAPACITY) {
				glyph_t glyph = {
					.scalar = ch,
					.width = event.data.ch.width > 0 ? event.data.ch.width : 0,
				};
				if (glyph.width == 0 && line.length == 0) {
					continue;
				}
				memmove(&line.glyphs[line.cursor + 1], &line.glyphs[line.cursor], (line.length - line.cursor) * sizeof(glyph_t));
				line.glyphs[line.cursor] = glyph;
				line.cursor++;
				line.length++;
			}
		} else if (event.type == TERSE_EVENT_BACKSPACE) {
			if (line.cursor > 0) {
				memmove(&line.glyphs[line.cursor - 1], &line.glyphs[line.cursor], (line.length - line.cursor) * sizeof(glyph_t));
				line.cursor--;
				line.length--;
			}
		} else if (event.type == TERSE_EVENT_ARROW_LEFT) {
			if (line.cursor > 0) {
				line.cursor--;
			}
		} else if (event.type == TERSE_EVENT_ARROW_RIGHT) {
			if (line.cursor < line.length) {
				line.cursor++;
			}
		} else if (event.type == TERSE_EVENT_ENTER) {
			break;
		}

		render_line(handle, 2, prompt, &line);
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
	glyphs_to_utf8(line.glyphs, line.length, utf8_snapshot, sizeof(utf8_snapshot));
	if (terse_write_text(handle, utf8_snapshot) < 0) {
		print_error(handle, "write_text");
	}
	if (terse_write_text(handle, "\n") < 0) {
		print_error(handle, "write_text");
	}
}

int main(int argc, char **argv)
{
	if (install_raw_terminal() != 0) {
		fprintf(stderr, "Failed to enable raw mode.\n");
		return 1;
	}

	const char *codec = "UTF-8";
	if (argc > 1) {
		if (strcmp(argv[1], "--shift-jis") == 0 || strcmp(argv[1], "--sjis") == 0) {
			codec = "Shift_JIS";
		}
	}

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = codec,
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
