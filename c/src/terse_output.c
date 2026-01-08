#include "terse_output.h"
#include "terse.h"
#include "terse_handle.h"
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
		char text[256];
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
