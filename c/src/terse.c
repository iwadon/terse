#include "terse.h"
#include "terse_handle.h"
#include "terse_platform.h"
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test_internal.h"
#endif
#include "terse_capabilities.h"
#include "terse_codec.h"
#include "terse_detection.h"
#include "terse_event_helpers.h"
#include "terse_input.h"
#include "terse_keyboard.h"
#include "terse_output.h"
#include "terse_state.h"
#include "terse_style.h"
#include "terse_unicode.h"

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
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
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

/* Common reset sequences - shared via terse_handle.h extern declarations */
const char TERSE_RESET_ALL_SEQ[] = "\x1b[0m";
const char TERSE_RESET_COLOR_SEQ[] = "\x1b[39;49m";
const char TERSE_RESET_EFFECTS_SEQ[] = "\x1b[22;23;24;27;29m";

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

void update_effective_style(terse_handle_t handle)
{
	handle->effective_style = terse_style_make_effective(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}

/* Forward declarations for internal functions */
int write_literal(terse_handle_t handle, const char *literal);
int write_sequence(terse_handle_t handle, const char *sequence, size_t length);

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

void set_error(terse_handle_t handle, terse_error_t error)
{
	if (!handle) {
		return;
	}
	handle->last_error = error;
}

void clear_error(terse_handle_t handle)
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
	terse_test_state_init(handle);
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
		terse_test_state_destroy(handle);
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

int ensure_handle(terse_handle_t handle)
{
	if (!handle) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_HANDLE;
	}
	return 0;
}

int write_literal(terse_handle_t handle, const char *literal)
{
	TERSE_CHECK_HANDLE(handle);
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

int write_sequence(terse_handle_t handle, const char *sequence, size_t length)
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
	if (write_sequence(handle, TERSE_RESET_ALL_SEQ, TERSE_RESET_ALL_SEQ_LEN) == 0) {
		handle->style = terse_style_default();
		update_effective_style(handle);
	}
}

terse_error_t terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	TERSE_CHECK_HANDLE(handle);
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
			errno = EAGAIN;
			set_error(handle, TERSE_ERR_WOULD_BLOCK);
			return TERSE_ERR_WOULD_BLOCK;
		}
	}
#endif

#if defined(__human68k__) || defined(_WIN32)
	return terse_platform_read_event(handle, timeout_ms, out_event);
#else
	int fd = handle->options.input_fd;

	unsigned char first = 0;
	int rc = terse_read_input_byte(handle, timeout_ms, &first);
	if (rc == 0) {
		clear_error(handle);
		return TERSE_ERR_NO_EVENT;
	}
	if (rc < 0) {
		set_error(handle, (terse_error_t)(-rc));
		return (terse_error_t)(-rc);
	}

	/* Handle CR/LF specially for Enter key detection */
	if (first == '\r') {
		unsigned char next = 0;
		int peek = terse_read_input_byte(handle, 0, &next);
		if (peek < 0) {
			set_error(handle, (terse_error_t)(-peek));
			return (terse_error_t)(-peek);
		}
		if (peek > 0 && next == '\n') {
			terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
			clear_error(handle);
			return TERSE_OK;
		}
		if (peek > 0) {
			handle->pending_byte = next;
			handle->has_pending_byte = 1;
		}
		terse_set_key_event(out_event, TERSE_EVENT_ENTER, 0);
		clear_error(handle);
		return TERSE_OK;
	}

	/* Handle other control characters */
	if (terse_parse_control_char(handle, out_event, first)) {
		clear_error(handle);
		return TERSE_OK;
	}

	/* Handle escape sequences */
	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = terse_platform_drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);

		/* Try Linux console function keys first (ESC [ [ A-L) */
		if (terse_parse_linux_console_fkey(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}

		/* Try CSI sequences (ESC [ ...) */
		if (terse_parse_csi_event(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}

		/* Try SS3 sequences (ESC O ...) */
		if (terse_parse_ss3_event(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}

		/* Try Alt+key combinations (ESC + char) */
		if (terse_handle_escape_prefixed_char(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_OK;
		}

		/* Unrecognized escape sequence */
		terse_set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_OK;
	}

	/* Handle printable characters and multi-byte sequences */
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

	/* Unknown byte: return as raw */
	unsigned char raw_bytes[1] = { first };
	terse_set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_OK;
#endif
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
	terse_cursor_position_t pos = { 0, 0, 0 };
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

	terse_cursor_position_t pos = { row, col, 1 };
	clear_error(handle);
	return pos;
}

terse_error_t terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	TERSE_CHECK_HANDLE(handle);
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
