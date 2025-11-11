#include "terse.h"

#include <stdio.h>
#include <unistd.h>

static const char *const k_basic_names[8] = {
	"Blk",
	"Red",
	"Grn",
	"Yel",
	"Blu",
	"Mag",
	"Cyn",
	"Wht",
};

static void print_error(const char *label, terse_handle_t handle)
{
	terse_error_t err = terse_get_last_error(handle);
	if (err != TERSE_OK) {
		fprintf(stderr, "%s failed: %d\n", label, err);
	}
}

static void render_basic_grid(terse_handle_t handle, int start_row)
{
	if (terse_move_to(handle, start_row, 0) < 0) {
		print_error("move_to", handle);
		return;
	}
	if (terse_write_text(handle, "Basic16 foreground/background combinations") < 0) {
		print_error("write_text", handle);
	}
	for (int bg_bright = 0; bg_bright < 2; ++bg_bright) {
		for (int bg = 0; bg < 8; ++bg) {
			int row = start_row + 2 + bg + (bg_bright * 9);
			if (terse_move_to(handle, row, 0) < 0) {
				print_error("move_to", handle);
				continue;
			}
			if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
				print_error("reset_style", handle);
			}
			char label[32];
			snprintf(label, sizeof(label), "BG %s%s ", bg_bright ? "Hi" : "Lo", k_basic_names[bg]);
			if (terse_write_text(handle, label) < 0) {
				print_error("write_text", handle);
			}
			for (int fg_bright = 0; fg_bright < 2; ++fg_bright) {
				for (int fg = 0; fg < 8; ++fg) {
					terse_style_t style = terse_style_default();
					style.foreground = terse_color_basic((terse_basic_color_t)fg, fg_bright);
					style.background = terse_color_basic((terse_basic_color_t)bg, bg_bright);
					if (terse_set_style(handle, &style) < 0) {
						print_error("set_style", handle);
						continue;
					}
					char cell[16];
					snprintf(cell, sizeof(cell), " %s%s ", fg_bright ? "Hi" : "Lo", k_basic_names[fg]);
					if (terse_write_text(handle, cell) < 0) {
						print_error("write_text", handle);
					}
				}
			}
			if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
				print_error("reset_style", handle);
			}
		}
	}
}

static void render_palette_block(terse_handle_t handle, int start_row)
{
	if (terse_move_to(handle, start_row, 0) < 0) {
		print_error("move_to", handle);
		return;
	}
	if (terse_write_text(handle, "Palette 256 (indices 16-231)") < 0) {
		print_error("write_text", handle);
	}
	int row = start_row + 2;
	for (int cube_r = 0; cube_r < 6; ++cube_r) {
		for (int cube_g = 0; cube_g < 6; ++cube_g) {
			if (terse_move_to(handle, row, cube_g * 52) < 0) {
				print_error("move_to", handle);
				continue;
			}
			for (int cube_b = 0; cube_b < 6; ++cube_b) {
				unsigned char index = (unsigned char)(16 + cube_r * 36 + cube_g * 6 + cube_b);
				terse_style_t style = terse_style_default();
				style.background = terse_color_palette(index);
				style.foreground = terse_color_basic(TERSE_BASIC_COLOR_BLACK, 0);
				if (terse_set_style(handle, &style) < 0) {
					print_error("set_style", handle);
					continue;
				}
				char cell[12];
				snprintf(cell, sizeof(cell), " %3u ", index);
				if (terse_write_text(handle, cell) < 0) {
					print_error("write_text", handle);
				}
			}
		}
		++row;
	}
	if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
		print_error("reset_style", handle);
	}
}

static void render_truecolor_gradient(terse_handle_t handle, int start_row)
{
	if (terse_move_to(handle, start_row, 0) < 0) {
		print_error("move_to", handle);
		return;
	}
	if (terse_write_text(handle, "Truecolor gradient") < 0) {
		print_error("write_text", handle);
	}
	int rows = 12;
	int cols = 36;
	for (int r = 0; r < rows; ++r) {
		if (terse_move_to(handle, start_row + 2 + r, 0) < 0) {
			print_error("move_to", handle);
			break;
		}
		for (int c = 0; c < cols; ++c) {
			unsigned char red = (unsigned char)((r * 255) / (rows - 1));
			unsigned char green = (unsigned char)((c * 255) / (cols - 1));
			unsigned char blue = (unsigned char)(((rows - r - 1) * 255) / (rows - 1));
			terse_style_t style = terse_style_default();
			style.background = terse_color_truecolor(red, green, blue);
			style.foreground = terse_color_basic(TERSE_BASIC_COLOR_BLACK, 0);
			if (terse_set_style(handle, &style) < 0) {
				print_error("set_style", handle);
				continue;
			}
			if (terse_write_text(handle, "  ") < 0) {
				print_error("write_text", handle);
			}
		}
	}
	if (terse_reset_style(handle, TERSE_RESET_ALL) < 0) {
		print_error("reset_style", handle);
	}
}

int main(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_SGR_EXTENDED | TERSE_CAP_ENABLE_TRUECOLOR
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

	render_basic_grid(handle, 0);
	render_palette_block(handle, 24);
	render_truecolor_gradient(handle, 40);

	if (terse_show_cursor(handle, 1) < 0) {
		print_error("show_cursor", handle);
	}
	terse_reset_style(handle, TERSE_RESET_ALL);
	terse_close(handle);
	return 0;
}
