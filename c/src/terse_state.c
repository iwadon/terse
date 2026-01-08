#include "terse_state.h"
#include "terse.h"
#include "terse_handle.h"
#include "terse_style.h"

#include <errno.h>
#include <string.h>

terse_error_t terse_state_override(terse_handle_t handle, const terse_state_t *state)
{
	TERSE_CHECK_HANDLE(handle);
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (state->cursor_known) {
		handle->cursor_known = 1;
		// Clamp to 0-based minimum
		handle->cursor_row = state->cursor_row >= 0 ? state->cursor_row : 0;
		handle->cursor_col = state->cursor_col >= 0 ? state->cursor_col : 0;
	} else {
		handle->cursor_known = 0;
		if (state->cursor_row >= 0) {
			handle->cursor_row = state->cursor_row;
		}
		if (state->cursor_col >= 0) {
			handle->cursor_col = state->cursor_col;
		}
	}
	handle->cursor_visible = state->cursor_visible ? 1 : 0;
	if (state->style_known) {
		terse_style_t sanitized = terse_style_sanitize_request(&state->style);
		handle->style = sanitized;
		handle->effective_style = terse_style_make_effective(&handle->capabilities, &sanitized);
		handle->style_known = 1;
	} else {
		handle->style_known = 0;
	}
	clear_error(handle);
	return 0;
}

terse_error_t terse_state_clear(terse_handle_t handle)
{
	TERSE_CHECK_HANDLE(handle);
	handle->cursor_known = 0;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_visible = 1;
	handle->style = terse_style_default();
	handle->effective_style = terse_style_make_effective(&handle->capabilities, &handle->style);
	handle->style_known = 0;
	clear_error(handle);
	return 0;
}

terse_error_t terse_push_state(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (handle->state_stack_top >= TERSE_STATE_STACK_MAX - 1) {
		set_error(handle, TERSE_ERR_STACK_OVERFLOW);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	terse_state_t *stack_state = &handle->state_stack[handle->state_stack_top + 1];
	stack_state->cursor_known = handle->cursor_known;
	stack_state->cursor_visible = handle->cursor_visible;
	stack_state->cursor_row = handle->cursor_row;
	stack_state->cursor_col = handle->cursor_col;
	stack_state->style_known = handle->style_known;
	stack_state->style = handle->style;
	handle->state_stack_top++;
	return 0;
}

terse_error_t terse_pop_state(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (handle->state_stack_top < 0) {
		set_error(handle, TERSE_ERR_STACK_UNDERFLOW);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	const terse_state_t *state = &handle->state_stack[handle->state_stack_top];
	handle->cursor_known = state->cursor_known;
	handle->cursor_visible = state->cursor_visible;
	handle->cursor_row = state->cursor_row;
	handle->cursor_col = state->cursor_col;
	handle->style_known = state->style_known;
	handle->style = state->style;
	if (state->style_known) {
		handle->effective_style = terse_style_make_effective(&handle->capabilities, &state->style);
	}
	handle->state_stack_top--;
	return 0;
}

terse_error_t terse_capture_state(terse_handle_t handle, terse_state_t *out_state)
{
	TERSE_CHECK_HANDLE(handle);
	if (!out_state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	out_state->cursor_known = handle->cursor_known;
	out_state->cursor_visible = handle->cursor_visible;
	out_state->cursor_row = handle->cursor_row;
	out_state->cursor_col = handle->cursor_col;
	out_state->style_known = handle->style_known;
	out_state->style = handle->style;
	clear_error(handle);
	return 0;
}

terse_error_t terse_restore_state(terse_handle_t handle, const terse_state_t *state)
{
	TERSE_CHECK_HANDLE(handle);
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	terse_state_t local = *state;
	if (local.cursor_known) {
		// Clamp to 0-based minimum
		if (local.cursor_row < 0) {
			local.cursor_row = 0;
		}
		if (local.cursor_col < 0) {
			local.cursor_col = 0;
		}
	}
	local.cursor_visible = state->cursor_visible ? 1 : 0;
	if (local.style_known) {
		local.style = terse_style_sanitize_request(&state->style);
	}

	int result = 0;

	// Apply outputs BEFORE updating internal state to avoid duplicate skipping
	if (local.cursor_known && handle->capabilities.has_move_absolute && local.cursor_row >= 0 && local.cursor_col >= 0) {
		int move_rc = terse_move_to(handle, local.cursor_row, local.cursor_col);
		if (move_rc < 0 && result == 0) {
			result = move_rc;
		}
	}
	if (handle->capabilities.has_cursor_visibility) {
		int vis_rc = terse_show_cursor(handle, local.cursor_visible);
		if (vis_rc < 0 && result == 0) {
			result = vis_rc;
		}
	}
	if (local.style_known) {
		int style_rc = terse_set_style(handle, &local.style);
		if (style_rc < 0 && result == 0) {
			result = style_rc;
		}
	}

	// Update internal state after outputs to keep state synchronized
	(void)terse_state_override(handle, &local);

	return result;
}
