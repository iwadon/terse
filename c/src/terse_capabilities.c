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

void
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
