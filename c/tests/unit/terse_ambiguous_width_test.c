#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

static void create_input_handle_with_ambiguous_option(int ambiguous_as_wide, terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = STDOUT_FILENO,
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.east_asian_ambiguous_as_wide = ambiguous_as_wide,
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(*out_handle);
}

// Test East Asian Ambiguous Width characters as narrow (default: width = 1)

TEST(TerseAmbiguousWidth, PlusMinusSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00B1 (±) PLUS-MINUS SIGN
	const char seq[] = "\xC2\xB1";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B1U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, SectionSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00A7 (§) SECTION SIGN
	const char seq[] = "\xC2\xA7";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00A7U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, PilcrowSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00B6 (¶) PILCROW SIGN
	const char seq[] = "\xC2\xB6";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B6U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, DegreeSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00B0 (°) DEGREE SIGN
	const char seq[] = "\xC2\xB0";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B0U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, MultiplicationSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00D7 (×) MULTIPLICATION SIGN
	const char seq[] = "\xC3\x97";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00D7U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, DivisionSign_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+00F7 (÷) DIVISION SIGN
	const char seq[] = "\xC3\xB7";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00F7U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Test East Asian Ambiguous Width characters as wide (width = 2)

TEST(TerseAmbiguousWidth, PlusMinusSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00B1 (±) PLUS-MINUS SIGN
	const char seq[] = "\xC2\xB1";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B1U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, SectionSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00A7 (§) SECTION SIGN
	const char seq[] = "\xC2\xA7";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00A7U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, PilcrowSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00B6 (¶) PILCROW SIGN
	const char seq[] = "\xC2\xB6";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B6U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, DegreeSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00B0 (°) DEGREE SIGN
	const char seq[] = "\xC2\xB0";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00B0U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, MultiplicationSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00D7 (×) MULTIPLICATION SIGN
	const char seq[] = "\xC3\x97";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00D7U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, DivisionSign_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+00F7 (÷) DIVISION SIGN
	const char seq[] = "\xC3\xB7";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x00F7U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

// Test that non-ambiguous characters are not affected

TEST(TerseAmbiguousWidth, AsciiNotAffected_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	const char ch = 'A';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, AsciiNotAffected_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	const char ch = 'A';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, FullWidthCJKNotAffected_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+3042 (あ) HIRAGANA LETTER A (wide character, not ambiguous)
	const char seq[] = "\xE3\x81\x82";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x3042U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, FullWidthCJKNotAffected_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+3042 (あ) HIRAGANA LETTER A (wide character, not ambiguous)
	const char seq[] = "\xE3\x81\x82";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x3042U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, GreekAlpha_Narrow)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(0, &handle, fds);

	// U+0391 (Α) GREEK CAPITAL LETTER ALPHA (ambiguous)
	const char seq[] = "\xCE\x91";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x0391U, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAmbiguousWidth, GreekAlpha_Wide)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_ambiguous_option(1, &handle, fds);

	// U+0391 (Α) GREEK CAPITAL LETTER ALPHA (ambiguous)
	const char seq[] = "\xCE\x91";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x0391U, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#endif /* HAVE_POSIX_PIPE */
