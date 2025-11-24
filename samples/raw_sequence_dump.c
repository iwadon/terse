/*
 * Raw sequence dump utility
 * Captures and displays raw escape sequences from terminal input
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

static struct termios original_termios;
static int raw_installed = 0;

static void restore_terminal(void)
{
	if (raw_installed) {
		(void)tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
		raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &original_termios) != 0) {
		return -1;
	}
	struct termios raw = original_termios;
	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_cflag |= CS8;
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
		return -1;
	}
	raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}

static void print_hex_bytes(const unsigned char *bytes, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		printf("%02X ", bytes[i]);
	}
}

static void print_readable(const unsigned char *bytes, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (bytes[i] == 0x1b) {
			printf("ESC");
		} else if (bytes[i] >= 0x20 && bytes[i] <= 0x7e) {
			printf("%c", bytes[i]);
		} else if (bytes[i] == '\r') {
			printf("CR");
		} else if (bytes[i] == '\n') {
			printf("LF");
		} else if (bytes[i] == '\t') {
			printf("TAB");
		} else {
			printf("\\x%02X", bytes[i]);
		}
		if (i < len - 1) {
			printf(" ");
		}
	}
}

int main(void)
{
	if (install_raw_terminal() != 0) {
		fprintf(stderr, "Failed to set raw mode.\r\n");
		return 1;
	}

	printf("Raw Sequence Dump Utility\r\n");
	printf("Press keys to see raw escape sequences\r\n");
	printf("Press Ctrl+C (will show as \\x03) to exit\r\n");
	printf("\r\n");
	fflush(stdout);

	unsigned char buffer[256];
	int ctrl_c_count = 0;

	while (1) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (ready == 0) {
			continue;
		}

		ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
		if (n <= 0) {
			break;
		}

		// Check for Ctrl+C
		if (n == 1 && buffer[0] == 0x03) {
			ctrl_c_count++;
			if (ctrl_c_count >= 2) {
				printf("\r\nCtrl+C detected twice, exiting.\r\n");
				break;
			}
			printf("\r\nCtrl+C detected once more to exit.\r\n");
		} else {
			ctrl_c_count = 0;
		}

		printf("Read %zd bytes:\r\n", n);
		printf("  HEX: ");
		print_hex_bytes(buffer, (size_t)n);
		printf("\r\n");
		printf("  SEQ: ");
		print_readable(buffer, (size_t)n);
		printf("\r\n\r\n");
		fflush(stdout);
	}

	return 0;
}
