#include "terse_device.h"
#include "terse.h"
#include "terse_handle.h"

#include <errno.h>
#include <string.h>

/* External helper functions from terse.c */
extern int write_literal(terse_handle_t handle, const char *sequence);
extern int write_sequence(terse_handle_t handle, const char *data, size_t length);
extern void set_error(terse_handle_t handle, terse_error_t error);
extern void clear_error(terse_handle_t handle);

/* External helper function from terse_graphics.c */
extern int payload_has_disallowed_chars(const char *payload);

/* ========================================================================
 * Mouse control
 * ======================================================================== */

int set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable)
{
	static const char *const enable_seqs[][2] = {
		{ "\x1b[?1000h", NULL },          // X10
		{ "\x1b[?1002h", NULL },          // VT200
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

terse_mouse_mode_t
clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available)
{
	if (requested > available) {
		return available;
	}
	return requested;
}

terse_error_t terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode)
{
	TERSE_CHECK_HANDLE(handle);
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
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Bracketed paste control
 * ======================================================================== */

int set_bracketed_paste(terse_handle_t handle, int enable)
{
	const char *seq = enable ? "\x1b[?2004h" : "\x1b[?2004l";
	return write_literal(handle, seq);
}

terse_error_t terse_enable_bracketed_paste(terse_handle_t handle)
{
	TERSE_CHECK_HANDLE(handle);
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
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Window title and hyperlinks
 * ======================================================================== */

terse_error_t terse_set_title(terse_handle_t handle, const char *title)
{
	TERSE_CHECK_HANDLE(handle);
	if (!title) {
		title = "";
	}
	if (!handle->capabilities.has_title || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (payload_has_disallowed_chars(title)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	TERSE_WRITE_LITERAL(handle, "\x1b]0;");
	TERSE_WRITE_SEQ(handle, title, strlen(title));
	TERSE_WRITE_LITERAL(handle, "\x07");
	clear_error(handle);
	return 0;
}

terse_error_t terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label)
{
	TERSE_CHECK_HANDLE(handle);
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
	if (payload_has_disallowed_chars(url) || payload_has_disallowed_chars(label)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	TERSE_WRITE_LITERAL(handle, "\x1b]8;;");
	TERSE_WRITE_SEQ(handle, url, strlen(url));
	TERSE_WRITE_LITERAL(handle, "\x07");
	TERSE_WRITE_SEQ(handle, label, strlen(label));
	TERSE_WRITE_LITERAL(handle, "\x1b]8;;\x07");
	clear_error(handle);
	return 0;
}
