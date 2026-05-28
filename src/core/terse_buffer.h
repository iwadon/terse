#ifndef TERSE_BUFFER_H
#define TERSE_BUFFER_H

#include "terse_handle.h"

/*
 * Virtual cell buffer for TERSE_RENDER_BUFFERED.
 *
 * Design borrowed from terse-prompt's tprompt_display.c (double buffer + diff +
 * dirty cells), redesigned to operate directly on terse_handle_t and to carry
 * per-cell color/effect attributes (terse_cell_t).
 *
 * In immediate mode none of these are called and the buffer pointers stay NULL.
 */

/* Allocate cur/prev/dirty for the given dimensions. Frees any prior allocation.
 * Returns 0 on success, -1 on allocation failure (buffer left freed). */
int terse_buffer_alloc(terse_handle_t handle, int rows, int cols);

/* Free cur/prev/dirty and reset dimensions to 0. Safe on a NULL/unallocated handle. */
void terse_buffer_free(terse_handle_t handle);

/* Reset the current frame to empty cells (does not touch prev). */
void terse_buffer_clear(terse_handle_t handle);

/* Resize buffers, preserving nothing (all cells reset, full redraw on next flush).
 * Returns 0 on success, -1 on failure. A no-op clear when dimensions are unchanged. */
int terse_buffer_resize(terse_handle_t handle, int rows, int cols);

/* Write a UTF-8 string into the current frame starting at (row, col), using the
 * handle's current style for fg/bg/effects. Advances over wide chars and stops at
 * the right edge. Updates handle->cursor_row/col to the resulting position. */
void terse_buffer_write_text(terse_handle_t handle, int row, int col, const char *utf8);

/* Compute dirty[] by comparing prev vs cur (content + attributes). Marks all dirty
 * when dimensions differ. */
void terse_buffer_diff(terse_handle_t handle);

/* Emit the diff to the terminal (cursor moves, SGR, text) for all dirty cells,
 * then swap cur<->prev and clear dirty[]. Must run with handle->in_flush set so the
 * write paths take the immediate route. Returns TERSE_OK or an error code. */
terse_error_t terse_buffer_flush(terse_handle_t handle);

#endif /* TERSE_BUFFER_H */
