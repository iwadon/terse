#include "terse.h"
#include <attest/attest.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

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

TEST(TerseHyperlink, SetsHyperlink)
{
	int fds[2];
	EXPECT_EQ(0, pipe(fds));

	fcntl(fds[0], F_SETFL, O_NONBLOCK);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.enabled_caps = TERSE_CAP_ENABLE_HYPERLINK,
	};

	terse_handle_t handle = terse_open(TERSE_P2, &options);
	EXPECT_TRUE(handle != NULL);

	const char *url = "https://example.com";
	const char *label = "Example";

	EXPECT_EQ(0, terse_set_hyperlink(handle, url, label));

	close(fds[1]); // Close write end

	char *output = read_all(fds[0]);
	close(fds[0]);

	const char *expected = "\x1b]8;;https://example.com\x07"
		"Example\x1b]8;;\x07";
	size_t expected_len = strlen(expected);
	size_t output_len = strlen(output);

	if (expected_len != output_len) {
		fprintf(stderr, "\n[DEBUG] Length mismatch! Expected: %zu, Actual: %zu\n", expected_len, output_len);
	}
	EXPECT_EQ(expected_len, output_len);

	for (size_t i = 0; i < expected_len; ++i) {
		if (output[i] != expected[i]) {
			fprintf(stderr, "\n[DEBUG] Mismatch at index %zu! Expected: 0x%02x, Actual: 0x%02x\n", i, (unsigned char)expected[i], (unsigned char)output[i]);
			EXPECT_TRUE(0); // Fail fast
			break;
		}
	}

	// Final check with strcmp, which should now pass if the above loop does.
	EXPECT_EQ(0, strcmp(output, expected));

	free(output);
	terse_close(handle);
}

#endif /* HAVE_POSIX_PIPE */
