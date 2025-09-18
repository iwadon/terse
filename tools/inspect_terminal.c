#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static void print_env(const char *name)
{
	const char *value = getenv(name);
	printf("%-24s : %s\n", name, value ? value : "(unset)");
}

static void print_header(const char *title)
{
	printf("\n=== %s ===\n", title);
}

static void print_bytes(const char *label, const unsigned char *data, size_t length)
{
	printf("%s: ", label);
	if (length == 0) {
		printf("(no response)\n");
		return;
	}
	for (size_t i = 0; i < length; ++i) {
		unsigned char ch = data[i];
		if (isprint(ch)) {
			putchar(ch);
		} else {
			printf("\\x%02X", ch);
		}
	}
	printf("\n");
}

static size_t read_bytes_with_timeout(int fd, unsigned char *buffer, size_t capacity, int timeout_ms)
{
	size_t total = 0;
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN,
	};

	int remaining = timeout_ms;
	const int slice = 50; // milliseconds per poll step

	while (remaining >= 0 && total < capacity) {
		int wait = remaining < slice ? remaining : slice;
		int ready = poll(&pfd, 1, wait);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			perror("poll");
			break;
		}
		if (ready == 0) {
			remaining -= slice;
			continue;
		}
		ssize_t n = read(fd, buffer + total, capacity - total);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			perror("read");
			break;
		}
		if (n == 0) {
			break;
		}
		total += (size_t)n;
		remaining -= slice;
	}

	return total;
}

static void probe_device_attributes(void)
{
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
		printf("stdin/stdout are not TTYs; skipping DA probe.\n");
		return;
	}

	struct termios original;
	if (tcgetattr(STDIN_FILENO, &original) != 0) {
		perror("tcgetattr");
		return;
	}

	struct termios raw = original;
	raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
		perror("tcsetattr");
		return;
	}

	unsigned char buffer[128];
	size_t length = 0;

	const char primary_request[] = "\x1b[c";
	if (write(STDOUT_FILENO, primary_request, sizeof(primary_request) - 1) < 0) {
		perror("write primary DA");
	} else {
		fflush(stdout);
		length = read_bytes_with_timeout(STDIN_FILENO, buffer, sizeof(buffer), 500);
		print_bytes("Primary DA response", buffer, length);
	}

	const char secondary_request[] = "\x1b[>0c";
	if (write(STDOUT_FILENO, secondary_request, sizeof(secondary_request) - 1) < 0) {
		perror("write secondary DA");
	} else {
		fflush(stdout);
		length = read_bytes_with_timeout(STDIN_FILENO, buffer, sizeof(buffer), 500);
		print_bytes("Secondary DA response", buffer, length);
	}

	const char focus_inquiry[] = "\x1b[?1004$p";
	if (write(STDOUT_FILENO, focus_inquiry, sizeof(focus_inquiry) - 1) < 0) {
		perror("write focus inquiry");
	} else {
		fflush(stdout);
		length = read_bytes_with_timeout(STDIN_FILENO, buffer, sizeof(buffer), 500);
		print_bytes("Focus tracking query response", buffer, length);
	}

	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("restore termios");
	}
}

int main(void)
{
	print_header("Environment Hints");
	print_env("TERM");
	print_env("TERM_PROGRAM");
	print_env("TERM_PROGRAM_VERSION");
	print_env("LC_TERMINAL");
	print_env("LC_TERMINAL_VERSION");
	print_env("COLORTERM");
	print_env("TERMINFO");
	print_env("TERMINFO_DIRS");
	print_env("ITERM_PROFILE");
	print_env("ITERM_SESSION_ID");
	print_env("WEZTERM_EXECUTABLE");
	print_env("KITTY_PID");
	print_env("WT_SESSION");
	print_env("VSCODE_INJECTION");

	print_header("TTY Status");
	printf("stdin isatty : %s\n", isatty(STDIN_FILENO) ? "yes" : "no");
	printf("stdout isatty: %s\n", isatty(STDOUT_FILENO) ? "yes" : "no");

	print_header("Device Attribute Probes");
	probe_device_attributes();

	return 0;
}
