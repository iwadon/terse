#define _XOPEN_SOURCE 600

#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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

TEST(TerseGetSize, ReturnsUnknown_OnPipe)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	terse_size_t size = terse_get_size(handle);
	EXPECT_TRUE(size.known == 0);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);

	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_TRUE(caps.has_size == 0);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

static int set_winsize(int fd, int rows, int cols)
{
	struct winsize ws;
	memset(&ws, 0, sizeof(ws));
	ws.ws_row = (unsigned short)rows;
	ws.ws_col = (unsigned short)cols;
	return ioctl(fd, TIOCSWINSZ, &ws);
}

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

static void set_raw_mode(int fd)
{
	struct termios tio;
	if (tcgetattr(fd, &tio) != 0) {
		return;
	}
	tio.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
	tio.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tio.c_oflag &= ~(OPOST);
	tio.c_cflag |= CS8;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	(void)tcsetattr(fd, TCSANOW, &tio);
}

TEST(TerseGetSize, ReturnsKnown_OnPty)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (open_pty_pair(&master_fd, &slave_fd) != 0) {
		return;
	}
	if (set_winsize(slave_fd, 48, 120) != 0) {
		close(master_fd);
		close(slave_fd);
		return;
	}

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	terse_size_t size = terse_get_size(handle);
	EXPECT_TRUE(size.known == 1);
	EXPECT_EQ(48, size.rows);
	EXPECT_EQ(120, size.cols);

	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_TRUE(caps.has_size == 1);

	terse_close(handle);
	close(master_fd);
	close(slave_fd);
}

TEST(TerseGetSize, UpdatesOnResizeEvent)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (open_pty_pair(&master_fd, &slave_fd) != 0) {
		return;
	}
	set_raw_mode(master_fd);
	set_raw_mode(slave_fd);

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	const char resize_seq[] = "\x1b[8;30;90t";
	EXPECT_TRUE(write(master_fd, resize_seq, sizeof(resize_seq) - 1) == (ssize_t)(sizeof(resize_seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_RESIZE, event.type);
	EXPECT_EQ(30, event.data.resize.rows);
	EXPECT_EQ(90, event.data.resize.cols);

	terse_size_t size = terse_get_size(handle);
	EXPECT_TRUE(size.known == 1);
	EXPECT_EQ(30, size.rows);
	EXPECT_EQ(90, size.cols);

	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_TRUE(caps.has_size == 1);

	terse_close(handle);
	close(master_fd);
	close(slave_fd);
}

TEST(TerseGetSize, ReturnsUnknown_WhenCapabilityDisabled)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (open_pty_pair(&master_fd, &slave_fd) != 0) {
		return;
	}
	set_raw_mode(master_fd);
	set_raw_mode(slave_fd);

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_SIZE,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_TRUE(caps.has_size == 0);
	terse_size_t size = terse_get_size(handle);
	EXPECT_TRUE(size.known == 0);

	terse_close(handle);
	close(master_fd);
	close(slave_fd);
}
