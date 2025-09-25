#include "terse_platform.h"

#include <errno.h>

terse_options_t
terse_platform_default_options(void)
{
	terse_options_t options = {
		.input_fd = -1,
		.output_fd = -1,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	return options;
}

terse_size_t
terse_platform_query_fd_size(int fd)
{
	(void)fd;
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
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
terse_platform_wait_for_input(int fd, int timeout_ms)
{
	(void)fd;
	(void)timeout_ms;
	errno = ENOTSUP;
	return -ENOTSUP;
}

ssize_t
terse_platform_read_byte(int fd, unsigned char *out)
{
	(void)fd;
	(void)out;
	errno = ENOTSUP;
	return -ENOTSUP;
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
	(void)fd;
	(void)bytes;
	(void)len;
	errno = ENOTSUP;
	return -ENOTSUP;
}
