#include "terse.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void create_pipe_handle(terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(*out_handle != NULL);
}

static ssize_t read_pipe(int fd, char *buffer, size_t size)
{
	int flags = fcntl(fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);

	memset(buffer, 0, size);
	ssize_t n = read(fd, buffer, size);
	if (n < 0 && errno == EAGAIN) {
		usleep(1000); // allow write side to flush
		n = read(fd, buffer, size);
	}
	return n;
}

TEST(TerseClearScreen, EmitsAfterSequence_OnAfter)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_screen(handle, TERSE_CLEAR_AFTER));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[J") != NULL);

	close(fds[0]);
}

TEST(TerseClearScreen, EmitsAllSequence_OnAll)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_screen(handle, TERSE_CLEAR_ALL));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[2J") != NULL);

	close(fds[0]);
}

TEST(TerseClearLine, EmitsModeSequence_OnBefore)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_line(handle, TERSE_CLEAR_BEFORE));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[1K") != NULL);

	close(fds[0]);
}

TEST(TerseMoveTo, EmitsClampedAbsoluteSequence_OnZeroInput)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_move_to(handle, 0, 0));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[1;1H") != NULL);

	close(fds[0]);
}

TEST(TerseMoveBy, EmitsDirectionalSequence_OnOffsets)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_move_by(handle, 2, -3));
	terse_close(handle);
	close(fds[1]);

	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[2B") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[3D") != NULL);

	close(fds[0]);
}

TEST(TerseShowCursor, EmitsHideSequence_OnFalse)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_show_cursor(handle, 0));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[?25l") != NULL);

	close(fds[0]);
}

TEST(TerseClearScreen, ReturnsEINVAL_OnInvalidMode)
{
	errno = 0;
	int result = terse_clear_screen(NULL, TERSE_CLEAR_AFTER);
	EXPECT_EQ(-EINVAL, result);
	EXPECT_EQ(EINVAL, errno);
}

TEST(TerseWriteText, ReturnsEBadf_OnUnreadableFd)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[0],
		.codec_name = "UTF-8",
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	close(fds[1]);

	errno = 0;
	int rc = terse_write_text(handle, "hi");
	EXPECT_EQ(-EBADF, rc);
	EXPECT_EQ(EBADF, errno);

	terse_close(handle);
	close(fds[0]);
}

int main()
{
	return RunAllTests();
}
