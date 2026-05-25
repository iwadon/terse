#include "terse.h"

#include <stdio.h>

#include "sample_compat.h"

static void wait_briefly(void)
{
	sample_sleep_ms(200); // 200ms for visual effect
}

static void print_error(const char *label, terse_handle_t handle)
{
	terse_error_t err = terse_get_last_error(handle);
	if (err != TERSE_OK) {
		fprintf(stderr, "%s failed: %d\n", label, err);
	}
}

static void write_line(terse_handle_t handle, int row, int col, const char *text)
{
	if (terse_move_to(handle, row, col) < 0) {
		print_error("move_to", handle);
		return;
	}
	if (terse_write_text(handle, text) < 0) {
		print_error("write_text", handle);
	}
}

static void demo_output(terse_handle_t handle)
{
	write_line(handle, 0, 0, "P0 Demo: Basic output");
	wait_briefly();
	write_line(handle, 2, 0, "Second line");
	wait_briefly();
	write_line(handle, 4, 0, "Third line");
	wait_briefly();
	write_line(handle, 6, 0, "Back to normal");
}

static void demo_restore(terse_handle_t handle)
{
	terse_state_t state;
	if (terse_capture_state(handle, &state) < 0) {
		print_error("capture_state", handle);
		return;
	}
	if (terse_move_to(handle, 8, 0) < 0) {
		print_error("move_to", handle);
	}
	if (terse_write_text(handle, "Moving cursor temporarily...") < 0) {
		print_error("write_text", handle);
	}
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
		if (rc == TERSE_ERR_NO_EVENT) {
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
		.enabled_caps = 0
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
