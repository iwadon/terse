#include "terse_event_helpers.h"
#include "terse_unicode.h"
#include <string.h>

/*
 * Event construction helpers for platform-specific input handling.
 * These functions populate terse_event_t structures with parsed input data.
 */

void
terse_set_char_event(terse_handle_t handle, terse_event_t *event, unsigned int scalar, int mods)
{
	int width = terse_compute_cell_width(handle, scalar);
	event->type = TERSE_EVENT_CHAR;
	event->data.ch.scalar = scalar;
	event->data.ch.width = width;
	event->data.ch.mods = mods;
}

void
terse_set_key_event(terse_event_t *event, terse_event_type_t type, int mods)
{
	event->type = type;
	event->data.key.mods = mods;
}

void
terse_set_function_event(terse_event_t *event, int fn_number, int mods)
{
	event->type = TERSE_EVENT_FUNCTION;
	event->data.function.number = fn_number;
	event->data.function.mods = mods;
}

void
terse_set_mouse_event(terse_event_t *event, terse_event_type_t type, terse_mouse_button_t button, int mods, int row, int col)
{
	event->type = type;
	event->data.mouse.button = button;
	event->data.mouse.mods = mods;
	event->data.mouse.row = row;
	event->data.mouse.col = col;
}

void
terse_set_resize_event(terse_event_t *event, int rows, int cols)
{
	event->type = TERSE_EVENT_RESIZE;
	event->data.resize.rows = rows;
	event->data.resize.cols = cols;
}

void
terse_set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length)
{
	event->type = TERSE_EVENT_RAW_SEQUENCE;
	if (length > TERSE_EVENT_RAW_MAX) {
		length = TERSE_EVENT_RAW_MAX;
	}
	event->data.raw.length = length;
	memset(event->data.raw.bytes, 0, TERSE_EVENT_RAW_MAX);
	memcpy(event->data.raw.bytes, bytes, length);
}
