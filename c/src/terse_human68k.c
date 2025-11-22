#include "terse_codec.h"
#include "terse_event_helpers.h"
#include "terse_internal.h"
#include "terse_platform.h"

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>

terse_options_t terse_platform_default_options(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "Shift_JIS", /* Human68k is Shift_JIS native */
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	return options;
}

terse_size_t terse_platform_query_fd_size(int fd)
{
	(void)fd; /* Human68k doesn't use fd for console I/O */

	/* Get current screen mode */
	terse_size_t size;
	int mode = _dos_c_width(-1);
	switch (mode) {
	case 0: /* 768x512 no graphics _CRTMOD=16 */
	case 1: /* 768x512 16-color graphics _CRTMOD=16 */
		size = (terse_size_t) { .rows = 32, .cols = 96, .known = 1 };
		break;
	case 2: /* 512x512 no graphics _CRTMOD=4 */
	case 3: /* 512x512 16-color graphics _CRTMOD=4 */
	case 4: /* 512x512 256-color graphics _CRTMOD=8 */
	case 5: /* 512x512 65536-color graphics _CRTMOD=12 */
		size = (terse_size_t) { .rows = 32, .cols = 64, .known = 1 };
		break;
	default:
		/* Return default for unknown mode */
		size = (terse_size_t) { .rows = 32, .cols = 96, .known = 1 };
	}

	int fnkmod = _dos_c_fnkmod(-1);
	/* If fnkmod is 3, it's 32-row mode; otherwise, it's 31-row mode. */
	if (fnkmod != 3) {
		size.rows = 31;
	}

	return size;
}

size_t terse_platform_probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity)
{
	(void)input_fd;
	(void)output_fd;
	(void)buffer;
	(void)capacity;
	return 0;
}

terse_error_t terse_platform_query_cursor_position(int input_fd, int output_fd, int *out_row, int *out_col)
{
	(void)input_fd; /* Human68k doesn't use fd for console I/O */
	(void)output_fd;

	if (!out_row || !out_col) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	/* Get cursor position */
	int result = _dos_c_locate(-1, 0);
	if (result == -1) {
		errno = EIO;
		return TERSE_ERR_IO;
	}

	/* Extract position from packed result */
	int col = (result >> 16) & 0xffff; /* Upper 16 bits: column */
	int row = result & 0xffff;		   /* Lower 16 bits: row */

	/* _dos_c_locate returns 0-based coordinates (already correct) */
	*out_row = row;
	*out_col = col;
	return 0;
}

terse_error_t terse_platform_wait_for_input(int fd, int timeout_ms)
{
	(void)fd; /* Human68k doesn't use fd for console I/O */

	if (timeout_ms < 0) {
		/* Infinite wait - block until input available */
		while (_iocs_b_keysns() == 0) {
			/* Busy wait - could add sleep here if available */
			usleep(10000); /* Sleep 10ms to reduce CPU usage */
		}
		return 1; /* Input available */
	}

	/* Timeout specified - poll with sleep intervals */
	int elapsed = 0;
	const int poll_interval = 10; /* Poll every 10ms */

	while (elapsed < timeout_ms) {
		if (_iocs_b_keysns() != 0) {
			return 1; /* Input available */
		}
		usleep(poll_interval * 1000);
		elapsed += poll_interval;
	}

	return 0; /* Timeout with no input */
}

ssize_t terse_platform_read_byte(int fd, unsigned char *out)
{
	(void)fd; /* Human68k doesn't use fd for console I/O */

	/* Use DOS _INKEY (no break check, no echo) */
	int ch = _dos_inkey();
	*out = (unsigned char)(ch & 0xff);
	return 1;
}

size_t terse_platform_drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	(void)fd;
	(void)buffer;
	(void)max;
	return 0;
}

terse_error_t terse_platform_write_bytes(int fd, const char *bytes, size_t len)
{
	(void)fd; /* Human68k doesn't use fd for console I/O */

	/* Output bytes one at a time using DOS _PUTCHAR */
	for (size_t i = 0; i < len; i++) {
		_dos_c_putc(bytes[i]);
	}

	return 0; /* Always succeeds */
}

/* Check if scancode represents a modifier key */
static int
is_modifier_key(unsigned int scancode)
{
	return scancode == 0x70 /* SHIFT */
		|| scancode == 0x71 /* CTRL */
		|| scancode == 0x72 /* OPT.1 (Alt) */
		|| scancode == 0x73 /* OPT.2 (AltGr) */
		|| scancode == 0x5a /* かな */
		|| scancode == 0x5b /* ローマ字 */
		|| scancode == 0x5c /* コード入力 */
		|| scancode == 0x5d /* CAPS */
		|| scancode == 0x5e /* INS */
		|| scancode == 0x5f /* ひらがな */
		|| scancode == 0x60 /* 全角 */
		;
}

/* Map Human68k scancode to terse event type */
static terse_event_type_t
scancode_to_event_type(unsigned int scancode)
{
	switch (scancode) {
	case 0x01:
		return TERSE_EVENT_RAW_SEQUENCE; /* ESC - handled separately */
	case 0x0f:
		return TERSE_EVENT_BACKSPACE;
	case 0x10:
		return TERSE_EVENT_TAB;
	case 0x1d:
		return TERSE_EVENT_ENTER; /* Enter (full) */
	case 0x36:
		return TERSE_EVENT_HOME;
	case 0x37:
		return TERSE_EVENT_DELETE;
	case 0x38:
		return TERSE_EVENT_PAGE_UP; /* Roll Up */
	case 0x39:
		return TERSE_EVENT_PAGE_DOWN; /* Roll Down */
	case 0x3a:
		return TERSE_EVENT_RAW_SEQUENCE; /* UNDO - no standard mapping */
	case 0x3b:
		return TERSE_EVENT_ARROW_RIGHT;
	case 0x3c:
		return TERSE_EVENT_ARROW_UP;
	case 0x3d:
		return TERSE_EVENT_ARROW_LEFT;
	case 0x3e:
		return TERSE_EVENT_ARROW_DOWN;
	case 0x3f:
		return TERSE_EVENT_HOME; /* CLR */
	case 0x4e:
		return TERSE_EVENT_ENTER; /* Enter (numeric) */
	case 0x63:
		return TERSE_EVENT_FUNCTION; /* F1 */
	case 0x64:
		return TERSE_EVENT_FUNCTION; /* F2 */
	case 0x65:
		return TERSE_EVENT_FUNCTION; /* F3 */
	case 0x66:
		return TERSE_EVENT_FUNCTION; /* F4 */
	case 0x67:
		return TERSE_EVENT_FUNCTION; /* F5 */
	case 0x68:
		return TERSE_EVENT_FUNCTION; /* F6 */
	case 0x69:
		return TERSE_EVENT_FUNCTION; /* F7 */
	case 0x6a:
		return TERSE_EVENT_FUNCTION; /* F8 */
	case 0x6b:
		return TERSE_EVENT_FUNCTION; /* F9 */
	case 0x6c:
		return TERSE_EVENT_FUNCTION; /* F10 */
	default:
		return TERSE_EVENT_RAW_SEQUENCE;
	}
}

/* Get function key number from scancode */
static int
scancode_to_function_number(unsigned int scancode)
{
	if (scancode >= 0x63 && scancode <= 0x6c) {
		return (int)(scancode - 0x63 + 1); /* F1=1, F2=2, ..., F10=10 */
	}
	return 0;
}

/* Convert IOCS modifier flags to terse modifiers */
static int
convert_modifiers(unsigned int iocs_mods)
{
	int mods = 0;
	if (iocs_mods & 0x01) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (iocs_mods & 0x02) {
		mods |= TERSE_MOD_CTRL;
	}
	if (iocs_mods & 0x04) {
		mods |= TERSE_MOD_ALT; /* OPT.1 */
	}
	/* OPT.2 (bit 3) is not mapped to standard modifier */
	return mods;
}

/* Human68k-specific event reading implementation */
terse_error_t
terse_platform_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	if (!handle || !out_event) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	/* Wait for input */
	int wait_result = terse_platform_wait_for_input(handle->options.input_fd, timeout_ms);
	if (wait_result == 0) {
		return TERSE_ERR_WOULD_BLOCK; /* Timeout */
	}
	if (wait_result < 0) {
		return TERSE_ERR_IO;
	}

	/* Get key information via IOCS (includes scancode + ASCII code) */
	int keysns = _iocs_b_keysns();
	if (keysns == 0) {
		return TERSE_ERR_WOULD_BLOCK; /* Spurious wakeup */
	}

	unsigned int scancode = (keysns >> 8) & 0x7f;
	unsigned int ascii_code = keysns & 0xff;

	/* Handle special keys (ASCII code = 0) */
	if (ascii_code == 0 || keysns == 0x11d0d) { // 0x11d0d = Enter key
		/* Consume the key from buffer */
		_iocs_b_keyinp();

		/* Ignore modifier key presses */
		if (is_modifier_key(scancode)) {
			/* Recursively read next event */
			return terse_platform_read_event(handle, timeout_ms, out_event);
		}

		/* Get modifier state */
		unsigned int iocs_mods = _iocs_b_sftsns();
		int mods = convert_modifiers(iocs_mods);

		/* Map scancode to event type */
		terse_event_type_t event_type = scancode_to_event_type(scancode);

		if (event_type == TERSE_EVENT_FUNCTION) {
			int fn_number = scancode_to_function_number(scancode);
			terse_set_function_event(out_event, fn_number, mods);
			return TERSE_OK;
		} else if (event_type == TERSE_EVENT_RAW_SEQUENCE) {
			/* Unrecognized special key - return scancode as raw sequence */
			unsigned char raw[2] = { (unsigned char)(keysns >> 8), 0 };
			terse_set_raw_event(out_event, raw, 1);
			return TERSE_OK;
		} else {
			terse_set_key_event(out_event, event_type, mods);
			return TERSE_OK;
		}
	}

	/* Regular character input */
	unsigned char buf[3] = { 0 };
	buf[0] = _dos_inkey();
	int len = 1;

	/* Check for Shift_JIS 2-byte character */
	if (terse_is_shift_jis_lead_byte(buf[0])) {
		buf[1] = _dos_inkey();
		len = 2;
	}

	/* Get modifier state */
	unsigned int iocs_mods = _iocs_b_sftsns();
	int mods = convert_modifiers(iocs_mods);

	/* Decode Shift_JIS to Unicode */
	unsigned int scalar = 0;
	if (len == 1) {
		if (buf[0] <= 0x7f) {
			scalar = buf[0];
		} else if (buf[0] >= 0xa1 && buf[0] <= 0xdf) {
			/* Half-width katakana */
			scalar = 0xff61u + (unsigned int)(buf[0] - 0xa1);
		} else {
			scalar = TERSE_SHIFT_JIS_REPLACEMENT;
		}
	} else {
		/* Convert 2-byte Shift_JIS to Unicode using iconv */
		scalar = terse_convert_shift_jis_pair(handle, buf[0], buf[1]);
	}

	/* Create character event */
	terse_set_char_event(handle, out_event, scalar, mods);
	return TERSE_OK;
}
