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

static terse_handle_t create_paste_handle(int out_pipe[2], int in_pipe[2], unsigned int enabled_caps)
{
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = enabled_caps,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	return handle;
}

TEST(TersePaste, EnableWritesSequence)
{
	int out_pipe[2];
	int in_pipe[2];
	terse_handle_t handle = create_paste_handle(out_pipe, in_pipe, TERSE_CAP_ENABLE_BRACKETED_PASTE);
	EXPECT_EQ(0, terse_enable_bracketed_paste(handle));
	char buffer[32];
	ssize_t n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b[?2004h") != NULL);
	EXPECT_EQ(0, terse_disable_bracketed_paste(handle));
	n = read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b[?2004l") != NULL);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TersePaste, GeneratesBeginEndEvents)
{
	int out_pipe[2];
	int in_pipe[2];
	terse_handle_t handle = create_paste_handle(out_pipe, in_pipe, TERSE_CAP_ENABLE_BRACKETED_PASTE);
	EXPECT_EQ(0, terse_enable_bracketed_paste(handle));
	char buffer[32];
	(void)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));

	const char begin_seq[] = "\x1b[200~";
	EXPECT_EQ((ssize_t)(sizeof(begin_seq) - 1), write(in_pipe[1], begin_seq, sizeof(begin_seq) - 1));
	terse_event_t ev;
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_PASTE_BEGIN, ev.type);

	const char end_seq[] = "\x1b[201~";
	EXPECT_EQ((ssize_t)(sizeof(end_seq) - 1), write(in_pipe[1], end_seq, sizeof(end_seq) - 1));
	EXPECT_EQ(TERSE_OK, terse_read_event(handle, 10, &ev));
	EXPECT_EQ(TERSE_EVENT_PASTE_END, ev.type);

	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TersePaste, NoopOnUnsupportedTerminal)
{
	int out_pipe[2];
	int in_pipe[2];
	terse_handle_t handle = create_paste_handle(out_pipe, in_pipe, 0);
	EXPECT_EQ(0, terse_enable_bracketed_paste(handle));
	expect_no_bytes_available_fd(out_pipe[0]);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

#endif /* HAVE_POSIX_PIPE */
