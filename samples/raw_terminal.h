/*
 * raw_terminal.h - Cross-platform raw terminal mode support
 *
 * Provides install_raw_terminal() and restore_terminal() for both
 * Windows and POSIX platforms.
 *
 * Usage:
 *   #include "raw_terminal.h"
 *
 *   int main(void) {
 *       if (install_raw_terminal() != 0) {
 *           // handle error
 *       }
 *       // ... use terminal ...
 *       // restore_terminal() is called automatically via atexit()
 *   }
 */
#ifndef RAW_TERMINAL_H
#define RAW_TERMINAL_H

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

#include <windows.h>

static DWORD g_raw_original_input_mode = 0;
static DWORD g_raw_original_output_mode = 0;
static UINT g_raw_original_cp = 0;
static UINT g_raw_original_output_cp = 0;
static int g_raw_installed = 0;

static void restore_terminal(void)
{
	if (g_raw_installed) {
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleMode(hStdin, g_raw_original_input_mode);
		SetConsoleMode(hStdout, g_raw_original_output_mode);

		if (g_raw_original_cp != 0) {
			SetConsoleCP(g_raw_original_cp);
		}
		if (g_raw_original_output_cp != 0) {
			SetConsoleOutputCP(g_raw_original_output_cp);
		}

		g_raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetStdHandle failed\n");
		return -1;
	}

	g_raw_original_cp = GetConsoleCP();
	g_raw_original_output_cp = GetConsoleOutputCP();

	if (!SetConsoleCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleCP(65001) failed (error %lu)\n", GetLastError());
	}
	if (!SetConsoleOutputCP(65001)) {
		fprintf(stderr, "Warning: SetConsoleOutputCP(65001) failed (error %lu)\n", GetLastError());
	}

	if (!GetConsoleMode(hStdin, &g_raw_original_input_mode)) {
		fprintf(stderr, "GetConsoleMode(input) failed (error %lu)\n", GetLastError());
		return -1;
	}

	if (!GetConsoleMode(hStdout, &g_raw_original_output_mode)) {
		fprintf(stderr, "GetConsoleMode(output) failed (error %lu)\n", GetLastError());
		return -1;
	}

	DWORD dwInputMode = g_raw_original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
	dwInputMode = (dwInputMode | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;

	if (!SetConsoleMode(hStdin, dwInputMode)) {
		fprintf(stderr, "SetConsoleMode(input) failed (error %lu)\n", GetLastError());
		return -1;
	}

	DWORD dwOutputMode = g_raw_original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (!SetConsoleMode(hStdout, dwOutputMode)) {
		fprintf(stderr, "SetConsoleMode(output) failed (error %lu)\n", GetLastError());
		return -1;
	}

	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}

/* POSIX file descriptor constants */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#else /* POSIX */

#include <termios.h>
#include <unistd.h>

static struct termios g_raw_original_termios;
static int g_raw_installed = 0;

static void restore_terminal(void)
{
	if (g_raw_installed) {
		(void)tcsetattr(STDIN_FILENO, TCSANOW, &g_raw_original_termios);
		g_raw_installed = 0;
	}
}

static int install_raw_terminal(void)
{
	if (tcgetattr(STDIN_FILENO, &g_raw_original_termios) != 0) {
		return -1;
	}
	struct termios raw = g_raw_original_termios;
	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_cflag |= CS8;
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
		return -1;
	}
	g_raw_installed = 1;
	atexit(restore_terminal);
	return 0;
}

#endif /* _WIN32 */

#endif /* RAW_TERMINAL_H */
