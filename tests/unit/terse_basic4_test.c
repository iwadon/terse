/*
 * Phase 4.5: Human68k テキスト 4 色（TERSE_COLOR_BASIC4）への色縮退テスト。
 *
 * 検出側の BASIC4 経路は #if defined(__human68k__) 下にあり、ホスト
 * （macOS/Linux）の CI では発火しない。一方、色縮退ロジック自体は純粋関数
 * （ハンドル非依存）なのでホストで直接検証できる。
 *
 * 4 色の代表色は Human68k テキストパレット初期値 pal[0..3]:
 *   黒 (0,0,0) / シアン (8,255,255) / 黄 (255,255,0) / 白 (255,255,255)
 * 返却の basic_color は SGR 標準色番号（黒=0/黄=3/シアン=6/白=7）。
 *
 * これは内部ロジックの検証なので内部ヘッダ terse_style.h を直接 include する。
 */
#include "terse.h"
#include "terse_style.h"
#include <attest/attest.h>

static terse_basic_color_t
degrade_basic4_of_truecolor(unsigned char r, unsigned char g, unsigned char b)
{
	terse_color_t c = terse_color_truecolor(r, g, b);
	terse_color_t d = terse_style_degrade_color(c, TERSE_COLOR_BASIC4);
	/* BASIC4 縮退の戻り値は basic16 kind（SGR 標準色番号を保持）。 */
	return d.data.basic16.color;
}

TEST(TerseBasic4, DegradesPrimariesToNearestText4Color)
{
	/* 純色は対応する 4 色へ寄る。 */
	EXPECT_EQ(TERSE_BASIC_COLOR_BLACK, degrade_basic4_of_truecolor(0, 0, 0));
	EXPECT_EQ(TERSE_BASIC_COLOR_WHITE, degrade_basic4_of_truecolor(255, 255, 255));
	EXPECT_EQ(TERSE_BASIC_COLOR_YELLOW, degrade_basic4_of_truecolor(255, 255, 0));
	EXPECT_EQ(TERSE_BASIC_COLOR_CYAN, degrade_basic4_of_truecolor(0, 255, 255));
}

TEST(TerseBasic4, DegradesNearBlacksAndWhites)
{
	/* 暗い色は黒、明るいグレーは白へ。 */
	EXPECT_EQ(TERSE_BASIC_COLOR_BLACK, degrade_basic4_of_truecolor(10, 10, 10));
	EXPECT_EQ(TERSE_BASIC_COLOR_WHITE, degrade_basic4_of_truecolor(250, 250, 250));
}

TEST(TerseBasic4, ResultKindIsBasic16NonBright)
{
	terse_color_t c = terse_color_truecolor(255, 255, 0);
	terse_color_t d = terse_style_degrade_color(c, TERSE_COLOR_BASIC4);
	EXPECT_EQ(TERSE_COLOR_KIND_BASIC16, d.kind);
	EXPECT_EQ(0, d.data.basic16.bright);
}

TEST(TerseBasic4, Basic16InputIsNotPassedThrough)
{
	/*
	 * BASIC4 と BASIC16 は support_rank 上は同ランク。早期 return させず、
	 * 16 色入力も 4 色へ寄せる必要がある（赤 → 4 色には赤が無いので最近傍へ）。
	 */
	terse_color_t red16 = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	terse_color_t d = terse_style_degrade_color(red16, TERSE_COLOR_BASIC4);
	EXPECT_EQ(TERSE_COLOR_KIND_BASIC16, d.kind);
	/* 黒/シアン/黄/白 のいずれか（4 色枠内）に収まる。 */
	terse_basic_color_t c = d.data.basic16.color;
	EXPECT_TRUE(c == TERSE_BASIC_COLOR_BLACK || c == TERSE_BASIC_COLOR_CYAN || c == TERSE_BASIC_COLOR_YELLOW || c == TERSE_BASIC_COLOR_WHITE);
}

TEST(TerseBasic4, DefaultColorIsPreserved)
{
	terse_color_t def = terse_color_default();
	terse_color_t d = terse_style_degrade_color(def, TERSE_COLOR_BASIC4);
	EXPECT_EQ(TERSE_COLOR_KIND_DEFAULT, d.kind);
}

TEST(TerseBasic4, PaletteInputDegradesToText4)
{
	/* 256 色パレットの黄系 index も黄へ寄る。 */
	terse_color_t yellow256 = terse_color_palette(226); /* xterm 226 = 純黄 */
	terse_color_t d = terse_style_degrade_color(yellow256, TERSE_COLOR_BASIC4);
	EXPECT_EQ(TERSE_BASIC_COLOR_YELLOW, d.data.basic16.color);
}
