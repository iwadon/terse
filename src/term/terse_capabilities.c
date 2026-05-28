#include "terse_capabilities.h"
#include "terse.h"
#include "terse_handle.h"
#include "terse_style.h"

#include <errno.h>

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

void recompute_capabilities(terse_handle_t handle)
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

	if (handle->detected_capabilities.colors == TERSE_COLOR_BASIC4) {
		/*
		 * 検出側が明示した 4 色（Human68k 等）を尊重する。3 フラグからの導出では
		 * 16 色未満を表現できないため、この経路でのみ BASIC4 が確定する。
		 */
		handle->capabilities.colors = TERSE_COLOR_BASIC4;
	} else if (handle->capabilities.has_truecolor) {
		handle->capabilities.colors = TERSE_COLOR_TRUECOLOR;
	} else if (handle->capabilities.has_sgr_extended) {
		handle->capabilities.colors = TERSE_COLOR_PALETTE256;
	} else if (handle->capabilities.has_sgr_basic) {
		handle->capabilities.colors = TERSE_COLOR_BASIC16;
	} else {
		handle->capabilities.colors = TERSE_COLOR_NONE;
	}
	handle->capabilities.effects = handle->capabilities.has_text_styles ? TERSE_STYLE_ALL_SUPPORTED : 0;

	/*
	 * Alternate screen (DEC private mode 1049) is a modern VT feature. Honor an
	 * explicit detection if one set it; otherwise derive it from the profile
	 * (P2+). P0/P1 terminals (incl. Human68k) get no alt screen.
	 */
	if (!handle->detected_capabilities.has_alt_screen) {
		handle->capabilities.has_alt_screen = (handle->capabilities.profile >= TERSE_P2) ? 1 : 0;
	}

	if (handle->style_known) {
		handle->effective_style = terse_style_make_effective(&handle->capabilities, &handle->style);
	}
}

static void
apply_runtime_overrides(terse_handle_t handle)
{
	recompute_capabilities(handle);
	clear_error(handle);
}

terse_error_t terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask)
{
	TERSE_CHECK_HANDLE(handle);
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
	TERSE_CHECK_HANDLE(handle);
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
	TERSE_CHECK_HANDLE(handle);
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	apply_runtime_overrides(handle);
	return 0;
}

/*
 * 検出ケイパビリティを「要求可能な機能」の平坦なビットマスクへ射影する。
 * 色は段階なので、該当段以下の色ビットをすべて立てる（PALETTE256 端末は
 * BASIC16 / BASIC4 の要求も満たす）。
 */
static uint64_t
features_from_capabilities(const terse_capabilities_t *caps)
{
	uint64_t feats = 0;
	if (caps->has_basic_output) {
		feats |= TERSE_FEAT_BASIC_OUTPUT;
	}
	if (caps->has_cursor_visibility) {
		feats |= TERSE_FEAT_CURSOR_VISIBILITY;
	}
	if (caps->has_move_absolute) {
		feats |= TERSE_FEAT_MOVE_ABSOLUTE;
	}
	if (caps->has_move_relative) {
		feats |= TERSE_FEAT_MOVE_RELATIVE;
	}
	if (caps->has_clear_line) {
		feats |= TERSE_FEAT_CLEAR_LINE;
	}
	if (caps->has_clear_screen) {
		feats |= TERSE_FEAT_CLEAR_SCREEN;
	}
	if (caps->has_size) {
		feats |= TERSE_FEAT_SIZE;
	}
	switch (caps->colors) {
	case TERSE_COLOR_TRUECOLOR:
		feats |= TERSE_FEAT_COLOR_TRUECOLOR;
		/* fallthrough */
	case TERSE_COLOR_PALETTE256:
		feats |= TERSE_FEAT_COLOR_PALETTE256;
		/* fallthrough */
	case TERSE_COLOR_BASIC16:
		feats |= TERSE_FEAT_COLOR_BASIC16;
		/* fallthrough */
	case TERSE_COLOR_BASIC4:
		feats |= TERSE_FEAT_COLOR_BASIC4;
		/* fallthrough */
	case TERSE_COLOR_NONE:
		break;
	}
	if (caps->has_text_styles) {
		feats |= TERSE_FEAT_TEXT_STYLES;
	}
	if (caps->mouse != TERSE_MOUSE_NONE) {
		feats |= TERSE_FEAT_MOUSE;
	}
	if (caps->has_bracketed_paste) {
		feats |= TERSE_FEAT_BRACKETED_PASTE;
	}
	if (caps->has_title) {
		feats |= TERSE_FEAT_TITLE;
	}
	if (caps->has_hyperlinks) {
		feats |= TERSE_FEAT_HYPERLINK;
	}
	if (caps->has_cursor_shape) {
		feats |= TERSE_FEAT_CURSOR_SHAPE;
	}
	if (caps->has_clipboard_write) {
		feats |= TERSE_FEAT_CLIPBOARD_WRITE;
	}
	if (caps->images != TERSE_IMAGE_NONE) {
		feats |= TERSE_FEAT_IMAGE_INLINE;
	}
	if (caps->notifications & TERSE_NOTIFICATION_SUPPORT_BELL) {
		feats |= TERSE_FEAT_NOTIFICATION_BELL;
	}
	if (caps->notifications & TERSE_NOTIFICATION_SUPPORT_VISUAL) {
		feats |= TERSE_FEAT_NOTIFICATION_VISUAL;
	}
	if (caps->notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP) {
		feats |= TERSE_FEAT_NOTIFICATION_DESKTOP;
	}
	return feats;
}

uint64_t terse_caps_missing(terse_handle_t handle, uint64_t wanted)
{
	if (!handle) {
		return wanted;
	}
	clear_error(handle);
	uint64_t provided = features_from_capabilities(&handle->capabilities);
	return wanted & ~provided;
}

uint64_t terse_require(terse_handle_t handle, uint64_t wanted)
{
	return terse_caps_missing(handle, wanted);
}

uint64_t terse_get_active_features(terse_handle_t handle)
{
	if (!handle) {
		return 0;
	}
	clear_error(handle);
	return features_from_capabilities(&handle->capabilities);
}
