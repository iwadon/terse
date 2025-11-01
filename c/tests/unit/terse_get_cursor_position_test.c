#include "terse.h"
#include <attest/attest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

TEST(CursorPosition, Basic)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	// Move to a known position
	EXPECT_EQ(0, terse_move_to(handle, 5, 10));
	EXPECT_EQ(0, terse_flush(handle));

	// Query cursor position
	terse_cursor_position_t pos = terse_get_cursor_position(handle);

	// In a real terminal, this should return the position
	// In CI/non-TTY environment, it might not work
	if (pos.known) {
		EXPECT_EQ(5, pos.row);
		EXPECT_EQ(10, pos.col);
	}

	terse_close(handle);
}

TEST(CursorPosition, AfterText)
{
	terse_options_t options = {
		.input_fd = STDIN_FILENO,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NE(NULL, handle);

	// Move to position and write text
	EXPECT_EQ(0, terse_move_to(handle, 1, 1));
	EXPECT_EQ(0, terse_write_text(handle, "Hello"));
	EXPECT_EQ(0, terse_flush(handle));

	// Query cursor position (should be at column 6 after "Hello")
	terse_cursor_position_t pos = terse_get_cursor_position(handle);

	if (pos.known) {
		EXPECT_EQ(1, pos.row);
		EXPECT_EQ(6, pos.col);
	}

	terse_close(handle);
}

TEST(CursorPosition, InvalidHandle)
{
	terse_cursor_position_t pos = terse_get_cursor_position(NULL);
	EXPECT_EQ(0, pos.known);
}
