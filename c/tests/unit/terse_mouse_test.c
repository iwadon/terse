#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

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
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
		nanosleep(&ts, NULL);
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

static terse_handle_t create_mouse_handle(int output_pipe[2], int input_pipe[2])
{
	EXPECT_TRUE(pipe(output_pipe) == 0);
	EXPECT_TRUE(pipe(input_pipe) == 0);

	terse_options_t options = {
		.input_fd = input_pipe[0],
		.output_fd = output_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_MOUSE,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);
	return handle;
}

TEST(TerseMouse, EnableSgrMode_WritesSequences)
{
	int out_pipe[2];
	int in_pipe[2];
	terse_handle_t handle = create_mouse_handle(out_pipe, in_pipe);
	EXPECT_EQ(0, terse_enable_mouse(handle, TERSE_MOUSE_SGR));
	char buffer[64];
	ssize_t n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b[?1002h") != NULL);
	EXPECT_TRUE(strstr(buffer, "\x1b[?1006h") != NULL);

	expect_no_bytes_available_fd(out_pipe[0]);
	EXPECT_EQ(0, terse_disable_mouse(handle));
	n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b[?1002l") != NULL);
	EXPECT_TRUE(strstr(buffer, "\x1b[?1006l") != NULL);

	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseMouse, GeneratesEvents_FromSgrSequences)
{
	int out_pipe[2];
	int in_pipe[2];
	terse_handle_t handle = create_mouse_handle(out_pipe, in_pipe);
	EXPECT_EQ(0, terse_enable_mouse(handle, TERSE_MOUSE_SGR));
	char buffer[64];
	(void)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));

	const char press_seq[] = "\x1b[<0;12;5M";
	EXPECT_EQ((ssize_t)(sizeof(press_seq) - 1), write(in_pipe[1], press_seq, sizeof(press_seq) - 1));
	terse_event_t ev;
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_MOUSE_DOWN, ev.type);
	EXPECT_EQ(TERSE_MOUSE_BUTTON_LEFT, ev.data.mouse.button);
	EXPECT_EQ(4, ev.data.mouse.row);
	EXPECT_EQ(11, ev.data.mouse.col);

	const char move_seq[] = "\x1b[<32;13;6M";
	EXPECT_EQ((ssize_t)(sizeof(move_seq) - 1), write(in_pipe[1], move_seq, sizeof(move_seq) - 1));
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_MOUSE_MOVE, ev.type);
	EXPECT_EQ(TERSE_MOUSE_BUTTON_LEFT, ev.data.mouse.button);
	EXPECT_EQ(5, ev.data.mouse.row);
	EXPECT_EQ(12, ev.data.mouse.col);

	const char release_seq[] = "\x1b[<0;13;6m";
	EXPECT_EQ((ssize_t)(sizeof(release_seq) - 1), write(in_pipe[1], release_seq, sizeof(release_seq) - 1));
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_MOUSE_UP, ev.type);
	EXPECT_EQ(TERSE_MOUSE_BUTTON_LEFT, ev.data.mouse.button);

	// Terminal sends 1-based coords, so col=1, row=1 becomes (0, 0) after conversion
	const char scroll_seq[] = "\x1b[<64;1;1M";
	EXPECT_EQ((ssize_t)(sizeof(scroll_seq) - 1), write(in_pipe[1], scroll_seq, sizeof(scroll_seq) - 1));
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_MOUSE_SCROLL, ev.type);
	EXPECT_EQ(TERSE_MOUSE_BUTTON_SCROLL_UP, ev.data.mouse.button);

	const char ctrl_press[] = "\x1b[<16;20;8M";
	EXPECT_EQ((ssize_t)(sizeof(ctrl_press) - 1), write(in_pipe[1], ctrl_press, sizeof(ctrl_press) - 1));
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_MOUSE_DOWN, ev.type);
	EXPECT_TRUE(ev.data.mouse.mods & TERSE_MOD_CTRL);

	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseMouse, EnableOnUnsupportedTerminal_IsNoop)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_MOUSE,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);
	EXPECT_EQ(0, terse_enable_mouse(handle, TERSE_MOUSE_SGR));
	char buffer[32];
	expect_no_bytes_available_fd(out_pipe[0]);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

#endif /* HAVE_POSIX_PIPE */
