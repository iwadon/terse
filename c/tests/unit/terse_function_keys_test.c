#include "terse.h"
#include <attest/attest.h>

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

// VT100 Function Keys (F1-F4): ESC O P/Q/R/S
TEST(TerseFunctionKeys, F1_VT100_EscOP)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOP";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(1, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F2_VT100_EscOQ)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOQ";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(2, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F3_VT100_EscOR)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOR";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(3, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F4_VT100_EscOS)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1bOS";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(4, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// xterm Function Keys (F1-F12): ESC [ 11 ~ through ESC [ 24 ~
TEST(TerseFunctionKeys, F1_xterm_Csi11Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[11~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(1, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F5_xterm_Csi15Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[15~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(5, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F6_xterm_Csi17Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[17~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(6, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F7_xterm_Csi18Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[18~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(7, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F10_xterm_Csi21Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[21~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(10, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F12_xterm_Csi24Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[24~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(12, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Linux Console Function Keys (F1-F12): ESC [ [ A through ESC [ [ L
TEST(TerseFunctionKeys, F1_LinuxConsole_EscBracketBracketA)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[[A";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(1, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F2_LinuxConsole_EscBracketBracketB)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[[B";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(2, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F5_LinuxConsole_EscBracketBracketE)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[[E";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(5, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F10_LinuxConsole_EscBracketBracketJ)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[[J";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(10, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, F12_LinuxConsole_EscBracketBracketL)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[[L";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(12, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Function Keys with Modifiers (xterm style with modifier parameter)
TEST(TerseFunctionKeys, ShiftF1_Csi11_2Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[11;2~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(1, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, CtrlF5_Csi15_5Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[15;5~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(5, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, AltF10_Csi21_3Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[21;3~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(10, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, ShiftAltF12_Csi24_4Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[24;4~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(12, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_SHIFT | TERSE_MOD_ALT, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, ShiftCtrlF7_Csi18_6Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[18;6~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(7, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_SHIFT | TERSE_MOD_CTRL, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, CtrlAltF6_Csi17_7Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[17;7~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(6, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_ALT | TERSE_MOD_CTRL, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseFunctionKeys, ShiftCtrlAltF11_Csi23_8Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[23;8~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(11, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_CTRL, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}
