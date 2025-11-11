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
	int mode = _dos_c_width(-1);
	switch (mode) {
	case 0: /* 768x512 no graphics _CRTMOD=16 */
	case 1: /* 768x512 16-color graphics _CRTMOD=16 */
		return (terse_size_t) { .rows = 32, .cols = 96, .known = 1 };
	case 2: /* 512x512 no graphics _CRTMOD=4 */
	case 3: /* 512x512 16-color graphics _CRTMOD=4 */
	case 4: /* 512x512 256-color graphics _CRTMOD=8 */
	case 5: /* 512x512 65536-color graphics _CRTMOD=12 */
		return (terse_size_t) { .rows = 32, .cols = 64, .known = 1 };
	default:
		/* Return default for unknown mode */
		return (terse_size_t) { .rows = 31, .cols = 96, .known = 1 };
	}
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
#if 0
	int result = _dos_c_locate(-1, 0);
	if (result == -1) {
		errno = EIO;
		return TERSE_ERR_IO;
	}
#else
	int result = _iocs_b_locate(-1, 0);
	if (result == -1) {
		errno = EIO;
		return TERSE_ERR_IO;
	}
#endif

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
		while (_dos_keysns() == 0) {
			/* Busy wait - could add sleep here if available */
			usleep(10000); /* Sleep 10ms to reduce CPU usage */
		}
		return 1; /* Input available */
	}

	/* Timeout specified - poll with sleep intervals */
	int elapsed = 0;
	const int poll_interval = 10; /* Poll every 10ms */

	while (elapsed < timeout_ms) {
		if (_dos_keysns() != 0) {
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
		_dos_putchar((unsigned char)bytes[i]);
	}

	return 0; /* Always succeeds */
}
