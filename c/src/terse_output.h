#ifndef TERSE_OUTPUT_H
#define TERSE_OUTPUT_H

#include "terse.h"

/* Internal header for output and control functions.
 * This module handles:
 * - Screen and line clearing
 * - Cursor movement
 * - Text output and flushing
 * - Mouse, bracketed paste control
 * - Window title, hyperlinks
 * - Image display (iTerm2, Sixel, Kitty)
 * - Notifications (bell, visual, desktop)
 */

/* Helper functions for image display */
int send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
int send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
int send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);

/* Helper for notification validation */
int payload_has_disallowed_chars(const char *payload);

/* Mouse mode control */
int set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable);
terse_mouse_mode_t clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available);

/* Bracketed paste control */
int set_bracketed_paste(terse_handle_t handle, int enable);

#endif // TERSE_OUTPUT_H
