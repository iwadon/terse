#include "terse.h"
#include "test_compat.h"
#include <attest/attest.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_POSIX_PIPE
#include <sys/select.h>
#include <sys/wait.h>

static int open_pty_pair(int *master_fd, int *slave_fd)
{
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0) {
		return -1;
	}
	if (grantpt(master) != 0 || unlockpt(master) != 0) {
		close(master);
		return -1;
	}
	char *slave_name = ptsname(master);
	if (!slave_name) {
		close(master);
		return -1;
	}
	int slave = open(slave_name, O_RDWR | O_NOCTTY);
	if (slave < 0) {
		close(master);
		return -1;
	}
	*master_fd = master;
	*slave_fd = slave;
	return 0;
}

static int wait_for_cpr_request(int fd)
{
	unsigned char seen[4] = { 0, 0, 0, 0 };
	for (;;) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		struct timeval tv = {
			.tv_sec = 1,
			.tv_usec = 0,
		};
		int ready = select(fd + 1, &readfds, NULL, NULL, &tv);
		if (ready <= 0) {
			return -1;
		}
		unsigned char byte = 0;
		if (read(fd, &byte, 1) != 1) {
			return -1;
		}
		seen[0] = seen[1];
		seen[1] = seen[2];
		seen[2] = seen[3];
		seen[3] = byte;
		if (seen[0] == '\x1b' && seen[1] == '[' && seen[2] == '6' && seen[3] == 'n') {
			return 0;
		}
	}
}

static pid_t start_cpr_responder(int master_fd, const char *response)
{
	pid_t pid = fork();
	if (pid != 0) {
		return pid;
	}
	if (wait_for_cpr_request(master_fd) != 0) {
		_exit(1);
	}
	size_t len = strlen(response);
	if (write(master_fd, response, len) != (ssize_t)len) {
		_exit(1);
	}
	_exit(0);
}

static void expect_responder_success(pid_t pid)
{
	int status = 0;
	EXPECT_TRUE(waitpid(pid, &status, 0) == pid);
	EXPECT_TRUE(WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

TEST(CursorPosition, Basic)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (open_pty_pair(&master_fd, &slave_fd) != 0) {
		return;
	}
	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	// Move to a known position
	EXPECT_EQ(0, terse_move_to(handle, 4, 9));
	EXPECT_EQ(0, terse_flush(handle));

	pid_t responder = start_cpr_responder(master_fd, "\x1b[5;10R");
	EXPECT_TRUE(responder > 0);

	// Query cursor position
	terse_cursor_position_t pos = terse_get_cursor_position(handle);

	EXPECT_EQ(1, pos.known);
	EXPECT_EQ(4, pos.row);
	EXPECT_EQ(9, pos.col);

	terse_close(handle);
	expect_responder_success(responder);
	close(master_fd);
	close(slave_fd);
}

TEST(CursorPosition, AfterText)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (open_pty_pair(&master_fd, &slave_fd) != 0) {
		return;
	}
	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	// Move to position and write text
	EXPECT_EQ(0, terse_move_to(handle, 0, 0));
	EXPECT_EQ(0, terse_write_text(handle, "Hello"));
	EXPECT_EQ(0, terse_flush(handle));

	pid_t responder = start_cpr_responder(master_fd, "\x1b[1;6R");
	EXPECT_TRUE(responder > 0);

	// Query cursor position (should be at column 6 after "Hello")
	terse_cursor_position_t pos = terse_get_cursor_position(handle);

	EXPECT_EQ(1, pos.known);
	EXPECT_EQ(0, pos.row);
	EXPECT_EQ(5, pos.col);

	terse_close(handle);
	expect_responder_success(responder);
	close(master_fd);
	close(slave_fd);
}

TEST(CursorPosition, ReturnsUnknown_OnPipe)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	terse_cursor_position_t pos = terse_get_cursor_position(handle);
	EXPECT_EQ(0, pos.known);
	EXPECT_EQ(TERSE_ERR_NOT_TTY, terse_get_last_error(handle));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}
#endif /* HAVE_POSIX_PIPE */

TEST(CursorPosition, InvalidHandle)
{
	terse_cursor_position_t pos = terse_get_cursor_position(NULL);
	EXPECT_EQ(0, pos.known);
}

#ifdef _WIN32
#include <io.h>

/*
 * Windows-specific tests for terse_get_cursor_position.
 *
 * On Windows we use GetConsoleScreenBufferInfo (not CPR), so:
 *  - Non-console FDs (files, pipes) must return NOT_TTY without writing
 *    any escape sequence to the output. The previous CPR-based code would
 *    leak "\x1b[6n" onto whatever the output_fd pointed at.
 *  - When the output FD is a real console, the call must succeed and
 *    return coordinates without producing visible text.
 */

TEST(CursorPosition, Windows_ReturnsNotTty_WhenOutputIsFile)
{
	// Create a temporary file and use it as both input/output FDs.
	// On Windows this fd is not a console, so the platform layer must
	// return NOT_TTY and must NOT write the CPR query sequence to it.
	char tmp_path[MAX_PATH];
	char tmp_dir[MAX_PATH];
	DWORD dir_len = GetTempPathA(sizeof(tmp_dir), tmp_dir);
	EXPECT_TRUE(dir_len > 0 && dir_len < sizeof(tmp_dir));
	UINT uniq = GetTempFileNameA(tmp_dir, "tcp", 0, tmp_path);
	EXPECT_TRUE(uniq != 0);

	int fd = _open(tmp_path, _O_RDWR | _O_BINARY);
	EXPECT_TRUE(fd >= 0);

	terse_options_t options = {
		.input_fd = fd,
		.output_fd = fd,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	terse_cursor_position_t pos = terse_get_cursor_position(handle);
	EXPECT_EQ(0, pos.known);
	EXPECT_EQ(TERSE_ERR_NOT_TTY, terse_get_last_error(handle));

	terse_close(handle);

	// Verify nothing (especially no CPR query "\x1b[6n") was written to
	// the file. terse_close may emit cleanup sequences, so only assert
	// that no CPR query bytes exist anywhere in the captured output.
	_lseek(fd, 0, SEEK_SET);
	unsigned char buf[256];
	int n = _read(fd, buf, sizeof(buf));
	for (int i = 0; i + 3 < n; i++) {
		int leaked = (buf[i] == 0x1b && buf[i + 1] == '[' &&
		              buf[i + 2] == '6' && buf[i + 3] == 'n');
		EXPECT_TRUE(!leaked);
	}

	_close(fd);
	DeleteFileA(tmp_path);
}

TEST(CursorPosition, Windows_SucceedsOnRealConsole)
{
	// Only run when stdout is actually attached to a Windows console.
	// In CI or under redirection this test no-ops.
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	if (!out || out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode)) {
		return;
	}

	terse_options_t options = {
		.input_fd = 0,
		.output_fd = 1,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	terse_cursor_position_t pos = terse_get_cursor_position(handle);
	EXPECT_EQ(1, pos.known);

	// Cross-check against the Win32 API directly. The library reports
	// viewport-relative 0-based coords, which must match what we compute
	// here independently from CONSOLE_SCREEN_BUFFER_INFO.
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	EXPECT_TRUE(GetConsoleScreenBufferInfo(out, &csbi));
	int expected_row = (int)csbi.dwCursorPosition.Y - (int)csbi.srWindow.Top;
	int expected_col = (int)csbi.dwCursorPosition.X - (int)csbi.srWindow.Left;
	EXPECT_EQ(expected_row, pos.row);
	EXPECT_EQ(expected_col, pos.col);

	terse_close(handle);
}
#endif /* _WIN32 */
