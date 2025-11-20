#ifndef TERSE_STATE_H
#define TERSE_STATE_H

#include "terse.h"
#include "terse_handle.h"

/* Forward declarations for internal functions */
int ensure_handle(terse_handle_t handle);
void set_error(terse_handle_t handle, terse_error_t error);
void clear_error(terse_handle_t handle);
void update_effective_style(terse_handle_t handle);

/* Internal state management functions */
terse_error_t terse_state_override_impl(terse_handle_t handle, const terse_state_t *state);
terse_error_t terse_state_clear_impl(terse_handle_t handle);
terse_error_t terse_push_state_impl(terse_handle_t handle);
terse_error_t terse_pop_state_impl(terse_handle_t handle);
terse_error_t terse_capture_state_impl(terse_handle_t handle, terse_state_t *out_state);
terse_error_t terse_restore_state_impl(terse_handle_t handle, const terse_state_t *state);

#endif /* TERSE_STATE_H */
