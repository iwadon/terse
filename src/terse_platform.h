#ifndef TERSE_PLATFORM_H_INCLUDED
#define TERSE_PLATFORM_H_INCLUDED

#include <sys/types.h>
#if defined(_WIN32) && !defined(_SSIZE_T_DEFINED)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

#include "terse.h"

terse_options_t terse_platform_default_options(void);

/* Platform-specific initialization and shutdown.
 * init() is called by terse_open() after the handle is otherwise initialized.
 * shutdown() is called by terse_close() before codec handles are destroyed.
 * Implementations may use handle->platform_data to store opaque per-handle state.
 * Both functions must be safe to call even when init() previously failed
 * (shutdown is invoked unconditionally on close). */
terse_error_t terse_platform_init(terse_handle_t handle);
void terse_platform_shutdown(terse_handle_t handle);

terse_size_t terse_platform_query_fd_size(int fd);
size_t terse_platform_probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity);
terse_error_t terse_platform_query_cursor_position(int input_fd, int output_fd, int *out_row, int *out_col);
terse_error_t terse_platform_wait_for_input(int fd, int timeout_ms);
ssize_t terse_platform_read_byte(int fd, unsigned char *out);
size_t terse_platform_drain_escape_sequence(int fd, unsigned char *buffer, size_t max);
terse_error_t terse_platform_write_bytes(int fd, const char *bytes, size_t len);

#if defined(__human68k__) || defined(_WIN32)
/* Platform-specific event reading API for Human68k and Windows */
terse_error_t terse_platform_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event);
#endif

/* Platform-specific fast path optimizations (optional) */
/* If a platform does not implement these, they should return TERSE_ERR_NOT_SUPPORTED */
terse_error_t terse_platform_move_to_fast(terse_handle_t handle, int row, int col);
terse_error_t terse_platform_clear_screen_fast(terse_handle_t handle, terse_clear_mode_t mode);

#endif
