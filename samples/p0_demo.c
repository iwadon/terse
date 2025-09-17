#include "terse.h"

#include <stdio.h>
#include <unistd.h>

static void wait_briefly(void)
{
	usleep(200000); // 200ms for visual effect
}

static void print_error(const char *label, terse_handle_t handle)
{
	terse_error_info_t err = terse_get_last_error(handle);
	if (err.category != TERSE_ERROR_NONE) {
		fprintf(stderr, "%s failed: category=%d code=%d\n", label, err.category, err.code);
	}
}

static void write_line(terse_handle_t handle, int row, int col, unsigned int style, const char *text)
{
	if (terse_move_to(handle, row, col) < 0) {
		print_error("move_to", handle);
		return;
	}
	if (terse_set_style(handle, style) < 0) {
		print_error("set_style", handle);
	}
	if (terse_write_text(handle, text) < 0) {
		print_error("write_text", handle);
	}
}

static void demo_output(terse_handle_t handle)
{
	write_line(handle, 1, 1, TERSE_STYLE_BOLD, "P0 Demo: Basic output");
	wait_briefly();
	write_line(handle, 3, 1, TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE, "Bold + Underline");
	wait_briefly();
	write_line(handle, 5, 1, TERSE_STYLE_ITALIC | TERSE_STYLE_STRIKE, "Italic + Strike");
	wait_briefly();
	write_line(handle, 7, 1, 0, "Back to normal");
	if (terse_set_style(handle, 0) < 0) {
		print_error("reset_style", handle);
	}
}

static void demo_restore(terse_handle_t handle)
{
	terse_state_t state;
	if (terse_capture_state(handle, &state) < 0) {
		print_error("capture_state", handle);
		return;
	}
	write_line(handle, 9, 1, TERSE_STYLE_BOLD, "Moving cursor temporarily...");
	wait_briefly();
	if (terse_restore_state(handle, &state) < 0) {
		print_error("restore_state", handle);
		return;
	}
	if (terse_write_text(handle, "Cursor restored") < 0) {
		print_error("write_text", handle);
	}
}

static void demo_read_events(terse_handle_t handle)
{
	printf("Press any key (q to quit) - demonstrating read_event...\n");
	while (1) {
		terse_event_t event;
		int rc = terse_read_event(handle, 200, &event);
		if (rc == TERSE_EVENT_NONE) {
			continue;
		}
		if (rc < 0) {
			fprintf(stderr, "read_event failed: %d\n", rc);
			break;
		}
		if (event.type == TERSE_EVENT_CHAR) {
			printf("Char: %u\n", event.data.ch.scalar);
			if (event.data.ch.scalar == 'q') {
				break;
			}
		} else if (event.type == TERSE_EVENT_RESIZE) {
			printf("Resize: %d x %d\n", event.data.resize.rows, event.data.resize.cols);
		} else {
			printf("Event type %d\n", event.type);
		}
	}
}

int main(void)
{
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

	if (terse_show_cursor(handle, 0) < 0) {
		print_error("hide_cursor", handle);
	}
	if (terse_clear_screen(handle, TERSE_CLEAR_ALL) < 0) {
		print_error("clear_screen", handle);
	}

	demo_output(handle);
	demo_restore(handle);
	demo_read_events(handle);

	if (terse_show_cursor(handle, 1) < 0) {
		print_error("show_cursor", handle);
	}
	terse_close(handle);
	return 0;
}
