#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void create_pipe_handle(terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(*out_handle != NULL);
}

static ssize_t read_pipe(int fd, char *buffer, size_t size)
{
	int flags = fcntl(fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);

	memset(buffer, 0, size);
	ssize_t n = read(fd, buffer, size);
	if (n < 0 && errno == EAGAIN) {
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
		nanosleep(&ts, NULL); // allow write side to flush
		n = read(fd, buffer, size);
	}
	return n;
}

static void expect_no_bytes_available(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
	char tmp[16];
	errno = 0;
	ssize_t n = read(fd, tmp, sizeof(tmp));
	EXPECT_TRUE(n == -1);
	EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
}

static terse_style_t make_effects_style(unsigned int effects)
{
	terse_style_t style = terse_style_default();
	style.effects = effects;
	return style;
}

static terse_style_t make_basic_foreground_style(terse_basic_color_t color, int bright)
{
	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(color, bright);
	return style;
}

static terse_style_t make_palette_foreground_style(unsigned char index)
{
	terse_style_t style = terse_style_default();
	style.foreground = terse_color_palette(index);
	return style;
}

static terse_style_t make_truecolor_style(unsigned char r, unsigned char g, unsigned char b)
{
	terse_style_t style = terse_style_default();
	style.foreground = terse_color_truecolor(r, g, b);
	return style;
}

TEST(TerseClearScreen, EmitsAfterSequence_OnAfter)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_screen(handle, TERSE_CLEAR_AFTER));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[J") != NULL);

	close(fds[0]);
}

TEST(TerseOutputCapabilities, NoOutput_WhenClearScreenDisabled)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_CLEAR_SCREEN,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	EXPECT_EQ(0, terse_clear_screen(handle, TERSE_CLEAR_ALL));
	expect_no_bytes_available(fds[0]);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputCapabilities, NoOutput_WhenMoveDisabled)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_MOVE_ABSOLUTE | TERSE_CAP_DISABLE_MOVE_RELATIVE,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	EXPECT_EQ(0, terse_move_to(handle, 4, 4));
	expect_no_bytes_available(fds[0]);
	EXPECT_EQ(0, terse_move_by(handle, 3, -2));
	expect_no_bytes_available(fds[0]);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputCapabilities, NoOutput_WhenCursorHiddenUnsupported)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_CURSOR_VISIBILITY,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	EXPECT_EQ(0, terse_show_cursor(handle, 0));
	expect_no_bytes_available(fds[0]);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputCapabilities, NoOutput_WhenBasicOutputDisabled)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_BASIC_OUTPUT,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	EXPECT_EQ(0, terse_write_text(handle, "hello"));
	expect_no_bytes_available(fds[0]);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputState, SkipsDuplicateCursorHide)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_show_cursor(handle, 0));
	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[?25l") != NULL);

	EXPECT_EQ(0, terse_show_cursor(handle, 0));
	expect_no_bytes_available(fds[0]);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputState, SkipsDuplicateMoveTo)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_move_to(handle, 4, 9));
	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[5;10H") != NULL);

	EXPECT_EQ(0, terse_move_to(handle, 4, 9));
	expect_no_bytes_available(fds[0]);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseOutputError, ReturnsConfigError_OnInvalidMode)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	int rc = terse_clear_screen(handle, 99);
	EXPECT_EQ(-EINVAL, rc);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}

TEST(TerseOutputError, ReturnsTransportError_OnWriteFailure)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	close(fds[1]);
	int rc = terse_write_text(handle, "hi");
	EXPECT_EQ(-EBADF, rc);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_TRANSPORT, err.category);
	EXPECT_EQ(EBADF, err.code);
	terse_close(handle);
	close(fds[0]);
}

TEST(TerseClearScreen, EmitsAllSequence_OnAll)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_screen(handle, TERSE_CLEAR_ALL));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[2J") != NULL);

	close(fds[0]);
}

TEST(TerseClearLine, EmitsModeSequence_OnBefore)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_clear_line(handle, TERSE_CLEAR_BEFORE));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[1K") != NULL);

	close(fds[0]);
}

TEST(TerseMoveTo, ConvertsZeroBasedToOneBased)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	// Discard initialization  output
	char init_buf[128];
	(void)read_pipe(fds[0], init_buf, sizeof(init_buf));

	// Move somewhere first, then to (0, 0) - this ensures the second move is not skipped
	EXPECT_EQ(0, terse_move_to(handle, 5, 10));
	(void)read_pipe(fds[0], init_buf, sizeof(init_buf)); // Discard

	// Now move to API coords (0, 0) - should generate terminal coords (1, 1)
	EXPECT_EQ(0, terse_move_to(handle, 0, 0));

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	// API (0, 0) becomes terminal (1, 1)
	EXPECT_TRUE(strstr(buf, "\x1b[1;1H") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseMoveBy, EmitsDirectionalSequence_OnOffsets)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_move_by(handle, 2, -3));
	terse_close(handle);
	close(fds[1]);

	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[2B") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[3D") != NULL);

	close(fds[0]);
}

TEST(TerseShowCursor, EmitsHideSequence_OnFalse)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_show_cursor(handle, 0));
	terse_close(handle);
	close(fds[1]);

	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[?25l") != NULL);

	close(fds[0]);
}

TEST(TerseClearScreen, ReturnsEINVAL_OnInvalidMode)
{
	errno = 0;
	int result = terse_clear_screen(NULL, TERSE_CLEAR_AFTER);
	EXPECT_EQ(-EINVAL, result);
	EXPECT_EQ(EINVAL, errno);
}

TEST(TerseWriteText, ReturnsEBadf_OnUnreadableFd)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[0],
		.codec_name = "UTF-8",
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	close(fds[1]);

	errno = 0;
	int rc = terse_write_text(handle, "hi");
	EXPECT_EQ(-EBADF, rc);
	EXPECT_EQ(EBADF, errno);

	terse_close(handle);
	close(fds[0]);
}

TEST(TerseStateCapture, RestoresCursorPositionAndVisibility)
{
	int fds1[2];
	EXPECT_TRUE(pipe(fds1) == 0);
	terse_options_t opt1 = {
		.input_fd = fds1[0],
		.output_fd = fds1[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};
	terse_handle_t handle1 = terse_open(TERSE_P0, &opt1);
	EXPECT_TRUE(handle1 != NULL);
	EXPECT_EQ(0, terse_move_to(handle1, 3, 6));
	EXPECT_EQ(0, terse_show_cursor(handle1, 0));
	terse_style_t capture_style = make_effects_style(TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE);
	EXPECT_EQ(0, terse_set_style(handle1, &capture_style));
	terse_state_t state;
	EXPECT_EQ(0, terse_capture_state(handle1, &state));
	terse_close(handle1);
	close(fds1[0]);
	close(fds1[1]);

	int fds2[2];
	EXPECT_TRUE(pipe(fds2) == 0);
	terse_options_t opt2 = {
		.input_fd = fds2[0],
		.output_fd = fds2[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};
	terse_handle_t handle2 = terse_open(TERSE_P0, &opt2);
	EXPECT_TRUE(handle2 != NULL);
	EXPECT_EQ(0, terse_restore_state(handle2, &state));
	char buf[64];
	ssize_t n = read_pipe(fds2[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[4;7H") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[?25l") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[1;4m") != NULL);
	terse_close(handle2);
	close(fds2[0]);
	close(fds2[1]);
}

TEST(TerseSetStyle, EmitsSequences_WhenTextStylesEnabled)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t bold_underline = make_effects_style(TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE);
	EXPECT_EQ(0, terse_set_style(handle, &bold_underline));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[1;4m") != NULL);
	EXPECT_EQ(0, terse_set_style(handle, &bold_underline));
	expect_no_bytes_available(fds[0]);
	terse_style_t reset_style = terse_style_default();
	EXPECT_EQ(0, terse_set_style(handle, &reset_style));
	char buf2[32];
	ssize_t n2 = read_pipe(fds[0], buf2, sizeof(buf2));
	EXPECT_TRUE(n2 > 0);
	EXPECT_TRUE(strstr(buf2, "\x1b[0m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, EmitsFaintAndBlinkSequences)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t style = make_effects_style(TERSE_STYLE_FAINT | TERSE_STYLE_BLINK);
	EXPECT_EQ(0, terse_set_style(handle, &style));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[2;5m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, NoOutput_WhenCapabilityDisabled)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);
	terse_style_t bold_only = make_effects_style(TERSE_STYLE_BOLD);
	EXPECT_EQ(0, terse_set_style(handle, &bold_only));
	expect_no_bytes_available(fds[0]);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_NONE, err.category);
	EXPECT_EQ(0, err.code);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, EmitsBasicColorSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t red = make_basic_foreground_style(TERSE_BASIC_COLOR_RED, 0);
	EXPECT_EQ(0, terse_set_style(handle, &red));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[31m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, DegradesPaletteToBasicWhenOnlyBasicSupported)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t palette_red = make_palette_foreground_style(196);
	EXPECT_EQ(0, terse_set_style(handle, &palette_red));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[91m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, EmitsPaletteColorSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_SGR_EXTENDED
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t palette = make_palette_foreground_style(82);
	EXPECT_EQ(0, terse_set_style(handle, &palette));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[38;5;82m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseSetStyle, EmitsTruecolorSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t truecolor = make_truecolor_style(12, 34, 200);
	EXPECT_EQ(0, terse_set_style(handle, &truecolor));
	char buf[96];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[38;2;12;34;200m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseResetStyle, EmitsAllResetSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES | TERSE_CAP_ENABLE_SGR_BASIC
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t style = terse_style_default();
	style.effects = TERSE_STYLE_BOLD;
	style.foreground.kind = TERSE_COLOR_KIND_BASIC16;
	style.foreground.data.basic16.color = TERSE_BASIC_COLOR_RED;
	EXPECT_EQ(0, terse_set_style(handle, &style));
	char drain[64];
	EXPECT_TRUE(read_pipe(fds[0], drain, sizeof(drain)) > 0);
	EXPECT_EQ(0, terse_reset_style(handle, TERSE_RESET_ALL));
	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[0m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseResetStyle, EmitsColorResetSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t style = terse_style_default();
	style.foreground.kind = TERSE_COLOR_KIND_BASIC16;
	style.foreground.data.basic16.color = TERSE_BASIC_COLOR_BLUE;
	EXPECT_EQ(0, terse_set_style(handle, &style));
	char drain[64];
	EXPECT_TRUE(read_pipe(fds[0], drain, sizeof(drain)) > 0);
	EXPECT_EQ(0, terse_reset_style(handle, TERSE_RESET_COLOR_ONLY));
	char buf[32];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[39;49m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseResetStyle, EmitsEffectResetSequence)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	terse_style_t style = make_effects_style(TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE);
	EXPECT_EQ(0, terse_set_style(handle, &style));
	char drain[64];
	EXPECT_TRUE(read_pipe(fds[0], drain, sizeof(drain)) > 0);
	EXPECT_EQ(0, terse_reset_style(handle, TERSE_RESET_EFFECTS_ONLY));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[22;23;24;27;29m") != NULL);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseResetStyle, NoOutputWhenCapabilitiesDisabled)
{
	int fds[2];
	terse_handle_t handle;
	create_pipe_handle(&handle, fds);
	EXPECT_EQ(0, terse_reset_style(handle, TERSE_RESET_COLOR_ONLY));
	expect_no_bytes_available(fds[0]);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseResetStyle, ReturnsEINVAL_OnInvalidScope)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	errno = 0;
	int rc = terse_reset_style(handle, (terse_reset_scope_t)99);
	EXPECT_EQ(-EINVAL, rc);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}

TEST(TerseStateCapture, ReturnsConfigError_OnNullState)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	int rc = terse_capture_state(handle, NULL);
	EXPECT_EQ(-EINVAL, rc);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}

TEST(TerseStateRestore, ReturnsConfigError_OnNullState)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	int rc = terse_restore_state(handle, NULL);
	EXPECT_EQ(-EINVAL, rc);
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}
