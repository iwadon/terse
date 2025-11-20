#include "terse.h"
#include "terse_platform.h"
#include "terse_test.h"
#include "terse_unicode.h"
#include "terse_detection.h"
#include "terse_codec.h"
#include "terse_event_helpers.h"
#include "terse_input.h"

/* State history stack depth macro.  Small for sanity, yet enough for
 * typical nested UI layers.
 */
#define TERSE_STATE_STACK_MAX 8

#ifndef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 1
#endif

#include <ctype.h>
#include <errno.h>
#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "mini_iconv.h"
#endif
#include <limits.h>
#ifndef _WIN32
#ifdef TERSE_HAVE_POLL_H
#include <poll.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif
#endif
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#include <unistd.h>
#else
// Windows compatibility
#define strcasecmp _stricmp
#endif

static const char TERSE_RESET_ALL_SEQ[] = "\x1b[0m";
static const char TERSE_RESET_COLOR_SEQ[] = "\x1b[39;49m";
static const char TERSE_RESET_EFFECTS_SEQ[] = "\x1b[22;23;24;27;29m";
static const char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char TERSE_MODIFY_OTHER_KEYS_ENABLE_SEQ[] = "\x1b[>4;2m";
static const char TERSE_MODIFY_OTHER_KEYS_DISABLE_SEQ[] = "\x1b[>4;0m";
static const char TERSE_KITTY_PROTOCOL_ENABLE_SEQ[] = "\x1b[>1u";
static const char TERSE_KITTY_PROTOCOL_DISABLE_SEQ[] = "\x1b[<u";

typedef struct terse_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} terse_rgb_t;

typedef enum terse_codec_kind {
	TERSE_CODEC_UNKNOWN = 0,
	TERSE_CODEC_UTF8,
	TERSE_CODEC_SHIFT_JIS
} terse_codec_kind_t;

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

static const unsigned int UTF8_REPLACEMENT = 0xfffdU;
static const unsigned int SHIFT_JIS_REPLACEMENT = '?';

static void
reset_iconv_state(iconv_t cd)
{
	if (cd == (iconv_t)-1) {
		return;
	}
	(void)iconv(cd, NULL, NULL, NULL, NULL);
}

static unsigned int
decode_utf8_bytes(const unsigned char *bytes, size_t length)
{
	if (!bytes || length == 0) {
		return UTF8_REPLACEMENT;
	}
	unsigned int scalar = 0;
	if (length == 1) {
	unsigned char b0 = bytes[0];
		if (b0 < 0x80) {
			return b0;
		}
		return UTF8_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		if ((b0 & 0xe0) != 0xc0 || (b1 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x1f) << 6) | (unsigned int)(b1 & 0x3f);
		if (scalar < 0x80) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 3) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		if ((b0 & 0xf0) != 0xe0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x0f) << 12) | ((unsigned int)(b1 & 0x3f) << 6) | (unsigned int)(b2 & 0x3f);
		if (scalar < 0x800 || (scalar >= 0xd800 && scalar <= 0xdfff)) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 4) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		unsigned char b3 = bytes[3];
		if ((b0 & 0xf8) != 0xf0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x07) << 18) | ((unsigned int)(b1 & 0x3f) << 12) | ((unsigned int)(b2 & 0x3f) << 6) | (unsigned int)(b3 & 0x3f);
		if (scalar < 0x10000 || scalar > 0x10ffff) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	return UTF8_REPLACEMENT;
}

static terse_codec_kind_t
codec_kind_from_name(const char *name)
{
	if (!name) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "UTF-8") == 0 || strcasecmp(name, "UTF8") == 0) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "SHIFT_JIS") == 0 || strcasecmp(name, "SHIFT-JIS") == 0 || strcasecmp(name, "Shift_JIS") == 0 || strcasecmp(name, "SJIS") == 0) {
		return TERSE_CODEC_SHIFT_JIS;
	}
	return TERSE_CODEC_UTF8;
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
	int requested_level = terse_color_kind_rank(color.kind);
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

#ifdef TERSE_ENABLE_TEST_MODE
typedef struct terse_test_state terse_test_state_t;
#endif

struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_capabilities_t detected_capabilities;
	terse_options_t options;
	terse_codec_kind_t codec_kind;
	iconv_t codec_to_utf8;
	iconv_t utf8_to_codec;
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
	terse_error_t last_error;
	unsigned int runtime_enabled;
	unsigned int runtime_disabled;
	unsigned int keyboard_supported;
	unsigned int keyboard_enabled;
	// State history
	terse_state_t state_stack[TERSE_STATE_STACK_MAX];
	int state_stack_top; // -1 when empty
	unsigned char pending_byte;
	int has_pending_byte;
#ifdef TERSE_ENABLE_TEST_MODE
	terse_test_state_t *test_state;
#endif
};

static void
update_effective_style(terse_handle_t handle)
{
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}

static int write_literal(terse_handle_t handle, const char *literal);
static int write_sequence(terse_handle_t handle, const char *sequence, size_t length);
static void set_error(terse_handle_t handle, terse_error_t error);
static int send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
static int send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
static int send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);

static int
initialize_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return -1;
	}
	handle->codec_kind = codec_kind_from_name(handle->options.codec_name);
	handle->codec_to_utf8 = (iconv_t)-1;
	handle->utf8_to_codec = (iconv_t)-1;
	if (handle->codec_kind == TERSE_CODEC_SHIFT_JIS) {
		handle->codec_to_utf8 = iconv_open("UTF-8", "SHIFT_JIS");
		if (handle->codec_to_utf8 == (iconv_t)-1) {
			return -1;
		}
		handle->utf8_to_codec = iconv_open("SHIFT_JIS", "UTF-8");
		if (handle->utf8_to_codec == (iconv_t)-1) {
			iconv_close(handle->codec_to_utf8);
			handle->codec_to_utf8 = (iconv_t)-1;
			return -1;
		}
	}
	return 0;
}

static void
destroy_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (handle->codec_to_utf8 != (iconv_t)-1) {
		iconv_close(handle->codec_to_utf8);
		handle->codec_to_utf8 = (iconv_t)-1;
	}
	if (handle->utf8_to_codec != (iconv_t)-1) {
		iconv_close(handle->utf8_to_codec);
		handle->utf8_to_codec = (iconv_t)-1;
	}
}

#ifdef TERSE_ENABLE_TEST_MODE
typedef struct terse_test_state {
	int recording;
	terse_call_record_t *calls;
	int call_count;
	int call_capacity;
	terse_capabilities_t mock_caps;
	int mock_caps_enabled;
	int mock_rows;
	int mock_cols;
	int mock_size_enabled;
	terse_event_t *mock_events;
	int mock_event_count;
	int mock_event_read_index;
} terse_test_state_t;

static void
record_call(terse_handle_t handle, terse_call_type_t type, const void *data, size_t data_size)
{
	if (!handle || !handle->test_state || !handle->test_state->recording) {
		return;
	}
	terse_test_state_t *ts = handle->test_state;
	if (ts->call_count >= ts->call_capacity) {
		int new_capacity = (ts->call_capacity == 0) ? 16 : (ts->call_capacity * 2);
		terse_call_record_t *new_calls = realloc(ts->calls, sizeof(terse_call_record_t) * (size_t)new_capacity);
		if (!new_calls) {
			return;
		}
		ts->calls = new_calls;
		ts->call_capacity = new_capacity;
	}
	terse_call_record_t *rec = &ts->calls[ts->call_count];
	memset(rec, 0, sizeof(*rec));
	rec->type = type;
	if (data && data_size > 0) {
		memcpy(&rec->data, data, data_size < sizeof(rec->data) ? data_size : sizeof(rec->data));
	}
	ts->call_count++;
}
#endif

static unsigned int
convert_shift_jis_pair(terse_handle_t handle, unsigned char lead, unsigned char trail)
{
	(void)lead;
	(void)trail;
	if (!handle || handle->codec_to_utf8 == (iconv_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	char inbuf[2];
	inbuf[0] = (char)lead;
	inbuf[1] = (char)trail;
	char *in_ptr = inbuf;
	size_t in_left = sizeof(inbuf);
	char outbuf[8] = { 0 };
	char *out_ptr = outbuf;
	size_t out_left = sizeof(outbuf);
	reset_iconv_state(handle->codec_to_utf8);
	if (iconv(handle->codec_to_utf8, &in_ptr, &in_left, &out_ptr, &out_left) == (size_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	if (in_left != 0) {
		return SHIFT_JIS_REPLACEMENT;
	}
	size_t produced = (size_t)(out_ptr - outbuf);
	return decode_utf8_bytes((const unsigned char *)outbuf, produced);
}

static unsigned int
disable_mask_from_enable(unsigned int enable_mask)
{
	unsigned int mask = 0;
	if (enable_mask & TERSE_CAP_ENABLE_SGR_BASIC) {
		mask |= TERSE_CAP_DISABLE_SGR_BASIC;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TEXT_STYLES) {
		mask |= TERSE_CAP_DISABLE_TEXT_STYLES;
	}
	if (enable_mask & TERSE_CAP_ENABLE_SGR_EXTENDED) {
		mask |= TERSE_CAP_DISABLE_SGR_EXTENDED;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TRUECOLOR) {
		mask |= TERSE_CAP_DISABLE_TRUECOLOR;
	}
	if (enable_mask & TERSE_CAP_ENABLE_MOUSE) {
		mask |= TERSE_CAP_DISABLE_MOUSE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_BRACKETED_PASTE) {
		mask |= TERSE_CAP_DISABLE_BRACKETED_PASTE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TITLE) {
		mask |= TERSE_CAP_DISABLE_TITLE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_HYPERLINK) {
		mask |= TERSE_CAP_DISABLE_HYPERLINK;
	}
	if (enable_mask & TERSE_CAP_ENABLE_CURSOR_SHAPE) {
		mask |= TERSE_CAP_DISABLE_CURSOR_SHAPE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_CLIPBOARD_WRITE) {
		mask |= TERSE_CAP_DISABLE_CLIPBOARD_WRITE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_IMAGE_INLINE) {
		mask |= TERSE_CAP_DISABLE_IMAGE_INLINE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_BELL) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_BELL;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_VISUAL) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_VISUAL;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP;
	}
	return mask;
}

static unsigned int
enable_mask_from_disable(unsigned int disable_mask)
{
	unsigned int mask = 0;
	if (disable_mask & TERSE_CAP_DISABLE_SGR_BASIC) {
		mask |= TERSE_CAP_ENABLE_SGR_BASIC;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TEXT_STYLES) {
		mask |= TERSE_CAP_ENABLE_TEXT_STYLES;
	}
	if (disable_mask & TERSE_CAP_DISABLE_SGR_EXTENDED) {
		mask |= TERSE_CAP_ENABLE_SGR_EXTENDED;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TRUECOLOR) {
		mask |= TERSE_CAP_ENABLE_TRUECOLOR;
	}
	if (disable_mask & TERSE_CAP_DISABLE_MOUSE) {
		mask |= TERSE_CAP_ENABLE_MOUSE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_BRACKETED_PASTE) {
		mask |= TERSE_CAP_ENABLE_BRACKETED_PASTE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TITLE) {
		mask |= TERSE_CAP_ENABLE_TITLE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_HYPERLINK) {
		mask |= TERSE_CAP_ENABLE_HYPERLINK;
	}
	if (disable_mask & TERSE_CAP_DISABLE_CURSOR_SHAPE) {
		mask |= TERSE_CAP_ENABLE_CURSOR_SHAPE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_CLIPBOARD_WRITE) {
		mask |= TERSE_CAP_ENABLE_CLIPBOARD_WRITE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_IMAGE_INLINE) {
		mask |= TERSE_CAP_ENABLE_IMAGE_INLINE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_BELL) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_BELL;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_VISUAL) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_VISUAL;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP;
	}
	return mask;
}

static void
recompute_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	handle->capabilities = handle->detected_capabilities;

	unsigned int disabled = handle->options.disabled_caps | handle->runtime_disabled;
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
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_BELL) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_VISUAL) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_VISUAL;
	}
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	}

	unsigned int enabled = handle->options.enabled_caps | handle->runtime_enabled;
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
		if (handle->detected_capabilities.images != TERSE_IMAGE_NONE) {
			handle->capabilities.images = handle->detected_capabilities.images;
		} else if (handle->capabilities.images == TERSE_IMAGE_NONE) {
			handle->capabilities.images = TERSE_IMAGE_ITERM_INLINE;
		}
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_BELL) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_VISUAL) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_VISUAL;
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
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
	if (handle->style_known) {
		handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	}
}

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
send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	size_t name_len = 0;
	char *name_encoded = base64_encode((const unsigned char *)name, strlen(name), &name_len);
	size_t data_len = 0;
	char *data_encoded = base64_encode(data, size, &data_len);
	if (!name_encoded || !data_encoded) {
		free(name_encoded);
		free(data_encoded);
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
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
		set_error(handle, TERSE_ERR_OVERFLOW);
		return TERSE_ERR_OVERFLOW;
	}
	if (write_sequence(handle, header, (size_t)header_len) != 0) {
		free(data_encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, data_encoded, data_len) != 0) {
		free(data_encoded);
		return handle->last_error;
	}
	free(data_encoded);
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static int
send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	static const char prefix[] = "\x1bPq";
	static const char suffix[] = "\x1b\\";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) != 0) {
		return handle->last_error;
	}
	const size_t chunk_size = 1024;
	size_t offset = 0;
	while (offset < size) {
		size_t remaining = size - offset;
		size_t to_write = remaining > chunk_size ? chunk_size : remaining;
		if (write_sequence(handle, (const char *)data + offset, to_write) != 0) {
			return handle->last_error;
		}
		offset += to_write;
	}
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static int
send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	size_t encoded_len = 0;
	char *encoded = base64_encode(data, size, &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	const char prefix[] = "\x1b_Ga=T,f=100,m=1;";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) != 0) {
		free(encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, encoded, encoded_len) != 0) {
		free(encoded);
		return handle->last_error;
	}
	free(encoded);
	const char suffix[] = "\x1b\\";
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static void
set_error(terse_handle_t handle, terse_error_t error)
{
	if (!handle) {
		return;
	}
	handle->last_error = error;
}

static void
clear_error(terse_handle_t handle)
{
	set_error(handle, TERSE_OK);
}

static void emit_reset_sequences(terse_handle_t handle);

terse_error_t terse_validate_options(const terse_options_t *options)
{
	if (!options) {
		return 0;
	}
	if (options->input_fd < 0 || options->output_fd < 0) {
		errno = EBADF;
		return TERSE_ERR_INVALID_HANDLE;
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

static void
refresh_size(terse_handle_t handle)
{
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		handle->size = make_unknown_size();
		return;
	}
	terse_size_t size = terse_platform_query_fd_size(handle->options.output_fd);
	if (!size.known && handle->options.input_fd != handle->options.output_fd) {
		size = terse_platform_query_fd_size(handle->options.input_fd);
	}
	if (size.known || !handle->size.known) {
		handle->size = size;
	}
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile != TERSE_PROFILE_AUTO && (requested_profile < TERSE_P0 || requested_profile > TERSE_P3)) {
		return NULL;
	}
	if (terse_validate_options(options) != 0) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}
	memset(handle, 0, sizeof(*handle));

	// Initialize state stack
	handle->state_stack_top = -1;

	handle->requested_profile = requested_profile;
	handle->capabilities = terse_make_p0_capabilities();

	terse_options_t defaults = terse_platform_default_options();
	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = defaults.codec_name;
		}
	} else {
		handle->options = defaults;
	}
	if (initialize_codec_handles(handle) < 0) {
		int err = errno ? errno : EINVAL;
		destroy_codec_handles(handle);
		free(handle);
		errno = err;
		return NULL;
	}
	handle->size = make_unknown_size();
	handle->capabilities = detect_environment_capabilities(handle->requested_profile, &handle->options);
	handle->detected_capabilities = handle->capabilities;
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	handle->keyboard_supported = handle->capabilities.keyboard_features;
	handle->keyboard_enabled = 0;
	recompute_capabilities(handle);
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
	handle->has_pending_byte = 0;

#ifdef TERSE_ENABLE_TEST_MODE
	handle->test_state = malloc(sizeof(terse_test_state_t));
	if (handle->test_state) {
		memset(handle->test_state, 0, sizeof(terse_test_state_t));
		handle->test_state->recording = 0;
		handle->test_state->calls = NULL;
		handle->test_state->call_count = 0;
		handle->test_state->call_capacity = 0;
		handle->test_state->mock_caps_enabled = 0;
		handle->test_state->mock_size_enabled = 0;
		handle->test_state->mock_events = NULL;
		handle->test_state->mock_event_count = 0;
		handle->test_state->mock_event_read_index = 0;
	}
#endif

	return handle;
}

void terse_close(terse_handle_t handle)
{
	if (handle) {
		if (handle->keyboard_enabled) {
			(void)terse_keyboard_disable(handle, handle->keyboard_enabled);
		}
#ifdef TERSE_ENABLE_TEST_MODE
		if (handle->test_state) {
			free(handle->test_state->calls);
			free(handle->test_state->mock_events);
			free(handle->test_state);
		}
#endif
	}
	emit_reset_sequences(handle);
	destroy_codec_handles(handle);
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return terse_make_p0_capabilities();
	}
#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_caps_enabled) {
		return handle->test_state->mock_caps;
	}
#endif
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
		return TERSE_ERR_INVALID_HANDLE;
	}
	return 0;
}

static void
apply_runtime_overrides(terse_handle_t handle)
{
	recompute_capabilities(handle);
	clear_error(handle);
}

terse_error_t terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (enable_mask == 0) {
		clear_error(handle);
		return 0;
	}
	handle->runtime_enabled |= enable_mask;
	handle->runtime_disabled &= ~disable_mask_from_enable(enable_mask);
	apply_runtime_overrides(handle);
	return 0;
}

terse_error_t terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (disable_mask == 0) {
		clear_error(handle);
		return 0;
	}
	handle->runtime_disabled |= disable_mask;
	handle->runtime_enabled &= ~enable_mask_from_disable(disable_mask);
	apply_runtime_overrides(handle);
	return 0;
}

terse_error_t terse_capabilities_reset_overrides(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	apply_runtime_overrides(handle);
	return 0;
}

terse_error_t terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (feature_mask == 0) {
		clear_error(handle);
		return 0;
	}
	unsigned int supported = handle->keyboard_supported & feature_mask;
	unsigned int to_enable = supported & ~handle->keyboard_enabled;
	if (to_enable == 0) {
		clear_error(handle);
		return 0;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (to_enable & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		rc = write_literal(handle, TERSE_MODIFY_OTHER_KEYS_ENABLE_SEQ);
		if (rc != 0) {
			return rc;
		}
	}
	if (to_enable & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		rc = write_literal(handle, TERSE_KITTY_PROTOCOL_ENABLE_SEQ);
		if (rc != 0) {
			return rc;
		}
	}
	handle->keyboard_enabled |= to_enable;
	clear_error(handle);
	return 0;
}

terse_error_t terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (feature_mask == 0) {
		clear_error(handle);
		return 0;
	}
	unsigned int to_disable = feature_mask & handle->keyboard_enabled;
	if (to_disable == 0) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (to_disable & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
			rc = write_literal(handle, TERSE_MODIFY_OTHER_KEYS_DISABLE_SEQ);
			if (rc != 0) {
				return rc;
			}
		}
		if (to_disable & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
			rc = write_literal(handle, TERSE_KITTY_PROTOCOL_DISABLE_SEQ);
			if (rc != 0) {
				return rc;
			}
		}
	}
	handle->keyboard_enabled &= ~to_disable;
	clear_error(handle);
	return 0;
}

unsigned int terse_keyboard_get_enabled(terse_handle_t handle)
{
	if (!handle) {
		return 0;
	}
	return handle->keyboard_enabled;
}

unsigned int terse_keyboard_get_supported(terse_handle_t handle)
{
	if (!handle) {
		return 0;
	}
	return handle->keyboard_supported;
}

terse_error_t terse_state_override(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (state->cursor_known) {
		handle->cursor_known = 1;
		// Clamp to 0-based minimum
		handle->cursor_row = state->cursor_row >= 0 ? state->cursor_row : 0;
		handle->cursor_col = state->cursor_col >= 0 ? state->cursor_col : 0;
	} else {
		handle->cursor_known = 0;
		if (state->cursor_row >= 0) {
			handle->cursor_row = state->cursor_row;
		}
		if (state->cursor_col >= 0) {
			handle->cursor_col = state->cursor_col;
		}
	}
	handle->cursor_visible = state->cursor_visible ? 1 : 0;
	if (state->style_known) {
		terse_style_t sanitized = sanitize_style_request(&state->style);
		handle->style = sanitized;
		handle->effective_style = make_effective_style(&handle->capabilities, &sanitized);
		handle->style_known = 1;
	} else {
		handle->style_known = 0;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_state_clear(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	handle->cursor_known = 0;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_visible = 1;
	handle->style = terse_style_default();
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 0;
	clear_error(handle);
	return 0;
}

// ---------- State history helpers ----------
terse_error_t terse_push_state(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (handle->state_stack_top >= TERSE_STATE_STACK_MAX - 1) {
		set_error(handle, TERSE_ERR_STACK_OVERFLOW);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	terse_state_t *stack_state = &handle->state_stack[handle->state_stack_top + 1];
	stack_state->cursor_known = handle->cursor_known;
	stack_state->cursor_visible = handle->cursor_visible;
	stack_state->cursor_row = handle->cursor_row;
	stack_state->cursor_col = handle->cursor_col;
	stack_state->style_known = handle->style_known;
	stack_state->style = handle->style;
	handle->state_stack_top++;
	return 0;
}

terse_error_t terse_pop_state(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (handle->state_stack_top < 0) {
		set_error(handle, TERSE_ERR_STACK_UNDERFLOW);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	const terse_state_t *state = &handle->state_stack[handle->state_stack_top];
	handle->cursor_known = state->cursor_known;
	handle->cursor_visible = state->cursor_visible;
	handle->cursor_row = state->cursor_row;
	handle->cursor_col = state->cursor_col;
	handle->style_known = state->style_known;
	handle->style = state->style;
	if (state->style_known) {
		handle->effective_style = make_effective_style(&handle->capabilities, &state->style);
	}
	handle->state_stack_top--;
	return 0;
}

static int
write_literal(terse_handle_t handle, const char *literal)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!literal) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	int out = terse_platform_write_bytes(handle->options.output_fd, literal, strlen(literal));
	if (out < 0) {
		set_error(handle, TERSE_ERR_IO);
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static int
write_sequence(terse_handle_t handle, const char *sequence, size_t length)
{
	int out = terse_platform_write_bytes(handle->options.output_fd, sequence, length);
	if (out < 0) {
		set_error(handle, TERSE_ERR_IO);
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static int
payload_has_disallowed_chars(const char *payload)
{
	if (!payload) {
		return 0;
	}
	for (const unsigned char *p = (const unsigned char *)payload; *p; ++p) {
		if (*p == 0x07 || *p == 0x1b) {
			return 1;
		}
	}
	return 0;
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

terse_error_t terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_screen) {
		clear_error(handle);
		return 0;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { terse_clear_mode_t mode; } rec_data;
		rec_data.mode = mode;
		record_call(handle, TERSE_CALL_CLEAR_SCREEN, &rec_data, sizeof(rec_data));
	}
#endif

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
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	return write_literal(handle, sequence);
}

terse_error_t terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_line) {
		clear_error(handle);
		return 0;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { terse_clear_mode_t mode; } rec_data;
		rec_data.mode = mode;
		record_call(handle, TERSE_CALL_CLEAR_LINE, &rec_data, sizeof(rec_data));
	}
#endif

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
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	return write_literal(handle, sequence);
}

terse_error_t terse_move_to(terse_handle_t handle, int row, int col)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_absolute) {
		clear_error(handle);
		return 0;
	}

	// Clamp to 0-based coordinate minimum
	if (row < 0) {
		row = 0;
	}
	if (col < 0) {
		col = 0;
	}
	if (row == handle->cursor_row && col == handle->cursor_col) {
		clear_error(handle);
		return 0;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { int row; int col; } rec_data = { row, col };
		record_call(handle, TERSE_CALL_MOVE_TO, &rec_data, sizeof(rec_data));
	}
#endif

	char sequence[32];
	// Terminal escape sequences use 1-based coordinates, convert from 0-based
	int written = snprintf(sequence, sizeof(sequence), "\x1b[%d;%dH", row + 1, col + 1);
	if (written <= 0 || (size_t)written >= sizeof(sequence)) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	terse_error_t out = write_sequence(handle, sequence, (size_t)written);
	if (out == TERSE_OK) {
		handle->cursor_row = row;
		handle->cursor_col = col;
		handle->cursor_known = 1;
	}
	return out;
}

terse_error_t terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
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
			return TERSE_ERR_INVALID_ARGUMENT;
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
			return TERSE_ERR_INVALID_ARGUMENT;
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
			return TERSE_ERR_INVALID_ARGUMENT;
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
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	}

	// Clamp to 0-based coordinate minimum
	if (new_row < 0) {
		new_row = 0;
	}
	if (new_col < 0) {
		new_col = 0;
	}
	handle->cursor_row = new_row;
	handle->cursor_col = new_col;
	handle->cursor_known = 1;
	clear_error(handle);
	return 0;
}

terse_error_t terse_show_cursor(terse_handle_t handle, int visible)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
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
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if ((size_t)written >= size - *pos) {
		errno = EOVERFLOW;
		return TERSE_ERR_OVERFLOW;
	}
	*pos += (size_t)written;
	*first = 0;
	return 0;
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
	if (reset != 0) {
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
		set_error(handle, TERSE_ERR_OVERFLOW);
		return TERSE_ERR_OVERFLOW;
	}
	size_t pos = (size_t)prefix;
	int rc = append_effects(seq, sizeof(seq), &pos, &first, style->effects);
	if (rc < 0) {
		set_error(handle, TERSE_ERR_IO);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 1, &style->foreground);
	if (rc < 0) {
		set_error(handle, TERSE_ERR_IO);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 0, &style->background);
	if (rc < 0) {
		set_error(handle, TERSE_ERR_IO);
		return rc;
	}
	if (first) {
		return 0;
	}
	if (pos >= sizeof(seq) - 1) {
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERR_OVERFLOW);
		return TERSE_ERR_OVERFLOW;
	}
	seq[pos++] = 'm';
	return write_sequence(handle, seq, pos);
}

terse_error_t terse_set_style(terse_handle_t handle, const terse_style_t *style)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!style) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { terse_style_t style; } rec_data;
		rec_data.style = *style;
		record_call(handle, TERSE_CALL_SET_STYLE, &rec_data, sizeof(rec_data));
	}
#endif

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

terse_error_t terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (scope < TERSE_RESET_ALL || scope > TERSE_RESET_EFFECTS_ONLY) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
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
		if (write_literal(handle, seqs[i]) != 0) {
			return handle->last_error;
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

terse_error_t terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (mode <= TERSE_MOUSE_NONE || mode > TERSE_MOUSE_SGR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
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
		return handle->last_error;
	}
	handle->mouse_mode = actual;
	handle->mouse_enabled = 1;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	clear_error(handle);
	return 0;
}

terse_error_t terse_disable_mouse(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!handle->mouse_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_mouse_mode(handle, handle->mouse_mode, 0) < 0) {
			return handle->last_error;
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

terse_error_t terse_enable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
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
		return handle->last_error;
	}
	handle->paste_enabled = 1;
	clear_error(handle);
	return 0;
}

terse_error_t terse_disable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!handle->paste_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_bracketed_paste(handle, 0) < 0) {
			return handle->last_error;
		}
	}
	handle->paste_enabled = 0;
	clear_error(handle);
	return 0;
}

terse_error_t terse_set_title(terse_handle_t handle, const char *title)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!title) {
		title = "";
	}
	if (!handle->capabilities.has_title || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x1b]0;") != 0) {
		return handle->last_error;
	}
	if (write_sequence(handle, title, strlen(title)) != 0) {
		return handle->last_error;
	}
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
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
	if (write_literal(handle, "\x1b]8;;") != 0) {
		return handle->last_error;
	}
	if (write_sequence(handle, url, strlen(url)) != 0) {
		return handle->last_error;
	}
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	if (write_sequence(handle, label, strlen(label)) != 0) {
		return handle->last_error;
	}
	if (write_literal(handle, "\x1b]8;;\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (shape < TERSE_CURSOR_SHAPE_DEFAULT || shape > TERSE_CURSOR_SHAPE_BAR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
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
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	return write_literal(handle, seq);
}

terse_error_t terse_set_clipboard(terse_handle_t handle, const char *data)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!data) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_clipboard_write || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	size_t encoded_len = 0;
	char *encoded = base64_encode((const unsigned char *)data, strlen(data), &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	if (write_literal(handle, "\x1b]52;;") != 0) {
		free(encoded);
		return handle->last_error;
	}
	if (write_sequence(handle, encoded, encoded_len) != 0) {
		free(encoded);
		return handle->last_error;
	}
	free(encoded);
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	terse_image_request_t request = {
		.data = data,
		.size = size,
		.name = name,
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};
	return terse_display_image(handle, &request);
}

terse_error_t terse_display_image(terse_handle_t handle, const terse_image_request_t *request)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!request) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!request->data || request->size == 0) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	unsigned int flags = request->flags;
	if (flags == 0) {
		flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE;
	}
	int degrade_allowed = (flags & TERSE_IMAGE_FLAG_ALLOW_DEGRADE) != 0;
	if (!handle->capabilities.has_basic_output) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	terse_image_support_t available = handle->capabilities.images;
	if (available == TERSE_IMAGE_NONE) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	terse_image_support_t target = available;
	switch (request->format) {
	case TERSE_IMAGE_FORMAT_AUTO:
	case TERSE_IMAGE_FORMAT_PNG:
	case TERSE_IMAGE_FORMAT_JPEG:
		break;
	case TERSE_IMAGE_FORMAT_SIXEL:
		if (available == TERSE_IMAGE_SIXEL) {
			target = TERSE_IMAGE_SIXEL;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	case TERSE_IMAGE_FORMAT_KITTY:
		if (available == TERSE_IMAGE_KITTY) {
			target = TERSE_IMAGE_KITTY;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	default:
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
	const char *name = request->name;
	if (!name || !*name) {
		name = "image";
	}
	(void)request->width;
	(void)request->height;
	switch (target) {
	case TERSE_IMAGE_ITERM_INLINE:
		return send_iterm_inline_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_SIXEL:
		return send_sixel_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_KITTY:
		return send_kitty_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_NONE:
	default:
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERR_UNSUPPORTED);
		return TERSE_ERR_UNSUPPORTED;
	}
}

terse_error_t terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	unsigned int required = 0;
	switch (kind) {
	case TERSE_NOTIFICATION_KIND_BELL:
		required = TERSE_NOTIFICATION_SUPPORT_BELL;
		break;
	case TERSE_NOTIFICATION_KIND_VISUAL:
		required = TERSE_NOTIFICATION_SUPPORT_VISUAL;
		break;
	case TERSE_NOTIFICATION_KIND_DESKTOP:
		required = TERSE_NOTIFICATION_SUPPORT_DESKTOP;
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.notifications & required) == 0) {
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_DESKTOP) {
		if (!payload || payload_has_disallowed_chars(payload)) {
			errno = EINVAL;
			set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		if (write_literal(handle, "\x1b]9;1;") != 0) {
			return handle->last_error;
		}
		if (write_sequence(handle, payload, strlen(payload)) != 0) {
			return handle->last_error;
		}
		if (write_literal(handle, "\x07") != 0) {
			return handle->last_error;
		}
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_VISUAL) {
		if (write_literal(handle, "\x1b[?5h\x1b[?5l") != 0) {
			return handle->last_error;
		}
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x07") != 0) {
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_write_text(terse_handle_t handle, const char *graphemes)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!graphemes) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { char text[256]; } rec_data;
		memset(&rec_data, 0, sizeof(rec_data));
		size_t len = strlen(graphemes);
		if (len >= sizeof(rec_data.text)) {
			len = sizeof(rec_data.text) - 1;
		}
		memcpy(rec_data.text, graphemes, len);
		rec_data.text[len] = '\0';
		record_call(handle, TERSE_CALL_WRITE_TEXT, &rec_data, sizeof(rec_data));
	}
#endif

	if (handle->codec_kind == TERSE_CODEC_UTF8) {
		return write_literal(handle, graphemes);
	}
	if (handle->codec_kind == TERSE_CODEC_SHIFT_JIS) {
		if (handle->utf8_to_codec == (iconv_t)-1) {
			errno = EINVAL;
			set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		const char *input = graphemes;
		size_t in_left = strlen(graphemes);
		unsigned char outbuf[256];
		reset_iconv_state(handle->utf8_to_codec);
		while (in_left > 0) {
			char *in_ptr = (char *)input;
			size_t local_in_left = in_left;
			char *out_ptr = (char *)outbuf;
			size_t out_left = sizeof(outbuf);
			size_t iconv_rc = iconv(handle->utf8_to_codec, &in_ptr, &local_in_left, &out_ptr, &out_left);
			size_t produced = (size_t)(out_ptr - (char *)outbuf);
			if (produced > 0) {
				if (write_sequence(handle, (const char *)outbuf, produced) != 0) {
					return handle->last_error;
				}
			}
			input = (const char *)in_ptr;
			in_left = local_in_left;
			if (iconv_rc == (size_t)-1) {
				if (errno == E2BIG) {
					continue;
				}
				if (errno == EILSEQ || errno == EINVAL) {
					if (in_left > 0) {
						input++;
						in_left--;
					}
					const char replacement = '?';
					if (write_sequence(handle, &replacement, 1) != 0) {
						return handle->last_error;
					}
					reset_iconv_state(handle->utf8_to_codec);
					continue;
				}
				// Map errno to terse_error_t
				terse_error_t err = (errno == EILSEQ) ? TERSE_ERR_INVALID_ENCODING : TERSE_ERR_IO;
				set_error(handle, err);
				return err;
			}
		}
		reset_iconv_state(handle->utf8_to_codec);
		clear_error(handle);
		return 0;
	}
	return write_literal(handle, graphemes);
}

terse_error_t terse_flush(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!out_event) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_events) {
		if (handle->test_state->mock_event_read_index < handle->test_state->mock_event_count) {
			*out_event = handle->test_state->mock_events[handle->test_state->mock_event_read_index++];
			clear_error(handle);
			return TERSE_OK;
		} else {
			// All mock events have been read, return EAGAIN equivalent
			errno = EAGAIN;
			set_error(handle, TERSE_ERR_WOULD_BLOCK);
			return TERSE_ERR_WOULD_BLOCK;
		}
	}
#endif

#if defined(__HUMAN68K__) || defined(_WIN32)
	/* Human68k and Windows: Use platform-specific event reading implementation */
	return terse_platform_read_event(handle, timeout_ms, out_event);
#else
	/* POSIX: Use escape sequence parsing implementation */
	int fd = handle->options.input_fd;

	unsigned char first = 0;
	rc = terse_read_input_byte(handle, timeout_ms, &first);
	if (rc == 0) {
		clear_error(handle);
		return TERSE_ERR_NO_EVENT;
	}
	if (rc < 0) {
		// rc is negative terse_error_t, convert to positive
		set_error(handle, (terse_error_t)(-rc));
		return (terse_error_t)(-rc);
	}

	switch (first) {
	case '\r': {
		unsigned char next = 0;
		int peek = terse_read_input_byte(handle, 0, &next);
		if (peek < 0) {
			// peek is negative terse_error_t, convert to positive
			set_error(handle, (terse_error_t)(-peek));
			return (terse_error_t)(-peek);
		}
		if (peek > 0 && next == '\n') {
			// Consume \r\n as Enter
			terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
			clear_error(handle);
			return TERSE_OK;
		}
		// Treat \r alone as Enter, push back next byte if any
		if (peek > 0) {
			handle->pending_byte = next;
			handle->has_pending_byte = 1;
		}
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
		clear_error(handle);
		return TERSE_OK;
	}
	case '\n':
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, TERSE_MOD_CTRL);
		clear_error(handle);
		return TERSE_OK;
	case '\b':
	case 0x7f:
		terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		clear_error(handle);
		return TERSE_OK;
	case '\t':
		terse_set_key_event(out_event, TERSE_EVENT_TAB, 0);
		clear_error(handle);
		return TERSE_OK;
	default:
		break;
	}

	if (first >= 0x01 && first <= 0x1a) {
		unsigned int scalar = 'A' + (first - 1);
		terse_set_char_event(handle, out_event, scalar, TERSE_MOD_CTRL);
		return TERSE_OK;
	}

	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = terse_platform_drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);

		// Linux console function keys: ESC [ [ A through ESC [ [ L (F1-F12)
		// Check BEFORE CSI parsing to prevent misinterpreting as arrow keys
		if (len == 4 && seq[1] == '[' && seq[2] == '[') {
			char code = (char)seq[3];
			int fn = 0;
			if (code >= 'A' && code <= 'L') {
				fn = 1 + (code - 'A');  // A=F1, B=F2, ..., L=F12
			}
			if (fn > 0 && fn <= 12) {
				terse_set_function_event(out_event, fn, 0);
				clear_error(handle);
				return TERSE_OK;
			}
		}

		int values[8] = { 0 };
		size_t value_count = 0;
		char final = 0;
		if (terse_parse_csi_sequence(seq, len, values, 8, &value_count, &final) == 0) {
			if ((final == 'M' || final == 'm') && len > 2 && seq[2] == '<') {
				if (terse_handle_sgr_mouse_sequence(handle, out_event, values, value_count, final)) {
					return TERSE_OK;
				}
			}
			if (final == '~' && value_count >= 1 && handle->paste_enabled) {
				if (values[0] == 200) {
					out_event->type = TERSE_EVENT_PASTE_BEGIN;
					clear_error(handle);
					return TERSE_OK;
				}
				if (values[0] == 201) {
					out_event->type = TERSE_EVENT_PASTE_END;
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'u' && value_count >= 1) {
				unsigned int code = (unsigned int)values[0];
				int mods = 0;
				if (value_count >= 2) {
					mods = terse_mods_from_kitty_param(values[1]);
				}
				if (code == 13) {
					terse_set_key_event(out_event, TERSE_EVENT_ENTER, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code == 9) {
					terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code == 127) {
					terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code >= 0x20 && code <= 0x10ffff) {
					terse_set_char_event(handle, out_event, code, mods);
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'Z') {
				int mods = TERSE_MOD_SHIFT;
				if (value_count > 0) {
					mods = terse_modifier_bits_from_param(values[value_count - 1]);
					if (!(mods & TERSE_MOD_SHIFT)) {
						mods |= TERSE_MOD_SHIFT;
					}
				}
				terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
				clear_error(handle);
				return TERSE_OK;
			}
			if (final == 't' && value_count >= 3 && values[0] == 8) {
				terse_set_resize_event(out_event, values[1], values[2]);
				handle->size.rows = values[1];
				handle->size.cols = values[2];
				handle->size.known = 1;
				handle->capabilities.has_size = 1;
				clear_error(handle);
				return TERSE_OK;
			}
			if (final == 'H' || final == 'F') {
				int mods = 0;
				if (value_count > 0) {
					mods = terse_modifier_bits_from_param(values[value_count - 1]);
				}
				if (final == 'H') {
					terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
				} else {
					terse_set_key_event(out_event, TERSE_EVENT_END, mods);
				}
				clear_error(handle);
				return TERSE_OK;
			}
			if (final == '~') {
				if (value_count == 0) {
					terse_set_raw_event(out_event, seq, len);
					clear_error(handle);
					return TERSE_OK;
				}
				int mods_param = 0;
				int key_code = values[0];
				if (key_code == 27 && value_count >= 3) {
					mods_param = values[1];
					key_code = values[2];
				} else if (value_count > 1) {
					mods_param = values[value_count - 1];
				}
				int mods = terse_modifier_bits_from_param(mods_param);
				switch (key_code) {
				case 1:
				case 7:
					terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
					clear_error(handle);
					return TERSE_OK;
				case 4:
				case 8:
					terse_set_key_event(out_event, TERSE_EVENT_END, mods);
					clear_error(handle);
					return TERSE_OK;
				case 2:
					terse_set_key_event(out_event, TERSE_EVENT_INSERT, mods);
					clear_error(handle);
					return TERSE_OK;
				case 3:
					terse_set_key_event(out_event, TERSE_EVENT_DELETE, mods);
					clear_error(handle);
					return TERSE_OK;
				case 5:
					terse_set_key_event(out_event, TERSE_EVENT_PAGE_UP, mods);
					clear_error(handle);
					return TERSE_OK;
				case 6:
					terse_set_key_event(out_event, TERSE_EVENT_PAGE_DOWN, mods);
					clear_error(handle);
					return TERSE_OK;
				case 9:
					terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_OK;
				case 13:
					if (values[0] == 27 && value_count >= 3) {
						terse_set_key_event(out_event, TERSE_EVENT_ENTER, terse_modifier_bits_from_param(values[1]));
						clear_error(handle);
						return TERSE_OK;
					}
					break;
				default:
					break;
				}
				int fn = terse_function_number_from_code(key_code);
				if (fn > 0) {
					terse_set_function_event(out_event, fn, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (key_code >= 0 && key_code <= 0x10ffff) {
					if (key_code == 9) {
						terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
						clear_error(handle);
						return TERSE_OK;
					}
					if (key_code == 8 || key_code == 127) {
						terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
						clear_error(handle);
						return TERSE_OK;
					}
					terse_set_char_event(handle, out_event, (unsigned int)key_code, mods);
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
				int mods = 0;
				if (value_count > 0) {
					mods = terse_modifier_bits_from_param(values[value_count - 1]);
				}
				switch (final) {
				case 'A':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'B':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'C':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'D':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
					clear_error(handle);
					return TERSE_OK;
				default:
					break;
				}
			}
		}
		if (len >= 3 && seq[1] == 'O') {
			int mods = 0;
			char code = (char)seq[2];
			switch (code) {
			case 'A':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'B':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'C':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'D':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'H':
				terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'F':
				terse_set_key_event(out_event, TERSE_EVENT_END, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'P':
				terse_set_function_event(out_event, 1, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'Q':
				terse_set_function_event(out_event, 2, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'R':
				terse_set_function_event(out_event, 3, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'S':
				terse_set_function_event(out_event, 4, mods);
				clear_error(handle);
				return TERSE_OK;
			default:
				break;
			}
		}
		if (terse_handle_escape_prefixed_char(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}
		terse_set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_OK;
	}

	if (first >= 0x20 || (handle->codec_kind == TERSE_CODEC_SHIFT_JIS && first >= 0x80)) {
		int decode_rc = terse_decode_stream_char(handle, fd, first, out_event);
		if (decode_rc == 0) {
			clear_error(handle);
			return TERSE_OK;
		}
		if (decode_rc < 0) {
			return decode_rc;
		}
	}

	unsigned char raw_bytes[1] = { first };
	terse_set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_OK;
#endif /* !__HUMAN68K__ */
}

terse_size_t
terse_get_size(terse_handle_t handle)
{
	terse_size_t unknown = make_unknown_size();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_size_enabled) {
		terse_size_t mock_size;
		mock_size.rows = handle->test_state->mock_rows;
		mock_size.cols = handle->test_state->mock_cols;
		mock_size.known = 1;
		return mock_size;
	}
#endif
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

static terse_cursor_position_t
make_unknown_cursor_position(void)
{
	terse_cursor_position_t pos = {0, 0, 0};
	return pos;
}

terse_cursor_position_t
terse_get_cursor_position(terse_handle_t handle)
{
	terse_cursor_position_t unknown = make_unknown_cursor_position();
	if (ensure_handle(handle) != 0) {
		return unknown;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return unknown;
	}

	int row = 0, col = 0;
	int rc = terse_platform_query_cursor_position(handle->options.input_fd,
	                                              handle->options.output_fd,
	                                              &row, &col);
	if (rc != 0) {
		// rc is already a terse_error_t
		set_error(handle, (terse_error_t)rc);
		return unknown;
	}

	terse_cursor_position_t pos = {row, col, 1};
	clear_error(handle);
	return pos;
}

terse_error_t terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!out_options) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	*out_options = handle->options;
	clear_error(handle);
	return 0;
}

terse_error_t
terse_get_last_error(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_HANDLE;
	}
	return handle->last_error;
}

terse_error_t terse_capture_state(terse_handle_t handle, terse_state_t *out_state)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!out_state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
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

terse_error_t terse_restore_state(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	terse_state_t local = *state;
	if (local.cursor_known) {
		// Clamp to 0-based minimum
		if (local.cursor_row < 0) {
			local.cursor_row = 0;
		}
		if (local.cursor_col < 0) {
			local.cursor_col = 0;
		}
	}
	local.cursor_visible = state->cursor_visible ? 1 : 0;
	if (local.style_known) {
		local.style = sanitize_style_request(&state->style);
	}

	int result = 0;

	// Apply outputs BEFORE updating internal state to avoid duplicate skipping
	if (local.cursor_known && handle->capabilities.has_move_absolute && local.cursor_row >= 0 && local.cursor_col >= 0) {
		int move_rc = terse_move_to(handle, local.cursor_row, local.cursor_col);
		if (move_rc < 0 && result == 0) {
			result = move_rc;
		}
	}
	if (handle->capabilities.has_cursor_visibility) {
		int vis_rc = terse_show_cursor(handle, local.cursor_visible);
		if (vis_rc < 0 && result == 0) {
			result = vis_rc;
		}
	}
	if (local.style_known) {
		int style_rc = terse_set_style(handle, &local.style);
		if (style_rc < 0 && result == 0) {
			result = style_rc;
		}
	}

	// Update internal state after outputs to keep state synchronized
	(void)terse_state_override(handle, &local);

	return result;
}

#ifdef TERSE_ENABLE_TEST_MODE

int terse_test_start_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 1;
	return 0;
}

int terse_test_stop_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 0;
	return 0;
}

const terse_call_record_t *terse_test_get_calls(terse_handle_t handle, int *out_count)
{
	if (!handle || !handle->test_state || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return NULL;
	}
	*out_count = handle->test_state->call_count;
	return handle->test_state->calls;
}

int terse_test_clear_calls(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->call_count = 0;
	return 0;
}

int terse_test_mock_capabilities(terse_handle_t handle, const terse_capabilities_t *caps)
{
	if (!handle || !handle->test_state || !caps) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps = *caps;
	handle->test_state->mock_caps_enabled = 1;
	return 0;
}

int terse_test_mock_size(terse_handle_t handle, int rows, int cols)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_rows = rows;
	handle->test_state->mock_cols = cols;
	handle->test_state->mock_size_enabled = 1;
	return 0;
}

int terse_test_mock_events(terse_handle_t handle, const terse_event_t *events, int count)
{
	if (!handle || !handle->test_state || !events || count < 0) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;

	if (count > 0) {
		handle->test_state->mock_events = malloc(sizeof(terse_event_t) * (size_t)count);
		if (!handle->test_state->mock_events) {
			errno = ENOMEM;
			return TERSE_ERR_OUT_OF_MEMORY;
		}
		memcpy(handle->test_state->mock_events, events, sizeof(terse_event_t) * (size_t)count);
		handle->test_state->mock_event_count = count;
	}
	return 0;
}

int terse_test_reset_mocks(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps_enabled = 0;
	handle->test_state->mock_size_enabled = 0;
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;
	return 0;
}

#endif // TERSE_ENABLE_TEST_MODE
