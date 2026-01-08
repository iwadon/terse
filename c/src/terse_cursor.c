#include "terse.h"
#include "terse_handle.h"
#include "terse_platform.h"
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test_internal.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* External helper functions from terse.c */
extern int write_literal(terse_handle_t handle, const char *sequence);
extern int write_sequence(terse_handle_t handle, const char *data, size_t length);

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
	struct {
		int row;
		int col;
	} rec_data = { row, col };
	TERSE_TEST_RECORD_CALL(handle, TERSE_CALL_MOVE_TO, rec_data);
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
