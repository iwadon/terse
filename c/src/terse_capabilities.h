#ifndef TERSE_CAPABILITIES_H
#define TERSE_CAPABILITIES_H

#include "terse.h"
#include "terse_handle.h"

/* Forward declarations for internal functions */
int ensure_handle(terse_handle_t handle);
void set_error(terse_handle_t handle, terse_error_t error);
void clear_error(terse_handle_t handle);

/* Internal capabilities management functions */
void recompute_capabilities(terse_handle_t handle);

#endif /* TERSE_CAPABILITIES_H */
