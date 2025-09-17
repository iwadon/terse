#include "terse.h"
#include "test.h"

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

int main()
{
	return RunAllTests();
}
