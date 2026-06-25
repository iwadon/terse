#include "terse.h"
#include <attest/attest.h>
#include <string.h>

TEST(ConvertEncoding, Utf8ToUtf8Passthrough)
{
	const char *in = "hello";
	char out[32];
	size_t outlen = sizeof(out);
	terse_error_t err = terse_convert_encoding("UTF-8", "UTF-8", in, 5, out, &outlen);
	EXPECT_EQ(err, TERSE_OK);
	EXPECT_EQ(outlen, 5u);
	EXPECT_EQ(memcmp(out, "hello", 5), 0);
}

TEST(ConvertEncoding, Utf8ToShiftJis)
{
	/* U+3042 あ = UTF-8: E3 81 82, Shift_JIS: 82 A0 */
	const unsigned char utf8[] = { 0xe3, 0x81, 0x82 };
	char out[32];
	size_t outlen = sizeof(out);
	terse_error_t err = terse_convert_encoding("UTF-8", "Shift_JIS",
	                                           (const char *)utf8, 3, out, &outlen);
	EXPECT_EQ(err, TERSE_OK);
	EXPECT_EQ(outlen, 2u);
	EXPECT_EQ((unsigned char)out[0], 0x82u);
	EXPECT_EQ((unsigned char)out[1], 0xa0u);
}

TEST(ConvertEncoding, ShiftJisToUtf8)
{
	/* Shift_JIS: 82 A0 = U+3042 あ = UTF-8: E3 81 82 */
	const unsigned char sjis[] = { 0x82, 0xa0 };
	char out[32];
	size_t outlen = sizeof(out);
	terse_error_t err = terse_convert_encoding("Shift_JIS", "UTF-8",
	                                           (const char *)sjis, 2, out, &outlen);
	EXPECT_EQ(err, TERSE_OK);
	EXPECT_EQ(outlen, 3u);
	EXPECT_EQ((unsigned char)out[0], 0xe3u);
	EXPECT_EQ((unsigned char)out[1], 0x81u);
	EXPECT_EQ((unsigned char)out[2], 0x82u);
}

TEST(ConvertEncoding, BufferTooSmall)
{
	const unsigned char utf8[] = { 0xe3, 0x81, 0x82 };
	char out[1];
	size_t outlen = sizeof(out);
	terse_error_t err = terse_convert_encoding("UTF-8", "Shift_JIS",
	                                           (const char *)utf8, 3, out, &outlen);
	EXPECT_EQ(err, TERSE_ERR_BUFFER_TOO_SMALL);
}

TEST(ConvertEncoding, NullArguments)
{
	char out[32];
	size_t outlen = sizeof(out);
	EXPECT_EQ(terse_convert_encoding(NULL, "UTF-8", "x", 1, out, &outlen),
	          TERSE_ERR_INVALID_ARGUMENT);
	EXPECT_EQ(terse_convert_encoding("UTF-8", NULL, "x", 1, out, &outlen),
	          TERSE_ERR_INVALID_ARGUMENT);
	EXPECT_EQ(terse_convert_encoding("UTF-8", "UTF-8", NULL, 1, out, &outlen),
	          TERSE_ERR_INVALID_ARGUMENT);
	EXPECT_EQ(terse_convert_encoding("UTF-8", "UTF-8", "x", 1, NULL, &outlen),
	          TERSE_ERR_INVALID_ARGUMENT);
	EXPECT_EQ(terse_convert_encoding("UTF-8", "UTF-8", "x", 1, out, NULL),
	          TERSE_ERR_INVALID_ARGUMENT);
}

TEST(ConvertEncoding, NonUtf8BothSidesRejectsed)
{
	char out[32];
	size_t outlen = sizeof(out);
	terse_error_t err = terse_convert_encoding("Shift_JIS", "EUC-JP",
	                                           "x", 1, out, &outlen);
	EXPECT_EQ(err, TERSE_ERR_INVALID_ARGUMENT);
}

TEST(ConvertEncoding, AsciiRoundtrip)
{
	const char *in = "ABC123";
	char sjis[32], utf8[32];
	size_t sjislen = sizeof(sjis), utf8len = sizeof(utf8);

	terse_error_t err = terse_convert_encoding("UTF-8", "Shift_JIS",
	                                           in, 6, sjis, &sjislen);
	EXPECT_EQ(err, TERSE_OK);

	err = terse_convert_encoding("Shift_JIS", "UTF-8",
	                             sjis, sjislen, utf8, &utf8len);
	EXPECT_EQ(err, TERSE_OK);
	EXPECT_EQ(utf8len, 6u);
	EXPECT_EQ(memcmp(utf8, in, 6), 0);
}
