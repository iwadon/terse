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
#else
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static int g_raw_installed = 0;
#endif

#define BUFFER_CAPACITY 1024
#define UTF8_BUFFER_CAPACITY (BUFFER_CAPACITY * 4 + 1)

typedef struct glyph {
	unsigned int scalar;
	int width;
	unsigned int combining[8];
	size_t combining_count;
} glyph_t;

typedef struct line_buffer {
	glyph_t glyphs[BUFFER_CAPACITY];
	size_t length; // glyph count
	size_t cursor; // glyph index
} line_buffer_t;

#ifdef _WIN32

static void restore_terminal(void)
{
	if (g_raw_installed) {
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleMode(hStdin, g_original_input_mode);
		SetConsoleMode(hStdout, g_original_output_mode);

		/* Restore original code pages */
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

	/* Save and set console code page to UTF-8 for proper Unicode handling */
	g_original_cp = GetConsoleCP();
	g_original_output_cp = GetConsoleOutputCP();

	if (!SetConsoleCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleCP(65001) failed (error %lu)\n", GetLastError());
	}
	if (!SetConsoleOutputCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleOutputCP(65001) failed (error %lu)\n", GetLastError());
	}

	if (!GetConsoleMode(hStdin, &g_original_input_mode)) {
		fprintf(stderr, "GetConsoleMode(input) failed (error %lu)\n", GetLastError());
		return -1;
	}

	if (!GetConsoleMode(hStdout, &g_original_output_mode)) {
		fprintf(stderr, "GetConsoleMode(output) failed (error %lu)\n", GetLastError());
		return -1;
	}

	/* Setup input mode: disable line input, echo, processed input, and quick edit */
	DWORD dwInputMode = g_original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
	/* Disable quick edit mode to prevent Ctrl+A selection and allow mouse input */
	dwInputMode = (dwInputMode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
	/* NOTE: ENABLE_VIRTUAL_TERMINAL_INPUT breaks ReadConsoleInput() modifier detection
	 * For now, keep it disabled to allow proper KEY_EVENT_RECORD handling
	 */
	/* dwInputMode |= ENABLE_VIRTUAL_TERMINAL_INPUT; */

	if (!SetConsoleMode(hStdin, dwInputMode)) {
		fprintf(stderr, "SetConsoleMode(input) failed (error %lu)\n", GetLastError());
		return -1;
	}

	/* Setup output mode: enable virtual terminal processing */
	DWORD dwOutputMode = g_original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (!SetConsoleMode(hStdout, dwOutputMode)) {
		fprintf(stderr, "SetConsoleMode(output) failed (error %lu)\n", GetLastError());
		return -1;
	}

	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}

#else  /* POSIX */

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

#endif  /* _WIN32 */

static void print_error(terse_handle_t handle, const char *label)
{
	terse_error_t err = terse_get_last_error(handle);
	fprintf(stderr, "%s failed: %d\n", label, err);
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
		int capacity_exhausted = 0;
		for (size_t c = 0; c < glyphs[i].combining_count; ++c) {
			len = encode_utf8(glyphs[i].combining[c], temp);
			if (written + len >= capacity) {
				capacity_exhausted = 1;
				break;
			}
			memcpy(out + written, temp, len);
			written += len;
		}
		if (capacity_exhausted) {
			break;
		}
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

	if (terse_move_to(handle, row, 0) < 0) {
		print_error(handle, "move_to");
		return;
	}
	if (terse_clear_line(handle, TERSE_CLEAR_AFTER) < 0) {
		print_error(handle, "clear_line");
	}
	// Use color for the prompt
	terse_style_t prompt_style = terse_style_default();
	prompt_style.effects = TERSE_STYLE_BOLD;
	prompt_style.foreground = terse_color_basic(TERSE_BASIC_COLOR_CYAN, 1);
	if (terse_set_style(handle, &prompt_style) < 0) {
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
	int col = (int)strlen(prompt) + glyphs_display_width(line->glyphs, line->cursor);
	if (terse_move_to(handle, row, col) < 0) {
		print_error(handle, "move_to");
	}
}

static void line_edit_loop(terse_handle_t handle)
{
	line_buffer_t line = { 0 };
	char utf8_snapshot[UTF8_BUFFER_CAPACITY];
	int in_paste = 0;

	const char *prompt = "edit> ";
	render_line(handle, 1, prompt, &line);

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
				} else if (ch == 'A') {
					// Ctrl+A: Move to beginning of line
					line.cursor = 0;
				} else if (ch == 'E') {
					// Ctrl+E: Move to end of line
					line.cursor = line.length;
				} else if (ch == 'U') {
					// Ctrl+U: Delete from cursor to beginning of line
					if (line.cursor > 0) {
						memmove(&line.glyphs[0], &line.glyphs[line.cursor], (line.length - line.cursor) * sizeof(glyph_t));
						line.length -= line.cursor;
						line.cursor = 0;
					}
				} else if (ch == 'K') {
					// Ctrl+K: Delete from cursor to end of line
					line.length = line.cursor;
				} else if (ch == 'W') {
					// Ctrl+W: Delete word before cursor
					if (line.cursor > 0) {
						size_t start = line.cursor;
						// Skip trailing whitespace
						while (start > 0 && line.glyphs[start - 1].scalar == ' ') {
							start--;
						}
						// Delete word
						while (start > 0 && line.glyphs[start - 1].scalar != ' ') {
							start--;
						}
						if (start < line.cursor) {
							memmove(&line.glyphs[start], &line.glyphs[line.cursor], (line.length - line.cursor) * sizeof(glyph_t));
							line.length -= (line.cursor - start);
							line.cursor = start;
						}
					}
				} else {
					continue;
				}
				render_line(handle, 1, prompt, &line);
				continue;
			}
			if (line.length < BUFFER_CAPACITY) {
				int width = event.data.ch.width > 0 ? event.data.ch.width : 0;
				if (width == 0) {
					if (line.cursor == 0) {
						continue;
					}
					glyph_t *base = &line.glyphs[line.cursor - 1];
					if (base->combining_count < sizeof(base->combining) / sizeof(base->combining[0])) {
						base->combining[base->combining_count++] = ch;
						render_line(handle, 1, prompt, &line);
					}
					continue;
				}
				glyph_t glyph = {
					.scalar = ch,
					.width = width,
					.combining_count = 0,
				};
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
		} else if (event.type == TERSE_EVENT_DELETE) {
			if (line.cursor < line.length) {
				memmove(&line.glyphs[line.cursor], &line.glyphs[line.cursor + 1], (line.length - line.cursor - 1) * sizeof(glyph_t));
				line.length--;
			}
		} else if (event.type == TERSE_EVENT_HOME) {
			line.cursor = 0;
		} else if (event.type == TERSE_EVENT_END) {
			line.cursor = line.length;
		} else if (event.type == TERSE_EVENT_ARROW_LEFT) {
			if (line.cursor > 0) {
				line.cursor--;
			}
		} else if (event.type == TERSE_EVENT_ARROW_RIGHT) {
			if (line.cursor < line.length) {
				line.cursor++;
			}
		} else if (event.type == TERSE_EVENT_RESIZE) {
			// Just re-render on resize
		} else if (event.type == TERSE_EVENT_PASTE_BEGIN) {
			in_paste = 1;
			continue; // Don't re-render yet
		} else if (event.type == TERSE_EVENT_PASTE_END) {
			in_paste = 0;
			// Re-render once after paste completes
		} else if (event.type == TERSE_EVENT_ENTER) {
			break;
		}

		// Skip re-rendering during paste for better performance
		if (!in_paste) {
			render_line(handle, 1, prompt, &line);
		}
	}

	if (terse_move_to(handle, 3, 0) < 0) {
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

#ifdef _WIN32
	terse_options_t options = {
		.input_fd = 0,   /* stdin */
		.output_fd = 1,  /* stdout */
		.codec_name = codec,
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_BRACKETED_PASTE | TERSE_CAP_ENABLE_CURSOR_SHAPE
	};
#else
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = codec,
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_BRACKETED_PASTE | TERSE_CAP_ENABLE_CURSOR_SHAPE
	};
#endif

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	if (!handle) {
		fprintf(stderr, "terse_open failed\n");
		return 1;
	}

	// Enable bracketed paste mode
	if (terse_enable_bracketed_paste(handle) < 0) {
		print_error(handle, "enable_bracketed_paste");
	}

	if (terse_push_state(handle) < 0) {
		print_error(handle, "push_state");
	}

	if (terse_show_cursor(handle, 1) < 0) {
		print_error(handle, "show_cursor");
	}
	// Set cursor to bar shape for insert mode
	if (terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_BAR, 1) < 0) {
		print_error(handle, "set_cursor_shape");
	}
	if (terse_clear_screen(handle, TERSE_CLEAR_ALL) < 0) {
		print_error(handle, "clear_screen");
	}
	if (terse_move_to(handle, 0, 0) < 0) {
		print_error(handle, "move_to");
	}
	if (terse_write_text(handle, "Line Editing Demo (press Enter to finish, Ctrl+C to abort)\n") < 0) {
		print_error(handle, "write_text");
	}

	line_edit_loop(handle);

	if (terse_pop_state(handle) < 0) {
		print_error(handle, "pop_state");
	}
	terse_close(handle);
	return 0;
}
