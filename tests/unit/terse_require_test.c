/*
 * Phase 4: ケイパビリティ要求 API（terse_require / terse_caps_missing /
 * terse_get_active_features）の単体テスト。
 *
 * すべて公開 API 経由で検証する。検出環境に依存しないよう、各テストは
 * test_env のヘルパで検出系環境変数をサニタイズし、enabled_caps /
 * disabled_caps で意図的にケイパビリティを制御する。
 *
 * 注: 検出側 BASIC4 経路（Human68k 等で colors=BASIC4 を立てる配線）と、
 *     それに伴う 4 色への色縮退の統合検証は Phase 4.5 で行う。BASIC4 を
 *     公開 API から注入する手段が Phase 4 時点では存在しないため。
 */
#include "terse.h"
#include <attest/attest.h>

#include <stdint.h>

#include "test_compat.h"
#include "test_env.h"

/* TrueColor 端末を擬似的に作る（検出環境はサニタイズ済み前提、override で色を持ち上げる）。 */
static terse_handle_t
open_truecolor(terse_profile_t profile)
{
	terse_options_t options = {
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_SGR_EXTENDED | TERSE_CAP_ENABLE_TRUECOLOR,
	};
	return terse_open(profile, &options);
}

TEST(TerseRequire, MissingReportsUnsupportedBits)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	/* TrueColor は持つがマウスは無効化した端末。 */
	terse_options_t options = {
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_SGR_EXTENDED | TERSE_CAP_ENABLE_TRUECOLOR,
		.disabled_caps = TERSE_CAP_DISABLE_MOUSE,
	};
	terse_handle_t handle = terse_open(TERSE_P2, &options);
	EXPECT_NOT_NULL(handle);

	uint64_t wanted = TERSE_FEAT_COLOR_TRUECOLOR | TERSE_FEAT_MOUSE;
	uint64_t missing = terse_caps_missing(handle, wanted);

	/* TrueColor は充足、mouse のみ不足。 */
	EXPECT_EQ((uint64_t)TERSE_FEAT_MOUSE, missing);

	terse_close(handle);
	terse_test_env_restore_detection(&env);
}

TEST(TerseRequire, ColorTiersSatisfyLowerTiers)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	terse_handle_t handle = open_truecolor(TERSE_P1);
	EXPECT_NOT_NULL(handle);

	/* TrueColor 端末は BASIC4 / BASIC16 / PALETTE256 の要求も満たす。 */
	uint64_t wanted = TERSE_FEAT_COLOR_BASIC4 | TERSE_FEAT_COLOR_BASIC16 | TERSE_FEAT_COLOR_PALETTE256 | TERSE_FEAT_COLOR_TRUECOLOR;
	EXPECT_EQ((uint64_t)0, terse_caps_missing(handle, wanted));

	terse_close(handle);
	terse_test_env_restore_detection(&env);
}

TEST(TerseRequire, BasicColorTerminalLacksHigherTiers)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	/* SGR basic のみ（16 色）の端末。256/TrueColor は無効。 */
	terse_options_t options = {
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC,
		.disabled_caps = TERSE_CAP_DISABLE_SGR_EXTENDED | TERSE_CAP_DISABLE_TRUECOLOR,
	};
	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_NOT_NULL(handle);

	/* BASIC16 / BASIC4 は充足、PALETTE256 / TRUECOLOR は不足。 */
	uint64_t wanted = TERSE_FEAT_COLOR_BASIC4 | TERSE_FEAT_COLOR_BASIC16 | TERSE_FEAT_COLOR_PALETTE256 | TERSE_FEAT_COLOR_TRUECOLOR;
	uint64_t missing = terse_caps_missing(handle, wanted);
	EXPECT_EQ((uint64_t)(TERSE_FEAT_COLOR_PALETTE256 | TERSE_FEAT_COLOR_TRUECOLOR), missing);

	terse_close(handle);
	terse_test_env_restore_detection(&env);
}

TEST(TerseRequire, RequireIsAliasOfCapsMissing)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	terse_handle_t handle = open_truecolor(TERSE_P2);
	EXPECT_NOT_NULL(handle);

	uint64_t wanted = TERSE_FEAT_COLOR_TRUECOLOR | TERSE_FEAT_MOUSE | TERSE_FEAT_TITLE;
	EXPECT_EQ(terse_caps_missing(handle, wanted), terse_require(handle, wanted));

	terse_close(handle);
	terse_test_env_restore_detection(&env);
}

TEST(TerseRequire, ActiveFeaturesTrackOverrides)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	/* 256 色（SGR extended）止まりの端末。色の持ち上げ/引き下げを runtime で行う。 */
	terse_options_t options = {
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC | TERSE_CAP_ENABLE_SGR_EXTENDED,
		.disabled_caps = TERSE_CAP_DISABLE_TRUECOLOR | TERSE_CAP_DISABLE_MOUSE,
	};
	terse_handle_t handle = terse_open(TERSE_P2, &options);
	EXPECT_NOT_NULL(handle);

	uint64_t before = terse_get_active_features(handle);
	EXPECT_TRUE((before & TERSE_FEAT_COLOR_PALETTE256) != 0);
	EXPECT_TRUE((before & TERSE_FEAT_COLOR_TRUECOLOR) == 0);
	EXPECT_TRUE((before & TERSE_FEAT_MOUSE) == 0);

	/* 実行時に mouse を有効化すると active に反映される。 */
	EXPECT_EQ(0, terse_capabilities_enable(handle, TERSE_CAP_ENABLE_MOUSE));
	uint64_t after = terse_get_active_features(handle);
	EXPECT_TRUE((after & TERSE_FEAT_MOUSE) != 0);

	/* 実行時に truecolor を有効化すると最上位の色ビットが立つ。 */
	EXPECT_EQ(0, terse_capabilities_enable(handle, TERSE_CAP_ENABLE_TRUECOLOR));
	uint64_t upgraded = terse_get_active_features(handle);
	EXPECT_TRUE((upgraded & TERSE_FEAT_COLOR_TRUECOLOR) != 0);
	/* 下位段はすべて立っている。 */
	EXPECT_TRUE((upgraded & TERSE_FEAT_COLOR_PALETTE256) != 0);
	EXPECT_TRUE((upgraded & TERSE_FEAT_COLOR_BASIC16) != 0);
	EXPECT_TRUE((upgraded & TERSE_FEAT_COLOR_BASIC4) != 0);

	terse_close(handle);
	terse_test_env_restore_detection(&env);
}

TEST(TerseRequire, NullHandleReportsAllMissing)
{
	uint64_t wanted = TERSE_FEAT_COLOR_TRUECOLOR | TERSE_FEAT_MOUSE;
	EXPECT_EQ(wanted, terse_caps_missing(NULL, wanted));
	EXPECT_EQ(wanted, terse_require(NULL, wanted));
	EXPECT_EQ((uint64_t)0, terse_get_active_features(NULL));
}

TEST(TerseRequire, EmptyRequestIsAlwaysSatisfied)
{
	terse_test_env_backup_t env;
	terse_test_env_backup_detection(&env);

	terse_handle_t handle = open_truecolor(TERSE_P1);
	EXPECT_NOT_NULL(handle);
	EXPECT_EQ((uint64_t)0, terse_caps_missing(handle, 0));
	terse_close(handle);

	terse_test_env_restore_detection(&env);
}
