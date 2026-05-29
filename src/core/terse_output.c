#include "terse_output.h"
#include "terse.h"
#include "terse_handle.h"
#include "terse_platform.h"
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test_internal.h"
#endif
#include "terse_buffer.h"
#include "terse_term_internal.h"

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

/* External helper from terse_graphics.c (used for terse_set_title and terse_set_hyperlink) */
extern int payload_has_disallowed_chars(const char *payload);

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
	struct {
		terse_clear_mode_t mode;
	} rec_data = { mode };
	TERSE_TEST_RECORD_CALL(handle, TERSE_CALL_CLEAR_SCREEN, rec_data);
#endif

	if (handle->render_mode == TERSE_RENDER_BUFFERED && !handle->in_flush) {
		/* Buffered: clearing is a virtual-buffer operation; the emptied cells are
		 * emitted by the next flush's diff. Only a full clear is meaningful here. */
		terse_buffer_clear(handle);
		clear_error(handle);
		return TERSE_OK;
	}

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
	struct {
		terse_clear_mode_t mode;
	} rec_data = { mode };
	TERSE_TEST_RECORD_CALL(handle, TERSE_CALL_CLEAR_LINE, rec_data);
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

/* Cursor movement functions (terse_move_to, terse_move_by) moved to terse_cursor.c */

/* ========================================================================
 * Cursor visibility
 * ======================================================================== */

/* Cursor visibility function (terse_show_cursor) moved to terse_cursor.c */

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
	struct {
		terse_style_t style;
	} rec_data = { *style };
	TERSE_TEST_RECORD_CALL(handle, TERSE_CALL_SET_STYLE, rec_data);
#endif

	terse_style_t requested = terse_style_sanitize_request(style);
	handle->style = requested;

	if (handle->render_mode == TERSE_RENDER_BUFFERED && !handle->in_flush) {
		/* Buffered: record the requested style on the handle; cells pick it up at
		 * write time and the diff emits SGR per run during flush. */
		clear_error(handle);
		return TERSE_OK;
	}

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
 * Cursor shape
 * ======================================================================== */

/* Cursor shape function (terse_set_cursor_shape) moved to terse_cursor.c */

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
	struct {
		char text[TERSE_TEXT_BUFFER_SIZE];
	} rec_data;
	memset(&rec_data, 0, sizeof(rec_data));
	size_t len = strlen(graphemes);
	if (len >= sizeof(rec_data.text)) {
		len = sizeof(rec_data.text) - 1;
	}
	memcpy(rec_data.text, graphemes, len);
	rec_data.text[len] = '\0';
	TERSE_TEST_RECORD_CALL(handle, TERSE_CALL_WRITE_TEXT, rec_data);
#endif

	if (handle->render_mode == TERSE_RENDER_BUFFERED && !handle->in_flush) {
		terse_buffer_write_text(handle, handle->cursor_row, handle->cursor_col, graphemes);
		clear_error(handle);
		return TERSE_OK;
	}

	if (!handle->codec.from_utf8) {
		/* passthrough: terminal is UTF-8 */
		terse_error_t result = write_literal(handle, graphemes);
		if (result == TERSE_OK) {
			handle->cursor_known = 0;
		}
		return result;
	}

	/* Convert UTF-8 input to terminal encoding in chunks */
	const char *input = graphemes;
	size_t in_left = strlen(graphemes);
	unsigned char outbuf[TERSE_TEXT_BUFFER_SIZE];
	while (in_left > 0) {
		size_t chunk_in = in_left;
		size_t out_avail = sizeof(outbuf);
		terse_error_t conv = handle->codec.from_utf8(&handle->codec,
		                                             input, chunk_in,
		                                             (char *)outbuf, &out_avail);
		if (out_avail > 0) {
			if (write_sequence(handle, (const char *)outbuf, out_avail) != 0) {
				return handle->last_error;
			}
		}
		if (conv == TERSE_ERR_BUFFER_TOO_SMALL) {
			/* chunk was too large for outbuf; advance by one UTF-8 codepoint
			 * to make progress and retry */
			if (in_left > 0) {
				input++;
				in_left--;
			}
			continue;
		}
		if (conv == TERSE_ERR_INVALID_ENCODING) {
			/* skip one byte and emit replacement */
			if (in_left > 0) {
				input++;
				in_left--;
			}
			const char replacement = '?';
			if (write_sequence(handle, &replacement, 1) != 0) {
				return handle->last_error;
			}
			continue;
		}
		if (conv != TERSE_OK) {
			set_error(handle, conv);
			return conv;
		}
		/* full input consumed */
		break;
	}
	handle->cursor_known = 0;
	clear_error(handle);
	return TERSE_OK;
}

terse_error_t terse_flush(terse_handle_t handle)
{
	TERSE_CHECK_HANDLE(handle);

	if (handle->render_mode == TERSE_RENDER_BUFFERED && handle->cur_cells) {
		terse_error_t err;
		handle->in_flush = 1;
		err = terse_buffer_flush(handle);
		handle->in_flush = 0;
		if (err != TERSE_OK) {
			set_error(handle, err);
			return err;
		}
	}

	clear_error(handle);
	return 0;
}

terse_error_t terse_write_raw(terse_handle_t handle, const char *bytes, size_t length)
{
	TERSE_CHECK_HANDLE(handle);

	if (!bytes) {
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (length == 0) {
		clear_error(handle);
		return TERSE_OK;
	}

	/* Emit the bytes to the terminal verbatim, regardless of render mode and
	 * without codec conversion: this is the escape-sequence / control-byte
	 * escape hatch for callers that must drive the terminal directly (e.g. a
	 * buffered-mode caller clearing area outside its rectangle, or writing a
	 * line break to advance past it). Cursor tracking is invalidated since the
	 * caller may have moved or scrolled the terminal in ways terse cannot model. */
	if (write_sequence(handle, bytes, length) != 0) {
		return handle->last_error;
	}
	handle->cursor_known = 0;
	clear_error(handle);
	return TERSE_OK;
}
