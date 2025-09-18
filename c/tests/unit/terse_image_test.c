#include "terse.h"
#include "test.h"

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

static void expect_no_bytes_available_fd(int fd)
{
	set_nonblocking(fd);
	char tmp[16];
	errno = 0;
	ssize_t n = read(fd, tmp, sizeof(tmp));
	EXPECT_TRUE(n == -1);
	EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
}

TEST(TerseImage, WritesItermSequence)
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
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x01, 0x02, 0x03 };
	EXPECT_EQ(0, terse_display_image_inline(handle, payload, sizeof(payload), "demo"));
	char buffer[256];
	size_t n = (size_t)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b]1337;File=name=") != NULL);
	EXPECT_TRUE(strstr(buffer, ";inline=1:") != NULL);
	EXPECT_TRUE(strstr(buffer, "AQID") != NULL);
	expect_no_bytes_available_fd(out_pipe[0]);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, NoopWhenDisabled)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_IMAGE_INLINE,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x01, 0x02, 0x03 };
	EXPECT_EQ(0, terse_display_image_inline(handle, payload, sizeof(payload), "demo"));
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

int main(void)
{
	return RunAllTests();
}
