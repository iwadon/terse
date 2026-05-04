#include "terse.h"
#include <attest/attest.h>

#include <errno.h>

#include "test_compat.h"

TEST(TerseValidateOptions, ReturnsZero_OnNull)
{
	errno = 0;
	EXPECT_EQ(TERSE_OK, terse_validate_options(NULL));
	EXPECT_EQ(0, errno);
}

TEST(TerseErrorInfo, ReturnsStateError_OnNullHandle)
{
	terse_error_t err = terse_get_last_error(NULL);
	EXPECT_EQ(TERSE_ERR_INVALID_HANDLE, err);
}

TEST(TerseValidateOptions, ReturnsEbaf_OnNegativeInputFd)
{
	terse_options_t options = {
		.input_fd = -1,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	errno = 0;
	terse_error_t rc = terse_validate_options(&options);
	EXPECT_EQ(TERSE_ERR_INVALID_HANDLE, rc);
	EXPECT_EQ(EBADF, errno);
}

TEST(TerseOpen, ReturnsNull_OnInvalidOptions)
{
	terse_options_t options = {
		.input_fd = -1,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
	};
	errno = 0;
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NULL(handle);
	EXPECT_EQ(EBADF, errno);
}

#ifdef HAVE_POSIX_PIPE
TEST(TerseGetOptions, CopiesHandleOptions_OnValidHandle)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name =
#if TERSE_HAVE_ICONV
			"Shift_JIS",
#else
			"UTF-8",
#endif
		.disabled_caps = TERSE_CAP_DISABLE_CLEAR_SCREEN,
		.enabled_caps = TERSE_CAP_ENABLE_TEXT_STYLES,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);

	terse_options_t got;
	EXPECT_EQ(TERSE_OK, terse_get_options(handle, &got));
	EXPECT_EQ(options.input_fd, got.input_fd);
	EXPECT_EQ(options.output_fd, got.output_fd);
	EXPECT_TRUE(got.codec_name == options.codec_name);
	EXPECT_EQ(options.disabled_caps, got.disabled_caps);
	EXPECT_EQ(options.enabled_caps, got.enabled_caps);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#if !TERSE_HAVE_ICONV
TEST(TerseOpen, ReturnsEnosys_OnShiftJisWithoutIconv)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "Shift_JIS",
	};

	errno = 0;
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NULL(handle);
	EXPECT_EQ(ENOSYS, errno);

	close(fds[0]);
	close(fds[1]);
}
#endif
#endif /* HAVE_POSIX_PIPE */

TEST(TerseCapabilities, DefaultsMatchP0)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_NOT_NULL(handle);
	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_EQ(TERSE_P0, caps.profile);
	EXPECT_EQ(1, caps.has_basic_output);
	EXPECT_EQ(1, caps.has_cursor_visibility);
	EXPECT_EQ(1, caps.has_move_absolute);
	EXPECT_EQ(1, caps.has_move_relative);
	EXPECT_EQ(1, caps.has_clear_line);
	EXPECT_EQ(1, caps.has_clear_screen);
	EXPECT_EQ(0, caps.has_size);
	EXPECT_EQ(0, caps.has_sgr_basic);
	EXPECT_EQ(0, caps.has_sgr_extended);
	EXPECT_EQ(0, caps.has_truecolor);
	EXPECT_EQ(0, caps.has_text_styles);
	EXPECT_EQ(TERSE_MOUSE_NONE, caps.mouse);
	EXPECT_EQ(0, caps.has_bracketed_paste);
	EXPECT_EQ(0, caps.has_title);
	EXPECT_EQ(0, caps.has_hyperlinks);
	EXPECT_EQ(0, caps.has_cursor_shape);
	EXPECT_EQ(0, caps.has_clipboard_write);
	EXPECT_EQ(TERSE_COLOR_NONE, caps.colors);
	EXPECT_EQ(0u, caps.effects);
	terse_close(handle);
}

TEST(TerseGetOptions, ReturnsEINVAL_OnNullOut)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_NOT_NULL(handle);
	errno = 0;
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_get_options(handle, NULL));
	EXPECT_EQ(EINVAL, errno);
	terse_close(handle);
}
