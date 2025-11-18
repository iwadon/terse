#include "terse.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void create_input_handle_with_codec(const char *codec, terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = STDOUT_FILENO,
		.codec_name = codec,
		.disabled_caps = 0,
	};

	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_TRUE(*out_handle != NULL);
}

static void create_input_handle(terse_handle_t *out_handle, int fds[2])
{
	create_input_handle_with_codec("UTF-8", out_handle, fds);
}

TEST(TerseReadEvent, ReturnsNone_OnTimeout)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	terse_event_t event;
	int result = terse_read_event(handle, 10, &event);
	EXPECT_EQ(TERSE_ERR_NO_EVENT, result);
	terse_error_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_OK, err);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsChar_OnAsciiInput)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = 'A';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);
	EXPECT_EQ(0, event.data.ch.mods);
	terse_error_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_OK, err);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEnter_OnCrLf)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\r\n";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEnter_OnCarriageReturn)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = '\r';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEnterWithCtrl_OnLineFeed)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = '\n';
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsArrow_OnEscapeSequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[A";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ARROW_UP, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsCtrlChar_WithControlModifier)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char ch = 0x01; // Ctrl+A
	EXPECT_TRUE(write(fds[1], &ch, 1) == 1);

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.ch.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsCharWithAlt_OnEscapePrefixedAscii)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char seq[] = { 0x1b, 'a' };
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq)) == (ssize_t)sizeof(seq));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'a', event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.ch.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEnterWithAlt_OnEscapePrefixedNewline)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char seq[] = { 0x1b, '\n' };
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq)) == (ssize_t)sizeof(seq));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsCtrlAltChar_OnEscapePrefixedControl)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char seq[] = { 0x1b, 0x01 };
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq)) == (ssize_t)sizeof(seq));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_ALT | TERSE_MOD_CTRL, event.data.ch.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsWideCharWithAlt_OnEscapePrefixedUtf8)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char seq[] = { 0x1b, 0xe6, 0xbc, 0xa2 };
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq)) == (ssize_t)sizeof(seq));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x6f22u, event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.ch.mods);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#if TERSE_HAVE_ICONV
TEST(TerseReadEvent, ReturnsWideCharWithAlt_OnEscapePrefixedShiftJis)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_codec("Shift_JIS", &handle, fds);

	const unsigned char seq[] = { 0x1b, 0x82, 0xa0 };
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq)) == (ssize_t)sizeof(seq));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x3042u, event.data.ch.scalar);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.ch.mods);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}
#endif

TEST(TerseReadEvent, ReturnsArrowWithShift_OnModifierSequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[1;2A";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ARROW_UP, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsWideWidth_OnUtf8Cjk)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char bytes[] = { 0xe6, 0xbc, 0xa2 }; // U+6F22
	EXPECT_TRUE(write(fds[1], bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x6f22u, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsZeroWidth_OnCombiningMark)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const unsigned char bytes[] = { 0xcc, 0x81 }; // U+0301 combining acute
	EXPECT_TRUE(write(fds[1], bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x301u, event.data.ch.scalar);
	EXPECT_EQ(0, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#if TERSE_HAVE_ICONV
TEST(TerseReadEvent, ReturnsWideWidth_OnShiftJisDoubleByte)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_codec("Shift_JIS", &handle, fds);

	const unsigned char bytes[] = { 0x82, 0xa0 }; // "あ"
	EXPECT_TRUE(write(fds[1], bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0x3042u, event.data.ch.scalar);
	EXPECT_EQ(2, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsHalfWidth_OnShiftJisKana)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle_with_codec("Shift_JIS", &handle, fds);

	const unsigned char bytes[] = { 0xa6 }; // Half-width Katakana U+FF66
	EXPECT_TRUE(write(fds[1], bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ(0xff66u, event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}
#endif

TEST(TerseReadEvent, ReturnsHome_OnCsiH)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[H";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsHomeWithShift_OnCsi1_2H)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[1;2H";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_HOME, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsPageDown_OnCsi6Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[6~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_PAGE_DOWN, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsDelete_OnCsi3Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[3~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_DELETE, event.type);
	EXPECT_EQ(0, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsFunction_OnCsi18Tilde)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[18~"; // F7
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(7, event.data.function.number);
	EXPECT_EQ(0, event.data.function.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsShiftTab_OnCsiZ)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[Z";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_TAB, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsShiftEnter_OnKittyCSIu)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[13;2u";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsShiftEnter_OnModifyOtherKeys)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[27;2;13~";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.key.mods);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsResize_OnCsi8Sequence)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b[8;24;80t";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_RESIZE, event.type);
	EXPECT_EQ(24, event.data.resize.rows);
	EXPECT_EQ(80, event.data.resize.cols);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsRawSequence_OnUnknownEscape)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);

	const char seq[] = "\x1b]0;title\x07";
	EXPECT_TRUE(write(fds[1], seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1));

	terse_event_t event;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_EVENT_OK, result);
	EXPECT_EQ(TERSE_EVENT_RAW_SEQUENCE, event.type);
	EXPECT_TRUE(event.data.raw.length >= 2);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseReadEvent, ReturnsEINVAL_OnNullArguments)
{
	terse_event_t event;
	errno = 0;
	EXPECT_EQ(TERSE_ERR_INVALID_HANDLE, terse_read_event(NULL, 0, &event));
	EXPECT_EQ(EINVAL, errno);

	errno = 0;
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_TRUE(handle != NULL);
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_read_event(handle, 0, NULL));
	EXPECT_EQ(EINVAL, errno);
	terse_close(handle);
}

TEST(TerseReadEvent, ReturnsEpipe_OnPipeClosed)
{
	int fds[2];
	terse_handle_t handle;
	create_input_handle(&handle, fds);
	close(fds[1]); // close writer end to force EOF

	terse_event_t event;
	errno = 0;
	int result = terse_read_event(handle, 50, &event);
	EXPECT_EQ(TERSE_ERR_IO, result);
	EXPECT_EQ(EPIPE, errno);
	terse_error_t err = terse_get_last_error(handle);
	EXPECT_EQ(TERSE_ERR_IO, err);

	terse_close(handle);
	close(fds[0]);
}
