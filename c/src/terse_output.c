#include "terse.h"
#include "terse_handle.h"
#include "terse_output.h"
#include "terse_platform.h"
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test_internal.h"
#endif
#include "terse_codec.h"
#include "terse_style.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* External helper functions from terse.c (ensure_handle, set_error, clear_error, reset sequences are in terse_handle.h) */
extern int write_literal(terse_handle_t handle, const char *sequence);
extern int write_sequence(terse_handle_t handle, const char *data, size_t length);
extern void update_effective_style(terse_handle_t handle);
extern char *base64_encode(const unsigned char *data, size_t size, size_t *out_len);

/* ========================================================================
 * Screen and line clearing
 * ======================================================================== */

terse_error_t terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	TERSE_CHECK_HANDLE(handle);
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

	// Try platform-specific fast path first
	terse_error_t fast_result = terse_platform_clear_screen_fast(handle, mode);
	if (fast_result == TERSE_OK) {
		return TERSE_OK;
	}

	// Fall back to standard escape sequence method
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
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Cursor movement
 * ======================================================================== */

terse_error_t terse_move_to(terse_handle_t handle, int row, int col)
{
	TERSE_CHECK_HANDLE(handle);
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
	if (handle->cursor_known && row == handle->cursor_row && col == handle->cursor_col) {
		clear_error(handle);
		return 0;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->recording) {
		struct { int row; int col; } rec_data = { row, col };
		record_call(handle, TERSE_CALL_MOVE_TO, &rec_data, sizeof(rec_data));
	}
#endif

	// Try platform-specific fast path first
	terse_error_t fast_result = terse_platform_move_to_fast(handle, row, col);
	if (fast_result == TERSE_OK) {
		handle->cursor_row = row;
		handle->cursor_col = col;
		handle->cursor_known = 1;
		return TERSE_OK;
	}

	// Fall back to standard escape sequence method
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
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Cursor visibility
 * ======================================================================== */

terse_error_t terse_show_cursor(terse_handle_t handle, int visible)
{
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Style and reset
 * ======================================================================== */

terse_error_t terse_set_style(terse_handle_t handle, const terse_style_t *style)
{
	TERSE_CHECK_HANDLE(handle);
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

	terse_style_t requested = terse_style_sanitize_request(style);
	handle->style = requested;
	terse_style_t effective = terse_style_make_effective(&handle->capabilities, &requested);
	if (handle->style_known && terse_style_styles_equal(&effective, &handle->effective_style)) {
		clear_error(handle);
		return 0;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.effects == 0 && handle->capabilities.colors == TERSE_COLOR_NONE)) {
		handle->effective_style = effective;
		handle->style_known = 1;
		clear_error(handle);
		return 0;
	}
	int result = terse_style_emit_sequence(handle, &effective);
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
		return write_sequence(handle, TERSE_RESET_ALL_SEQ, TERSE_RESET_ALL_SEQ_LEN);
	case TERSE_RESET_COLOR_ONLY:
		return write_sequence(handle, TERSE_RESET_COLOR_SEQ, TERSE_RESET_COLOR_SEQ_LEN);
	case TERSE_RESET_EFFECTS_ONLY:
		return write_sequence(handle, TERSE_RESET_EFFECTS_SEQ, TERSE_RESET_EFFECTS_SEQ_LEN);
	default:
		return 0;
	}
}

terse_error_t terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope)
{
	TERSE_CHECK_HANDLE(handle);
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
		handle->style.effects = handle->style.effects & TERSE_STYLE_ALL_SUPPORTED;
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

/* ========================================================================
 * Mouse control
 * ======================================================================== */

int
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

int
set_bracketed_paste(terse_handle_t handle, int enable)
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

/* ========================================================================
 * Cursor shape
 * ======================================================================== */

terse_error_t terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking)
{
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Clipboard
 * ======================================================================== */

terse_error_t terse_set_clipboard(terse_handle_t handle, const char *data)
{
	TERSE_CHECK_HANDLE(handle);
	if (!data) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (!handle->capabilities.has_clipboard_write || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (payload_has_disallowed_chars(data)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
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

/* ========================================================================
 * Image display helpers
 * ======================================================================== */

int
send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	if (payload_has_disallowed_chars(name)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
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

int
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

int
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

/* ========================================================================
 * Image display API
 * ======================================================================== */

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
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Notifications
 * ======================================================================== */

int
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

terse_error_t terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload)
{
	TERSE_CHECK_HANDLE(handle);
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

/* ========================================================================
 * Text output
 * ======================================================================== */

terse_error_t terse_write_text(terse_handle_t handle, const char *graphemes)
{
	TERSE_CHECK_HANDLE(handle);
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
		terse_error_t result = write_literal(handle, graphemes);
		if (result == TERSE_OK) {
			handle->cursor_known = 0;
		}
		return result;
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
		terse_codec_reset_iconv_state(handle->utf8_to_codec);
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
					terse_codec_reset_iconv_state(handle->utf8_to_codec);
					continue;
				}
				// Map errno to terse_error_t
				terse_error_t err = (errno == EILSEQ) ? TERSE_ERR_INVALID_ENCODING : TERSE_ERR_IO;
				set_error(handle, err);
				return err;
			}
		}
		terse_codec_reset_iconv_state(handle->utf8_to_codec);
		handle->cursor_known = 0;
		clear_error(handle);
		return 0;
	}
	terse_error_t result = write_literal(handle, graphemes);
	if (result == TERSE_OK) {
		handle->cursor_known = 0;
	}
	return result;
}

terse_error_t terse_flush(terse_handle_t handle)
{
	TERSE_CHECK_HANDLE(handle);
	clear_error(handle);
	return 0;
}
