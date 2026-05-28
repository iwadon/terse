#include "terse_style.h"
#include "terse_detection.h"
#include "terse_handle.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* RGB color representation for internal conversions. */
typedef struct terse_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} terse_rgb_t;

/* Basic 16-color palette in RGB values.
 * Used for color degradation from truecolor/256-color to basic16.
 */
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

/* External functions from terse.c */
extern int write_literal(terse_handle_t handle, const char *literal);
extern int write_sequence(terse_handle_t handle, const char *sequence, size_t length);
extern void set_error(terse_handle_t handle, terse_error_t error);

int terse_style_color_support_rank(terse_color_support_t support)
{
	/*
	 * ランクは terse_color_kind_rank() と比較可能でなければならない
	 * （degrade_color が両者を直接比較する）。color_kind には 4 色専用の種別が
	 * 無く、4 色も 16 色も BASIC16 kind で表す。よって BASIC4 / BASIC16 は
	 * ともに「basic16 kind を表示できる段」として同じランク 1 を返す。
	 * 4 色か 16 色かの区別は degrade_color の switch（support 値）が行う。
	 */
	switch (support) {
	case TERSE_COLOR_NONE:
		return 0;
	case TERSE_COLOR_BASIC4:
		return 1;
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

/*
 * 色を RGB へ展開する（truecolor はそのまま、palette は LUT、basic16 は basic16_rgb）。
 * default 色は黒として扱う（degrade_color 側で default は事前に弾かれている）。
 */
static void
degrade_color_to_rgb(terse_color_t color, unsigned char *r, unsigned char *g, unsigned char *b)
{
	*r = 0;
	*g = 0;
	*b = 0;
	if (color.kind == TERSE_COLOR_KIND_TRUECOLOR) {
		*r = color.data.truecolor.r;
		*g = color.data.truecolor.g;
		*b = color.data.truecolor.b;
	} else if (color.kind == TERSE_COLOR_KIND_PALETTE256) {
		palette_index_to_rgb(color.data.palette.value, r, g, b);
	} else if (color.kind == TERSE_COLOR_KIND_BASIC16) {
		unsigned char idx = (unsigned char)(color.data.basic16.color + (color.data.basic16.bright ? 8 : 0));
		palette_index_to_rgb(idx, r, g, b);
	}
}

/*
 * RGB を 4 色（黒・赤・緑・青）の最近傍へ寄せる暫定マッピング。
 * Phase 4.5 で Human68k のテキスト 4 色の実機定義に合わせて見直す。
 */
static terse_basic_color_t
closest_basic4_color(unsigned char r, unsigned char g, unsigned char b)
{
	static const terse_basic_color_t palette4[4] = {
		TERSE_BASIC_COLOR_BLACK,
		TERSE_BASIC_COLOR_RED,
		TERSE_BASIC_COLOR_GREEN,
		TERSE_BASIC_COLOR_BLUE,
	};
	unsigned int best_distance = UINT_MAX;
	terse_basic_color_t best = TERSE_BASIC_COLOR_BLACK;
	for (int i = 0; i < 4; ++i) {
		unsigned char pr = 0;
		unsigned char pg = 0;
		unsigned char pb = 0;
		palette_index_to_rgb((unsigned char)palette4[i], &pr, &pg, &pb);
		int dr = (int)r - (int)pr;
		int dg = (int)g - (int)pg;
		int db = (int)b - (int)pb;
		unsigned int distance = (unsigned int)(dr * dr + dg * dg + db * db);
		if (distance < best_distance) {
			best_distance = distance;
			best = palette4[i];
		}
	}
	return best;
}

static unsigned int
mask_effects(unsigned int effects)
{
	return effects & TERSE_STYLE_ALL_SUPPORTED;
}

int terse_style_colors_equal(const terse_color_t *a, const terse_color_t *b)
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

int terse_style_styles_equal(const terse_style_t *a, const terse_style_t *b)
{
	if (a->effects != b->effects) {
		return 0;
	}
	if (!terse_style_colors_equal(&a->foreground, &b->foreground)) {
		return 0;
	}
	if (!terse_style_colors_equal(&a->background, &b->background)) {
		return 0;
	}
	return 1;
}

terse_color_t
terse_style_degrade_color(terse_color_t color, terse_color_support_t support)
{
	if (color.kind == TERSE_COLOR_KIND_DEFAULT) {
		return color;
	}
	int support_level = terse_style_color_support_rank(support);
	if (support_level == 0) {
		return terse_color_default();
	}
	int requested_level = terse_color_kind_rank(color.kind);
	/*
	 * BASIC4 は BASIC16 と同ランクだが 16 色をそのまま出せないため、
	 * basic16 kind が来ても素通りさせず 4 色へ寄せる（下の switch で処理）。
	 */
	if (requested_level <= support_level && support != TERSE_COLOR_BASIC4) {
		return color;
	}
	switch (support) {
	case TERSE_COLOR_BASIC4: {
		/*
		 * 任意の色を 4 色（黒/赤/緑/青の代表 8 色系下位）に寄せる暫定マッピング。
		 * いったん basic16 へ縮退してから 4 色枠へ丸める。
		 * 4 色の具体的な構成は Phase 4.5 で Human68k 実機色に合わせて確定する。
		 */
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		degrade_color_to_rgb(color, &r, &g, &b);
		terse_basic_color_t basic4 = closest_basic4_color(r, g, b);
		terse_color_t basic = {
			.kind = TERSE_COLOR_KIND_BASIC16,
			.data.basic16 = {
				.color = basic4,
				.bright = 0,
			},
		};
		return basic;
	}
	case TERSE_COLOR_BASIC16: {
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		degrade_color_to_rgb(color, &r, &g, &b);
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
	case TERSE_COLOR_PALETTE256: {
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

terse_style_t
terse_style_sanitize_request(const terse_style_t *style)
{
	terse_style_t sanitized = *style;
	sanitized.effects = mask_effects(style->effects);
	return sanitized;
}

terse_style_t
terse_style_make_effective(const terse_capabilities_t *caps, const terse_style_t *requested)
{
	terse_style_t effective = *requested;
	effective.effects &= caps->effects;
	effective.foreground = terse_style_degrade_color(requested->foreground, caps->colors);
	effective.background = terse_style_degrade_color(requested->background, caps->colors);
	return effective;
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

int terse_style_emit_sequence(terse_handle_t handle, const terse_style_t *style)
{
	int reset = write_literal(handle, "\x1b[0m");
	if (reset != 0) {
		return reset;
	}
	if (style->effects == 0 && style->foreground.kind == TERSE_COLOR_KIND_DEFAULT && style->background.kind == TERSE_COLOR_KIND_DEFAULT) {
		return 0;
	}
	char seq[TERSE_LARGE_BUFFER_SIZE];
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
