#include "terse_internal.h"
#include <limits.h>

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

unsigned int
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
		return a->data.basic16.color == b->data.basic16.color && 
		       a->data.basic16.bright == b->data.basic16.bright;
	case TERSE_COLOR_KIND_PALETTE256:
		return a->data.palette.value == b->data.palette.value;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return a->data.truecolor.r == b->data.truecolor.r &&
		       a->data.truecolor.g == b->data.truecolor.g &&
		       a->data.truecolor.b == b->data.truecolor.b;
	default:
		return 0;
	}
}

int
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
	int color_rank = color_kind_rank(color.kind);
	int support_rank = color_support_rank(support);
	if (color_rank <= support_rank) {
		return color;
	}
	switch (color.kind) {
	case TERSE_COLOR_KIND_TRUECOLOR:
		if (support == TERSE_COLOR_PALETTE256) {
			unsigned char index = truecolor_to_palette_index(
				color.data.truecolor.r,
				color.data.truecolor.g,
				color.data.truecolor.b
			);
			return terse_color_palette(index);
		}
		if (support == TERSE_COLOR_BASIC16) {
			unsigned char index = closest_basic16_index(
				color.data.truecolor.r,
				color.data.truecolor.g,
				color.data.truecolor.b
			);
			terse_basic_color_t basic = (terse_basic_color_t)(index % 8);
			int bright = index >= 8 ? 1 : 0;
			return terse_color_basic(basic, bright);
		}
		break;
	case TERSE_COLOR_KIND_PALETTE256:
		if (support == TERSE_COLOR_BASIC16) {
			unsigned char r, g, b;
			palette_index_to_rgb(color.data.palette.value, &r, &g, &b);
			unsigned char index = closest_basic16_index(r, g, b);
			terse_basic_color_t basic = (terse_basic_color_t)(index % 8);
			int bright = index >= 8 ? 1 : 0;
			return terse_color_basic(basic, bright);
		}
		break;
	default:
		break;
	}
	return terse_color_default();
}

terse_style_t
sanitize_style_request(const terse_style_t *style)
{
	terse_style_t sanitized = *style;
	sanitized.effects = mask_effects(style->effects);
	return sanitized;
}

terse_style_t
make_effective_style(const terse_capabilities_t *caps, const terse_style_t *requested)
{
	terse_style_t effective = *requested;
	effective.effects &= caps->effects;
	effective.foreground = degrade_color(requested->foreground, caps->colors);
	effective.background = degrade_color(requested->background, caps->colors);
	return effective;
}

void
update_effective_style(terse_handle_t handle)
{
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}