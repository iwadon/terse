#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <string.h>

#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE
static void create_pipe_handle(terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(*out_handle != NULL);
}

static ssize_t read_pipe(int fd, char *buffer, size_t size)
{
	memset(buffer, 0, size);
	return read(fd, buffer, size);
}

TEST(TerseClose, EmitsResetSequences_OnClose)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);
	EXPECT_EQ(TERSE_OK, terse_show_cursor(handle, 0));
	char initial[32];
	ssize_t initial_n = read_pipe(fds[0], initial, sizeof(initial));
	EXPECT_TRUE(initial_n > 0);
	EXPECT_TRUE(strstr(initial, "\x1b[?25l") != NULL);

	char buf[32];
	errno = 0;
	terse_close(handle);
	close(fds[1]);

	size_t total = 0;
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	total = (size_t)(n > 0 ? n : 0);
	EXPECT_TRUE(total >= 6);
	EXPECT_TRUE(strstr(buf, "\x1b[?25h") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	terse_error_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_OK, err);

	close(fds[0]);
}

TEST(TerseClose, SkipsReset_WhenBasicOutputDisabled)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_BASIC_OUTPUT,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	char buf[32];
	errno = 0;
	terse_close(handle);
	close(fds[1]);
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n == 0);
	terse_error_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_OK, err);
	close(fds[0]);
}
#endif /* HAVE_POSIX_PIPE */

TEST(TerseClose, AcceptsNullHandle)
{
	errno = 0;
	terse_close(NULL);
	EXPECT_EQ(0, errno);
}
