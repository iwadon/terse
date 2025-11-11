#include "terse_platform.h"

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>

/* EPROTO may not be defined on Human68k, use EIO as fallback */
#ifndef EPROTO
#define EPROTO EIO
#endif

terse_options_t
terse_platform_default_options(void)
{
	terse_options_t options = {
		.input_fd = -1,
		.output_fd = -1,
		.codec_name = "Shift_JIS",  /* Human68k is Shift_JIS native */
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	return options;
}

terse_size_t
terse_platform_query_fd_size(int fd)
{
	(void)fd;  /* Human68k doesn't use fd for console I/O */

	terse_size_t size = {
		.rows = 31,   /* Human68k default: 31 rows */
		.cols = 96,   /* Human68k default: 96 columns */
		.known = 1,
	};

	/* TODO: Use DSR (Device Status Report) to detect actual size */
	/* For now, use the standard Human68k console size */

	return size;
}

size_t
terse_platform_probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity)
{
	(void)input_fd;
	(void)output_fd;
	(void)buffer;
	(void)capacity;
	return 0;
}

int
terse_platform_query_cursor_position(int input_fd, int output_fd, int *out_row, int *out_col)
{
	(void)input_fd;  /* Human68k doesn't use fd for console I/O */
	(void)output_fd;

	if (!out_row || !out_col) {
		errno = EINVAL;
		return -EINVAL;
	}

	/* Send CPR (Cursor Position Report) request: CSI 6 n */
	const char request[] = "\x1b[6n";
	for (size_t i = 0; i < sizeof(request) - 1; i++) {
		_dos_putchar((unsigned char)request[i]);
	}

	/* Read response: CSI row ; col R */
	unsigned char buffer[32];
	size_t length = 0;
	const int timeout_ms = 200;
	const int poll_interval = 10;  /* Poll every 10ms */
	int elapsed = 0;

	while (length < sizeof(buffer) && elapsed < timeout_ms) {
		/* Check if input is available */
		if (_dos_keysns() != 0) {
			int ch = _dos_inkey();
			buffer[length++] = (unsigned char)(ch & 0xFF);

			/* Check if we have a complete response (ends with 'R') */
			if (buffer[length - 1] == 'R') {
				break;
			}
		} else {
			/* No input available, wait a bit */
			usleep(poll_interval * 1000);
			elapsed += poll_interval;
		}
	}

	/* Check if we got any response */
	if (length == 0) {
		/* No response - terminal doesn't support CPR or timeout */
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	/* Parse response: ESC [ row ; col R */
	if (length < 6 || buffer[0] != 0x1b || buffer[1] != '[' || buffer[length - 1] != 'R') {
		/* Invalid response format - possibly not supported or corrupted */
		errno = EPROTO;
		return -EPROTO;
	}

	/* Parse row and col */
	int row = 0, col = 0;
	size_t i = 2;
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		row = row * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != ';') {
		errno = EPROTO;
		return -EPROTO;
	}
	i++; /* skip ';' */
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		col = col * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != 'R') {
		errno = EPROTO;
		return -EPROTO;
	}

	/* Terminal returns 1-based coordinates, convert to 0-based */
	*out_row = row - 1;
	*out_col = col - 1;
	return 0;
}

int
terse_platform_wait_for_input(int fd, int timeout_ms)
{
	(void)fd;  /* Human68k doesn't use fd for console I/O */

	if (timeout_ms < 0) {
		/* Infinite wait - block until input available */
		while (_dos_keysns() == 0) {
			/* Busy wait - could add sleep here if available */
			usleep(10000);  /* Sleep 10ms to reduce CPU usage */
		}
		return 1;  /* Input available */
	}

	/* Timeout specified - poll with sleep intervals */
	int elapsed = 0;
	const int poll_interval = 10;  /* Poll every 10ms */

	while (elapsed < timeout_ms) {
		if (_dos_keysns() != 0) {
			return 1;  /* Input available */
		}
		usleep(poll_interval * 1000);
		elapsed += poll_interval;
	}

	return 0;  /* Timeout with no input */
}

ssize_t
terse_platform_read_byte(int fd, unsigned char *out)
{
	(void)fd;  /* Human68k doesn't use fd for console I/O */

	/* Use DOS _INKEY (no break check, no echo) */
	int ch = _dos_inkey();
	*out = (unsigned char)(ch & 0xFF);
	return 1;
}

size_t
terse_platform_drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	(void)fd;
	(void)buffer;
	(void)max;
	return 0;
}

int
terse_platform_write_bytes(int fd, const char *bytes, size_t len)
{
	(void)fd;  /* Human68k doesn't use fd for console I/O */

	/* Output bytes one at a time using DOS _PUTCHAR */
	for (size_t i = 0; i < len; i++) {
		_dos_putchar((unsigned char)bytes[i]);
	}

	return 0;  /* Always succeeds */
}
