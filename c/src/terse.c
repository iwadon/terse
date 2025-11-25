#include "terse.h"
#include "terse_handle.h"
#include "terse_platform.h"
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test_internal.h"
#endif
#include "terse_unicode.h"
#include "terse_detection.h"
#include "terse_codec.h"
#include "terse_event_helpers.h"
#include "terse_input.h"
#include "terse_style.h"
#include "terse_output.h"
#include "terse_state.h"
#include "terse_capabilities.h"
#include "terse_keyboard.h"

#include <ctype.h>
#include <errno.h>
#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "mini_iconv.h"
#endif
#include <limits.h>
#ifndef _WIN32
#ifdef TERSE_HAVE_POLL_H
#include <poll.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif
#endif
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#include <unistd.h>
#else
// Windows compatibility
#define strcasecmp _stricmp
#endif

static const char TERSE_RESET_ALL_SEQ[] = "\x1b[0m";
static const char TERSE_RESET_COLOR_SEQ[] = "\x1b[39;49m";
static const char TERSE_RESET_EFFECTS_SEQ[] = "\x1b[22;23;24;27;29m";
static const char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned int UTF8_REPLACEMENT = 0xfffdU;
static const unsigned int SHIFT_JIS_REPLACEMENT = '?';

static void
reset_iconv_state(iconv_t cd)
{
	if (cd == (iconv_t)-1) {
		return;
	}
	(void)iconv(cd, NULL, NULL, NULL, NULL);
}

static unsigned int
decode_utf8_bytes(const unsigned char *bytes, size_t length)
{
	if (!bytes || length == 0) {
		return UTF8_REPLACEMENT;
	}
	unsigned int scalar = 0;
	if (length == 1) {
	unsigned char b0 = bytes[0];
		if (b0 < 0x80) {
			return b0;
		}
		return UTF8_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		if ((b0 & 0xe0) != 0xc0 || (b1 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x1f) << 6) | (unsigned int)(b1 & 0x3f);
		if (scalar < 0x80) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 3) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		if ((b0 & 0xf0) != 0xe0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x0f) << 12) | ((unsigned int)(b1 & 0x3f) << 6) | (unsigned int)(b2 & 0x3f);
		if (scalar < 0x800 || (scalar >= 0xd800 && scalar <= 0xdfff)) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 4) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		unsigned char b3 = bytes[3];
		if ((b0 & 0xf8) != 0xf0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x07) << 18) | ((unsigned int)(b1 & 0x3f) << 12) | ((unsigned int)(b2 & 0x3f) << 6) | (unsigned int)(b3 & 0x3f);
		if (scalar < 0x10000 || scalar > 0x10ffff) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	return UTF8_REPLACEMENT;
}

static terse_codec_kind_t
codec_kind_from_name(const char *name)
{
	if (!name) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "UTF-8") == 0 || strcasecmp(name, "UTF8") == 0) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "SHIFT_JIS") == 0 || strcasecmp(name, "SHIFT-JIS") == 0 || strcasecmp(name, "Shift_JIS") == 0 || strcasecmp(name, "SJIS") == 0) {
		return TERSE_CODEC_SHIFT_JIS;
	}
	return TERSE_CODEC_UTF8;
}

terse_color_t
terse_color_default(void)
{
	terse_color_t color = { .kind = TERSE_COLOR_KIND_DEFAULT };
	return color;
}

terse_color_t
terse_color_basic(terse_basic_color_t color, int bright)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_BASIC16,
		.data.basic16 = {
			.color = color,
			.bright = bright ? 1 : 0,
		},
	};
	return result;
}

terse_color_t
terse_color_palette(unsigned char index)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_PALETTE256,
		.data.palette = {
			.value = index,
		},
	};
	return result;
}

terse_color_t
terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_TRUECOLOR,
		.data.truecolor = {
			.r = r,
			.g = g,
			.b = b,
		},
	};
	return result;
}

terse_style_t
terse_style_default(void)
{
	terse_style_t style = {
		.foreground = terse_color_default(),
		.background = terse_color_default(),
		.effects = 0,
	};
	return style;
}

void
update_effective_style(terse_handle_t handle)
{
	handle->effective_style = terse_style_make_effective(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}

/* Forward declarations for internal functions used by terse_style.c */
int write_literal(terse_handle_t handle, const char *literal);
int write_sequence(terse_handle_t handle, const char *sequence, size_t length);
void set_error(terse_handle_t handle, terse_error_t error);

static int
initialize_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return -1;
	}
	handle->codec_kind = codec_kind_from_name(handle->options.codec_name);
	handle->codec_to_utf8 = (iconv_t)-1;
	handle->utf8_to_codec = (iconv_t)-1;
	if (handle->codec_kind == TERSE_CODEC_SHIFT_JIS) {
		handle->codec_to_utf8 = iconv_open("UTF-8", "SHIFT_JIS");
		if (handle->codec_to_utf8 == (iconv_t)-1) {
			return -1;
		}
		handle->utf8_to_codec = iconv_open("SHIFT_JIS", "UTF-8");
		if (handle->utf8_to_codec == (iconv_t)-1) {
			iconv_close(handle->codec_to_utf8);
			handle->codec_to_utf8 = (iconv_t)-1;
			return -1;
		}
	}
	return 0;
}

static void
destroy_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (handle->codec_to_utf8 != (iconv_t)-1) {
		iconv_close(handle->codec_to_utf8);
		handle->codec_to_utf8 = (iconv_t)-1;
	}
	if (handle->utf8_to_codec != (iconv_t)-1) {
		iconv_close(handle->utf8_to_codec);
		handle->utf8_to_codec = (iconv_t)-1;
	}
}

#ifdef TERSE_ENABLE_TEST_MODE
void
record_call(terse_handle_t handle, terse_call_type_t type, const void *data, size_t data_size)
{
	if (!handle || !handle->test_state || !handle->test_state->recording) {
		return;
	}
	terse_test_state_t *ts = handle->test_state;
	if (ts->call_count >= ts->call_capacity) {
		int new_capacity = (ts->call_capacity == 0) ? 16 : (ts->call_capacity * 2);
		terse_call_record_t *new_calls = realloc(ts->calls, sizeof(terse_call_record_t) * (size_t)new_capacity);
		if (!new_calls) {
			return;
		}
		ts->calls = new_calls;
		ts->call_capacity = new_capacity;
	}
	terse_call_record_t *rec = &ts->calls[ts->call_count];
	memset(rec, 0, sizeof(*rec));
	rec->type = type;
	if (data && data_size > 0) {
		memcpy(&rec->data, data, data_size < sizeof(rec->data) ? data_size : sizeof(rec->data));
	}
	ts->call_count++;
}
#endif

static unsigned int
convert_shift_jis_pair(terse_handle_t handle, unsigned char lead, unsigned char trail)
{
	(void)lead;
	(void)trail;
	if (!handle || handle->codec_to_utf8 == (iconv_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	char inbuf[2];
	inbuf[0] = (char)lead;
	inbuf[1] = (char)trail;
	char *in_ptr = inbuf;
	size_t in_left = sizeof(inbuf);
	char outbuf[8] = { 0 };
	char *out_ptr = outbuf;
	size_t out_left = sizeof(outbuf);
	reset_iconv_state(handle->codec_to_utf8);
	if (iconv(handle->codec_to_utf8, &in_ptr, &in_left, &out_ptr, &out_left) == (size_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	if (in_left != 0) {
		return SHIFT_JIS_REPLACEMENT;
	}
	size_t produced = (size_t)(out_ptr - outbuf);
	return decode_utf8_bytes((const unsigned char *)outbuf, produced);
}


char *
base64_encode(const unsigned char *data, size_t length, size_t *out_len)
{
	if (!data || length == 0) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t encoded = ((length + 2) / 3) * 4;
	char *output = malloc(encoded + 1);
	if (!output) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t out_index = 0;
	for (size_t i = 0; i < length; i += 3) {
		unsigned int triple = data[i] << 16;
		if (i + 1 < length) {
			triple |= data[i + 1] << 8;
		}
		if (i + 2 < length) {
			triple |= data[i + 2];
		}
		output[out_index++] = BASE64_ALPHABET[(triple >> 18) & 0x3f];
		output[out_index++] = BASE64_ALPHABET[(triple >> 12) & 0x3f];
		if (i + 1 < length) {
			output[out_index++] = BASE64_ALPHABET[(triple >> 6) & 0x3f];
		} else {
			output[out_index++] = '=';
		}
		if (i + 2 < length) {
			output[out_index++] = BASE64_ALPHABET[triple & 0x3f];
		} else {
			output[out_index++] = '=';
		}
	}
	output[out_index] = '\0';
	if (out_len) {
		*out_len = out_index;
	}
	return output;
}

void
set_error(terse_handle_t handle, terse_error_t error)
{
	if (!handle) {
		return;
	}
	handle->last_error = error;
}

void
clear_error(terse_handle_t handle)
{
	set_error(handle, TERSE_OK);
}

static void emit_reset_sequences(terse_handle_t handle);

terse_error_t terse_validate_options(const terse_options_t *options)
{
	if (!options) {
		return 0;
	}
	if (options->input_fd < 0 || options->output_fd < 0) {
		errno = EBADF;
		return TERSE_ERR_INVALID_HANDLE;
	}
	return 0;
}

static terse_size_t
make_unknown_size(void)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
	return size;
}

static void
refresh_size(terse_handle_t handle)
{
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		handle->size = make_unknown_size();
		return;
	}
	terse_size_t size = terse_platform_query_fd_size(handle->options.output_fd);
	if (!size.known && handle->options.input_fd != handle->options.output_fd) {
		size = terse_platform_query_fd_size(handle->options.input_fd);
	}
	if (size.known || !handle->size.known) {
		handle->size = size;
	}
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile != TERSE_PROFILE_AUTO && (requested_profile < TERSE_P0 || requested_profile > TERSE_P3)) {
		return NULL;
	}
	if (terse_validate_options(options) != 0) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}
	memset(handle, 0, sizeof(*handle));

	// Initialize state stack
	handle->state_stack_top = -1;

	handle->requested_profile = requested_profile;
	handle->capabilities = terse_make_p0_capabilities();

	terse_options_t defaults = terse_platform_default_options();
	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = defaults.codec_name;
		}
	} else {
		handle->options = defaults;
	}
	if (initialize_codec_handles(handle) < 0) {
		int err = errno ? errno : EINVAL;
		destroy_codec_handles(handle);
		free(handle);
		errno = err;
		return NULL;
	}
	handle->size = make_unknown_size();
	handle->capabilities = detect_environment_capabilities(handle->requested_profile, &handle->options);
	handle->detected_capabilities = handle->capabilities;
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	handle->keyboard_supported = handle->capabilities.keyboard_features;
	handle->keyboard_enabled = 0;
	recompute_capabilities(handle);
	handle->cursor_visible = 1;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_known = 0;
	handle->style = terse_style_default();
	update_effective_style(handle);
	handle->mouse_mode = TERSE_MOUSE_NONE;
	handle->mouse_enabled = 0;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	handle->paste_enabled = 0;
	clear_error(handle);
	refresh_size(handle);
	handle->has_pending_byte = 0;

#ifdef TERSE_ENABLE_TEST_MODE
	handle->test_state = malloc(sizeof(terse_test_state_t));
	if (handle->test_state) {
		memset(handle->test_state, 0, sizeof(terse_test_state_t));
		handle->test_state->recording = 0;
		handle->test_state->calls = NULL;
		handle->test_state->call_count = 0;
		handle->test_state->call_capacity = 0;
		handle->test_state->mock_caps_enabled = 0;
		handle->test_state->mock_size_enabled = 0;
		handle->test_state->mock_events = NULL;
		handle->test_state->mock_event_count = 0;
		handle->test_state->mock_event_read_index = 0;
	}
#endif

	return handle;
}

void terse_close(terse_handle_t handle)
{
	if (handle) {
		if (handle->keyboard_enabled) {
			(void)terse_keyboard_disable(handle, handle->keyboard_enabled);
		}
#ifdef TERSE_ENABLE_TEST_MODE
		if (handle->test_state) {
			free(handle->test_state->calls);
			free(handle->test_state->mock_events);
			free(handle->test_state);
		}
#endif
	}
	emit_reset_sequences(handle);
	destroy_codec_handles(handle);
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return terse_make_p0_capabilities();
	}
#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_caps_enabled) {
		return handle->test_state->mock_caps;
	}
#endif
	if (!handle->size.known) {
		refresh_size(handle);
	}
	clear_error(handle);
	return handle->capabilities;
}

int
ensure_handle(terse_handle_t handle)
{
	if (!handle) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_HANDLE;
	}
	return 0;
}




int
write_literal(terse_handle_t handle, const char *literal)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!literal) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	int out = terse_platform_write_bytes(handle->options.output_fd, literal, strlen(literal));
	if (out < 0) {
		set_error(handle, TERSE_ERR_IO);
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

int
write_sequence(terse_handle_t handle, const char *sequence, size_t length)
{
	int out = terse_platform_write_bytes(handle->options.output_fd, sequence, length);
	if (out < 0) {
		set_error(handle, TERSE_ERR_IO);
		return handle->last_error;
	}
	clear_error(handle);
	return 0;
}

static void
emit_reset_sequences(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (!handle->capabilities.has_basic_output) {
		return;
	}
	if (handle->mouse_enabled) {
		(void)terse_disable_mouse(handle);
	}
	if (handle->paste_enabled) {
		(void)terse_disable_bracketed_paste(handle);
	}
	static const char *const cursor_on_seq = "\x1b[?25h";
	if (!handle->cursor_visible) {
		if (write_sequence(handle, cursor_on_seq, strlen(cursor_on_seq)) == 0) {
			handle->cursor_visible = 1;
		}
	}
	if (write_sequence(handle, TERSE_RESET_ALL_SEQ, sizeof(TERSE_RESET_ALL_SEQ) - 1) == 0) {
		handle->style = terse_style_default();
		update_effective_style(handle);
	}
}

terse_error_t terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!out_event) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}

#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_events) {
		if (handle->test_state->mock_event_read_index < handle->test_state->mock_event_count) {
			*out_event = handle->test_state->mock_events[handle->test_state->mock_event_read_index++];
			clear_error(handle);
			return TERSE_OK;
		} else {
			// All mock events have been read, return EAGAIN equivalent
			errno = EAGAIN;
			set_error(handle, TERSE_ERR_WOULD_BLOCK);
			return TERSE_ERR_WOULD_BLOCK;
		}
	}
#endif

#if defined(__human68k__) || defined(_WIN32)
	/* Human68k and Windows: Use platform-specific event reading implementation */
	return terse_platform_read_event(handle, timeout_ms, out_event);
#else
	/* POSIX: Use escape sequence parsing implementation */
	int fd = handle->options.input_fd;

	unsigned char first = 0;
	rc = terse_read_input_byte(handle, timeout_ms, &first);
	if (rc == 0) {
		clear_error(handle);
		return TERSE_ERR_NO_EVENT;
	}
	if (rc < 0) {
		// rc is negative terse_error_t, convert to positive
		set_error(handle, (terse_error_t)(-rc));
		return (terse_error_t)(-rc);
	}

	switch (first) {
	case '\r': {
		unsigned char next = 0;
		int peek = terse_read_input_byte(handle, 0, &next);
		if (peek < 0) {
			// peek is negative terse_error_t, convert to positive
			set_error(handle, (terse_error_t)(-peek));
			return (terse_error_t)(-peek);
		}
		if (peek > 0 && next == '\n') {
			// Consume \r\n as Enter
			terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
			clear_error(handle);
			return TERSE_OK;
		}
		// Treat \r alone as Enter, push back next byte if any
		if (peek > 0) {
			handle->pending_byte = next;
			handle->has_pending_byte = 1;
		}
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
		clear_error(handle);
		return TERSE_OK;
	}
	case '\n':
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, TERSE_MOD_CTRL);
		clear_error(handle);
		return TERSE_OK;
	case '\b':
	case 0x7f:
		terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		clear_error(handle);
		return TERSE_OK;
	case '\t':
		terse_set_key_event(out_event, TERSE_EVENT_TAB, 0);
		clear_error(handle);
		return TERSE_OK;
	default:
		break;
	}

	if (first >= 0x01 && first <= 0x1a) {
		unsigned int scalar = 'a' + (first - 1);
		terse_set_char_event(handle, out_event, scalar, TERSE_MOD_CTRL);
		return TERSE_OK;
	}

	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = terse_platform_drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);

		// Linux console function keys: ESC [ [ A through ESC [ [ L (F1-F12)
		// Check BEFORE CSI parsing to prevent misinterpreting as arrow keys
		if (len == 4 && seq[1] == '[' && seq[2] == '[') {
			char code = (char)seq[3];
			int fn = 0;
			if (code >= 'A' && code <= 'L') {
				fn = 1 + (code - 'A');  // A=F1, B=F2, ..., L=F12
			}
			if (fn > 0 && fn <= 12) {
				terse_set_function_event(out_event, fn, 0);
				clear_error(handle);
				return TERSE_OK;
			}
		}

		int values[8] = { 0 };
		size_t value_count = 0;
		char final = 0;
		if (terse_parse_csi_sequence(seq, len, values, 8, &value_count, &final) == 0) {
			if ((final == 'M' || final == 'm') && len > 2 && seq[2] == '<') {
				if (terse_handle_sgr_mouse_sequence(handle, out_event, values, value_count, final)) {
					return TERSE_OK;
				}
			}
			if (final == '~' && value_count >= 1 && handle->paste_enabled) {
				if (values[0] == 200) {
					out_event->type = TERSE_EVENT_PASTE_BEGIN;
					clear_error(handle);
					return TERSE_OK;
				}
				if (values[0] == 201) {
					out_event->type = TERSE_EVENT_PASTE_END;
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'u' && value_count >= 1) {
				unsigned int code = (unsigned int)values[0];
				int mods = 0;
				if (value_count >= 2) {
					mods = terse_mods_from_kitty_param(values[1]);
				}
				if (code == 13) {
					terse_set_key_event(out_event, TERSE_EVENT_ENTER, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code == 9) {
					terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code == 127) {
					terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (code >= 0x20 && code <= 0x10ffff) {
					terse_set_char_event(handle, out_event, code, mods);
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'Z') {
				int mods = TERSE_MOD_SHIFT;
				if (value_count > 0) {
					mods = terse_modifier_bits_from_param(values[value_count - 1]);
					if (!(mods & TERSE_MOD_SHIFT)) {
						mods |= TERSE_MOD_SHIFT;
					}
				}
				terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
				clear_error(handle);
				return TERSE_OK;
			}
			if (final == 't' && value_count >= 3 && values[0] == 8) {
				terse_set_resize_event(out_event, values[1], values[2]);
				handle->size.rows = values[1];
				handle->size.cols = values[2];
				handle->size.known = 1;
				handle->capabilities.has_size = 1;
				clear_error(handle);
				return TERSE_OK;
			}
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
				clear_error(handle);
				return TERSE_OK;
			}
			if (final == '~') {
				if (value_count == 0) {
					terse_set_raw_event(out_event, seq, len);
					clear_error(handle);
					return TERSE_OK;
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
					clear_error(handle);
					return TERSE_OK;
				case 4:
				case 8:
					terse_set_key_event(out_event, TERSE_EVENT_END, mods);
					clear_error(handle);
					return TERSE_OK;
				case 2:
					terse_set_key_event(out_event, TERSE_EVENT_INSERT, mods);
					clear_error(handle);
					return TERSE_OK;
				case 3:
					terse_set_key_event(out_event, TERSE_EVENT_DELETE, mods);
					clear_error(handle);
					return TERSE_OK;
				case 5:
					terse_set_key_event(out_event, TERSE_EVENT_PAGE_UP, mods);
					clear_error(handle);
					return TERSE_OK;
				case 6:
					terse_set_key_event(out_event, TERSE_EVENT_PAGE_DOWN, mods);
					clear_error(handle);
					return TERSE_OK;
				case 9:
					terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_OK;
				case 13:
					if (values[0] == 27 && value_count >= 3) {
						terse_set_key_event(out_event, TERSE_EVENT_ENTER, terse_modifier_bits_from_param(values[1]));
						clear_error(handle);
						return TERSE_OK;
					}
					break;
				default:
					break;
				}
				int fn = terse_function_number_from_code(key_code);
				if (fn > 0) {
					terse_set_function_event(out_event, fn, mods);
					clear_error(handle);
					return TERSE_OK;
				}
				if (key_code >= 0 && key_code <= 0x10ffff) {
					if (key_code == 9) {
						terse_set_key_event(out_event, TERSE_EVENT_TAB, mods);
						clear_error(handle);
						return TERSE_OK;
					}
					if (key_code == 8 || key_code == 127) {
						terse_set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
						clear_error(handle);
						return TERSE_OK;
					}
					terse_set_char_event(handle, out_event, (unsigned int)key_code, mods);
					clear_error(handle);
					return TERSE_OK;
				}
			}
			if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
				int mods = 0;
				if (value_count > 0) {
					mods = terse_modifier_bits_from_param(values[value_count - 1]);
				}
				switch (final) {
				case 'A':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'B':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'C':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
					clear_error(handle);
					return TERSE_OK;
				case 'D':
					terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
					clear_error(handle);
					return TERSE_OK;
				default:
					break;
				}
			}
		}
		if (len >= 3 && seq[1] == 'O') {
			int mods = 0;
			char code = (char)seq[2];
			switch (code) {
			case 'A':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'B':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'C':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'D':
				terse_set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'H':
				terse_set_key_event(out_event, TERSE_EVENT_HOME, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'F':
				terse_set_key_event(out_event, TERSE_EVENT_END, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'P':
				terse_set_function_event(out_event, 1, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'Q':
				terse_set_function_event(out_event, 2, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'R':
				terse_set_function_event(out_event, 3, mods);
				clear_error(handle);
				return TERSE_OK;
			case 'S':
				terse_set_function_event(out_event, 4, mods);
				clear_error(handle);
				return TERSE_OK;
			default:
				break;
			}
		}
		if (terse_handle_escape_prefixed_char(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}
		terse_set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_OK;
	}

	if (first >= 0x20 || (handle->codec_kind == TERSE_CODEC_SHIFT_JIS && first >= 0x80)) {
		int decode_rc = terse_decode_stream_char(handle, fd, first, out_event);
		if (decode_rc == 0) {
			clear_error(handle);
			return TERSE_OK;
		}
		if (decode_rc < 0) {
			return decode_rc;
		}
	}

	unsigned char raw_bytes[1] = { first };
	terse_set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_OK;
#endif /* !__human68k__ */
}

terse_size_t
terse_get_size(terse_handle_t handle)
{
	terse_size_t unknown = make_unknown_size();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
#ifdef TERSE_ENABLE_TEST_MODE
	if (handle->test_state && handle->test_state->mock_size_enabled) {
		terse_size_t mock_size;
		mock_size.rows = handle->test_state->mock_rows;
		mock_size.cols = handle->test_state->mock_cols;
		mock_size.known = 1;
		return mock_size;
	}
#endif
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		clear_error(handle);
		return unknown;
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	if (!(handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE)) {
		handle->capabilities.has_size = handle->size.known;
	}
	clear_error(handle);
	return handle->size;
}

static terse_cursor_position_t
make_unknown_cursor_position(void)
{
	terse_cursor_position_t pos = {0, 0, 0};
	return pos;
}

terse_cursor_position_t
terse_get_cursor_position(terse_handle_t handle)
{
	terse_cursor_position_t unknown = make_unknown_cursor_position();
	if (ensure_handle(handle) != 0) {
		return unknown;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return unknown;
	}

	int row = 0, col = 0;
	int rc = terse_platform_query_cursor_position(handle->options.input_fd,
	                                              handle->options.output_fd,
	                                              &row, &col);
	if (rc != 0) {
		// rc is already a terse_error_t
		set_error(handle, (terse_error_t)rc);
		return unknown;
	}

	terse_cursor_position_t pos = {row, col, 1};
	clear_error(handle);
	return pos;
}

terse_error_t terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	int rc = ensure_handle(handle);
	if (rc != 0) {
		return rc;
	}
	if (!out_options) {
		errno = EINVAL;
		set_error(handle, TERSE_ERR_INVALID_ARGUMENT);
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	*out_options = handle->options;
	clear_error(handle);
	return 0;
}

terse_error_t
terse_get_last_error(terse_handle_t handle)
{
	if (!handle) {
		return TERSE_ERR_INVALID_HANDLE;
	}
	return handle->last_error;
}


#ifdef TERSE_ENABLE_TEST_MODE

int terse_test_start_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 1;
	return 0;
}

int terse_test_stop_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 0;
	return 0;
}

const terse_call_record_t *terse_test_get_calls(terse_handle_t handle, int *out_count)
{
	if (!handle || !handle->test_state || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return NULL;
	}
	*out_count = handle->test_state->call_count;
	return handle->test_state->calls;
}

int terse_test_clear_calls(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->call_count = 0;
	return 0;
}

int terse_test_mock_capabilities(terse_handle_t handle, const terse_capabilities_t *caps)
{
	if (!handle || !handle->test_state || !caps) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps = *caps;
	handle->test_state->mock_caps_enabled = 1;
	return 0;
}

int terse_test_mock_size(terse_handle_t handle, int rows, int cols)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_rows = rows;
	handle->test_state->mock_cols = cols;
	handle->test_state->mock_size_enabled = 1;
	return 0;
}

int terse_test_mock_events(terse_handle_t handle, const terse_event_t *events, int count)
{
	if (!handle || !handle->test_state || !events || count < 0) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;

	if (count > 0) {
		handle->test_state->mock_events = malloc(sizeof(terse_event_t) * (size_t)count);
		if (!handle->test_state->mock_events) {
			errno = ENOMEM;
			return TERSE_ERR_OUT_OF_MEMORY;
		}
		memcpy(handle->test_state->mock_events, events, sizeof(terse_event_t) * (size_t)count);
		handle->test_state->mock_event_count = count;
	}
	return 0;
}

int terse_test_reset_mocks(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps_enabled = 0;
	handle->test_state->mock_size_enabled = 0;
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;
	return 0;
}

#endif // TERSE_ENABLE_TEST_MODE
