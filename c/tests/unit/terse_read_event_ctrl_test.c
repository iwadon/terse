#include "terse.h"
#include <attest/attest.h>

#include <unistd.h>

static void assert_ctrl_char(unsigned int expected_scalar, unsigned int expected_mod, terse_event_t event)
{
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(expected_scalar, event.data.ch.scalar);
	EXPECT_EQ(expected_mod, (unsigned int)event.data.ch.mods);
}

TEST(TerseReadEventCtrl, ReportsCtrlModifier_OnControlSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	const char ctrl_c[] = "\x03";
	EXPECT_TRUE(write(fds[1], ctrl_c, sizeof(ctrl_c) - 1) == (ssize_t)(sizeof(ctrl_c) - 1));

	terse_event_t event;
	int rc = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, rc);
	assert_ctrl_char('c', TERSE_MOD_CTRL, event);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}
