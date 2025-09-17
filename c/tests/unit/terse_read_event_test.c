#include "terse.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void create_input_handle(terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(*out_handle != NULL);
}

TEST(TerseReadEvent, ReturnsNone_OnTimeout)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	terse_event_t event;
	int result = terse_read_event(handle, 10, &event);
	EXPECT_EQ(TERSE_EVENT_NONE, result);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsChar_OnAsciiInput)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = 'A';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);
	EXPECT_EQ(0, event.data.ch.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEnter_OnNewline)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = '\n';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsArrow_OnEscapeSequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[A";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ARROW_UP, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsCtrlChar_WithControlModifier)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = 0x01; // Ctrl+A
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.ch.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsArrowWithShift_OnModifierSequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[1;2A";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ARROW_UP, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsResize_OnCsi8Sequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[8;24;80t";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_RESIZE, event.type);
	EXPECT_EQ(24, event.data.resize.rows);
	EXPECT_EQ(80, event.data.resize.cols);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsRawSequence_OnUnknownEscape)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b]0;title\x07";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_RAW_SEQUENCE, event.type);
	EXPECT_TRUE(event.data.raw.length >= 2);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEINVAL_OnNullArguments)
{
	terse_event_t event;
	errno = 0;
	EXPECT_EQ(-EINVAL, terse_read_event(NULL, 0, &event));
	EXPECT_EQ(EINVAL, errno);

	errno = 0;
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(-EINVAL, terse_read_event(handle, 0, NULL));
	EXPECT_EQ(EINVAL, errno);
	terse_close(handle);
}

TEST(TerseReadEvent, ReturnsEpipe_OnPipeClosed)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);
	close(fds[1]); // close writer end to force EOF

	terse_event_t event;
	errno = 0;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(-EPIPE, result);
	EXPECT_EQ(EPIPE, errno);

	terse_close(handle);
	close(fds[0]);
}

int main()
{
	return RunAllTests();
}
