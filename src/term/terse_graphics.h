#ifndef TERSE_GRAPHICS_H
#define TERSE_GRAPHICS_H

#include "terse.h"

/* Internal header for graphics and extended output functions.
 * This module handles:
 * - Image display (iTerm2, Sixel, Kitty)
 * - Clipboard operations
 * - Notifications (bell, visual, desktop)
 */

/* Helper functions for image display */
int send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
int send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
int send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);

/* Helper for payload validation (shared with terse_output.c) */
int payload_has_disallowed_chars(const char *payload);

#endif // TERSE_GRAPHICS_H
