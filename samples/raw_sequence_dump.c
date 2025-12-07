/*
 * Raw sequence dump utility
 * Captures and displays raw escape sequences from terminal input
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raw_terminal.h"
#include "sample_compat.h"

#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <errno.h>
#endif

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

#ifdef _WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetStdHandle failed\r\n");
		return 1;
	}

	while (1) {
		DWORD waitResult = WaitForSingleObject(hStdin, 1000);
		if (waitResult == WAIT_TIMEOUT) {
			continue;
		}
		if (waitResult != WAIT_OBJECT_0) {
			break;
		}

		INPUT_RECORD rec;
		DWORD numRead;
		if (!PeekConsoleInput(hStdin, &rec, 1, &numRead) || numRead == 0) {
			continue;
		}

		if (!ReadConsoleInput(hStdin, &rec, 1, &numRead) || numRead == 0) {
			continue;
		}

		if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) {
			continue;
		}

		size_t n = 0;
		KEY_EVENT_RECORD *key = &rec.Event.KeyEvent;

		if (key->uChar.UnicodeChar != 0) {
			/* Convert Unicode character to UTF-8 */
			WCHAR wc = key->uChar.UnicodeChar;
			int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, (char *)buffer, sizeof(buffer) - 1, NULL, NULL);
			if (len > 0) {
				n = (size_t)len;
			}
		} else {
			/* Virtual key code only - report it */
			printf("VK: 0x%02X Scan: 0x%02X Mods: 0x%lx\r\n",
				key->wVirtualKeyCode, key->wVirtualScanCode, key->dwControlKeyState);
			fflush(stdout);
			continue;
		}

		if (n == 0) {
			continue;
		}

		/* Check for Ctrl+C */
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

		printf("Read %zu bytes:\r\n", n);
		printf("  HEX: ");
		print_hex_bytes(buffer, n);
		printf("\r\n");
		printf("  SEQ: ");
		print_readable(buffer, n);
		printf("\r\n\r\n");
		fflush(stdout);
	}
#else
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

		/* Check for Ctrl+C */
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
#endif

	return 0;
}
