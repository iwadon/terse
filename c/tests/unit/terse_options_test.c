#include "terse.h"
#include "test.h"

#include <errno.h>
#include <unistd.h>

TEST(TerseValidateOptions, ReturnsZero_OnNull)
{
	errno = 0;
	EXPECT_EQ(0, terse_validate_options(NULL));
	EXPECT_EQ(0, errno);
}

TEST(TerseErrorInfo, ReturnsStateError_OnNullHandle)
{
	terse_error_info_t info = terse_get_last_error(NULL);
	EXPECT_EQ(TERSE_ERROR_STATE, info.category);
	EXPECT_EQ(EINVAL, info.code);
}

TEST(TerseValidateOptions, ReturnsEbaf_OnNegativeInputFd)
{
	terse_options_t options = {
		.input_fd = -1,
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
	};
	errno = 0;
	int rc = terse_validate_options(&options);
	EXPECT_EQ(-EBADF, rc);
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
	EXPECT_TRUE(handle == NULL);
	EXPECT_EQ(EBADF, errno);
}

TEST(TerseGetOptions, CopiesHandleOptions_OnValidHandle)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "Shift_JIS",
		.disabled_caps = TERSE_CAP_DISABLE_CLEAR_SCREEN,
	};

	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(handle != NULL);

	terse_options_t got;
	EXPECT_EQ(0, terse_get_options(handle, &got));
	EXPECT_EQ(options.input_fd, got.input_fd);
	EXPECT_EQ(options.output_fd, got.output_fd);
	EXPECT_TRUE(got.codec_name == options.codec_name);
	EXPECT_EQ(options.disabled_caps, got.disabled_caps);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseGetOptions, ReturnsEINVAL_OnNullOut)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	errno = 0;
	EXPECT_EQ(-EINVAL, terse_get_options(handle, NULL));
	EXPECT_EQ(EINVAL, errno);
	terse_close(handle);
}

int main()
{
	return RunAllTests();
}
