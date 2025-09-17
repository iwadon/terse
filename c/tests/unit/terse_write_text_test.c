#include "terse.h"
#include "test.h"

#include <string.h>
#include <unistd.h>

TEST(TerseWriteText, Succeeds_OnPipeOutput)
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

	const char *message = "hello";
	EXPECT_EQ(0, terse_write_text(handle, message));
	EXPECT_EQ(0, terse_flush(handle));

	char buffer[16] = { 0 };
	ssize_t n = read(fds[0], buffer, sizeof(buffer));
	EXPECT_TRUE(n >= (ssize_t)strlen(message));
	EXPECT_TRUE(strncmp(buffer, message, strlen(message)) == 0);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

int main()
{
	return RunAllTests();
}
