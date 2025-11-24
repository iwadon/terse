#include "terse_platform.h"

#include <errno.h>
#ifdef TERSE_HAVE_POLL_H
#include <poll.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static size_t
read_bytes_with_timeout(int fd, unsigned char *buffer, size_t capacity, int timeout_ms)
{
	size_t total = 0;
#ifdef TERSE_HAVE_POLL_H
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};
#endif
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
#ifdef TERSE_HAVE_POLL_H
		int ready = poll(&pfd, 1, poll_timeout);
#else
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		struct timeval tv;
		struct timeval *tvp = NULL;
		if (poll_timeout >= 0) {
			tv.tv_sec = poll_timeout / 1000;
			tv.tv_usec = (poll_timeout % 1000) * 1000;
			tvp = &tv;
		}
		int ready = select(fd + 1, &readfds, NULL, NULL, tvp);
#endif
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

terse_error_t
terse_platform_query_cursor_position(int input_fd, int output_fd, int *out_row, int *out_col)
{
	if (!out_row || !out_col) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	if (input_fd < 0 || output_fd < 0) {
		return TERSE_ERR_INVALID_HANDLE;
	}
	if (!isatty(input_fd) || !isatty(output_fd)) {
		return TERSE_ERR_NOT_TTY;
	}

	// Save current terminal settings and switch to raw mode
	struct termios original;
	if (tcgetattr(input_fd, &original) != 0) {
		return TERSE_ERR_IO;
	}
	struct termios raw = original;
	raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(input_fd, TCSANOW, &raw) != 0) {
		return TERSE_ERR_IO;
	}

	// Send CPR (Cursor Position Report) request: CSI 6 n
	const char request[] = "\x1b[6n";
	if (write(output_fd, request, sizeof(request) - 1) < 0) {
		(void)tcsetattr(input_fd, TCSANOW, &original);
		return TERSE_ERR_IO;
	}

	// Read response: CSI row ; col R
	unsigned char buffer[32];
	size_t length = 0;
#ifdef TERSE_HAVE_POLL_H
	struct pollfd pfd = {
		.fd = input_fd,
		.events = POLLIN,
	};
#endif
	const int timeout_ms = 200;
	const int slice = 25;
	int remaining = timeout_ms;
	while (length < sizeof(buffer)) {
		int poll_timeout = (remaining < slice) ? remaining : slice;
#ifdef TERSE_HAVE_POLL_H
		int ready = poll(&pfd, 1, poll_timeout);
#else
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(input_fd, &readfds);
		struct timeval tv = {
			.tv_sec = poll_timeout / 1000,
			.tv_usec = (poll_timeout % 1000) * 1000,
		};
		int ready = select(input_fd + 1, &readfds, NULL, NULL, &tv);
#endif
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			(void)tcsetattr(input_fd, TCSANOW, &original);
			return TERSE_ERR_IO;
		}
		if (ready == 0) {
			remaining -= poll_timeout;
			if (remaining <= 0) {
				break;
			}
			continue;
		}
		ssize_t n = read(input_fd, buffer + length, sizeof(buffer) - length);
		if (n <= 0) {
			break;
		}
		length += (size_t)n;
		remaining -= poll_timeout;

		// Check if we have a complete response (ends with 'R')
		if (length > 0 && buffer[length - 1] == 'R') {
			break;
		}
	}

	// Restore terminal settings
	(void)tcsetattr(input_fd, TCSANOW, &original);

	// Parse response: ESC [ row ; col R
	if (length < 6 || buffer[0] != 0x1b || buffer[1] != '[' || buffer[length - 1] != 'R') {
		return TERSE_ERR_PROTOCOL;
	}

	// Parse row and col
	int row = 0, col = 0;
	size_t i = 2;
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		row = row * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != ';') {
		return TERSE_ERR_PROTOCOL;
	}
	i++; // skip ';'
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		col = col * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != 'R') {
		return TERSE_ERR_PROTOCOL;
	}

	// Terminal returns 1-based coordinates, convert to 0-based
	*out_row = row - 1;
	*out_col = col - 1;
	return 0;
}

terse_error_t
terse_platform_wait_for_input(int fd, int timeout_ms)
{
#ifdef TERSE_HAVE_POLL_H
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
			// Map errno to terse_error_t
			return (err == EAGAIN || err == EWOULDBLOCK) ? -TERSE_ERR_WOULD_BLOCK : -TERSE_ERR_IO;
		}
		return ready;
	}
#else
	for (;;) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		struct timeval tv;
		struct timeval *tvp = NULL;
		if (timeout_ms >= 0) {
			tv.tv_sec = timeout_ms / 1000;
			tv.tv_usec = (timeout_ms % 1000) * 1000;
			tvp = &tv;
		}
		int ready = select(fd + 1, &readfds, NULL, NULL, tvp);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			// Map errno to terse_error_t
			return (err == EAGAIN || err == EWOULDBLOCK) ? -TERSE_ERR_WOULD_BLOCK : -TERSE_ERR_IO;
		}
		return ready;
	}
#endif
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
#ifdef TERSE_HAVE_POLL_H
		struct pollfd pfd = {
			.fd = fd,
			.events = POLLIN,
		};
		int ready = poll(&pfd, 1, 10);
#else
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		struct timeval tv = {
			.tv_sec = 0,
			.tv_usec = 10000,
		};
		int ready = select(fd + 1, &readfds, NULL, NULL, &tv);
#endif
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
			// Special case: ESC [ [ needs one more byte (Linux console)
			if (len == 3 && buffer[1] == '[' && buffer[2] == '[') {
				// Continue reading
			} else if (c >= '@' && c <= '~') {
				break;
			}
		}
	}
	return len;
}

terse_error_t
terse_platform_write_bytes(int fd, const char *bytes, size_t len)
{
	if (!bytes) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	while (len > 0) {
		ssize_t written = write(fd, bytes, len);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			int err = errno;
			errno = err;
			// Map errno to terse_error_t
			return (err == EAGAIN || err == EWOULDBLOCK) ? -TERSE_ERR_WOULD_BLOCK : -TERSE_ERR_IO;
		}
		if (written == 0) {
			errno = EPIPE;
			return -TERSE_ERR_IO;
		}
		bytes += (size_t)written;
		len -= (size_t)written;
	}
	return 0;
}

terse_error_t
terse_platform_move_to_fast(terse_handle_t handle, int row, int col)
{
	(void)handle;
	(void)row;
	(void)col;
	/* POSIX platforms use standard escape sequences, no fast path available */
	return TERSE_ERR_NOT_SUPPORTED;
}

terse_error_t
terse_platform_clear_screen_fast(terse_handle_t handle, terse_clear_mode_t mode)
{
	(void)handle;
	(void)mode;
	/* POSIX platforms use standard escape sequences, no fast path available */
	return TERSE_ERR_NOT_SUPPORTED;
}
