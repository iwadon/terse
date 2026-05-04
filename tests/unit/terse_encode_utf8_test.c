#include "terse.h"
#include <attest/attest.h>

/* ASCII (1 byte) */
TEST(EncodeUtf8, Ascii)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8('A', buf);
	EXPECT_EQ(len, 1);
	EXPECT_EQ(buf[0], 'A');
}

TEST(EncodeUtf8, Null)
{
	unsigned char buf[4] = { 0xff, 0xff, 0xff, 0xff };
	int len = terse_encode_utf8(0, buf);
	EXPECT_EQ(len, 1);
	EXPECT_EQ(buf[0], 0);
}

/* 2-byte (U+0080 - U+07FF) */
TEST(EncodeUtf8, TwoByteMin)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x80, buf);
	EXPECT_EQ(len, 2);
	EXPECT_EQ(buf[0], 0xc2);
	EXPECT_EQ(buf[1], 0x80);
}

TEST(EncodeUtf8, TwoByteMax)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x7ff, buf);
	EXPECT_EQ(len, 2);
	EXPECT_EQ(buf[0], 0xdf);
	EXPECT_EQ(buf[1], 0xbf);
}

/* 3-byte (U+0800 - U+FFFF) */
TEST(EncodeUtf8, ThreeByteMin)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x800, buf);
	EXPECT_EQ(len, 3);
	EXPECT_EQ(buf[0], 0xe0);
	EXPECT_EQ(buf[1], 0xa0);
	EXPECT_EQ(buf[2], 0x80);
}

TEST(EncodeUtf8, HiraganaA)
{
	/* U+3042 あ */
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x3042, buf);
	EXPECT_EQ(len, 3);
	EXPECT_EQ(buf[0], 0xe3);
	EXPECT_EQ(buf[1], 0x81);
	EXPECT_EQ(buf[2], 0x82);
}

TEST(EncodeUtf8, ThreeByteMax)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0xffff, buf);
	EXPECT_EQ(len, 3);
	EXPECT_EQ(buf[0], 0xef);
	EXPECT_EQ(buf[1], 0xbf);
	EXPECT_EQ(buf[2], 0xbf);
}

/* 4-byte (U+10000 - U+10FFFF) */
TEST(EncodeUtf8, FourByteMin)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x10000, buf);
	EXPECT_EQ(len, 4);
	EXPECT_EQ(buf[0], 0xf0);
	EXPECT_EQ(buf[1], 0x90);
	EXPECT_EQ(buf[2], 0x80);
	EXPECT_EQ(buf[3], 0x80);
}

TEST(EncodeUtf8, Emoji)
{
	/* U+1F600 😀 */
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x1f600, buf);
	EXPECT_EQ(len, 4);
	EXPECT_EQ(buf[0], 0xf0);
	EXPECT_EQ(buf[1], 0x9f);
	EXPECT_EQ(buf[2], 0x98);
	EXPECT_EQ(buf[3], 0x80);
}

TEST(EncodeUtf8, FourByteMax)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x10ffff, buf);
	EXPECT_EQ(len, 4);
	EXPECT_EQ(buf[0], 0xf4);
	EXPECT_EQ(buf[1], 0x8f);
	EXPECT_EQ(buf[2], 0xbf);
	EXPECT_EQ(buf[3], 0xbf);
}

/* Invalid: surrogate pairs */
TEST(EncodeUtf8, SurrogateLow)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0xd800, buf);
	EXPECT_EQ(len, 0);
}

TEST(EncodeUtf8, SurrogateHigh)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0xdfff, buf);
	EXPECT_EQ(len, 0);
}

/* Invalid: out of range */
TEST(EncodeUtf8, OutOfRange)
{
	unsigned char buf[4] = { 0 };
	int len = terse_encode_utf8(0x110000, buf);
	EXPECT_EQ(len, 0);
}

/* NULL pointer */
TEST(EncodeUtf8, NullBuffer)
{
	int len = terse_encode_utf8('A', NULL);
	EXPECT_EQ(len, 0);
}
