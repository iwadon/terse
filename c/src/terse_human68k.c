#include "terse_codec.h"
#include "terse_event_helpers.h"
#include "terse_internal.h"
#include "terse_platform.h"

#include <errno.h>
#include <string.h>
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

	if (len == 0) {
		return 0;
	}

	/* Use IOCS for fast bulk output by creating null-terminated string */
	/* Stack buffer size is chosen to handle most common cases efficiently */
	#define WRITE_BUFFER_SIZE 512

	if (len < WRITE_BUFFER_SIZE) {
		/* Fast path: use stack buffer and IOCS for bulk output */
		char buf[WRITE_BUFFER_SIZE];
		memcpy(buf, bytes, len);
		buf[len] = '\0';
		_iocs_b_print(buf);
	} else {
		/* Long output: split into chunks */
		size_t remaining = len;
		const char *ptr = bytes;

		while (remaining > 0) {
			size_t chunk_size = (remaining < WRITE_BUFFER_SIZE - 1)
			                    ? remaining
			                    : WRITE_BUFFER_SIZE - 1;
			char buf[WRITE_BUFFER_SIZE];
			memcpy(buf, ptr, chunk_size);
			buf[chunk_size] = '\0';
			_iocs_b_print(buf);

			ptr += chunk_size;
			remaining -= chunk_size;
		}
	}

	#undef WRITE_BUFFER_SIZE
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
	static const terse_event_type_t scancode_event_types[256] = {
		[0x01] = TERSE_EVENT_RAW_SEQUENCE, /* ESC - handled separately */
		[0x02] = TERSE_EVENT_CHAR,		   /* 1! */
		[0x03] = TERSE_EVENT_CHAR,		   /* 2" */
		[0x04] = TERSE_EVENT_CHAR,		   /* 3# */
		[0x05] = TERSE_EVENT_CHAR,		   /* 4$ */
		[0x06] = TERSE_EVENT_CHAR,		   /* 5% */
		[0x07] = TERSE_EVENT_CHAR,		   /* 6& */
		[0x08] = TERSE_EVENT_CHAR,		   /* 7' */
		[0x09] = TERSE_EVENT_CHAR,		   /* 8( */
		[0x0a] = TERSE_EVENT_CHAR,		   /* 9) */
		[0x0b] = TERSE_EVENT_CHAR,		   /* 0 */
		[0x0c] = TERSE_EVENT_CHAR,		   /* -= */
		[0x0d] = TERSE_EVENT_CHAR,		   /* ^~ */
		[0x0e] = TERSE_EVENT_CHAR,		   /* \| */
		[0x0f] = TERSE_EVENT_BACKSPACE,	   /* BS */
		[0x10] = TERSE_EVENT_TAB,		   /* TAB */
		[0x11] = TERSE_EVENT_CHAR,		   /* Q */
		[0x12] = TERSE_EVENT_CHAR,		   /* W */
		[0x13] = TERSE_EVENT_CHAR,		   /* E */
		[0x14] = TERSE_EVENT_CHAR,		   /* R */
		[0x15] = TERSE_EVENT_CHAR,		   /* T */
		[0x16] = TERSE_EVENT_CHAR,		   /* Y */
		[0x17] = TERSE_EVENT_CHAR,		   /* U */
		[0x18] = TERSE_EVENT_CHAR,		   /* I */
		[0x19] = TERSE_EVENT_CHAR,		   /* O */
		[0x1a] = TERSE_EVENT_CHAR,		   /* P */
		[0x1b] = TERSE_EVENT_CHAR,		   /* @` */
		[0x1c] = TERSE_EVENT_CHAR,		   /* [{ */
		[0x1d] = TERSE_EVENT_ENTER,		   /* CR */
		[0x1e] = TERSE_EVENT_CHAR,		   /* A */
		[0x1f] = TERSE_EVENT_CHAR,		   /* S */
		[0x20] = TERSE_EVENT_CHAR,		   /* D */
		[0x21] = TERSE_EVENT_CHAR,		   /* F */
		[0x22] = TERSE_EVENT_CHAR,		   /* G */
		[0x23] = TERSE_EVENT_CHAR,		   /* H */
		[0x24] = TERSE_EVENT_CHAR,		   /* J */
		[0x25] = TERSE_EVENT_CHAR,		   /* K */
		[0x26] = TERSE_EVENT_CHAR,		   /* L */
		[0x27] = TERSE_EVENT_CHAR,		   /* ;+ */
		[0x28] = TERSE_EVENT_CHAR,		   /* :* */
		[0x29] = TERSE_EVENT_CHAR,		   /* ]} */
		[0x2a] = TERSE_EVENT_CHAR,		   /* Z */
		[0x2b] = TERSE_EVENT_CHAR,		   /* X */
		[0x2c] = TERSE_EVENT_CHAR,		   /* C */
		[0x2d] = TERSE_EVENT_CHAR,		   /* V */
		[0x2e] = TERSE_EVENT_CHAR,		   /* B */
		[0x2f] = TERSE_EVENT_CHAR,		   /* N */
		[0x30] = TERSE_EVENT_CHAR,		   /* M */
		[0x31] = TERSE_EVENT_CHAR,		   /* ,< */
		[0x32] = TERSE_EVENT_CHAR,		   /* .> */
		[0x33] = TERSE_EVENT_CHAR,		   /* /? */
		[0x34] = TERSE_EVENT_CHAR,		   /* _ */
		[0x35] = TERSE_EVENT_CHAR,		   /* SPACE */
		[0x36] = TERSE_EVENT_HOME,		   /* HOME */
		[0x37] = TERSE_EVENT_DELETE,	   /* DEL */
		[0x38] = TERSE_EVENT_PAGE_UP,	   /* R_UP */
		[0x39] = TERSE_EVENT_PAGE_DOWN,	   /* R_DOWN */
		[0x3a] = TERSE_EVENT_RAW_SEQUENCE, /* UNDO */
		[0x3b] = TERSE_EVENT_ARROW_LEFT,   /* ← */
		[0x3c] = TERSE_EVENT_ARROW_UP,	   /* ↑ */
		[0x3d] = TERSE_EVENT_ARROW_RIGHT,  /* → */
		[0x3e] = TERSE_EVENT_ARROW_DOWN,   /* ↓ */
		[0x3f] = TERSE_EVENT_HOME,		   /* CLR */
		[0x40] = TERSE_EVENT_CHAR,		   /* / */
		[0x41] = TERSE_EVENT_CHAR,		   /* * */
		[0x42] = TERSE_EVENT_CHAR,		   /* - */
		[0x43] = TERSE_EVENT_CHAR,		   /* 7 */
		[0x44] = TERSE_EVENT_CHAR,		   /* 8 */
		[0x45] = TERSE_EVENT_CHAR,		   /* 9 */
		[0x46] = TERSE_EVENT_CHAR,		   /* + */
		[0x47] = TERSE_EVENT_CHAR,		   /* 4 */
		[0x48] = TERSE_EVENT_CHAR,		   /* 5 */
		[0x49] = TERSE_EVENT_CHAR,		   /* 6 */
		[0x4a] = TERSE_EVENT_CHAR,		   /* = */
		[0x4b] = TERSE_EVENT_CHAR,		   /* 1 */
		[0x4c] = TERSE_EVENT_CHAR,		   /* 2 */
		[0x4d] = TERSE_EVENT_CHAR,		   /* 3 */
		[0x4e] = TERSE_EVENT_ENTER,		   /* ENTER */
		[0x4f] = TERSE_EVENT_CHAR,		   /* 0 */
		[0x50] = TERSE_EVENT_CHAR,		   /* , */
		[0x51] = TERSE_EVENT_CHAR,		   /* . */
		[0x52] = TERSE_EVENT_RAW_SEQUENCE, /* 記号 */
		[0x53] = TERSE_EVENT_RAW_SEQUENCE, /* 登録 */
		[0x54] = TERSE_EVENT_RAW_SEQUENCE, /* HELP */
		[0x55] = TERSE_EVENT_RAW_SEQUENCE, /* XF1 */
		[0x56] = TERSE_EVENT_RAW_SEQUENCE, /* XF2 */
		[0x57] = TERSE_EVENT_RAW_SEQUENCE, /* XF3 */
		[0x58] = TERSE_EVENT_RAW_SEQUENCE, /* XF4 */
		[0x59] = TERSE_EVENT_RAW_SEQUENCE, /* XF5 */
		[0x5a] = TERSE_EVENT_RAW_SEQUENCE, /* かな */
		[0x5b] = TERSE_EVENT_RAW_SEQUENCE, /* ローマ字 */
		[0x5c] = TERSE_EVENT_RAW_SEQUENCE, /* コード入力 */
		[0x5d] = TERSE_EVENT_RAW_SEQUENCE, /* CAPS */
		[0x5e] = TERSE_EVENT_RAW_SEQUENCE, /* INS */
		[0x5f] = TERSE_EVENT_RAW_SEQUENCE, /* ひらがな */
		[0x60] = TERSE_EVENT_RAW_SEQUENCE, /* 全角 */
		[0x61] = TERSE_EVENT_RAW_SEQUENCE, /* BREAK */
		[0x62] = TERSE_EVENT_RAW_SEQUENCE, /* COPY */
		[0x63] = TERSE_EVENT_FUNCTION,	   /* F1 */
		[0x64] = TERSE_EVENT_FUNCTION,	   /* F2 */
		[0x65] = TERSE_EVENT_FUNCTION,	   /* F3 */
		[0x66] = TERSE_EVENT_FUNCTION,	   /* F4 */
		[0x67] = TERSE_EVENT_FUNCTION,	   /* F5 */
		[0x68] = TERSE_EVENT_FUNCTION,	   /* F6 */
		[0x69] = TERSE_EVENT_FUNCTION,	   /* F7 */
		[0x6a] = TERSE_EVENT_FUNCTION,	   /* F8 */
		[0x6b] = TERSE_EVENT_FUNCTION,	   /* F9 */
		[0x6c] = TERSE_EVENT_FUNCTION,	   /* F10 */
		[0x70] = TERSE_EVENT_RAW_SEQUENCE, /* SHIFT */
		[0x71] = TERSE_EVENT_RAW_SEQUENCE, /* CTRL */
		[0x72] = TERSE_EVENT_RAW_SEQUENCE, /* OPT.1 */
		[0x73] = TERSE_EVENT_RAW_SEQUENCE, /* OPT.2 */
		[0x74] = TERSE_EVENT_RAW_SEQUENCE, /* NUM */
	};

	if (scancode < 256) {
		return scancode_event_types[scancode];
	}
	return TERSE_EVENT_RAW_SEQUENCE;
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
	terse_event_type_t event_type = scancode_to_event_type(scancode);

	/* Handle special keys */
	if (event_type != TERSE_EVENT_CHAR) {
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

/* ========================================================================
 * Platform-specific fast path optimizations for Human68k
 * ======================================================================== */

terse_error_t
terse_platform_move_to_fast(terse_handle_t handle, int row, int col)
{
	(void)handle; /* Human68k doesn't use handle for console I/O */

	/* Use IOCS directly for maximum performance */
	/* Note: _iocs_b_locate takes (x, y) in that order */
	_iocs_b_locate(col, row);

	return TERSE_OK;
}

terse_error_t
terse_platform_clear_screen_fast(terse_handle_t handle, terse_clear_mode_t mode)
{
	(void)handle; /* Human68k doesn't use handle for console I/O */

	switch (mode) {
	case TERSE_CLEAR_ALL:
		/* Clear entire screen using IOCS */
		_iocs_b_clr_al();
		break;

	case TERSE_CLEAR_AFTER:
		/* Clear from cursor to end of screen using IOCS */
		_iocs_b_clr_ed();
		break;

	case TERSE_CLEAR_BEFORE:
		/* Clear from start of screen to cursor using IOCS */
		_iocs_b_clr_st();
		break;

	default:
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	return TERSE_OK;
}
