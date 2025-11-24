#if !defined(__human68k__) && !defined(__HUMAN68K__)
#define _POSIX_C_SOURCE 200809L
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__human68k__) || defined(__HUMAN68K__)
/* Human68k platform */
#include <x68k/dos.h>
#include <x68k/iocs.h>
#elif defined(_WIN32)
/* Windows platform */
#include <windows.h>
#include <io.h>
#else
/* POSIX platform */
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

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
	printf("\r\n");
}

#if defined(_WIN32)
static size_t read_bytes_with_timeout(HANDLE handle, unsigned char *buffer, size_t capacity, int timeout_ms)
{
	size_t total = 0;
	DWORD start_time = GetTickCount();
	DWORD last_read_time = start_time;
	const DWORD idle_threshold = 150; // Wait 150ms after last byte before giving up

	// Use overlapped I/O for timeout support
	OVERLAPPED overlapped = {0};
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!overlapped.hEvent) {
		return 0;
	}

	while (total < capacity) {
		DWORD elapsed = GetTickCount() - start_time;
		if (elapsed >= (DWORD)timeout_ms) {
			break;
		}

		// Check if input is available
		DWORD num_events = 0;
		if (!GetNumberOfConsoleInputEvents(handle, &num_events)) {
			break;
		}

		if (num_events == 0) {
			// No input available, check if we've been idle too long after receiving data
			if (total > 0 && (GetTickCount() - last_read_time) > idle_threshold) {
				break;
			}
			Sleep(10);
			continue;
		}

		// Read input records
		INPUT_RECORD records[32];
		DWORD num_read = 0;
		if (!ReadConsoleInput(handle, records, 32, &num_read)) {
			break;
		}

		// Process input records - in VT mode, key events contain the VT sequences
		for (DWORD i = 0; i < num_read && total < capacity; i++) {
			if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown) {
				// Get the character from the key event
				WCHAR wch = records[i].Event.KeyEvent.uChar.UnicodeChar;
				if (wch != 0) {
					// Convert to byte - VT sequences are ASCII
					if (wch < 256) {
						buffer[total++] = (unsigned char)wch;
						last_read_time = GetTickCount();
					}
				}
			}
		}
	}

	CloseHandle(overlapped.hEvent);
	return total;
}
#elif !defined(__human68k__) && !defined(__HUMAN68K__)
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
#endif

static void probe_device_attributes(void)
{
#if defined(__human68k__) || defined(__HUMAN68K__)
	printf("Device attribute probing not supported on Human68k.\n");
	printf("Console I/O is handled through DOS/IOCS calls.\n");
#elif defined(_WIN32)
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
		printf("Failed to get console handles.\n");
		return;
	}

	if (_isatty(_fileno(stdin)) == 0 || _isatty(_fileno(stdout)) == 0) {
		printf("stdin/stdout are not TTYs; skipping DA probe.\n");
		return;
	}

	DWORD original_input_mode = 0;
	DWORD original_output_mode = 0;
	if (!GetConsoleMode(hStdin, &original_input_mode)) {
		printf("Failed to get console input mode.\n");
		return;
	}
	if (!GetConsoleMode(hStdout, &original_output_mode)) {
		printf("Failed to get console output mode.\n");
		return;
	}

	// Enable VT processing for output
	DWORD new_output_mode = original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hStdout, new_output_mode);

	// Disable line input and echo for input
	DWORD new_input_mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
	SetConsoleMode(hStdin, new_input_mode);

	unsigned char buffer[128];
	size_t length = 0;

	const char primary_request[] = "\x1b[c";
	DWORD written = 0;
	if (!WriteFile(hStdout, primary_request, sizeof(primary_request) - 1, &written, NULL)) {
		printf("Failed to write primary DA request.\n");
	} else {
		FlushFileBuffers(hStdout);
		length = read_bytes_with_timeout(hStdin, buffer, sizeof(buffer), 500);
		print_bytes("Primary DA response", buffer, length);
	}

	const char secondary_request[] = "\x1b[>0c";
	if (!WriteFile(hStdout, secondary_request, sizeof(secondary_request) - 1, &written, NULL)) {
		printf("Failed to write secondary DA request.\n");
	} else {
		FlushFileBuffers(hStdout);
		length = read_bytes_with_timeout(hStdin, buffer, sizeof(buffer), 500);
		print_bytes("Secondary DA response", buffer, length);
	}

	const char focus_inquiry[] = "\x1b[?1004$p";
	if (!WriteFile(hStdout, focus_inquiry, sizeof(focus_inquiry) - 1, &written, NULL)) {
		printf("Failed to write focus inquiry.\n");
	} else {
		FlushFileBuffers(hStdout);
		length = read_bytes_with_timeout(hStdin, buffer, sizeof(buffer), 500);
		print_bytes("Focus tracking query response", buffer, length);
	}

	SetConsoleMode(hStdin, original_input_mode);
	SetConsoleMode(hStdout, original_output_mode);

	// Flush any remaining input to prevent it from appearing at the prompt
	FlushConsoleInputBuffer(hStdin);
#else
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
#endif
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
#if defined(__human68k__) || defined(__HUMAN68K__)
	printf("stdin isatty : n/a (DOS console)\n");
	printf("stdout isatty: n/a (DOS console)\n");
#elif defined(_WIN32)
	printf("stdin isatty : %s\n", _isatty(_fileno(stdin)) ? "yes" : "no");
	printf("stdout isatty: %s\n", _isatty(_fileno(stdout)) ? "yes" : "no");
#else
	printf("stdin isatty : %s\n", isatty(STDIN_FILENO) ? "yes" : "no");
	printf("stdout isatty: %s\n", isatty(STDOUT_FILENO) ? "yes" : "no");
#endif

	print_header("Device Attribute Probes");
	probe_device_attributes();

	return 0;
}
