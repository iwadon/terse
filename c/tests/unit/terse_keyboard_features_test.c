#include "terse.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
	char *saved_term_program = save_env("TERM_PROGRAM");
	setenv("TERM_PROGRAM", "iTerm.app", 1);

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
	EXPECT_TRUE(handle != NULL);

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
	EXPECT_EQ(TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS, terse_keyboard_get_enabled(handle));

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

	restore_env("TERM_PROGRAM", saved_term_program);
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
	EXPECT_TRUE(handle != NULL);
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

int main(void)
{
	return RunAllTests();
}
