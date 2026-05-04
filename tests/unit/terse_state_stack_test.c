#include "terse.h"
#include <attest/attest.h>

static terse_state_t make_known_state(int row, int col, terse_basic_color_t color)
{
	terse_state_t state = {
		.cursor_known = 1,
		.cursor_visible = 0,
		.cursor_row = row,
		.cursor_col = col,
		.style_known = 1,
		.style = terse_style_default(),
	};
	state.style.effects = TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE;
	state.style.foreground = terse_color_basic(color, 1);
	return state;
}

TEST(TerseStateStack, RestoresState_OnPop)
{
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_NOT_NULL(handle);

	terse_state_t initial = make_known_state(6, 2, TERSE_BASIC_COLOR_GREEN);
	EXPECT_EQ(TERSE_OK, terse_state_override(handle, &initial));
	EXPECT_EQ(TERSE_OK, terse_push_state(handle));

	terse_state_t changed = make_known_state(1, 4, TERSE_BASIC_COLOR_BLUE);
	changed.cursor_visible = 1;
	changed.style.effects = TERSE_STYLE_ITALIC;
	EXPECT_EQ(TERSE_OK, terse_state_override(handle, &changed));

	EXPECT_EQ(TERSE_OK, terse_pop_state(handle));

	terse_state_t captured;
	EXPECT_EQ(TERSE_OK, terse_capture_state(handle, &captured));
	EXPECT_EQ(1, captured.cursor_known);
	EXPECT_EQ(0, captured.cursor_visible);
	EXPECT_EQ(6, captured.cursor_row);
	EXPECT_EQ(2, captured.cursor_col);
	EXPECT_EQ(1, captured.style_known);
	EXPECT_NE(captured.style.effects & TERSE_STYLE_BOLD, 0u);
	EXPECT_NE(captured.style.effects & TERSE_STYLE_UNDERLINE, 0u);
	EXPECT_EQ(TERSE_COLOR_KIND_BASIC16, captured.style.foreground.kind);
	EXPECT_EQ(TERSE_BASIC_COLOR_GREEN, captured.style.foreground.data.basic16.color);

	terse_close(handle);
}

TEST(TerseStateStack, UnderflowPreservesState_OnEmptyPop)
{
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_NOT_NULL(handle);

	terse_state_t initial = make_known_state(3, 5, TERSE_BASIC_COLOR_RED);
	EXPECT_EQ(TERSE_OK, terse_state_override(handle, &initial));

	terse_state_t before;
	EXPECT_EQ(TERSE_OK, terse_capture_state(handle, &before));

	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_pop_state(handle));
	EXPECT_EQ(TERSE_ERR_STACK_UNDERFLOW, terse_get_last_error(handle));

	terse_state_t after;
	EXPECT_EQ(TERSE_OK, terse_capture_state(handle, &after));
	EXPECT_EQ(before.cursor_known, after.cursor_known);
	EXPECT_EQ(before.cursor_visible, after.cursor_visible);
	EXPECT_EQ(before.cursor_row, after.cursor_row);
	EXPECT_EQ(before.cursor_col, after.cursor_col);
	EXPECT_EQ(before.style_known, after.style_known);
	EXPECT_EQ(before.style.effects, after.style.effects);
	EXPECT_EQ(before.style.foreground.kind, after.style.foreground.kind);
	EXPECT_EQ(before.style.foreground.data.basic16.color, after.style.foreground.data.basic16.color);

	terse_close(handle);
}

TEST(TerseStateStack, OverflowSetsLastError_OnTooManyPushes)
{
	terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, NULL);
	EXPECT_NOT_NULL(handle);

	int overflow_hit = 0;
	for (int i = 0; i < 32; i++) {
		int rc = terse_push_state(handle);
		if (rc != TERSE_OK) {
			overflow_hit = 1;
			EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, rc);
			EXPECT_EQ(TERSE_ERR_STACK_OVERFLOW, terse_get_last_error(handle));
			break;
		}
	}
	EXPECT_TRUE(overflow_hit);

	terse_close(handle);
}
