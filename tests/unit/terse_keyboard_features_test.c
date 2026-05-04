#include "terse.h"
#include <attest/attest.h>

#include "test_compat.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_POSIX_PIPE

static char *save_env(const char *name)
{
	const char *current = getenv(name);
	if (!current) {
		return NULL;
	}
	return strdup(current);
}

static void restore_env(const char *name, char *saved)
{
	if (saved) {
		setenv(name, saved, 1);
		free(saved);
	} else {
		unsetenv(name);
	}
}

static void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	EXPECT_TRUE(flags >= 0);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

TEST(TerseKeyboardFeatures, EnablesModifyOtherKeysAndTracksState)
{
	char *saved_term = save_env("TERM");
	char *saved_term_program = save_env("TERM_PROGRAM");
	char *saved_term_program_version = save_env("TERM_PROGRAM_VERSION");
	char *saved_lc_terminal = save_env("LC_TERMINAL");
	char *saved_lc_terminal_version = save_env("LC_TERMINAL_VERSION");
	char *saved_colorterm = save_env("COLORTERM");
	char *saved_gnome_screen = save_env("GNOME_TERMINAL_SCREEN");
	char *saved_gnome_service = save_env("GNOME_TERMINAL_SERVICE");
	char *saved_vte_version = save_env("VTE_VERSION");
	char *saved_kitty_pid = save_env("KITTY_PID");
	char *saved_wezterm_exec = save_env("WEZTERM_EXECUTABLE");
	char *saved_secondary_hint = save_env("TERSE_SECONDARY_DA_HINT");
	setenv("TERM", "xterm-256color", 1);
	setenv("TERM_PROGRAM", "WezTerm", 1);
	setenv("TERM_PROGRAM_VERSION", "20240203-110809-5046fc22", 1);
	unsetenv("LC_TERMINAL");
	unsetenv("LC_TERMINAL_VERSION");
	setenv("COLORTERM", "truecolor", 1);
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");
	unsetenv("KITTY_PID");
	setenv("WEZTERM_EXECUTABLE", "/Applications/WezTerm.app/Contents/MacOS/wezterm-gui", 1);
	setenv("TERSE_SECONDARY_DA_HINT", "\x1b[>1;277;0c", 1);
	EXPECT_TRUE(getenv("TERM_PROGRAM") != NULL);
	EXPECT_TRUE(strcmp("WezTerm", getenv("TERM_PROGRAM")) == 0);
	EXPECT_TRUE(getenv("COLORTERM") != NULL);
	EXPECT_TRUE(strcmp("truecolor", getenv("COLORTERM")) == 0);
	EXPECT_TRUE(getenv("LC_TERMINAL") == NULL);
	EXPECT_TRUE(getenv("GNOME_TERMINAL_SERVICE") == NULL);
	const char *secondary_hint = getenv("TERSE_SECONDARY_DA_HINT");
	EXPECT_NOT_NULL(secondary_hint);
	EXPECT_EQ(11u, (unsigned)strlen(secondary_hint));

	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	set_nonblocking(fds[0]);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	EXPECT_NOT_NULL(handle);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(TERSE_P3, caps.profile);
	EXPECT_EQ(TERSE_IMAGE_KITTY, caps.images);
	EXPECT_TRUE((caps.keyboard_features & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) != 0);

	unsigned int supported = terse_keyboard_get_supported(handle);
	EXPECT_TRUE((supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) != 0);
	EXPECT_EQ(0u, terse_keyboard_get_enabled(handle));

	EXPECT_EQ(0, terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS));

	char buffer[32] = { 0 };
	const char *enable_seq = "\x1b[>4;2m";
	errno = 0;
	ssize_t n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ((size_t)n, strlen(enable_seq));
	EXPECT_TRUE(memcmp(buffer, enable_seq, strlen(enable_seq)) == 0);
	EXPECT_TRUE((terse_keyboard_get_enabled(handle) & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) != 0);

	EXPECT_EQ(0, terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS));
	errno = 0;
	n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n < 0);
	EXPECT_EQ(EAGAIN, errno);

	EXPECT_EQ(0, terse_keyboard_disable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS));
	const char *disable_seq = "\x1b[>4;0m";
	errno = 0;
	n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ((size_t)n, strlen(disable_seq));
	EXPECT_TRUE(memcmp(buffer, disable_seq, strlen(disable_seq)) == 0);
	EXPECT_EQ(0u, terse_keyboard_get_enabled(handle));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);

	restore_env("TERSE_SECONDARY_DA_HINT", saved_secondary_hint);
	restore_env("WEZTERM_EXECUTABLE", saved_wezterm_exec);
	restore_env("KITTY_PID", saved_kitty_pid);
	restore_env("VTE_VERSION", saved_vte_version);
	restore_env("GNOME_TERMINAL_SERVICE", saved_gnome_service);
	restore_env("GNOME_TERMINAL_SCREEN", saved_gnome_screen);
	restore_env("COLORTERM", saved_colorterm);
	restore_env("LC_TERMINAL_VERSION", saved_lc_terminal_version);
	restore_env("LC_TERMINAL", saved_lc_terminal);
	restore_env("TERM_PROGRAM_VERSION", saved_term_program_version);
	restore_env("TERM_PROGRAM", saved_term_program);
	restore_env("TERM", saved_term);
}

TEST(TerseKeyboardFeatures, EnableDegradesWhenUnsupported)
{
	char *saved_term_program = save_env("TERM_PROGRAM");
	setenv("TERM_PROGRAM", "Apple_Terminal", 1);

	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	set_nonblocking(fds[0]);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};

	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	EXPECT_NOT_NULL(handle);
	EXPECT_EQ(0u, terse_keyboard_get_supported(handle));

	EXPECT_EQ(0, terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS));
	errno = 0;
	char buffer[8];
	ssize_t n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n < 0);
	EXPECT_EQ(EAGAIN, errno);
	EXPECT_EQ(0u, terse_keyboard_get_enabled(handle));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);

	restore_env("TERM_PROGRAM", saved_term_program);
}

TEST(TerseKeyboardFeatures, KittyProtocolHandshake)
{
	char *saved_term = save_env("TERM");
	char *saved_term_program = save_env("TERM_PROGRAM");
	char *saved_lc_terminal = save_env("LC_TERMINAL");
	char *saved_colorterm = save_env("COLORTERM");
	char *saved_gnome_screen = save_env("GNOME_TERMINAL_SCREEN");
	char *saved_gnome_service = save_env("GNOME_TERMINAL_SERVICE");
	char *saved_vte_version = save_env("VTE_VERSION");
	setenv("TERM", "xterm-kitty", 1);
	unsetenv("TERM_PROGRAM");
	unsetenv("LC_TERMINAL");
	unsetenv("COLORTERM");
	unsetenv("GNOME_TERMINAL_SCREEN");
	unsetenv("GNOME_TERMINAL_SERVICE");
	unsetenv("VTE_VERSION");

	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);
	set_nonblocking(fds[0]);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
	EXPECT_NOT_NULL(handle);
	EXPECT_TRUE((terse_keyboard_get_supported(handle) & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) != 0);
	EXPECT_EQ(0, terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL));
	char buffer[32] = { 0 };
	const char *enable_seq = "\x1b[>1u";
	errno = 0;
	ssize_t n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ((size_t)n, strlen(enable_seq));
	EXPECT_TRUE(memcmp(buffer, enable_seq, strlen(enable_seq)) == 0);
	EXPECT_TRUE((terse_keyboard_get_enabled(handle) & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) != 0);
	EXPECT_EQ(0, terse_keyboard_disable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL));
	const char *disable_seq = "\x1b[<u";
	errno = 0;
	n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n > 0);
	EXPECT_EQ((size_t)n, strlen(disable_seq));
	EXPECT_TRUE(memcmp(buffer, disable_seq, strlen(disable_seq)) == 0);
	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
	restore_env("TERM", saved_term);
	restore_env("TERM_PROGRAM", saved_term_program);
	restore_env("LC_TERMINAL", saved_lc_terminal);
	restore_env("COLORTERM", saved_colorterm);
	restore_env("GNOME_TERMINAL_SCREEN", saved_gnome_screen);
	restore_env("GNOME_TERMINAL_SERVICE", saved_gnome_service);
	restore_env("VTE_VERSION", saved_vte_version);
}

#endif /* HAVE_POSIX_PIPE */
