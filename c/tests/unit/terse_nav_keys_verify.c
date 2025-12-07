#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

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
	EXPECT_NOT_NULL(*out_handle);
}

// Home key variants
TEST(TerseNavKeys, Home_CsiH)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[H";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, Home_Csi1Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[1~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, Home_SS3_H)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOH";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// End key variants
TEST(TerseNavKeys, End_CsiF)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[F";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_END, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, End_Csi4Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[4~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_END, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, End_SS3_F)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOF";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_END, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// PageUp key
TEST(TerseNavKeys, PageUp_Csi5Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[5~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_PAGE_UP, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// PageDown key
TEST(TerseNavKeys, PageDown_Csi6Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[6~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_PAGE_DOWN, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Insert key
TEST(TerseNavKeys, Insert_Csi2Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[2~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_INSERT, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Delete key
TEST(TerseNavKeys, Delete_Csi3Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[3~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_DELETE, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Test modifiers with navigation keys
TEST(TerseNavKeys, Home_WithShift)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[1;2~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, Insert_WithCtrl)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[2;5~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_INSERT, event.type);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseNavKeys, Delete_WithAlt)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[3;3~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_DELETE, event.type);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#endif /* HAVE_POSIX_PIPE */
