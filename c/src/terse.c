#include "terse.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TERSE_STYLE_ALL_SUPPORTED (TERSE_STYLE_BOLD | TERSE_STYLE_FAINT | TERSE_STYLE_ITALIC | TERSE_STYLE_UNDERLINE | TERSE_STYLE_INVERSE | TERSE_STYLE_BLINK | TERSE_STYLE_STRIKE)

static const char TERSE_RESET_ALL_SEQ[] = "\x1b[0m";
static const char TERSE_RESET_COLOR_SEQ[] = "\x1b[39;49m";
static const char TERSE_RESET_EFFECTS_SEQ[] = "\x1b[22;23;24;27;29m";
static const char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef struct terse_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} terse_rgb_t;

static const terse_rgb_t basic16_rgb[16] = {
	{ 0, 0, 0 },
	{ 205, 0, 0 },
	{ 0, 205, 0 },
	{ 205, 205, 0 },
	{ 0, 0, 205 },
	{ 205, 0, 205 },
	{ 0, 205, 205 },
	{ 229, 229, 229 },
	{ 127, 127, 127 },
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 255, 255, 0 },
	{ 92, 92, 255 },
	{ 255, 0, 255 },
	{ 0, 255, 255 },
	{ 255, 255, 255 },
};

static int
color_kind_rank(terse_color_kind_t kind)
{
	switch (kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 0;
	case TERSE_COLOR_KIND_BASIC16:
		return 1;
	case TERSE_COLOR_KIND_PALETTE256:
		return 2;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return 3;
	default:
		return 0;
	}
}

static int
color_support_rank(terse_color_support_t support)
{
	switch (support) {
	case TERSE_COLOR_NONE:
		return 0;
	case TERSE_COLOR_BASIC16:
		return 1;
	case TERSE_COLOR_PALETTE256:
		return 2;
	case TERSE_COLOR_TRUECOLOR:
		return 3;
	default:
		return 0;
	}
}

static unsigned char
cube_component_from_truecolor(unsigned char value)
{
	if (value < 48) {
		return 0;
	}
	if (value < 114) {
		return 1;
	}
	return (unsigned char)((value - 35) / 40);
}

static unsigned char
cube_component_to_value(unsigned char component)
{
	static const unsigned char values[6] = { 0, 95, 135, 175, 215, 255 };
	if (component > 5) {
		component = 5;
	}
	return values[component];
}

static unsigned char
truecolor_to_palette_index(unsigned char r, unsigned char g, unsigned char b)
{
	if (r == g && g == b) {
		if (r < 8) {
			return 16;
		}
		if (r > 248) {
			return 231;
		}
		return (unsigned char)(232 + (r - 8) / 10);
	}
	unsigned char rc = cube_component_from_truecolor(r);
	unsigned char gc = cube_component_from_truecolor(g);
	unsigned char bc = cube_component_from_truecolor(b);
	return (unsigned char)(16 + rc * 36 + gc * 6 + bc);
}

static void
palette_index_to_rgb(unsigned char index, unsigned char *r, unsigned char *g, unsigned char *b)
{
	if (index < 16) {
		*r = basic16_rgb[index].r;
		*g = basic16_rgb[index].g;
		*b = basic16_rgb[index].b;
		return;
	}
	if (index < 232) {
		unsigned char adj = (unsigned char)(index - 16);
		unsigned char rc = (unsigned char)(adj / 36);
		unsigned char gc = (unsigned char)((adj / 6) % 6);
		unsigned char bc = (unsigned char)(adj % 6);
		*r = cube_component_to_value(rc);
		*g = cube_component_to_value(gc);
		*b = cube_component_to_value(bc);
		return;
	}
	unsigned char level = (unsigned char)(8 + (index - 232) * 10);
	*r = level;
	*g = level;
	*b = level;
}

static unsigned char
closest_basic16_index(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned int best_distance = UINT_MAX;
	unsigned char best_index = 0;
	for (unsigned char i = 0; i < 16; ++i) {
		int dr = (int)r - (int)basic16_rgb[i].r;
		int dg = (int)g - (int)basic16_rgb[i].g;
		int db = (int)b - (int)basic16_rgb[i].b;
		unsigned int distance = (unsigned int)(dr * dr + dg * dg + db * db);
		if (distance < best_distance) {
			best_distance = distance;
			best_index = i;
		}
	}
	return best_index;
}

terse_color_t
terse_color_default(void)
{
	terse_color_t color = { .kind = TERSE_COLOR_KIND_DEFAULT };
	return color;
}

terse_color_t
terse_color_basic(terse_basic_color_t color, int bright)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_BASIC16,
		.data.basic16 = {
			.color = color,
			.bright = bright ? 1 : 0,
		},
	};
	return result;
}

terse_color_t
terse_color_palette(unsigned char index)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_PALETTE256,
		.data.palette = {
			.value = index,
		},
	};
	return result;
}

terse_color_t
terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_TRUECOLOR,
		.data.truecolor = {
			.r = r,
			.g = g,
			.b = b,
		},
	};
	return result;
}

terse_style_t
terse_style_default(void)
{
	terse_style_t style = {
		.foreground = terse_color_default(),
		.background = terse_color_default(),
		.effects = 0,
	};
	return style;
}

static unsigned int
mask_effects(unsigned int effects)
{
	return effects & TERSE_STYLE_ALL_SUPPORTED;
}

static int
colors_equal(const terse_color_t *a, const terse_color_t *b)
{
	if (a->kind != b->kind) {
		return 0;
	}
	switch (a->kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 1;
	case TERSE_COLOR_KIND_BASIC16:
		return a->data.basic16.color == b->data.basic16.color && a->data.basic16.bright == b->data.basic16.bright;
	case TERSE_COLOR_KIND_PALETTE256:
		return a->data.palette.value == b->data.palette.value;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return a->data.truecolor.r == b->data.truecolor.r && a->data.truecolor.g == b->data.truecolor.g && a->data.truecolor.b == b->data.truecolor.b;
	default:
		return 0;
	}
}

static int
styles_equal(const terse_style_t *a, const terse_style_t *b)
{
	if (a->effects != b->effects) {
		return 0;
	}
	if (!colors_equal(&a->foreground, &b->foreground)) {
		return 0;
	}
	if (!colors_equal(&a->background, &b->background)) {
		return 0;
	}
	return 1;
}

static terse_color_t
degrade_color(terse_color_t color, terse_color_support_t support)
{
	if (color.kind == TERSE_COLOR_KIND_DEFAULT) {
		return color;
	}
	int support_level = color_support_rank(support);
	if (support_level == 0) {
		return terse_color_default();
	}
	int requested_level = color_kind_rank(color.kind);
	if (requested_level <= support_level) {
		return color;
	}
	switch (support) {
	case TERSE_COLOR_BASIC16:
		{
			unsigned char r = 0;
			unsigned char g = 0;
			unsigned char b = 0;
			if (color.kind == TERSE_COLOR_KIND_TRUECOLOR) {
				r = color.data.truecolor.r;
				g = color.data.truecolor.g;
				b = color.data.truecolor.b;
			} else if (color.kind == TERSE_COLOR_KIND_PALETTE256) {
				palette_index_to_rgb(color.data.palette.value, &r, &g, &b);
			}
			unsigned char idx = closest_basic16_index(r, g, b);
			terse_color_t basic = {
				.kind = TERSE_COLOR_KIND_BASIC16,
				.data.basic16 = {
					.color = (terse_basic_color_t)(idx % 8),
					.bright = idx >= 8,
				},
			};
			return basic;
		}
	case TERSE_COLOR_PALETTE256:
		{
			if (color.kind == TERSE_COLOR_KIND_TRUECOLOR) {
				unsigned char idx = truecolor_to_palette_index(color.data.truecolor.r, color.data.truecolor.g, color.data.truecolor.b);
				terse_color_t palette = {
					.kind = TERSE_COLOR_KIND_PALETTE256,
					.data.palette = { .value = idx },
				};
				return palette;
			}
			break;
		}
	case TERSE_COLOR_TRUECOLOR:
		break;
	default:
		break;
	}
	return terse_color_default();
}

static terse_style_t
sanitize_style_request(const terse_style_t *style)
{
	terse_style_t sanitized = *style;
	sanitized.effects = mask_effects(style->effects);
	return sanitized;
}

static terse_style_t
make_effective_style(const terse_capabilities_t *caps, const terse_style_t *requested)
{
	terse_style_t effective = *requested;
	effective.effects &= caps->effects;
	effective.foreground = degrade_color(requested->foreground, caps->colors);
	effective.background = degrade_color(requested->background, caps->colors);
	return effective;
}

struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_options_t options;
	terse_size_t size;
	int cursor_visible;
	int cursor_row;
	int cursor_col;
	int cursor_known;
	terse_style_t style;
	terse_style_t effective_style;
	int style_known;
	terse_mouse_mode_t mouse_mode;
	int mouse_enabled;
	terse_mouse_button_t mouse_button;
	int paste_enabled;
	terse_error_category_t last_error;
	int last_errno;
};

static void
update_effective_style(terse_handle_t handle)
{
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}

static void
set_mouse_event(terse_event_t *event, terse_event_type_t type, terse_mouse_button_t button, int mods, int row, int col)
{
	event->type = type;
	event->data.mouse.button = button;
	event->data.mouse.mods = mods;
	event->data.mouse.row = row;
	event->data.mouse.col = col;
}

static int mouse_modifiers_from_param(int param);
static void clear_error(terse_handle_t handle);

static char *
base64_encode(const unsigned char *data, size_t length, size_t *out_len)
{
	if (!data || length == 0) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t encoded = ((length + 2) / 3) * 4;
	char *output = malloc(encoded + 1);
	if (!output) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t out_index = 0;
	for (size_t i = 0; i < length; i += 3) {
		unsigned int triple = data[i] << 16;
		if (i + 1 < length) {
			triple |= data[i + 1] << 8;
		}
		if (i + 2 < length) {
			triple |= data[i + 2];
		}
		output[out_index++] = BASE64_ALPHABET[(triple >> 18) & 0x3f];
		output[out_index++] = BASE64_ALPHABET[(triple >> 12) & 0x3f];
		if (i + 1 < length) {
			output[out_index++] = BASE64_ALPHABET[(triple >> 6) & 0x3f];
		} else {
			output[out_index++] = '=';
		}
		if (i + 2 < length) {
			output[out_index++] = BASE64_ALPHABET[triple & 0x3f];
		} else {
			output[out_index++] = '=';
		}
	}
	output[out_index] = '\0';
	if (out_len) {
		*out_len = out_index;
	}
	return output;
}

static int
handle_sgr_mouse_sequence(terse_handle_t handle, terse_event_t *out_event, const int *values, size_t value_count, char final)
{
	if (!handle || value_count < 3) {
		return 0;
	}
	if (!handle->mouse_enabled || handle->mouse_mode == TERSE_MOUSE_NONE) {
		return 0;
	}
	int raw_cb = values[0];
	int col = values[1];
	int row = values[2];
	if (col < 0 || row < 0) {
		return 0;
	}
	int mods = mouse_modifiers_from_param(raw_cb);
	int cb = raw_cb & ~(4 | 8 | 16);
	int is_motion = cb & 32;
	int is_wheel = cb & 64;
	int base = cb & 3;
	terse_mouse_button_t button = TERSE_MOUSE_BUTTON_NONE;
	terse_event_type_t type = TERSE_EVENT_MOUSE_MOVE;
	if (is_wheel) {
		button = (base == 0) ? TERSE_MOUSE_BUTTON_SCROLL_UP : TERSE_MOUSE_BUTTON_SCROLL_DOWN;
		type = TERSE_EVENT_MOUSE_SCROLL;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (final == 'm') {
		if (base <= 2) {
			button = (base == 0) ? TERSE_MOUSE_BUTTON_LEFT : (base == 1 ? TERSE_MOUSE_BUTTON_MIDDLE : TERSE_MOUSE_BUTTON_RIGHT);
		} else {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_UP;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (is_motion) {
		if (handle->mouse_button != TERSE_MOUSE_BUTTON_NONE) {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_MOVE;
	} else {
		if (base == 0) {
			button = TERSE_MOUSE_BUTTON_LEFT;
		} else if (base == 1) {
			button = TERSE_MOUSE_BUTTON_MIDDLE;
		} else if (base == 2) {
			button = TERSE_MOUSE_BUTTON_RIGHT;
		}
		type = TERSE_EVENT_MOUSE_DOWN;
		handle->mouse_button = button;
	}
	set_mouse_event(out_event, type, button, mods, row, col);
	clear_error(handle);
	return 1;
}

static void
set_error(terse_handle_t handle, terse_error_category_t category, int code)
{
	if (!handle) {
		return;
	}
	handle->last_error = category;
	handle->last_errno = code;
}

static void
clear_error(terse_handle_t handle)
{
	set_error(handle, TERSE_ERROR_NONE, 0);
}

static terse_capabilities_t
make_p0_capabilities(void)
{
	terse_capabilities_t caps = {
		.profile = TERSE_P0,
		.has_basic_output = 1,
		.has_cursor_visibility = 1,
		.has_move_absolute = 1,
		.has_move_relative = 1,
		.has_clear_line = 1,
		.has_clear_screen = 1,
		.has_size = 0,
		.has_sgr_basic = 0,
		.has_sgr_extended = 0,
		.has_truecolor = 0,
		.has_text_styles = 0,
		.mouse = TERSE_MOUSE_NONE,
		.has_bracketed_paste = 0,
		.has_title = 0,
		.has_hyperlinks = 0,
		.has_cursor_shape = 0,
		.colors = TERSE_COLOR_NONE,
		.effects = 0,
		.has_clipboard_write = 0,
		.images = TERSE_IMAGE_NONE,
	};
	return caps;
}

static void emit_reset_sequences(terse_handle_t handle);

static terse_options_t
default_options(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	return options;
}

int terse_validate_options(const terse_options_t *options)
{
	if (!options) {
		return 0;
	}
	if (options->input_fd < 0 || options->output_fd < 0) {
		errno = EBADF;
		return -EBADF;
	}
	return 0;
}

static terse_size_t
make_unknown_size(void)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
	return size;
}

static terse_size_t
query_fd_size(int fd)
{
	terse_size_t size = make_unknown_size();
	if (fd < 0) {
		return size;
	}
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
		size.rows = ws.ws_row;
		size.cols = ws.ws_col;
		size.known = 1;
	}
	return size;
}

static void
refresh_size(terse_handle_t handle)
{
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		handle->size = make_unknown_size();
		return;
	}
	terse_size_t size = query_fd_size(handle->options.output_fd);
	if (!size.known && handle->options.input_fd != handle->options.output_fd) {
		size = query_fd_size(handle->options.input_fd);
	}
	if (size.known || !handle->size.known) {
		handle->size = size;
	}
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile < TERSE_P0 || requested_profile > TERSE_P3) {
		return NULL;
	}
	if (terse_validate_options(options) < 0) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}

	handle->requested_profile = requested_profile;
	handle->capabilities = make_p0_capabilities();

	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = default_options().codec_name;
		}
	} else {
		handle->options = default_options();
	}
	handle->size = make_unknown_size();

	unsigned int disabled = handle->options.disabled_caps;
	if (disabled & TERSE_CAP_DISABLE_BASIC_OUTPUT) {
		handle->capabilities.has_basic_output = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CURSOR_VISIBILITY) {
		handle->capabilities.has_cursor_visibility = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_ABSOLUTE) {
		handle->capabilities.has_move_absolute = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_RELATIVE) {
		handle->capabilities.has_move_relative = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_LINE) {
		handle->capabilities.has_clear_line = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_SCREEN) {
		handle->capabilities.has_clear_screen = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SIZE) {
		handle->capabilities.has_size = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_BASIC) {
		handle->capabilities.has_sgr_basic = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_EXTENDED) {
		handle->capabilities.has_sgr_extended = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TRUECOLOR) {
		handle->capabilities.has_truecolor = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TEXT_STYLES) {
		handle->capabilities.has_text_styles = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOUSE) {
		handle->capabilities.mouse = TERSE_MOUSE_NONE;
	}
	if (disabled & TERSE_CAP_DISABLE_BRACKETED_PASTE) {
		handle->capabilities.has_bracketed_paste = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TITLE) {
		handle->capabilities.has_title = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_HYPERLINK) {
		handle->capabilities.has_hyperlinks = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CURSOR_SHAPE) {
		handle->capabilities.has_cursor_shape = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLIPBOARD_WRITE) {
		handle->capabilities.has_clipboard_write = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_IMAGE_INLINE) {
		handle->capabilities.images = TERSE_IMAGE_NONE;
	}
	unsigned int enabled = handle->options.enabled_caps;
	if (enabled & TERSE_CAP_ENABLE_SGR_BASIC) {
		handle->capabilities.has_sgr_basic = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TEXT_STYLES) {
		handle->capabilities.has_text_styles = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_SGR_EXTENDED) {
		handle->capabilities.has_sgr_extended = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TRUECOLOR) {
		handle->capabilities.has_truecolor = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_MOUSE) {
		handle->capabilities.mouse = TERSE_MOUSE_SGR;
	}
	if (enabled & TERSE_CAP_ENABLE_BRACKETED_PASTE) {
		handle->capabilities.has_bracketed_paste = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TITLE) {
		handle->capabilities.has_title = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_HYPERLINK) {
		handle->capabilities.has_hyperlinks = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_CURSOR_SHAPE) {
		handle->capabilities.has_cursor_shape = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_CLIPBOARD_WRITE) {
		handle->capabilities.has_clipboard_write = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_IMAGE_INLINE) {
		handle->capabilities.images = TERSE_IMAGE_ITERM_INLINE;
	}
	if (handle->capabilities.has_truecolor) {
		handle->capabilities.colors = TERSE_COLOR_TRUECOLOR;
	} else if (handle->capabilities.has_sgr_extended) {
		handle->capabilities.colors = TERSE_COLOR_PALETTE256;
	} else if (handle->capabilities.has_sgr_basic) {
		handle->capabilities.colors = TERSE_COLOR_BASIC16;
	} else {
		handle->capabilities.colors = TERSE_COLOR_NONE;
	}
	handle->capabilities.effects = handle->capabilities.has_text_styles ? TERSE_STYLE_ALL_SUPPORTED : 0;
	handle->cursor_visible = 1;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_known = 0;
	handle->style = terse_style_default();
	update_effective_style(handle);
	handle->mouse_mode = TERSE_MOUSE_NONE;
	handle->mouse_enabled = 0;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	handle->paste_enabled = 0;
	clear_error(handle);
	refresh_size(handle);

	return handle;
}

void terse_close(terse_handle_t handle)
{
	emit_reset_sequences(handle);
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return make_p0_capabilities();
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	clear_error(handle);
	return handle->capabilities;
}

static int
ensure_handle(terse_handle_t handle)
{
	if (!handle) {
		errno = EINVAL;
		return -EINVAL;
	}
	return 0;
}

static int
write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		errno = EINVAL;
		return -EINVAL;
	}
	while (len > 0) {
		ssize_t written = write(fd, bytes, len);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		if (written == 0) {
			errno = EPIPE;
			return -EPIPE;
		}
		bytes += (size_t)written;
		len -= (size_t)written;
	}
	return 0;
}

static int
write_literal(terse_handle_t handle, const char *literal)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!literal) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int out = write_bytes(handle->options.output_fd, literal, strlen(literal));
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static int
write_sequence(terse_handle_t handle, const char *sequence, size_t length)
{
	int out = write_bytes(handle->options.output_fd, sequence, length);
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static void
emit_reset_sequences(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (!handle->capabilities.has_basic_output) {
		return;
	}
	if (handle->mouse_enabled) {
		(void)terse_disable_mouse(handle);
	}
	if (handle->paste_enabled) {
		(void)terse_disable_bracketed_paste(handle);
	}
	static const char *const cursor_on_seq = "\x1b[?25h";
	if (!handle->cursor_visible) {
		if (write_sequence(handle, cursor_on_seq, strlen(cursor_on_seq)) == 0) {
			handle->cursor_visible = 1;
		}
	}
	if (write_sequence(handle, TERSE_RESET_ALL_SEQ, sizeof(TERSE_RESET_ALL_SEQ) - 1) == 0) {
		handle->style = terse_style_default();
		update_effective_style(handle);
	}
}

static int
wait_for_input(int fd, int timeout_ms)
{
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
	int poll_timeout = timeout_ms < 0 ? -1 : timeout_ms;
	for (;;) {
		int ready = poll(&pfd, 1, poll_timeout);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return ready;
	}
}

static ssize_t
read_byte(int fd, unsigned char *out)
{
	for (;;) {
		ssize_t n = read(fd, out, 1);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return n;
	}
}

static size_t
drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	size_t len = 1;
	while (len < max) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		int ready = poll(&pfd, 1, 10);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (ready == 0) {
			break;
		}
		ssize_t n = read(fd, buffer + len, 1);
		if (n <= 0) {
			break;
		}
		len += (size_t)n;
		if (len >= 3) {
			unsigned char c = buffer[len - 1];
			if (c >= '@' && c <= '~') {
				break;
			}
		}
	}
	return len;
}

static int
parse_csi_sequence(const unsigned char *seq, size_t len, int *values, size_t max_values, size_t *value_count, char *final)
{
	if (len < 2 || seq[0] != 0x1b || seq[1] != '[') {
		return -1;
	}
	if (len < 3) {
		return -1;
	}
	char terminator = (char)seq[len - 1];
	if (terminator < '@' || terminator > '~') {
		return -1;
	}
	size_t count = 0;
	size_t index = 2;
	while (index < len - 1) {
		if (seq[index] == '<') {
			++index;
			continue;
		}
		int value = 0;
		int has_digit = 0;
		while (index < len - 1 && isdigit((unsigned char)seq[index])) {
			has_digit = 1;
			value = (value * 10) + (seq[index] - '0');
			++index;
		}
		if (has_digit) {
			if (count < max_values) {
				values[count] = value;
			}
			++count;
		}
		if (index >= len - 1) {
			break;
		}
		if (seq[index] == ';') {
			++index;
			continue;
		}
		break;
	}
	*value_count = count;
	*final = terminator;
	return 0;
}

static int
modifier_bits_from_param(int param)
{
	int mods = 0;
	switch (param) {
	case 2:
		mods = TERSE_MOD_SHIFT;
		break;
	case 3:
		mods = TERSE_MOD_ALT;
		break;
	case 4:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT;
		break;
	case 5:
		mods = TERSE_MOD_CTRL;
		break;
	case 6:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_CTRL;
		break;
	case 7:
		mods = TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	case 8:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	default:
		break;
	}
	return mods;
}

int terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_screen) {
		clear_error(handle);
		return 0;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[J";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1J";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2J";
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_line) {
		clear_error(handle);
		return 0;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[K";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1K";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2K";
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int terse_move_to(terse_handle_t handle, int row, int col)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_absolute) {
		clear_error(handle);
		return 0;
	}

	if (row < 1) {
		row = 1;
	}
	if (col < 1) {
		col = 1;
	}
	if (row == handle->cursor_row && col == handle->cursor_col) {
		clear_error(handle);
		return 0;
	}

	char sequence[32];
	int written = snprintf(sequence, sizeof(sequence), "\x1b[%d;%dH", row, col);
	if (written <= 0 || (size_t)written >= sizeof(sequence)) {
		errno = EINVAL;
		return -EINVAL;
	}

	int out = write_sequence(handle, sequence, (size_t)written);
	if (out == 0) {
		handle->cursor_row = row;
		handle->cursor_col = col;
		handle->cursor_known = 1;
	}
	return out;
}

int terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_relative) {
		clear_error(handle);
		return 0;
	}
	if (drow == 0 && dcol == 0) {
		clear_error(handle);
		return 0;
	}

	int new_row = handle->cursor_row;
	int new_col = handle->cursor_col;

	if (drow < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dA", -drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	} else if (drow > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dB", drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	}

	if (dcol < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dD", -dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	} else if (dcol > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dC", dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	}

	if (new_row < 1) {
		new_row = 1;
	}
	if (new_col < 1) {
		new_col = 1;
	}
	handle->cursor_row = new_row;
	handle->cursor_col = new_col;
	handle->cursor_known = 1;
	clear_error(handle);
	return 0;
}

int terse_show_cursor(terse_handle_t handle, int visible)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_cursor_visibility) {
		clear_error(handle);
		return 0;
	}
	int target = visible ? 1 : 0;
	if (handle->cursor_visible == target) {
		clear_error(handle);
		return 0;
	}
	int result = write_literal(handle, target ? "\x1b[?25h" : "\x1b[?25l");
	if (result == 0) {
		handle->cursor_visible = target;
	}
	return result;
}

static int
append_param(char *seq, size_t size, size_t *pos, int *first, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int written = vsnprintf(seq + *pos, size - *pos, fmt, ap);
	va_end(ap);
	if (written < 0) {
		return -EINVAL;
	}
	if ((size_t)written >= size - *pos) {
		errno = EOVERFLOW;
		return -EOVERFLOW;
	}
	*pos += (size_t)written;
	*first = 0;
	return 0;
}

static int
mouse_modifiers_from_param(int param)
{
	int mods = 0;
	if (param & 4) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (param & 8) {
		mods |= TERSE_MOD_ALT;
	}
	if (param & 16) {
		mods |= TERSE_MOD_CTRL;
	}
	return mods;
}

static int
append_effects(char *seq, size_t size, size_t *pos, int *first, unsigned int effects)
{
	if (effects & TERSE_STYLE_BOLD) {
		int rc = append_param(seq, size, pos, first, *first ? "1" : ";1");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_FAINT) {
		int rc = append_param(seq, size, pos, first, *first ? "2" : ";2");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_ITALIC) {
		int rc = append_param(seq, size, pos, first, *first ? "3" : ";3");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_UNDERLINE) {
		int rc = append_param(seq, size, pos, first, *first ? "4" : ";4");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_INVERSE) {
		int rc = append_param(seq, size, pos, first, *first ? "7" : ";7");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_BLINK) {
		int rc = append_param(seq, size, pos, first, *first ? "5" : ";5");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_STRIKE) {
		int rc = append_param(seq, size, pos, first, *first ? "9" : ";9");
		if (rc < 0) {
			return rc;
		}
	}
	return 0;
}

static int
append_basic16_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, terse_basic_color_t color, int bright)
{
	int base = is_foreground ? 30 : 40;
	int hi_base = is_foreground ? 90 : 100;
	int code = bright ? (hi_base + color) : (base + color);
	return append_param(seq, size, pos, first, *first ? "%d" : ";%d", code);
}

static int
append_palette_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, unsigned int index)
{
	const char *prefix = is_foreground ? "38;5;" : "48;5;";
	return append_param(seq, size, pos, first, *first ? "%s%u" : ";%s%u", prefix, index);
}

static int
append_truecolor(char *seq, size_t size, size_t *pos, int *first, int is_foreground, unsigned char r, unsigned char g, unsigned char b)
{
	const char *prefix = is_foreground ? "38;2;" : "48;2;";
	return append_param(seq, size, pos, first, *first ? "%s%u;%u;%u" : ";%s%u;%u;%u", prefix, r, g, b);
}

static int
append_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, const terse_color_t *color)
{
	switch (color->kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 0;
	case TERSE_COLOR_KIND_BASIC16:
		return append_basic16_color(seq, size, pos, first, is_foreground, color->data.basic16.color, color->data.basic16.bright);
	case TERSE_COLOR_KIND_PALETTE256:
		return append_palette_color(seq, size, pos, first, is_foreground, color->data.palette.value);
	case TERSE_COLOR_KIND_TRUECOLOR:
		return append_truecolor(seq, size, pos, first, is_foreground, color->data.truecolor.r, color->data.truecolor.g, color->data.truecolor.b);
	default:
		return 0;
	}
}

static int
emit_style_sequence(terse_handle_t handle, const terse_style_t *style)
{
	int reset = write_literal(handle, "\x1b[0m");
	if (reset < 0) {
		return reset;
	}
	if (style->effects == 0 && style->foreground.kind == TERSE_COLOR_KIND_DEFAULT && style->background.kind == TERSE_COLOR_KIND_DEFAULT) {
		return 0;
	}
	char seq[128];
	int first = 1;
	int prefix = snprintf(seq, sizeof(seq), "\x1b[");
	if (prefix < 0 || (size_t)prefix >= sizeof(seq)) {
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	size_t pos = (size_t)prefix;
	int rc = append_effects(seq, sizeof(seq), &pos, &first, style->effects);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 1, &style->foreground);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 0, &style->background);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	if (first) {
		return 0;
	}
	if (pos >= sizeof(seq) - 1) {
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	seq[pos++] = 'm';
	return write_sequence(handle, seq, pos);
}

int terse_set_style(terse_handle_t handle, const terse_style_t *style)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!style) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	terse_style_t requested = sanitize_style_request(style);
	handle->style = requested;
	terse_style_t effective = make_effective_style(&handle->capabilities, &requested);
	if (handle->style_known && styles_equal(&effective, &handle->effective_style)) {
		clear_error(handle);
		return 0;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.effects == 0 && handle->capabilities.colors == TERSE_COLOR_NONE)) {
		handle->effective_style = effective;
		handle->style_known = 1;
		clear_error(handle);
		return 0;
	}
	int result = emit_style_sequence(handle, &effective);
	if (result == 0) {
		handle->effective_style = effective;
		handle->style_known = 1;
	}
	return result;
}

static int
write_reset_sequence(terse_handle_t handle, terse_reset_scope_t scope)
{
	switch (scope) {
	case TERSE_RESET_ALL:
		return write_sequence(handle, TERSE_RESET_ALL_SEQ, sizeof(TERSE_RESET_ALL_SEQ) - 1);
	case TERSE_RESET_COLOR_ONLY:
		return write_sequence(handle, TERSE_RESET_COLOR_SEQ, sizeof(TERSE_RESET_COLOR_SEQ) - 1);
	case TERSE_RESET_EFFECTS_ONLY:
		return write_sequence(handle, TERSE_RESET_EFFECTS_SEQ, sizeof(TERSE_RESET_EFFECTS_SEQ) - 1);
	default:
		return 0;
	}
}

int terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (scope < TERSE_RESET_ALL || scope > TERSE_RESET_EFFECTS_ONLY) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int result = 0;
	if (handle->capabilities.has_basic_output) {
		int emit = 0;
		switch (scope) {
		case TERSE_RESET_ALL:
			emit = (handle->capabilities.colors != TERSE_COLOR_NONE || handle->capabilities.effects != 0);
			break;
		case TERSE_RESET_COLOR_ONLY:
			emit = (handle->capabilities.colors != TERSE_COLOR_NONE);
			break;
		case TERSE_RESET_EFFECTS_ONLY:
			emit = (handle->capabilities.effects != 0);
			break;
		default:
			emit = 0;
			break;
		}
		if (emit) {
			result = write_reset_sequence(handle, scope);
			if (result < 0) {
				return result;
			}
		}
	}
	switch (scope) {
	case TERSE_RESET_ALL:
		handle->style = terse_style_default();
		break;
	case TERSE_RESET_COLOR_ONLY:
		handle->style.foreground = terse_color_default();
		handle->style.background = terse_color_default();
		handle->style.effects = mask_effects(handle->style.effects);
		break;
	case TERSE_RESET_EFFECTS_ONLY:
		handle->style.effects = 0;
		break;
	default:
		break;
	}
	update_effective_style(handle);
	clear_error(handle);
	return result;
}

static int
set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable)
{
	static const char *const enable_seqs[][2] = {
		{ "\x1b[?1000h", NULL },		  // X10
		{ "\x1b[?1002h", NULL },		  // VT200
		{ "\x1b[?1002h", "\x1b[?1006h" }, // SGR
	};
	static const char *const disable_seqs[][2] = {
		{ "\x1b[?1000l", NULL },
		{ "\x1b[?1002l", NULL },
		{ "\x1b[?1002l", "\x1b[?1006l" },
	};
	int index = 0;
	switch (mode) {
	case TERSE_MOUSE_X10:
		index = 0;
		break;
	case TERSE_MOUSE_VT200:
		index = 1;
		break;
	case TERSE_MOUSE_SGR:
		index = 2;
		break;
	default:
		return 0;
	}
	const char *const *seqs = enable ? enable_seqs[index] : disable_seqs[index];
	for (int i = 0; i < 2 && seqs[i]; ++i) {
		if (write_literal(handle, seqs[i]) < 0) {
			return -handle->last_errno;
		}
	}
	return 0;
}

static terse_mouse_mode_t
clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available)
{
	if (requested > available) {
		return available;
	}
	return requested;
}

int terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (mode <= TERSE_MOUSE_NONE || mode > TERSE_MOUSE_SGR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (handle->capabilities.mouse == TERSE_MOUSE_NONE || !handle->capabilities.has_basic_output) {
		handle->mouse_mode = TERSE_MOUSE_NONE;
		handle->mouse_enabled = 0;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
		clear_error(handle);
		return 0;
	}
	terse_mouse_mode_t actual = clamp_mouse_mode(mode, handle->capabilities.mouse);
	if (handle->mouse_enabled && handle->mouse_mode == actual) {
		clear_error(handle);
		return 0;
	}
	if (handle->mouse_enabled) {
		int disable_rc = terse_disable_mouse(handle);
		if (disable_rc < 0) {
			return disable_rc;
		}
	}
	if (set_mouse_mode(handle, actual, 1) < 0) {
		return -handle->last_errno;
	}
	handle->mouse_mode = actual;
	handle->mouse_enabled = 1;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	clear_error(handle);
	return 0;
}

int terse_disable_mouse(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->mouse_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_mouse_mode(handle, handle->mouse_mode, 0) < 0) {
			return -handle->last_errno;
		}
	}
	handle->mouse_enabled = 0;
	handle->mouse_mode = TERSE_MOUSE_NONE;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	clear_error(handle);
	return 0;
}

static int
set_bracketed_paste(terse_handle_t handle, int enable)
{
	const char *seq = enable ? "\x1b[?2004h" : "\x1b[?2004l";
	return write_literal(handle, seq);
}

int terse_enable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_bracketed_paste || !handle->capabilities.has_basic_output) {
		handle->paste_enabled = 0;
		clear_error(handle);
		return 0;
	}
	if (handle->paste_enabled) {
		clear_error(handle);
		return 0;
	}
	if (set_bracketed_paste(handle, 1) < 0) {
		return -handle->last_errno;
	}
	handle->paste_enabled = 1;
	clear_error(handle);
	return 0;
}

int terse_disable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->paste_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_bracketed_paste(handle, 0) < 0) {
			return -handle->last_errno;
		}
	}
	handle->paste_enabled = 0;
	clear_error(handle);
	return 0;
}

int terse_set_title(terse_handle_t handle, const char *title)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!title) {
		title = "";
	}
	if (!handle->capabilities.has_title || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x1b]0;") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, title, strlen(title)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!url) {
		url = "";
	}
	if (!label) {
		label = "";
	}
	if (!handle->capabilities.has_hyperlinks || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x1b]8;;") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, url, strlen(url)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, label, strlen(label)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x1b]8;;\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (shape < TERSE_CURSOR_SHAPE_DEFAULT || shape > TERSE_CURSOR_SHAPE_BAR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_cursor_shape || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	int value = 0;
	switch (shape) {
	case TERSE_CURSOR_SHAPE_DEFAULT:
		value = blinking ? 1 : 2;
		break;
	case TERSE_CURSOR_SHAPE_BLOCK:
		value = blinking ? 1 : 2;
		break;
	case TERSE_CURSOR_SHAPE_UNDERLINE:
		value = blinking ? 3 : 4;
		break;
	case TERSE_CURSOR_SHAPE_BAR:
		value = blinking ? 5 : 6;
		break;
	default:
		value = 1;
		break;
	}
	char seq[16];
	int len = snprintf(seq, sizeof(seq), "\x1b[%d q", value);
	if (len <= 0 || len >= (int)sizeof(seq)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	return write_literal(handle, seq);
}

int terse_set_clipboard(terse_handle_t handle, const char *data)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!data) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_clipboard_write || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	size_t encoded_len = 0;
	char *encoded = base64_encode((const unsigned char *)data, strlen(data), &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERROR_RESOURCE, ENOMEM);
		return -ENOMEM;
	}
	if (write_literal(handle, "\x1b]52;;") < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	if (write_sequence(handle, encoded, encoded_len) < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	free(encoded);
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!data || size == 0) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (handle->capabilities.images != TERSE_IMAGE_ITERM_INLINE || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (!name) {
		name = "image";
	}
	size_t name_len = 0;
	char *name_encoded = base64_encode((const unsigned char *)name, strlen(name), &name_len);
	size_t data_len = 0;
	char *data_encoded = base64_encode(data, size, &data_len);
	if (!name_encoded || !data_encoded) {
		free(name_encoded);
		free(data_encoded);
		errno = ENOMEM;
		set_error(handle, TERSE_ERROR_RESOURCE, ENOMEM);
		return -ENOMEM;
	}
	char header[256];
	int header_len = snprintf(header,
	    sizeof(header),
	    "\x1b]1337;File=name=%s;size=%zu;inline=1:",
	    name_encoded,
	    size);
	free(name_encoded);
	if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
		free(data_encoded);
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	if (write_sequence(handle, header, (size_t)header_len) < 0) {
		free(data_encoded);
		return -handle->last_errno;
	}
	if (write_sequence(handle, data_encoded, data_len) < 0) {
		free(data_encoded);
		return -handle->last_errno;
	}
	free(data_encoded);
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_write_text(terse_handle_t handle, const char *graphemes)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!graphemes) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}

	return write_literal(handle, graphemes);
}

int terse_flush(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	clear_error(handle);
	return 0;
}

static void
set_key_event(terse_event_t *event, terse_event_type_t type, int mods)
{
	event->type = type;
	event->data.key.mods = mods;
}

static void
set_char_event(terse_event_t *event, unsigned int scalar, int mods)
{
	event->type = TERSE_EVENT_CHAR;
	event->data.ch.scalar = scalar;
	event->data.ch.width = 1;
	event->data.ch.mods = mods;
}

static void
set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length)
{
	event->type = TERSE_EVENT_RAW_SEQUENCE;
	if (length > TERSE_EVENT_RAW_MAX) {
		length = TERSE_EVENT_RAW_MAX;
	}
	event->data.raw.length = length;
	memset(event->data.raw.bytes, 0, TERSE_EVENT_RAW_MAX);
	memcpy(event->data.raw.bytes, bytes, length);
}

static void
set_resize_event(terse_event_t *event, int rows, int cols)
{
	event->type = TERSE_EVENT_RESIZE;
	event->data.resize.rows = rows;
	event->data.resize.cols = cols;
}

int terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_event) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	int fd = handle->options.input_fd;
	int ready = wait_for_input(fd, timeout_ms);
	if (ready == 0) {
		clear_error(handle);
		return TERSE_EVENT_NONE;
	}
	if (ready < 0) {
		int err = -ready;
		set_error(handle, TERSE_ERROR_TRANSPORT, err);
		return -err;
	}

	unsigned char first = 0;
	ssize_t n = read_byte(fd, &first);
	if (n == 0) {
		errno = EPIPE;
		set_error(handle, TERSE_ERROR_TRANSPORT, EPIPE);
		return -EPIPE;
	}
	if (n < 0) {
		int err = -(int)n;
		set_error(handle, TERSE_ERROR_TRANSPORT, err);
		return (int)n;
	}

	switch (first) {
	case '\r':
	case '\n':
		set_key_event(out_event, TERSE_EVENT_ENTER, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\b':
	case 0x7f:
		set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\t':
		set_key_event(out_event, TERSE_EVENT_TAB, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	default:
		break;
	}

	if (first >= 0x01 && first <= 0x1a) {
		unsigned int scalar = 'A' + (first - 1);
		set_char_event(out_event, scalar, TERSE_MOD_CTRL);
		return TERSE_EVENT_OK;
	}

	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);
		int values[8] = { 0 };
		size_t value_count = 0;
		char final = 0;
		if (parse_csi_sequence(seq, len, values, 8, &value_count, &final) == 0) {
			if ((final == 'M' || final == 'm') && len > 2 && seq[2] == '<') {
				if (handle_sgr_mouse_sequence(handle, out_event, values, value_count, final)) {
					return TERSE_EVENT_OK;
				}
			}
			if (final == '~' && value_count >= 1 && handle->paste_enabled) {
				if (values[0] == 200) {
					out_event->type = TERSE_EVENT_PASTE_BEGIN;
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (values[0] == 201) {
					out_event->type = TERSE_EVENT_PASTE_END;
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
			}
			if (final == 't' && value_count >= 3 && values[0] == 8) {
				set_resize_event(out_event, values[1], values[2]);
				handle->size.rows = values[1];
				handle->size.cols = values[2];
				handle->size.known = 1;
				handle->capabilities.has_size = 1;
				clear_error(handle);
				return TERSE_EVENT_OK;
			}
			if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
				int mods = 0;
				if (value_count > 0) {
					mods = modifier_bits_from_param(values[value_count - 1]);
				}
				switch (final) {
				case 'A':
					set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'B':
					set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'C':
					set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'D':
					set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				default:
					break;
				}
			}
		}
		set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_EVENT_OK;
	}

	if (first >= 0x20 && first <= 0x7e) {
		set_char_event(out_event, first, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	}

	unsigned char raw_bytes[1] = { first };
	set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_EVENT_OK;
}

terse_size_t
terse_get_size(terse_handle_t handle)
{
	terse_size_t unknown = make_unknown_size();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		clear_error(handle);
		return unknown;
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	if (!(handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE)) {
		handle->capabilities.has_size = handle->size.known;
	}
	clear_error(handle);
	return handle->size;
}

int terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_options) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	*out_options = handle->options;
	clear_error(handle);
	return 0;
}

terse_error_info_t
terse_get_last_error(terse_handle_t handle)
{
	terse_error_info_t info = { TERSE_ERROR_NONE, 0 };
	if (!handle) {
		info.category = TERSE_ERROR_STATE;
		info.code = EINVAL;
		return info;
	}
	info.category = handle->last_error;
	info.code = handle->last_errno;
	return info;
}

int terse_capture_state(terse_handle_t handle, terse_state_t *out_state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	out_state->cursor_known = handle->cursor_known;
	out_state->cursor_visible = handle->cursor_visible;
	out_state->cursor_row = handle->cursor_row;
	out_state->cursor_col = handle->cursor_col;
	out_state->style_known = handle->style_known;
	out_state->style = handle->style;
	clear_error(handle);
	return 0;
}

int terse_restore_state(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int result = 0;
	if (state->cursor_known) {
		if (handle->capabilities.has_move_absolute && state->cursor_row > 0 && state->cursor_col > 0) {
			int move_rc = terse_move_to(handle, state->cursor_row, state->cursor_col);
			if (move_rc < 0 && result == 0) {
				result = move_rc;
			}
		}
	}
	if (handle->capabilities.has_cursor_visibility) {
		int want_visible = state->cursor_known ? state->cursor_visible : 1;
		int vis_rc = terse_show_cursor(handle, want_visible);
		if (vis_rc < 0 && result == 0) {
			result = vis_rc;
		}
	}
	if (state->style_known) {
		int style_rc = terse_set_style(handle, &state->style);
		if (style_rc < 0 && result == 0) {
			result = style_rc;
		}
	}
	return result;
}
