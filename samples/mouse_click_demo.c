#include "terse.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raw_terminal.h"
#include "sample_compat.h"

typedef struct {
	int row;
	int col;
	terse_mouse_button_t button;
} mark_t;

#define MAX_MARKS 100
static mark_t marks[MAX_MARKS];
static int mark_count = 0;
static volatile sig_atomic_t running = 1;

static void handle_signal(int sig)
{
	(void)sig;
	running = 0;
}

static void redraw_screen(terse_handle_t handle, const char *last_event_msg)
{
	terse_size_t size = terse_get_size(handle);
	if (!size.known) {
		return;
	}

	int rows = size.rows;
	int cols = size.cols;

	terse_clear_screen(handle, TERSE_CLEAR_ALL);

	// Draw header
	terse_move_to(handle, 0, 0);
	terse_write_text(handle, "Mouse Click Demo - Press 'q' to quit, 'c' to clear");

	// Draw all marks
	for (int i = 0; i < mark_count; i++) {
		if (marks[i].row >= 2 && marks[i].row < rows && marks[i].col >= 0 && marks[i].col < cols) {
			terse_move_to(handle, marks[i].row, marks[i].col);
			switch (marks[i].button) {
			case TERSE_MOUSE_BUTTON_LEFT:
				terse_write_text(handle, "\xE2\x97\x8F"); /* ● U+25CF */
				break;
			case TERSE_MOUSE_BUTTON_RIGHT:
				terse_write_text(handle, "\xE2\x97\x8B"); /* ○ U+25CB */
				break;
			case TERSE_MOUSE_BUTTON_MIDDLE:
				terse_write_text(handle, "\xE2\x97\x86"); /* ◆ U+25C6 */
				break;
			default:
				break;
			}
		}
	}

	// Draw status line at bottom
	if (last_event_msg && last_event_msg[0]) {
		terse_move_to(handle, rows-1, 0);
		terse_clear_line(handle, TERSE_CLEAR_ALL);
		terse_move_to(handle, rows-1, 0);
		terse_write_text(handle, last_event_msg);
	}

	// Move cursor to bottom-right corner to keep it out of the way
	terse_move_to(handle, rows-1, cols-1);
	terse_flush(handle);
}

int main(void)
{
	// Install raw mode first
	if (install_raw_terminal() < 0) {
		fprintf(stderr, "Failed to set raw terminal mode\n");
		return 1;
	}

	signal(SIGINT, handle_signal);

	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0
	};

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	if (!handle) {
		fprintf(stderr, "Failed to initialize terse\n");
		return 1;
	}

	// Check if mouse support is available
	terse_capabilities_t caps = terse_get_capabilities(handle);
	if (caps.mouse == TERSE_MOUSE_NONE) {
		fprintf(stderr, "This terminal does not support mouse tracking.\n");
		terse_close(handle);
		return 1;
	}

	// Enable mouse tracking
	if (terse_enable_mouse(handle, TERSE_MOUSE_SGR) < 0) {
		fprintf(stderr, "Failed to enable mouse tracking\n");
		terse_close(handle);
		return 1;
	}

	// Hide cursor for cleaner display
	terse_show_cursor(handle, 0);

	char last_event_msg[256] = {0};
	redraw_screen(handle, NULL);

	while (running) {
		terse_event_t event;
		int result = terse_read_event(handle, 100, &event);

		if (result == TERSE_ERR_NO_EVENT) {
			continue;
		}
		if (result < 0) {
			break;
		}

		if (event.type == TERSE_EVENT_MOUSE_DOWN) {
			// Add mark at click position
			if (mark_count < MAX_MARKS) {
				marks[mark_count].row = event.data.mouse.row;
				marks[mark_count].col = event.data.mouse.col;
				marks[mark_count].button = event.data.mouse.button;
				mark_count++;
			} else {
				// FIFO: remove oldest mark
				memmove(&marks[0], &marks[1], sizeof(mark_t) * (MAX_MARKS - 1));
				marks[MAX_MARKS - 1].row = event.data.mouse.row;
				marks[MAX_MARKS - 1].col = event.data.mouse.col;
				marks[MAX_MARKS - 1].button = event.data.mouse.button;
			}

			const char *button_name = "Unknown";
			if (event.data.mouse.button == TERSE_MOUSE_BUTTON_LEFT) {
				button_name = "Left";
			} else if (event.data.mouse.button == TERSE_MOUSE_BUTTON_RIGHT) {
				button_name = "Right";
			} else if (event.data.mouse.button == TERSE_MOUSE_BUTTON_MIDDLE) {
				button_name = "Middle";
			}

			snprintf(last_event_msg, sizeof(last_event_msg),
			         "Last: %s Button Click at (%d,%d)", button_name, event.data.mouse.row, event.data.mouse.col);
			redraw_screen(handle, last_event_msg);
		} else if (event.type == TERSE_EVENT_MOUSE_SCROLL) {
			const char *dir = "Unknown";
			if (event.data.mouse.button == TERSE_MOUSE_BUTTON_SCROLL_UP) {
				dir = "Up";
			} else if (event.data.mouse.button == TERSE_MOUSE_BUTTON_SCROLL_DOWN) {
				dir = "Down";
			}

			snprintf(last_event_msg, sizeof(last_event_msg),
			         "Scroll %s at (%d,%d)", dir, event.data.mouse.row, event.data.mouse.col);
			redraw_screen(handle, last_event_msg);
		} else if (event.type == TERSE_EVENT_CHAR) {
			if (event.data.ch.scalar == 'q' || event.data.ch.scalar == 'Q') {
				break;
			} else if (event.data.ch.scalar == 'c' || event.data.ch.scalar == 'C') {
				mark_count = 0;
				last_event_msg[0] = '\0';
				redraw_screen(handle, NULL);
			}
		} else if (event.type == TERSE_EVENT_RESIZE) {
			redraw_screen(handle, last_event_msg);
		}
	}

	// Cleanup
	terse_disable_mouse(handle);
	terse_show_cursor(handle, 1);
	terse_clear_screen(handle, TERSE_CLEAR_ALL);
	terse_move_to(handle, 0, 0);
	terse_flush(handle);
	terse_close(handle);

	return 0;
}
