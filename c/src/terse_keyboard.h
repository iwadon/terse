#ifndef TERSE_KEYBOARD_H
#define TERSE_KEYBOARD_H

#include "terse.h"
#include "terse_handle.h"

/* Forward declarations for internal functions */
int ensure_handle(terse_handle_t handle);
void set_error(terse_handle_t handle, terse_error_t error);
void clear_error(terse_handle_t handle);
int write_literal(terse_handle_t handle, const char *literal);

#endif /* TERSE_KEYBOARD_H */
