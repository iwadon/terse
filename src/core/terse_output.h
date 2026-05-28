#ifndef TERSE_OUTPUT_H
#define TERSE_OUTPUT_H

#include "terse.h"

/* Internal header for output and control functions.
 * This module handles:
 * - Screen and line clearing
 * - Cursor movement
 * - Text output and flushing
 * - Mouse, bracketed paste control
 * - Window title, hyperlinks, cursor shape
 *
 * Graphics functions (images, clipboard, notifications) moved to terse_graphics.h
 */

/* Mouse mode control */
int set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable);
terse_mouse_mode_t clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available);

/* Bracketed paste control */
int set_bracketed_paste(terse_handle_t handle, int enable);

#endif // TERSE_OUTPUT_H
