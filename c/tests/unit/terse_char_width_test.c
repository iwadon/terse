#include "terse.h"
#include <attest/attest.h>

#include "test_compat.h"
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_POSIX_PIPE

static void create_handle_with_ambiguous_option(int ambiguous_as_wide, terse_handle_t *out_handle, int fds[2])
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

/* Test ASCII characters (width = 1) */

TEST(TerseCharWidth, AsciiLetters)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	EXPECT_EQ(1, terse_char_width(handle, 'A'));
	EXPECT_EQ(1, terse_char_width(handle, 'z'));
	EXPECT_EQ(1, terse_char_width(handle, '0'));
	EXPECT_EQ(1, terse_char_width(handle, ' '));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test control characters (width = 0) */

TEST(TerseCharWidth, ControlCharacters)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	EXPECT_EQ(0, terse_char_width(handle, 0x00)); /* NUL */
	EXPECT_EQ(0, terse_char_width(handle, 0x1F)); /* US */
	EXPECT_EQ(0, terse_char_width(handle, 0x7F)); /* DEL */
	EXPECT_EQ(0, terse_char_width(handle, 0x9F)); /* C1 control */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test CJK wide characters (width = 2) */

TEST(TerseCharWidth, WideCharacters)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	EXPECT_EQ(2, terse_char_width(handle, 0x3042)); /* あ HIRAGANA LETTER A */
	EXPECT_EQ(2, terse_char_width(handle, 0x4E2D)); /* 中 CJK unified ideograph */
	EXPECT_EQ(2, terse_char_width(handle, 0xAC00)); /* 가 HANGUL SYLLABLE GA */
	EXPECT_EQ(2, terse_char_width(handle, 0xFF21)); /* Ａ FULLWIDTH LATIN CAPITAL LETTER A */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test combining characters (width = 0) */

TEST(TerseCharWidth, CombiningCharacters)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	EXPECT_EQ(0, terse_char_width(handle, 0x0300)); /* COMBINING GRAVE ACCENT */
	EXPECT_EQ(0, terse_char_width(handle, 0x0301)); /* COMBINING ACUTE ACCENT */
	EXPECT_EQ(0, terse_char_width(handle, 0x0308)); /* COMBINING DIAERESIS */
	EXPECT_EQ(0, terse_char_width(handle, 0x20DD)); /* COMBINING ENCLOSING CIRCLE */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test ambiguous characters with option = 0 (narrow) */

TEST(TerseCharWidth, AmbiguousNarrow)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	EXPECT_EQ(1, terse_char_width(handle, 0x00B1)); /* ± PLUS-MINUS SIGN */
	EXPECT_EQ(1, terse_char_width(handle, 0x00A7)); /* § SECTION SIGN */
	EXPECT_EQ(1, terse_char_width(handle, 0x00B0)); /* ° DEGREE SIGN */
	EXPECT_EQ(1, terse_char_width(handle, 0x0391)); /* Α GREEK CAPITAL LETTER ALPHA */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test ambiguous characters with option = 1 (wide) */

TEST(TerseCharWidth, AmbiguousWide)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(1, &handle, fds);

	EXPECT_EQ(2, terse_char_width(handle, 0x00B1)); /* ± PLUS-MINUS SIGN */
	EXPECT_EQ(2, terse_char_width(handle, 0x00A7)); /* § SECTION SIGN */
	EXPECT_EQ(2, terse_char_width(handle, 0x00B0)); /* ° DEGREE SIGN */
	EXPECT_EQ(2, terse_char_width(handle, 0x0391)); /* Α GREEK CAPITAL LETTER ALPHA */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test consistency with read_event width */

TEST(TerseCharWidth, ConsistentWithReadEvent)
{
	int fds[2];
	terse_handle_t handle;
	create_handle_with_ambiguous_option(0, &handle, fds);

	/* Write hiragana あ (U+3042) */
	const char seq[] = "\xE3\x81\x82";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);

	/* terse_char_width should return the same value as event.data.ch.width */
	EXPECT_EQ(event.data.ch.width, terse_char_width(handle, event.data.ch.scalar));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Test with NULL handle (should use default behavior) */

TEST(TerseCharWidth, NullHandle)
{
	/* With NULL handle, ambiguous characters should be treated as narrow */
	EXPECT_EQ(1, terse_char_width(NULL, 'A'));
	EXPECT_EQ(2, terse_char_width(NULL, 0x3042)); /* あ - wide character */
	EXPECT_EQ(1, terse_char_width(NULL, 0x00B1)); /* ± - ambiguous, default narrow */
	EXPECT_EQ(0, terse_char_width(NULL, 0x0300)); /* combining */
}

#endif /* HAVE_POSIX_PIPE */
