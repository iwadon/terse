#include "terse_platform.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static size_t
read_bytes_with_timeout(int fd, unsigned char *buffer, size_t capacity, int timeout_ms)
{
	size_t total = 0;
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
	const int slice = 25;
	int remaining = timeout_ms;
	while (total < capacity) {
		int poll_timeout;
		int wait = slice;
		if (timeout_ms < 0) {
			poll_timeout = -1;
		} else {
			if (remaining <= 0) {
				break;
			}
			if (remaining < slice) {
				wait = remaining;
			}
			poll_timeout = wait;
		}
		int ready = poll(&pfd, 1, poll_timeout);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (ready == 0) {
			if (timeout_ms >= 0) {
				remaining -= wait;
			}
			continue;
		}
		ssize_t n = read(fd, buffer + total, capacity - total);
		if (n <= 0) {
			break;
		}
		total += (size_t)n;
		if (timeout_ms >= 0) {
			remaining -= wait;
		}
	}
	return total;
}

terse_options_t
terse_platform_default_options(void)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	return options;
}

terse_size_t
terse_platform_query_fd_size(int fd)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
	if (fd < 0) {
		return size;
	}
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
		size.rows = ws.ws_row;
		size.cols = ws.ws_col;
		size.known = 1;
	}
	return size;
}

size_t
terse_platform_probe_secondary_da(int input_fd, int output_fd, unsigned char *buffer, size_t capacity)
{
	if (!buffer || capacity == 0) {
		return 0;
	}
	if (input_fd < 0 || output_fd < 0) {
		return 0;
	}
	if (!isatty(input_fd) || !isatty(output_fd)) {
		return 0;
	}
	struct termios original;
	if (tcgetattr(input_fd, &original) != 0) {
		return 0;
	}
	struct termios raw = original;
	raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(input_fd, TCSANOW, &raw) != 0) {
		return 0;
	}
	const char request[] = "\x1b[>0c";
	if (write(output_fd, request, sizeof(request) - 1) < 0) {
		(void)tcsetattr(input_fd, TCSANOW, &original);
		return 0;
	}
	unsigned char local[128];
	if (!buffer) {
		buffer = local;
	}
	size_t length = read_bytes_with_timeout(input_fd, buffer, capacity, 200);
	(void)tcsetattr(input_fd, TCSANOW, &original);
	return length;
}

int
terse_platform_wait_for_input(int fd, int timeout_ms)
{
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
	int poll_timeout = timeout_ms < 0 ? -1 : timeout_ms;
	for (;;) {
		int ready = poll(&pfd, 1, poll_timeout);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return ready;
	}
}

ssize_t
terse_platform_read_byte(int fd, unsigned char *out)
{
	for (;;) {
		ssize_t n = read(fd, out, 1);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		return n;
	}
}

size_t
terse_platform_drain_escape_sequence(int fd, unsigned char *buffer, size_t max)
{
	size_t len = 1;
	while (len < max) {
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		int ready = poll(&pfd, 1, 10);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (ready == 0) {
			break;
		}
		ssize_t n = read(fd, buffer + len, 1);
		if (n <= 0) {
			break;
		}
		len += (size_t)n;
		if (len >= 3) {
			unsigned char c = buffer[len - 1];
			if (c >= '@' && c <= '~') {
				break;
			}
		}
	}
	return len;
}

int
terse_platform_write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		errno = EINVAL;
		return -EINVAL;
	}
	while (len > 0) {
		ssize_t written = write(fd, bytes, len);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			return -err;
		}
		if (written == 0) {
			errno = EPIPE;
			return -EPIPE;
		}
		bytes += (size_t)written;
		len -= (size_t)written;
	}
	return 0;
}
