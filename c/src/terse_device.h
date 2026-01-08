#ifndef TERSE_DEVICE_H
#define TERSE_DEVICE_H

#include "terse.h"

/* Internal header for device control functions.
 * This module handles:
 * - Mouse tracking control
 * - Bracketed paste mode
 * - Window title
 * - Hyperlinks
 */

/* Mouse mode control */
int set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable);
terse_mouse_mode_t clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available);

/* Bracketed paste control */
int set_bracketed_paste(terse_handle_t handle, int enable);

#endif // TERSE_DEVICE_H
