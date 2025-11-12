#ifndef TERSE_EVENT_HELPERS_H_INCLUDED
#define TERSE_EVENT_HELPERS_H_INCLUDED

#include "terse.h"

/*
 * Internal helper functions for creating terse_event_t structures.
 * These functions are used by platform-specific code to construct events.
 */

/* Create a character event with Unicode scalar, cell width, and modifiers */
void terse_set_char_event(terse_handle_t handle, terse_event_t *event, unsigned int scalar, int mods);

/* Create a key event (arrows, enter, backspace, etc.) with modifiers */
void terse_set_key_event(terse_event_t *event, terse_event_type_t type, int mods);

/* Create a function key event (F1-F12) with key number and modifiers */
void terse_set_function_event(terse_event_t *event, int fn_number, int mods);

/* Create a mouse event with button, position, and modifiers */
void terse_set_mouse_event(terse_event_t *event, terse_event_type_t type, terse_mouse_button_t button, int mods, int row, int col);

/* Create a resize event with terminal dimensions */
void terse_set_resize_event(terse_event_t *event, int rows, int cols);

/* Create a raw sequence event for unrecognized input */
void terse_set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length);

/* Compute cell width for a Unicode scalar value */
int terse_compute_cell_width(terse_handle_t handle, unsigned int scalar);

#endif /* TERSE_EVENT_HELPERS_H_INCLUDED */
