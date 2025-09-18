#include "terse.h"

#include <stdio.h>
#include <unistd.h>

static void wait_briefly(void)
{
	usleep(150000);
}

static void print_error(const char *label, terse_handle_t handle)
{
	terse_error_info_t err = terse_get_last_error(handle);
	if (err.category != TERSE_ERROR_NONE) {
		fprintf(stderr, "%s failed: category=%d code=%d\n", label, err.category, err.code);
	}
}

static void show_effect(terse_handle_t handle, int row, unsigned int effects, const char *name)
{
	if (terse_move_to(handle, row, 1) < 0) {
		print_error("move_to", handle);
		return;
	}
	terse_style_t style = terse_style_default();
	style.effects = effects;
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_WHITE, 1);
	style.background = terse_color_basic(TERSE_BASIC_COLOR_BLUE, 0);
	if (terse_set_style(handle, &style) < 0) {
		print_error("set_style", handle);
	}
	char line[64];
	snprintf(line, sizeof(line), "%s", name);
	if (terse_write_text(handle, line) < 0) {
		print_error("write_text", handle);
	}
	if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
		print_error("reset_style", handle);
	}
}

static void demo_effects(terse_handle_t handle)
{
	show_effect(handle, 1, TERSE_STYLE_BOLD, "Bold");
	wait_briefly();
	show_effect(handle, 3, TERSE_STYLE_FAINT, "Faint");
	wait_briefly();
	show_effect(handle, 5, TERSE_STYLE_ITALIC, "Italic");
	wait_briefly();
	show_effect(handle, 7, TERSE_STYLE_UNDERLINE, "Underline");
	wait_briefly();
	show_effect(handle, 9, TERSE_STYLE_INVERSE, "Inverse");
	wait_briefly();
	show_effect(handle, 11, TERSE_STYLE_BLINK, "Blink");
	wait_briefly();
	show_effect(handle, 13, TERSE_STYLE_STRIKE, "Strike");
	wait_briefly();
	show_effect(handle, 15, TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE | TERSE_STYLE_STRIKE,
		"Bold+Underline+Strike");
}

int main(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_SGR_BASIC
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

	demo_effects(handle);

	if (terse_move_to(handle, 18, 1) == 0) {
		if (terse_write_text(handle, "Press Ctrl+C to exit") < 0) {
			print_error("write_text", handle);
		}
	}

	if (terse_show_cursor(handle, 1) < 0) {
		print_error("show_cursor", handle);
	}
	terse_reset_style(handle, TERSE_RESET_ALL);
	terse_close(handle);
	return 0;
}
