#include "terse.h"
#include <attest/attest.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Helper to read all data from a file descriptor until EOF.
static char *
read_all(int fd)
{
	char *buf = malloc(1024);
	size_t size = 1024;
	size_t pos = 0;
	ssize_t n;

	while ((n = read(fd, buf + pos, size - pos)) > 0) {
		pos += n;
		if (pos == size) {
			size *= 2;
			buf = realloc(buf, size);
		}
	}
	buf[pos] = '\0';
	return buf;
}

TEST(TerseStyle, AppliesBasicStyleAndEffects)
{
	int fds[2];
	EXPECT_EQ(0, pipe(fds));

	// Set the read end to non-blocking
	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	terse_options_t options = {
		.input_fd = fds[0], // Must be a valid FD
		.output_fd = fds[1],
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_TEXT_STYLES,
	};

	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_TRUE(handle != NULL);

	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	style.background = terse_color_basic(TERSE_BASIC_COLOR_CYAN, 1);
	style.effects = TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE;

	// This should write the SGR sequence to the pipe.
	int result = terse_set_style(handle, &style);
	EXPECT_EQ(0, result);

	// Close the write end to signal EOF to the reader.
	close(fds[1]);

	char *output = read_all(fds[0]);
	close(fds[0]);

	// The implementation first resets, then applies the new style.
	const char *expected = "\x1b[0m\x1b[1;4;31;106m";
	EXPECT_EQ(0, strcmp(output, expected));

	free(output);
	terse_close(handle);
}


TEST(TerseStyle, SetStyleIsNoOpForSameStyle)
{
	int fds[2];
	EXPECT_EQ(0, pipe(fds));

	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_TEXT_STYLES,
	};

	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_TRUE(handle != NULL);

	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_MAGENTA, 0);
	style.effects = TERSE_STYLE_BOLD;

	// 1. Set the style for the first time.
	EXPECT_EQ(0, terse_set_style(handle, &style));

	// 2. Drain the pipe.
	char drain[256];
	read(fds[0], drain, sizeof(drain));

	// 3. Set the exact same style again.
	EXPECT_EQ(0, terse_set_style(handle, &style));

	close(fds[1]); // Close write end

	char *output = read_all(fds[0]);
	close(fds[0]);

	// 4. Expect no output, as the style did not change.
	EXPECT_EQ(0, strcmp(output, ""));

	free(output);
	terse_close(handle);
}

TEST(TerseStyle, SetStyleDegradesTrueColorTo256)
{
	int fds[2];
	EXPECT_EQ(0, pipe(fds));

	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.enabled_caps = TERSE_CAP_ENABLE_SGR_EXTENDED, // Enable 256 colors
		.disabled_caps = TERSE_CAP_DISABLE_TRUECOLOR, // Disable truecolor
	};

	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_TRUE(handle != NULL);

	// This color should be approximated to index 209 in the 256-color palette.
	terse_style_t style = terse_style_default();
	style.foreground = terse_color_truecolor(255, 135, 95);

	EXPECT_EQ(0, terse_set_style(handle, &style));

	close(fds[1]); // Close write end

	char *output = read_all(fds[0]);
	close(fds[0]);

	// Expect reset, then the SGR sequence for the approximated 256-color.
	const char *expected = "\x1b[0m\x1b[38;5;209m";
	EXPECT_EQ(0, strcmp(output, expected));

	free(output);
	terse_close(handle);
}

TEST(TerseStyle, ResetStyleResetsToDefault)
{
	int fds[2];
	EXPECT_EQ(0, pipe(fds));

	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_TEXT_STYLES,
	};

	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_TRUE(handle != NULL);

	// Set an initial style
	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 1);
	style.effects = TERSE_STYLE_ITALIC;
	EXPECT_EQ(0, terse_set_style(handle, &style));

	// Drain the pipe to ignore the output from the initial set
	char drain[256];
	read(fds[0], drain, sizeof(drain));

	// Reset the style
	EXPECT_EQ(0, terse_reset_style(handle, TERSE_RESET_ALL));

	close(fds[1]); // Close write end

	char *output = read_all(fds[0]);
	close(fds[0]);

	const char *expected = "\x1b[0m";
	EXPECT_EQ(0, strcmp(output, expected));

	free(output);
	terse_close(handle);
}

int main(int argc, char **argv)
{
	return attest_main(argc, argv);
}
