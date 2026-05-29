#include "terse_buffer.h"
#include "terse.h"
#include "terse_handle.h"
#include "terse_term_internal.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * UTF-8 helpers
 *
 * terse_unicode.c exposes scalar->width (terse_char_width) and scalar->bytes
 * (terse_encode_utf8) but no public byte->scalar decode, so we provide the
 * minimal decode needed to walk a UTF-8 string cell by cell.  Logic borrowed
 * from terse-prompt's tprompt_utf8_* helpers.
 * ======================================================================== */

static size_t utf8_char_length(unsigned char first)
{
	if (first < 0x80) {
		return 1;
	}
	if ((first & 0xE0) == 0xC0) {
		return 2;
	}
	if ((first & 0xF0) == 0xE0) {
		return 3;
	}
	if ((first & 0xF8) == 0xF0) {
		return 4;
	}
	return 0; /* invalid leading byte */
}

static unsigned int utf8_decode(const unsigned char *s, size_t len)
{
	switch (len) {
	case 1:
		return s[0];
	case 2:
		return ((unsigned int)(s[0] & 0x1F) << 6) | (unsigned int)(s[1] & 0x3F);
	case 3:
		return ((unsigned int)(s[0] & 0x0F) << 12) | ((unsigned int)(s[1] & 0x3F) << 6) | (unsigned int)(s[2] & 0x3F);
	case 4:
		return ((unsigned int)(s[0] & 0x07) << 18) | ((unsigned int)(s[1] & 0x3F) << 12) | ((unsigned int)(s[2] & 0x3F) << 6) | (unsigned int)(s[3] & 0x3F);
	default:
		return 0xFFFD;
	}
}

/* ========================================================================
 * Cell initialization
 * ======================================================================== */

static void cell_make_empty(terse_cell_t *cell)
{
	memset(cell, 0, sizeof(*cell));
	cell->display_width = 1;
	cell->fg = terse_color_default();
	cell->bg = terse_color_default();
}

static void cells_fill_empty(terse_cell_t *cells, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++) {
		cell_make_empty(&cells[i]);
	}
}

/* ========================================================================
 * Buffer lifecycle
 * ======================================================================== */

int terse_buffer_alloc(terse_handle_t handle, int rows, int cols)
{
	size_t count;
	terse_cell_t *cur;
	terse_cell_t *prev;
	unsigned char *dirty;

	if (!handle || rows <= 0 || cols <= 0) {
		return -1;
	}

	terse_buffer_free(handle);

	count = (size_t)rows * (size_t)cols;
	cur = (terse_cell_t *)calloc(count, sizeof(terse_cell_t));
	prev = (terse_cell_t *)calloc(count, sizeof(terse_cell_t));
	dirty = (unsigned char *)calloc(count, sizeof(unsigned char));
	if (!cur || !prev || !dirty) {
		free(cur);
		free(prev);
		free(dirty);
		return -1;
	}

	cells_fill_empty(cur, count);
	cells_fill_empty(prev, count);

	handle->cur_cells = cur;
	handle->prev_cells = prev;
	handle->dirty = dirty;
	handle->buf_rows = rows;
	handle->buf_cols = cols;
	return 0;
}

void terse_buffer_free(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	free(handle->cur_cells);
	free(handle->prev_cells);
	free(handle->dirty);
	handle->cur_cells = NULL;
	handle->prev_cells = NULL;
	handle->dirty = NULL;
	handle->buf_rows = 0;
	handle->buf_cols = 0;
}

void terse_buffer_clear(terse_handle_t handle)
{
	if (!handle || !handle->cur_cells) {
		return;
	}
	cells_fill_empty(handle->cur_cells, (size_t)handle->buf_rows * (size_t)handle->buf_cols);
}

int terse_buffer_resize(terse_handle_t handle, int rows, int cols)
{
	if (!handle || rows <= 0 || cols <= 0) {
		return -1;
	}
	if (handle->buf_rows == rows && handle->buf_cols == cols) {
		terse_buffer_clear(handle);
		return 0;
	}
	/* Reallocate from scratch; contents are not preserved (full redraw next flush).
	 * prev_cells no longer reflects what is on screen, so terse_get_cell must report
	 * "no displayed frame" until the next flush. The prev_rect_* record survives so
	 * residue erase still cleans up the old extent. */
	if (terse_buffer_alloc(handle, rows, cols) != 0) {
		return -1;
	}
	handle->prev_valid = 0;
	return 0;
}

/* ========================================================================
 * Region (origin) control — public API
 * ======================================================================== */

terse_error_t terse_buffer_set_region(terse_handle_t handle,
                                      int origin_row, int origin_col,
                                      int rows, int cols)
{
	TERSE_CHECK_HANDLE(handle);

	if (origin_row < 0 || origin_col < 0 || rows <= 0 || cols <= 0) {
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	/* Lazy buffered-mode startup: if the caller requested buffered rendering at
	 * open time but the terminal size was unknown then (so the cell buffer was
	 * never allocated — e.g. a pipe or a test handle whose size is mocked after
	 * open), allocate it now from the rectangle dimensions given here and switch
	 * the handle into buffered mode. This lets a caller defer buffered startup
	 * until it knows the size to use. */
	if (!handle->cur_cells &&
	    handle->options.render_mode == TERSE_RENDER_BUFFERED) {
		if (terse_buffer_alloc(handle, rows, cols) != 0) {
			set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
			return TERSE_ERR_OUT_OF_MEMORY;
		}
		handle->render_mode = TERSE_RENDER_BUFFERED;
		handle->prev_rect_valid = 0;
		handle->prev_valid = 0;
	}

	if (handle->render_mode != TERSE_RENDER_BUFFERED || !handle->cur_cells) {
		set_error(handle, TERSE_ERR_NOT_SUPPORTED);
		return TERSE_ERR_NOT_SUPPORTED;
	}

	/* A dimension change reallocates the cell buffers (contents not preserved; the
	 * next flush fully redraws). The previous rectangle is remembered in the handle
	 * via the prev_origin and prev_buf fields, so the next flush residue erase still
	 * covers the old extent even after the cells are rebuilt: shrink cleanup needs
	 * no special path here. */
	if (rows != handle->buf_rows || cols != handle->buf_cols) {
		if (terse_buffer_resize(handle, rows, cols) != 0) {
			set_error(handle, TERSE_ERR_OUT_OF_MEMORY);
			return TERSE_ERR_OUT_OF_MEMORY;
		}
	}

	handle->buf_origin_row = origin_row;
	handle->buf_origin_col = origin_col;
	clear_error(handle);
	return TERSE_OK;
}

terse_error_t terse_get_cell(terse_handle_t handle, int row, int col,
                             terse_cell_t *out)
{
	size_t index;

	TERSE_CHECK_HANDLE(handle);

	/* prev_cells holds the frame last committed to the terminal (cur and prev are
	 * swapped at the end of each flush). Without a completed flush there is nothing
	 * displayed to report. */
	if (handle->render_mode != TERSE_RENDER_BUFFERED || !handle->prev_cells || !handle->prev_valid) {
		set_error(handle, TERSE_ERR_NOT_SUPPORTED);
		return TERSE_ERR_NOT_SUPPORTED;
	}
	if (!out || row < 0 || row >= handle->buf_rows || col < 0 || col >= handle->buf_cols) {
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	index = (size_t)row * (size_t)handle->buf_cols + (size_t)col;
	*out = handle->prev_cells[index];
	clear_error(handle);
	return TERSE_OK;
}

terse_error_t terse_buffer_set_cursor(terse_handle_t handle, int row, int col)
{
	TERSE_CHECK_HANDLE(handle);

	if (handle->render_mode != TERSE_RENDER_BUFFERED || !handle->cur_cells) {
		set_error(handle, TERSE_ERR_NOT_SUPPORTED);
		return TERSE_ERR_NOT_SUPPORTED;
	}
	if (row < 0 || col < 0) {
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	/* Recorded in rectangle-local coordinates; the next flush projects it to
	 * the terminal as origin + (row, col) and moves the cursor there as its
	 * final step. The position may sit outside the cell buffer (e.g. a trailing
	 * cursor column reserved past the content), so it is not clamped to the
	 * buffer dimensions. */
	handle->buf_cursor_row = row;
	handle->buf_cursor_col = col;
	handle->buf_cursor_set = 1;
	clear_error(handle);
	return TERSE_OK;
}

terse_error_t terse_buffer_invalidate(terse_handle_t handle)
{
	TERSE_CHECK_HANDLE(handle);

	if (handle->render_mode != TERSE_RENDER_BUFFERED || !handle->prev_cells) {
		set_error(handle, TERSE_ERR_NOT_SUPPORTED);
		return TERSE_ERR_NOT_SUPPORTED;
	}

	/* Empty prev_cells so the next flush diffs the current frame against blank
	 * cells and redraws everything within the rectangle. The prev_rect_* record
	 * is left intact so residue from a previous, larger rectangle is still
	 * erased. prev_valid is cleared because prev no longer reflects the screen,
	 * matching the resize path. Use when the terminal content under the
	 * rectangle changed out from under terse (scroll, a fresh prompt line). */
	cells_fill_empty(handle->prev_cells, (size_t)handle->buf_rows * (size_t)handle->buf_cols);
	handle->prev_valid = 0;
	clear_error(handle);
	return TERSE_OK;
}

/* ========================================================================
 * Writing into the current frame
 * ======================================================================== */

static void buffer_set_cell(terse_handle_t handle, int row, int col,
                            const char *utf8, size_t char_len, int width)
{
	size_t index;
	terse_cell_t *cell;

	if (row < 0 || row >= handle->buf_rows || col < 0 || col >= handle->buf_cols) {
		return;
	}
	index = (size_t)row * (size_t)handle->buf_cols + (size_t)col;
	cell = &handle->cur_cells[index];

	if (char_len > 4) {
		char_len = 4;
	}
	if (char_len > 0) {
		memcpy(cell->utf8_char, utf8, char_len);
	}
	cell->utf8_char[char_len] = '\0';
	cell->char_len = (uint8_t)char_len;
	cell->display_width = (uint8_t)width;
	cell->is_continuation = 0;
	cell->fg = handle->style.foreground;
	cell->bg = handle->style.background;
	cell->effects = (uint16_t)handle->style.effects;

	/* Wide character: mark the trailing column as a continuation cell. */
	if (width == 2 && col + 1 < handle->buf_cols) {
		terse_cell_t *next = &handle->cur_cells[index + 1];
		cell_make_empty(next);
		next->display_width = 0;
		next->is_continuation = 1;
		next->fg = handle->style.foreground;
		next->bg = handle->style.background;
		next->effects = (uint16_t)handle->style.effects;
	}
}

void terse_buffer_write_text(terse_handle_t handle, int row, int col, const char *utf8)
{
	size_t i;
	size_t len;
	int cur_col;

	if (!handle || !handle->cur_cells || !utf8) {
		return;
	}

	len = strlen(utf8);
	cur_col = col;
	i = 0;
	while (i < len && cur_col < handle->buf_cols) {
		size_t char_len = utf8_char_length((unsigned char)utf8[i]);
		unsigned int scalar;
		int width;

		if (char_len == 0 || i + char_len > len) {
			i++;
			continue;
		}
		scalar = utf8_decode((const unsigned char *)&utf8[i], char_len);
		width = terse_char_width(handle, scalar);
		if (width < 1) {
			width = 1;
		}
		if (cur_col + width > handle->buf_cols) {
			break; /* would overflow the line */
		}
		buffer_set_cell(handle, row, cur_col, &utf8[i], char_len, width);
		cur_col += width;
		i += char_len;
	}

	handle->cursor_row = row;
	handle->cursor_col = cur_col;
	handle->cursor_known = 1;
}

/* ========================================================================
 * Diff
 * ======================================================================== */

static int colors_equal(const terse_color_t *a, const terse_color_t *b)
{
	if (a->kind != b->kind) {
		return 0;
	}
	switch (a->kind) {
	case TERSE_COLOR_KIND_BASIC16:
		return a->data.basic16.color == b->data.basic16.color && a->data.basic16.bright == b->data.basic16.bright;
	case TERSE_COLOR_KIND_PALETTE256:
		return a->data.palette.value == b->data.palette.value;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return a->data.truecolor.r == b->data.truecolor.r && a->data.truecolor.g == b->data.truecolor.g && a->data.truecolor.b == b->data.truecolor.b;
	case TERSE_COLOR_KIND_DEFAULT:
	default:
		return 1;
	}
}

static int cells_equal(const terse_cell_t *a, const terse_cell_t *b)
{
	if (a->char_len != b->char_len || a->display_width != b->display_width || a->is_continuation != b->is_continuation || a->effects != b->effects) {
		return 0;
	}
	if (a->char_len > 0 && memcmp(a->utf8_char, b->utf8_char, a->char_len) != 0) {
		return 0;
	}
	if (!colors_equal(&a->fg, &b->fg) || !colors_equal(&a->bg, &b->bg)) {
		return 0;
	}
	return 1;
}

void terse_buffer_diff(terse_handle_t handle)
{
	size_t total;
	size_t i;

	if (!handle || !handle->cur_cells || !handle->prev_cells || !handle->dirty) {
		return;
	}
	total = (size_t)handle->buf_rows * (size_t)handle->buf_cols;
	for (i = 0; i < total; i++) {
		handle->dirty[i] = (unsigned char)(cells_equal(&handle->prev_cells[i], &handle->cur_cells[i]) ? 0 : 1);
	}
}

/* ========================================================================
 * Flush: emit diff, then swap and reset
 * ======================================================================== */

static void swap_buffers(terse_handle_t handle)
{
	terse_cell_t *tmp = handle->prev_cells;
	handle->prev_cells = handle->cur_cells;
	handle->cur_cells = tmp;
}

/* Emit a run of `count` spaces at the absolute terminal position (abs_row, abs_col),
 * resetting style first so the erased cells carry no leftover attributes. */
static terse_error_t erase_run(terse_handle_t handle, int abs_row, int abs_col, int count)
{
	terse_error_t err;
	int i;

	if (count <= 0) {
		return TERSE_OK;
	}
	err = terse_move_to(handle, abs_row, abs_col);
	if (err != TERSE_OK) {
		return err;
	}
	err = terse_reset_style(handle, TERSE_RESET_ALL);
	if (err != TERSE_OK) {
		return err;
	}
	for (i = 0; i < count; i++) {
		err = terse_write_text(handle, " ");
		if (err != TERSE_OK) {
			return err;
		}
	}
	return TERSE_OK;
}

/* Erase the residue left by the previous flush: terminal cells that the previous
 * rectangle covered but the current rectangle does not. Both rectangles are
 * compared in absolute terminal coordinates (origin + size). terse only owns the
 * cells it emitted itself, so the difference is exactly the previous rectangle
 * minus the current one. */
static terse_error_t erase_residue(terse_handle_t handle)
{
	int prev_top, prev_bottom, prev_left, prev_right;
	int cur_top, cur_bottom, cur_left, cur_right;
	int abs_row;
	terse_error_t err;

	if (!handle->prev_rect_valid) {
		return TERSE_OK;
	}

	prev_top = handle->prev_origin_row;
	prev_bottom = handle->prev_origin_row + handle->prev_buf_rows; /* exclusive */
	prev_left = handle->prev_origin_col;
	prev_right = handle->prev_origin_col + handle->prev_buf_cols; /* exclusive */

	cur_top = handle->buf_origin_row;
	cur_bottom = handle->buf_origin_row + handle->buf_rows;
	cur_left = handle->buf_origin_col;
	cur_right = handle->buf_origin_col + handle->buf_cols;

	for (abs_row = prev_top; abs_row < prev_bottom; abs_row++) {
		int inside_row = (abs_row >= cur_top && abs_row < cur_bottom);

		if (!inside_row) {
			/* Entire previous row lies outside the current rectangle. */
			err = erase_run(handle, abs_row, prev_left, prev_right - prev_left);
			if (err != TERSE_OK) {
				return err;
			}
			continue;
		}
		/* Row overlaps vertically: erase only the columns of the previous row
		 * that fall outside the current rectangle (left and/or right margins). */
		if (prev_left < cur_left) {
			int end = prev_right < cur_left ? prev_right : cur_left;
			err = erase_run(handle, abs_row, prev_left, end - prev_left);
			if (err != TERSE_OK) {
				return err;
			}
		}
		if (prev_right > cur_right) {
			int begin = prev_left > cur_right ? prev_left : cur_right;
			err = erase_run(handle, abs_row, begin, prev_right - begin);
			if (err != TERSE_OK) {
				return err;
			}
		}
	}
	return TERSE_OK;
}

terse_error_t terse_buffer_flush(terse_handle_t handle)
{
	int row;
	int cols;
	terse_error_t err = TERSE_OK;

	if (!handle || !handle->cur_cells) {
		return TERSE_OK;
	}

	/* Erase residue from the previous rectangle (origin moved or buffer shrank)
	 * before drawing the current frame. */
	err = erase_residue(handle);
	if (err != TERSE_OK) {
		return err;
	}

	terse_buffer_diff(handle);

	/* When the origin moved, prev_cells describes a different absolute region, so
	 * the per-cell diff against it is meaningless: force a full redraw at the new
	 * position. (Size changes already mark everything dirty via the diff path once
	 * resize lands in 5.5-B; here we only need to cover the origin shift.) */
	if (handle->prev_rect_valid &&
	    (handle->prev_origin_row != handle->buf_origin_row ||
	     handle->prev_origin_col != handle->buf_origin_col)) {
		memset(handle->dirty, 1, (size_t)handle->buf_rows * (size_t)handle->buf_cols);
	}

	cols = handle->buf_cols;
	for (row = 0; row < handle->buf_rows; row++) {
		int col = 0;
		while (col < cols) {
			int start;
			int c;

			/* Skip clean cells. */
			while (col < cols && !handle->dirty[(size_t)row * (size_t)cols + (size_t)col]) {
				col++;
			}
			if (col >= cols) {
				break;
			}
			start = col;
			/* Extend over the contiguous dirty run. */
			while (col < cols && handle->dirty[(size_t)row * (size_t)cols + (size_t)col]) {
				col++;
			}

			err = terse_move_to(handle, handle->buf_origin_row + row, handle->buf_origin_col + start);
			if (err != TERSE_OK) {
				return err;
			}

			for (c = start; c < col; c++) {
				terse_cell_t *cell = &handle->cur_cells[(size_t)row * (size_t)cols + (size_t)c];
				terse_style_t style;

				if (cell->is_continuation) {
					continue; /* handled by the wide char in the preceding column */
				}

				style.foreground = cell->fg;
				style.background = cell->bg;
				style.effects = cell->effects;
				err = terse_set_style(handle, &style);
				if (err != TERSE_OK) {
					return err;
				}

				err = terse_write_text(handle, cell->char_len > 0 ? cell->utf8_char : " ");
				if (err != TERSE_OK) {
					return err;
				}
			}
		}
	}

	/* Reset style so trailing attributes don't leak past the frame. */
	err = terse_reset_style(handle, TERSE_RESET_ALL);
	if (err != TERSE_OK) {
		return err;
	}

	/* Place the terminal cursor at the caller's requested logical position
	 * (rectangle-local), projected to absolute as origin + local. Runs while
	 * in_flush is set, so the move emits immediately. Without a request the
	 * cursor stays wherever the last diff run left it (legacy behavior). */
	if (handle->buf_cursor_set) {
		err = terse_move_to(handle,
		                    handle->buf_origin_row + handle->buf_cursor_row,
		                    handle->buf_origin_col + handle->buf_cursor_col);
		if (err != TERSE_OK) {
			return err;
		}
	}

	/* Record the rectangle we just emitted so the next flush can erase any
	 * residue when the origin moves or the buffer shrinks. */
	handle->prev_origin_row = handle->buf_origin_row;
	handle->prev_origin_col = handle->buf_origin_col;
	handle->prev_buf_rows = handle->buf_rows;
	handle->prev_buf_cols = handle->buf_cols;
	handle->prev_rect_valid = 1;
	/* After the swap below, prev_cells holds the frame just displayed. */
	handle->prev_valid = 1;

	swap_buffers(handle);
	terse_buffer_clear(handle);
	memset(handle->dirty, 0, (size_t)handle->buf_rows * (size_t)handle->buf_cols);
	return TERSE_OK;
}
