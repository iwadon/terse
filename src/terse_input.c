#include "terse_input.h"
#include "terse_codec.h"
#include "terse_event_helpers.h"
#include "terse_internal.h"
#include "terse_platform.h"
#include "terse_test.h"
#include "terse_unicode.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>

/* Forward declarations for internal helpers */
static size_t expected_bytes_for_codec(terse_codec_kind_t kind, unsigned char first);
static unsigned int decode_shift_jis_bytes(terse_handle_t handle, const unsigned char *bytes, size_t length);
static void set_error(terse_handle_t handle, terse_error_t error);
static void clear_error(terse_handle_t handle);

/* Replacement characters for invalid sequences (from terse_codec.h) */
#define UTF8_REPLACEMENT TERSE_UTF8_REPLACEMENT
#define SHIFT_JIS_REPLACEMENT TERSE_SHIFT_JIS_REPLACEMENT

/*
 * Error handling helpers (minimal wrappers for handle->last_error)
 */
static void
set_error(terse_handle_t handle, terse_error_t error)
{
	if (!handle) {
		return;
	}
	handle->last_error = error;
}

static void
clear_error(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	handle->last_error = TERSE_OK;
}

/*
 * CSI sequence parser
 * Parses sequences like ESC [ <params> <final>
 */
int terse_parse_csi_sequence(const unsigned char *seq, size_t len, int *values, size_t max_values, size_t *value_count, char *final)
{
	if (len < 2 || seq[0] != 0x1b || seq[1] != '[') {
		return -1;
	}
	if (len < 3) {
		return -1;
	}
	char terminator = (char)seq[len - 1];
	if (terminator < '@' || terminator > '~') {
		return -1;
	}
	size_t count = 0;
	size_t index = 2;
	while (index < len - 1) {
		if (seq[index] == '<') {
			++index;
			continue;
		}
		int value = 0;
		int has_digit = 0;
		while (index < len - 1 && isdigit((unsigned char)seq[index])) {
			has_digit = 1;
			value = (value * 10) + (seq[index] - '0');
			++index;
		}
		if (has_digit) {
			if (count < max_values) {
				values[count] = value;
			}
			++count;
		}
		if (index >= len - 1) {
			break;
		}
		if (seq[index] == ';') {
			++index;
			continue;
		}
		break;
	}
	*value_count = count;
	*final = terminator;
	return 0;
}

/*
 * Modifier conversion functions
 */
int terse_modifier_bits_from_param(int param)
{
	int mods = 0;
	switch (param) {
	case 2:
		mods = TERSE_MOD_SHIFT;
		break;
	case 3:
		mods = TERSE_MOD_ALT;
		break;
	case 4:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT;
		break;
	case 5:
		mods = TERSE_MOD_CTRL;
		break;
	case 6:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_CTRL;
		break;
	case 7:
		mods = TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	case 8:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	default:
		break;
	}
	return mods;
}

int terse_mods_from_kitty_param(int param)
{
	if (param <= 0) {
		return 0;
	}
	int bits = param - 1;
	int mods = 0;
	if (bits & 0x01) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (bits & 0x02) {
		mods |= TERSE_MOD_ALT;
	}
	if (bits & 0x04) {
		mods |= TERSE_MOD_CTRL;
	}
	if (bits & 0x08) {
		mods |= TERSE_MOD_META;
	}
	return mods;
}

int terse_mouse_modifiers_from_param(int param)
{
	int mods = 0;
	if (param & 4) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (param & 8) {
		mods |= TERSE_MOD_ALT;
	}
	if (param & 16) {
		mods |= TERSE_MOD_CTRL;
	}
	return mods;
}

/*
 * SGR mouse sequence handler (1006 format)
 */
int terse_handle_sgr_mouse_sequence(terse_handle_t handle, terse_event_t *out_event, const int *values, size_t value_count, char final)
{
	if (!handle || value_count < 3) {
		return 0;
	}
	if (!handle->mouse_enabled || handle->mouse_mode == TERSE_MOUSE_NONE) {
		return 0;
	}
	int raw_cb = values[0];
	// Terminal sends 1-based coordinates, convert to 0-based
	int col = values[1] - 1;
	int row = values[2] - 1;
	if (col < 0 || row < 0) {
		return 0;
	}
	int mods = terse_mouse_modifiers_from_param(raw_cb);
	int cb = raw_cb & ~(4 | 8 | 16);
	int is_motion = cb & 32;
	int is_wheel = cb & 64;
	int base = cb & 3;
	terse_mouse_button_t button = TERSE_MOUSE_BUTTON_NONE;
	terse_event_type_t type = TERSE_EVENT_MOUSE_MOVE;
	if (is_wheel) {
		button = (base == 0) ? TERSE_MOUSE_BUTTON_SCROLL_UP : TERSE_MOUSE_BUTTON_SCROLL_DOWN;
		type = TERSE_EVENT_MOUSE_SCROLL;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (final == 'm') {
		if (base <= 2) {
			button = (base == 0) ? TERSE_MOUSE_BUTTON_LEFT : (base == 1 ? TERSE_MOUSE_BUTTON_MIDDLE : TERSE_MOUSE_BUTTON_RIGHT);
		} else {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_UP;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (is_motion) {
		if (handle->mouse_button != TERSE_MOUSE_BUTTON_NONE) {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_MOVE;
	} else {
		if (base == 0) {
			button = TERSE_MOUSE_BUTTON_LEFT;
		} else if (base == 1) {
			button = TERSE_MOUSE_BUTTON_MIDDLE;
		} else if (base == 2) {
			button = TERSE_MOUSE_BUTTON_RIGHT;
		}
		type = TERSE_EVENT_MOUSE_DOWN;
		handle->mouse_button = button;
	}
	terse_set_mouse_event(out_event, type, button, mods, row, col);
	clear_error(handle);
	return 1;
}

/*
 * Codec helper functions
 */
static size_t
expected_bytes_for_codec(terse_codec_kind_t kind, unsigned char first)
{
	switch (kind) {
	case TERSE_CODEC_UTF8:
		if (first < 0x80) {
			return 1;
		}
		if ((first & 0xe0u) == 0xc0u) {
			return 2;
		}
		if ((first & 0xf0u) == 0xe0u) {
			return 3;
		}
		if ((first & 0xf8u) == 0xf0u) {
			return 4;
		}
		return 0;
	case TERSE_CODEC_SHIFT_JIS:
		if (first <= 0x7f) {
			return 1;
		}
		if (first >= 0xa1 && first <= 0xdf) {
			return 1;
		}
		if ((first >= 0x81 && first <= 0x9f) || (first >= 0xe0 && first <= 0xfc)) {
			return 2;
		}
		return 0;
	case TERSE_CODEC_UNKNOWN:
	default:
		return 1;
	}
}

static unsigned int
decode_shift_jis_bytes(terse_handle_t handle, const unsigned char *bytes, size_t length)
{
	if (length == 0 || !bytes) {
		return SHIFT_JIS_REPLACEMENT;
	}
	unsigned char first = bytes[0];
	if (length == 1) {
		if (first <= 0x7f) {
			return first;
		}
		if (first >= 0xa1 && first <= 0xdf) {
			return 0xff61u + (unsigned int)(first - 0xa1);
		}
		return SHIFT_JIS_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char lead = bytes[0];
		unsigned char trail = bytes[1];
		if (!((trail >= 0x40 && trail <= 0x7e) || (trail >= 0x80 && trail <= 0xfc))) {
			return SHIFT_JIS_REPLACEMENT;
		}
		return terse_convert_shift_jis_pair(handle, lead, trail);
	}
	return SHIFT_JIS_REPLACEMENT;
}

int terse_function_number_from_code(int code)
{
	switch (code) {
	case 11:
		return 1;
	case 12:
		return 2;
	case 13:
		return 3;
	case 14:
		return 4;
	case 15:
		return 5;
	case 17:
		return 6;
	case 18:
		return 7;
	case 19:
		return 8;
	case 20:
		return 9;
	case 21:
		return 10;
	case 23:
		return 11;
	case 24:
		return 12;
	case 25:
		return 13;
	case 26:
		return 14;
	case 28:
		return 15;
	case 29:
		return 16;
	case 31:
		return 17;
	case 32:
		return 18;
	case 33:
		return 19;
	case 34:
		return 20;
	case 35:
		return 21;
	case 36:
		return 22;
	case 37:
		return 23;
	case 38:
		return 24;
	default:
		return 0;
	}
}

/*
 * Stream character decoder
 */
int terse_decode_stream_char(terse_handle_t handle, int fd, unsigned char first, terse_event_t *event)
{
	unsigned int scalar = 0;
	int rc = 0;
	switch (handle->codec_kind) {
	case TERSE_CODEC_UTF8:
		rc = terse_decode_utf8_stream(fd, first, &scalar);
		if (rc < 0) {
			return rc;
		}
		if (scalar == 0) {
			scalar = UTF8_REPLACEMENT;
		}
		break;
	case TERSE_CODEC_SHIFT_JIS:
		rc = terse_decode_shift_jis_stream(handle, fd, first, &scalar);
		if (rc < 0) {
			return rc;
		}
		break;
	default:
		scalar = first;
		break;
	}
	terse_set_char_event(handle, event, scalar, 0);
	return 0;
}

/*
 * Input byte reader with timeout and pending byte buffer
 */
int terse_read_input_byte(terse_handle_t handle, int timeout_ms, unsigned char *out)
{
	if (!handle || !out) {
		return -TERSE_ERR_INVALID_ARGUMENT;
	}
	if (handle->has_pending_byte) {
		*out = handle->pending_byte;
		handle->has_pending_byte = 0;
		return 1;
	}
	int fd = handle->options.input_fd;
	int ready = terse_platform_wait_for_input(fd, timeout_ms);
	if (ready == 0) {
		return 0;
	}
	if (ready < 0) {
		return ready;
	}
	ssize_t n = terse_platform_read_byte(fd, out);
	if (n == 0) {
		errno = EPIPE;
		return -TERSE_ERR_IO;
	}
	if (n < 0) {
		return (int)n;
	}
	return 1;
}

/*
 * Parse control character into event
 */
int terse_parse_control_char(terse_handle_t handle, terse_event_t *out_event, unsigned char ch)
{
	switch (ch) {
	case '\r':
	case '\n':
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, (ch == '\n') ? TERSE_MOD_CTRL : 0);
		return 1;
	case '\b':
	case 0x7f:
		terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		return 1;
	case '\t':
		terse_set_key_event(out_event, TERSE_EVENT_TAB, 0);
		return 1;
	default:
		break;
	}
	if (ch >= 0x01 && ch <= 0x1a) {
		unsigned int scalar = 'a' + (ch - 1);
		terse_set_char_event(handle, out_event, scalar, TERSE_MOD_CTRL);
		return 1;
	}
	return 0;
}

/*
 * Parse Linux console function key sequence (ESC [ [ A-L)
 */
int terse_parse_linux_console_fkey(terse_handle_t handle, terse_event_t *out_event,
                                   const unsigned char *seq, size_t len)
{
	(void)handle;
	if (len != 4 || seq[1] != '[' || seq[2] != '[') {
		return 0;
	}
	char code = (char)seq[3];
	int fn = 0;
	if (code >= 'A' && code <= 'L') {
		fn = 1 + (code - 'A');
	}
	if (fn > 0 && fn <= 12) {
		terse_set_function_event(out_event, fn, 0);
		return 1;
	}
	return 0;
}

/*
 * Parse SS3 (ESC O) sequence into event
 */
int terse_parse_ss3_event(terse_handle_t handle, terse_event_t *out_event,
                          const unsigned char *seq, size_t len)
{
	(void)handle;
	if (len < 3 || seq[1] != 'O') {
		return 0;
	}
	int mods = 0;
	char code = (char)seq[2];
	switch (code) {
	case 'A':
		terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
		return 1;
	case 'B':
		terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
		return 1;
	case 'C':
		terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
		return 1;
	case 'D':
		terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
		return 1;
	case 'H':
		terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
		return 1;
	case 'F':
		terse_set_key_event(out_event, TERSE_EVENT_END, mods);
		return 1;
	case 'P':
		terse_set_function_event(out_event, 1, mods);
		return 1;
	case 'Q':
		terse_set_function_event(out_event, 2, mods);
		return 1;
	case 'R':
		terse_set_function_event(out_event, 3, mods);
		return 1;
	case 'S':
		terse_set_function_event(out_event, 4, mods);
		return 1;
	default:
		break;
	}
	return 0;
}

/*
 * Parse CSI (ESC [) sequence into event
 */
int terse_parse_csi_event(terse_handle_t handle, terse_event_t *out_event,
                          const unsigned char *seq, size_t len)
{
	int values[8] = { 0 };
	size_t value_count = 0;
	char final = 0;

	if (terse_parse_csi_sequence(seq, len, values, 8, &value_count, &final) != 0) {
		return 0;
	}

	/* SGR mouse tracking (ESC [ < ...) */
	if ((final == 'M' || final == 'm') && len > 2 && seq[2] == '<') {
		if (terse_handle_sgr_mouse_sequence(handle, out_event, values, value_count, final)) {
			return 1;
		}
	}

	/* Bracketed paste sequences */
	if (final == '~' && value_count >= 1 && handle->paste_enabled) {
		if (values[0] == 200) {
			out_event->type = TERSE_EVENT_PASTE_BEGIN;
			return 1;
		}
		if (values[0] == 201) {
			out_event->type = TERSE_EVENT_PASTE_END;
			return 1;
		}
	}

	/* Kitty keyboard protocol (CSI ... u) */
	if (final == 'u' && value_count >= 1) {
		unsigned int code = (unsigned int)values[0];
		int mods = 0;
		if (value_count >= 2) {
			mods = terse_mods_from_kitty_param(values[1]);
		}
		if (code == 13) {
			terse_set_key_event(out_event, TERSE_EVENT_ENTER, mods);
			return 1;
		}
		if (code == 9) {
			terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
			return 1;
		}
		if (code == 127) {
			terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
			return 1;
		}
		if (code >= 0x20 && code <= 0x10ffff) {
			terse_set_char_event(handle, out_event, code, mods);
			return 1;
		}
	}

	/* Shift-Tab (CSI Z) */
	if (final == 'Z') {
		int mods = TERSE_MOD_SHIFT;
		if (value_count > 0) {
			mods = terse_modifier_bits_from_param(values[value_count - 1]);
			if (!(mods & TERSE_MOD_SHIFT)) {
				mods |= TERSE_MOD_SHIFT;
			}
		}
		terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
		return 1;
	}

	/* Terminal resize report (CSI 8 ; rows ; cols t) */
	if (final == 't' && value_count >= 3 && values[0] == 8) {
		terse_set_resize_event(out_event, values[1], values[2]);
		handle->size.rows = values[1];
		handle->size.cols = values[2];
		handle->size.known = 1;
		handle->capabilities.has_size = 1;
		return 1;
	}

	/* Home/End keys (CSI H / CSI F) */
	if (final == 'H' || final == 'F') {
		int mods = 0;
		if (value_count > 0) {
			mods = terse_modifier_bits_from_param(values[value_count - 1]);
		}
		if (final == 'H') {
			terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
		} else {
			terse_set_key_event(out_event, TERSE_EVENT_END, mods);
		}
		return 1;
	}

	/* Extended key sequences (CSI ... ~) */
	if (final == '~') {
		if (value_count == 0) {
			return 0;
		}
		int mods_param = 0;
		int key_code = values[0];
		if (key_code == 27 && value_count >= 3) {
			mods_param = values[1];
			key_code = values[2];
		} else if (value_count > 1) {
			mods_param = values[value_count - 1];
		}
		int mods = terse_modifier_bits_from_param(mods_param);
		switch (key_code) {
		case 1:
		case 7:
			terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
			return 1;
		case 4:
		case 8:
			terse_set_key_event(out_event, TERSE_EVENT_END, mods);
			return 1;
		case 2:
			terse_set_key_event(out_event, TERSE_EVENT_INSERT, mods);
			return 1;
		case 3:
			terse_set_key_event(out_event, TERSE_EVENT_DELETE, mods);
			return 1;
		case 5:
			terse_set_key_event(out_event, TERSE_EVENT_PAGE_UP, mods);
			return 1;
		case 6:
			terse_set_key_event(out_event, TERSE_EVENT_PAGE_DOWN, mods);
			return 1;
		case 9:
			terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
			return 1;
		case 13:
			if (values[0] == 27 && value_count >= 3) {
				terse_set_key_event(out_event, TERSE_EVENT_ENTER, terse_modifier_bits_from_param(values[1]));
				return 1;
			}
			break;
		default:
			break;
		}
		int fn = terse_function_number_from_code(key_code);
		if (fn > 0) {
			terse_set_function_event(out_event, fn, mods);
			return 1;
		}
		if (key_code >= 0 && key_code <= 0x10ffff) {
			if (key_code == 9) {
				terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
				return 1;
			}
			if (key_code == 8 || key_code == 127) {
				terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
				return 1;
			}
			terse_set_char_event(handle, out_event, (unsigned int)key_code, mods);
			return 1;
		}
	}

	/* Arrow keys (CSI A/B/C/D) */
	if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
		int mods = 0;
		if (value_count > 0) {
			mods = terse_modifier_bits_from_param(values[value_count - 1]);
		}
		switch (final) {
		case 'A':
			terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
			return 1;
		case 'B':
			terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
			return 1;
		case 'C':
			terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
			return 1;
		case 'D':
			terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
			return 1;
		default:
			break;
		}
	}

	return 0;
}

/*
 * Escape-prefixed character handler (Alt+key combinations)
 */
int terse_handle_escape_prefixed_char(terse_handle_t handle, terse_event_t *event, const unsigned char *seq, size_t len)
{
	if (len < 2 || !handle) {
		return 0;
	}
	size_t payload_len = len - 1;
	if (payload_len == 0) {
		return 0;
	}
	const unsigned char *payload = seq + 1;
	unsigned char first = payload[0];
	if (first == 0x1b) {
		return 0;
	}
	size_t expected = expected_bytes_for_codec(handle->codec_kind, first);
	if (expected == 0 || expected != payload_len) {
		return 0;
	}
	int mods = TERSE_MOD_ALT;
	if (payload_len == 1) {
		if (payload[0] == '\r' || payload[0] == '\n') {
			terse_set_key_event(event, TERSE_EVENT_ENTER, mods);
			return 1;
		}
		if (payload[0] == '\t') {
			terse_set_key_event(event, TERSE_EVENT_TAB, mods);
			return 1;
		}
		if (payload[0] == '\b' || payload[0] == 0x7f) {
			terse_set_key_event(event, TERSE_EVENT_BACKSPACE, mods);
			return 1;
		}
		if (payload[0] >= 0x01 && payload[0] <= 0x1a) {
			unsigned int scalar = 'a' + (unsigned int)(payload[0] - 1);
			terse_set_char_event(handle, event, scalar, mods | TERSE_MOD_CTRL);
			return 1;
		}
	}
	unsigned int scalar = 0;
	switch (handle->codec_kind) {
	case TERSE_CODEC_UTF8:
		scalar = terse_decode_utf8_bytes(payload, payload_len);
		if (scalar == 0) {
			scalar = UTF8_REPLACEMENT;
		}
		break;
	case TERSE_CODEC_SHIFT_JIS:
		scalar = decode_shift_jis_bytes(handle, payload, payload_len);
		break;
	case TERSE_CODEC_UNKNOWN:
	default:
		scalar = payload[0];
		break;
	}
	terse_set_char_event(handle, event, scalar, mods);
	return 1;
}
