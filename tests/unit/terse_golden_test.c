/*
 * terse_golden_test.c - バイト列ゴールデンテストの代表シナリオ（Phase 6）
 *
 * ケイパビリティの組み合わせを enabled_caps で固定し（端末名に依存しない。
 * redesign-proposal.md §4.4.3）、terse が吐く SGR バイト列をゴールデンファイル
 * tests/golden/<name>.txt と比較する。基盤の詳細は tests/golden_helpers.h を参照。
 */
#include "golden_helpers.h"

#ifdef HAVE_POSIX_PIPE

/* TrueColor 有効時、RGB 形式の SGR を吐くこと。 */
TEST(TerseGolden, TruecolorForegroundSgr)
{
	golden_capture_t cap;
	terse_options_t options = {
		.codec_name = "UTF-8",
		.enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR,
	};
	golden_begin(&cap, TERSE_P1, &options);

	terse_style_t style = terse_style_default();
	style.foreground = terse_color_truecolor(0x12, 0x34, 0x56);
	EXPECT_EQ(0, terse_set_style(cap.handle, &style));

	golden_capture_assert(&cap, "truecolor_foreground");
}

/* 基本 16 色 + テキスト装飾の SGR を吐くこと。 */
TEST(TerseGolden, Basic16ForegroundAndEffects)
{
	golden_capture_t cap;
	terse_options_t options = {
		.codec_name = "UTF-8",
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_TEXT_STYLES,
	};
	golden_begin(&cap, TERSE_P1, &options);

	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	style.background = terse_color_basic(TERSE_BASIC_COLOR_CYAN, 1);
	style.effects = TERSE_STYLE_BOLD | TERSE_STYLE_UNDERLINE;
	EXPECT_EQ(0, terse_set_style(cap.handle, &style));

	golden_capture_assert(&cap, "basic16_foreground_effects");
}

#endif /* HAVE_POSIX_PIPE */
