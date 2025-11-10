#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static char *save_env_value(const char *name)
{
	const char *value = getenv(name);
	if (!value) {
		return NULL;
	}
	char *copy = strdup(value);
	EXPECT_TRUE(copy != NULL);
	return copy;
}

static void restore_env_value(const char *name, char *saved)
{
	if (saved) {
		setenv(name, saved, 1);
		free(saved);
	} else {
		unsetenv(name);
	}
}

TEST(TerseImage, WritesItermSequence)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x01, 0x02, 0x03 };
	EXPECT_EQ(0, terse_display_image_inline(handle, payload, sizeof(payload), "demo"));
	char buffer[256];
	size_t n = (size_t)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b]1337;File=name=") != NULL);
	EXPECT_TRUE(strstr(buffer, ";inline=1:") != NULL);
	EXPECT_TRUE(strstr(buffer, "AQID") != NULL);
	expect_no_bytes_available_fd(out_pipe[0]);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, WritesItermSequenceViaRequest)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0xAA, 0xBB, 0xCC };
	terse_image_request_t request = {
		.data = payload,
		.size = sizeof(payload),
		.name = "req",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE,
	};
	EXPECT_EQ(0, terse_display_image(handle, &request));
	char buffer[256];
	size_t n = (size_t)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buffer, "\x1b]1337;File=name=") != NULL);
	EXPECT_TRUE(strstr(buffer, ";inline=1:") != NULL);
	EXPECT_TRUE(strstr(buffer, "qrvM") != NULL);
	expect_no_bytes_available_fd(out_pipe[0]);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, NoopWhenDisabled)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = TERSE_CAP_DISABLE_IMAGE_INLINE,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x01, 0x02, 0x03 };
	EXPECT_EQ(0, terse_display_image_inline(handle, payload, sizeof(payload), "demo"));
	set_nonblocking(out_pipe[0]);
	char tmp[16];
	errno = 0;
	ssize_t n = read(out_pipe[0], tmp, sizeof(tmp));
	EXPECT_TRUE(n == -1);
	EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, WritesSixelSequenceWhenAvailable)
{
	char *saved_term = save_env_value("TERM");
	char *saved_term_program = save_env_value("TERM_PROGRAM");
	char *saved_lc_terminal = save_env_value("LC_TERMINAL");
	char *saved_wezterm = save_env_value("WEZTERM_EXECUTABLE");
	char *saved_kitty = save_env_value("KITTY_PID");
	char *saved_da = save_env_value("TERSE_SECONDARY_DA_HINT");
	char *saved_tmux = save_env_value("TMUX");
	char *saved_colorterm = save_env_value("COLORTERM");
	char *saved_gnome_screen = save_env_value("GNOME_TERMINAL_SCREEN");
	char *saved_gnome_service = save_env_value("GNOME_TERMINAL_SERVICE");
	char *saved_vte_version = save_env_value("VTE_VERSION");
	setenv("TERM", "xterm-sixel", 1);
	unsetenv("TERM_PROGRAM");
	unsetenv("LC_TERMINAL");
	unsetenv("WEZTERM_EXECUTABLE");
	unsetenv("KITTY_PID");
	unsetenv("TERSE_SECONDARY_DA_HINT");
	unsetenv("TMUX");
	unsetenv("COLORTERM");
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(TERSE_IMAGE_SIXEL, caps.images);
	unsigned char payload[] = "#$-!";
	terse_image_request_t request = {
		.data = payload,
		.size = sizeof(payload) - 1,
		.name = "sixel",
		.format = TERSE_IMAGE_FORMAT_SIXEL,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE,
	};
	EXPECT_EQ(0, terse_display_image(handle, &request));
	char buffer[256] = { 0 };
	size_t n = (size_t)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ('\x1b', buffer[0]);
	EXPECT_EQ('P', buffer[1]);
	EXPECT_EQ('q', buffer[2]);
	EXPECT_TRUE(strstr(buffer, "\x1b\\") != NULL);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
	restore_env_value("TERM", saved_term);
	restore_env_value("TERM_PROGRAM", saved_term_program);
	restore_env_value("LC_TERMINAL", saved_lc_terminal);
	restore_env_value("WEZTERM_EXECUTABLE", saved_wezterm);
	restore_env_value("KITTY_PID", saved_kitty);
	restore_env_value("TERSE_SECONDARY_DA_HINT", saved_da);
	restore_env_value("TMUX", saved_tmux);
	restore_env_value("COLORTERM", saved_colorterm);
	restore_env_value("GNOME_TERMINAL_SCREEN", saved_gnome_screen);
	restore_env_value("GNOME_TERMINAL_SERVICE", saved_gnome_service);
	restore_env_value("VTE_VERSION", saved_vte_version);
}

TEST(TerseImage, DegradesWhenFormatMismatch)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x11, 0x22 };
	terse_image_request_t request = {
		.data = payload,
		.size = sizeof(payload),
		.name = "mismatch",
		.format = TERSE_IMAGE_FORMAT_SIXEL,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};
	EXPECT_EQ(0, terse_display_image(handle, &request));
	set_nonblocking(out_pipe[0]);
	char tmp[8];
	errno = 0;
	ssize_t n = read(out_pipe[0], tmp, sizeof(tmp));
	EXPECT_TRUE(n == -1);
	EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, WritesKittySequenceWhenAvailable)
{
	char *saved_term = save_env_value("TERM");
	char *saved_term_program = save_env_value("TERM_PROGRAM");
	char *saved_lc_terminal = save_env_value("LC_TERMINAL");
	char *saved_wezterm = save_env_value("WEZTERM_EXECUTABLE");
	char *saved_kitty = save_env_value("KITTY_PID");
	char *saved_da = save_env_value("TERSE_SECONDARY_DA_HINT");
	char *saved_tmux = save_env_value("TMUX");
	char *saved_colorterm = save_env_value("COLORTERM");
	char *saved_gnome_screen = save_env_value("GNOME_TERMINAL_SCREEN");
	char *saved_gnome_service = save_env_value("GNOME_TERMINAL_SERVICE");
	char *saved_vte_version = save_env_value("VTE_VERSION");
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "WezTerm", 1);
	unsetenv("LC_TERMINAL");
	setenv("WEZTERM_EXECUTABLE", "/Applications/WezTerm.app/Contents/MacOS/wezterm-gui", 1);
	unsetenv("KITTY_PID");
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>1;277;0c", 1);
	unsetenv("TMUX");
	unsetenv("COLORTERM");
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	EXPECT_TRUE(handle != NULL);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(TERSE_IMAGE_KITTY, caps.images);
	unsigned char payload[] = { 0x01, 0x02, 0x03 };
	terse_image_request_t request = {
		.data = payload,
		.size = sizeof(payload),
		.name = "kitty",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE,
	};
	EXPECT_EQ(0, terse_display_image(handle, &request));
	char buffer[256] = { 0 };
	size_t n = (size_t)read_pipe_data(out_pipe[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ('\x1b', buffer[0]);
	EXPECT_EQ('G', buffer[2]);
	EXPECT_TRUE(strstr(buffer, "a=T") != NULL);
	EXPECT_TRUE(strstr(buffer, "AQID") != NULL);
	EXPECT_TRUE(strstr(buffer, "\x1b\\") != NULL);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
	restore_env_value("TERM", saved_term);
	restore_env_value("TERM_PROGRAM", saved_term_program);
	restore_env_value("LC_TERMINAL", saved_lc_terminal);
	restore_env_value("WEZTERM_EXECUTABLE", saved_wezterm);
	restore_env_value("KITTY_PID", saved_kitty);
	restore_env_value("TERSE_SECONDARY_DA_HINT", saved_da);
	restore_env_value("TMUX", saved_tmux);
	restore_env_value("COLORTERM", saved_colorterm);
	restore_env_value("GNOME_TERMINAL_SCREEN", saved_gnome_screen);
	restore_env_value("GNOME_TERMINAL_SERVICE", saved_gnome_service);
	restore_env_value("VTE_VERSION", saved_vte_version);
}

TEST(TerseImage, ErrorsWhenFormatMismatchWithoutDegrade)
{
	int out_pipe[2];
	int in_pipe[2];
	EXPECT_TRUE(pipe(out_pipe) == 0);
	EXPECT_TRUE(pipe(in_pipe) == 0);
	terse_options_t options = {
		.input_fd = in_pipe[0],
		.output_fd = out_pipe[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = TERSE_CAP_ENABLE_IMAGE_INLINE,
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);
	unsigned char payload[] = { 0x44, 0x55 };
	terse_image_request_t request = {
		.data = payload,
		.size = sizeof(payload),
		.name = "strict",
		.format = TERSE_IMAGE_FORMAT_SIXEL,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_INLINE,
	};
	EXPECT_EQ(-ENOTSUP, terse_display_image(handle, &request));
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(ENOTSUP, err.code);
	terse_close(handle);
	close(out_pipe[0]);
	close(out_pipe[1]);
	close(in_pipe[0]);
	close(in_pipe[1]);
}

TEST(TerseImage, ErrorsOnInvalidRequest)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(-EINVAL, terse_display_image(handle, NULL));
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}

TEST(TerseImage, ErrorsOnMissingData)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	terse_image_request_t request = {
		.data = NULL,
		.size = 10,
		.name = "bad",
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = 0,
	};
	EXPECT_EQ(-EINVAL, terse_display_image(handle, &request));
	terse_error_info_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERROR_CONFIG, err.category);
	EXPECT_EQ(EINVAL, err.code);
	terse_close(handle);
}
