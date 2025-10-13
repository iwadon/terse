#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

static ssize_t read_pipe_data(int fd, char *buffer, size_t size)
{
	set_nonblocking(fd);
	memset(buffer, 0, size);
	ssize_t n = read(fd, buffer, size);
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		usleep(1000);
		n = read(fd, buffer, size);
	}
	return n;
}

TEST(TerseTitle, SetTitleWritesOsc)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TITLE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(0, terse_set_title(handle, "Hello"));
	char buffer[64];
	ssize_t n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b]0;Hello\x07") != NULL);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseTitle, SetHyperlinkWritesOsc8)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_HYPERLINK,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(0, terse_set_hyperlink(handle, "https://example.com", "Link"));
	char buffer[128];
	ssize_t n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b]8;;https://example.com\x07Link\x1b]8;;\x07") != NULL);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseTitle, NoopOnUnsupportedTerminal)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_TITLE | TERSE_CAP_DISABLE_HYPERLINK,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(0, terse_set_title(handle, "Hello"));
	EXPECT_EQ(0, terse_set_hyperlink(handle, "https://example.com", "Link"));
	set_nonblocking(out_pipe[0]);
	char tmp[16];
	errno = 0;
	ssize_t n = read(out_pipe[0], tmp, sizeof(tmp));
	EXPECT_TRUE(n == -1);
	EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

int main(int argc, char **argv)
{
	return attest_main(argc, argv);
}
