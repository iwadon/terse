#include "terse_event_helpers.h"
#include <attest/attest.h>

#include <string.h>

static void close_quietly(terse_handle_t handle)
{
	(void)terse_capabilities_disable(handle, TERSE_CAP_DISABLE_BASIC_OUTPUT);
	terse_close(handle);
}

TEST(TerseEventHelpers, SetsCharEventFields)
{
	terse_handle_t handle = terse_open(TERSE_P0, NULL);
	EXPECT_NOT_NULL(handle);

	terse_event_t event = { 0 };
	terse_set_char_event(handle, &event, 'A', TERSE_MOD_CTRL);
	EXPECT_EQ(TERSE_EVENT_CHAR, event.type);
	EXPECT_EQ((unsigned int)'A', event.data.ch.scalar);
	EXPECT_EQ(1, event.data.ch.width);
	EXPECT_EQ(TERSE_MOD_CTRL, event.data.ch.mods);

	close_quietly(handle);
}

TEST(TerseEventHelpers, SetsKeyEventFields)
{
	terse_event_t event = { 0 };
	terse_set_key_event(&event, TERSE_EVENT_ENTER, TERSE_MOD_ALT);
	EXPECT_EQ(TERSE_EVENT_ENTER, event.type);
	EXPECT_EQ(TERSE_MOD_ALT, event.data.key.mods);
}

TEST(TerseEventHelpers, SetsFunctionEventFields)
{
	terse_event_t event = { 0 };
	terse_set_function_event(&event, 5, TERSE_MOD_SHIFT);
	EXPECT_EQ(TERSE_EVENT_FUNCTION, event.type);
	EXPECT_EQ(5, event.data.function.number);
	EXPECT_EQ(TERSE_MOD_SHIFT, event.data.function.mods);
}

TEST(TerseEventHelpers, SetsMouseEventFields)
{
	terse_event_t event = { 0 };
	terse_set_mouse_event(&event, TERSE_EVENT_MOUSE_DOWN, TERSE_MOUSE_BUTTON_LEFT, TERSE_MOD_META, 7, 9);
	EXPECT_EQ(TERSE_EVENT_MOUSE_DOWN, event.type);
	EXPECT_EQ(TERSE_MOUSE_BUTTON_LEFT, event.data.mouse.button);
	EXPECT_EQ(TERSE_MOD_META, event.data.mouse.mods);
	EXPECT_EQ(7, event.data.mouse.row);
	EXPECT_EQ(9, event.data.mouse.col);
}

TEST(TerseEventHelpers, SetsResizeEventFields)
{
	terse_event_t event = { 0 };
	terse_set_resize_event(&event, 24, 80);
	EXPECT_EQ(TERSE_EVENT_RESIZE, event.type);
	EXPECT_EQ(24, event.data.resize.rows);
	EXPECT_EQ(80, event.data.resize.cols);
}

TEST(TerseEventHelpers, CopiesRawEventBytes)
{
	terse_event_t event = { 0 };
	unsigned char bytes[] = { 0x01, 0x02, 0x03, 0x04 };
	terse_set_raw_event(&event, bytes, sizeof(bytes));
	EXPECT_EQ(TERSE_EVENT_RAW_SEQUENCE, event.type);
	EXPECT_EQ(sizeof(bytes), event.data.raw.length);
	EXPECT_EQ(0, memcmp(bytes, event.data.raw.bytes, sizeof(bytes)));
	EXPECT_EQ(0, event.data.raw.bytes[sizeof(bytes)]);
	EXPECT_EQ(0, event.data.raw.bytes[TERSE_EVENT_RAW_MAX - 1]);
}

TEST(TerseEventHelpers, TruncatesRawEventBytes)
{
	terse_event_t event = { 0 };
	unsigned char bytes[TERSE_EVENT_RAW_MAX + 4];
	for (size_t i = 0; i < sizeof(bytes); i++) {
		bytes[i] = (unsigned char)(i + 1);
	}
	terse_set_raw_event(&event, bytes, sizeof(bytes));
	EXPECT_EQ(TERSE_EVENT_RAW_SEQUENCE, event.type);
	EXPECT_EQ(TERSE_EVENT_RAW_MAX, event.data.raw.length);
	EXPECT_EQ(0, memcmp(bytes, event.data.raw.bytes, TERSE_EVENT_RAW_MAX));
}
