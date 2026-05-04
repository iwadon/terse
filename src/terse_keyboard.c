#include "terse_keyboard.h"
#include "terse.h"
#include "terse_handle.h"

#include <errno.h>

static const char TERSE_MODIFY_OTHER_KEYS_ENABLE_SEQ[] = "\x1b[>4;2m";
static const char TERSE_MODIFY_OTHER_KEYS_DISABLE_SEQ[] = "\x1b[>4;0m";
static const char TERSE_KITTY_PROTOCOL_ENABLE_SEQ[] = "\x1b[>1u";
static const char TERSE_KITTY_PROTOCOL_DISABLE_SEQ[] = "\x1b[<u";

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
