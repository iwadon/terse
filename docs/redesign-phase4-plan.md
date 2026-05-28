# Phase 4 詳細計画: ケイパビリティ API の拡充

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.1, §3 Phase 4, §4.2.1, §4.2.2
> 前提: Phase 1〜3 完了（commit `efe2cfc` まで、全テスト通過）

## 0. このフェーズの位置づけ

再設計で**初の機能追加フェーズ**。Phase 1〜3 は振る舞い不変のリファクタリングだったが、
Phase 4 は新しい公開 API を追加する。ただし方針は一貫して **「追加のみ・破壊的変更なし」**:
既存利用側コードは無変更でビルド・動作すること。新 API はすべて opt-in。

### スコープ（プロポーザル §3 Phase 4）
1. 要求 API の追加: `terse_require()` / `terse_caps_missing()` / `terse_get_active_features()`
2. 既存プロファイル API を内部でケイパビリティ展開する形に整理（両 API 並行提供）
3. 色 API を「経路非依存」に整理（散在の整理）
4. `terse_color_support_t` に `BASIC4` 段階を追加（5 段階化、Human68k 用の布石）

### スコープ外
- Human68k の実際の検出・色出力配線（Phase 4.5）
- プロファイル API の去就判断（Phase 4 完了後に持ち越し、プロポーザル §4.1）
- バッファドモード（Phase 5 以降）

---

## 1. 現状分析（調査結果）

### 1.1 色フィールドの「散在」の正体

色情報は実質 **2 系統**で表現されている。

| 系統 | フィールド | 役割 | 操作者 |
|------|-----------|------|--------|
| 入力系 | `has_sgr_basic` / `has_sgr_extended` / `has_truecolor`（int×3） | 検出と override の操作対象 | `terse_detection.c`、enable/disable マスク |
| 出力系 | `colors`（`terse_color_support_t` 列挙） | degrade とガードが参照 | `terse_style.c`、`terse_output.c` |

`recompute_capabilities()`（`terse_capabilities.c:226-234`）が毎回 **3 フラグ → `colors` の一方向導出**を行う:

```c
if      (caps.has_truecolor)     caps.colors = TERSE_COLOR_TRUECOLOR;
else if (caps.has_sgr_extended)  caps.colors = TERSE_COLOR_PALETTE256;
else if (caps.has_sgr_basic)     caps.colors = TERSE_COLOR_BASIC16;
else                             caps.colors = TERSE_COLOR_NONE;
```

**重要な発見**: `colors` は派生値で、3 フラグが真実の源。一方で**消費側は既に `colors` 一本に統合済み**
（`has_sgr_*` を直接読む消費コードは存在しない。検出時の代入を除く）。
したがって「色 API の経路非依存化」は**消費側ではほぼ達成済み**であり、Phase 4 で新たに統合構造を
作り直す必要はない。残る課題は「3 フラグでは BASIC4 のような 16 色未満を表現できない」点だけ。

### 1.2 BASIC4 を表現するには

3 フラグの導出ロジックは「`has_sgr_basic` があれば最低 BASIC16」しか出せない。
4 色は 3 フラグの組み合わせでは表現できないため、**`colors` を直接指定する経路**が必要。
base capabilities は既に `.colors = TERSE_COLOR_NONE` で初期化済みなので、
検出側が `colors = BASIC4` を立てたとき recompute がそれを上書きしないガードを足せばよい。

### 1.3 既存の override ビット体系

- `TERSE_CAP_DISABLE_*`: 21 個（`1u<<0` .. `1u<<20`）
- `TERSE_CAP_ENABLE_*`: 14 個（`1u<<0` .. `1u<<13`）

これらは「強制的に有効化/無効化する override」の意味。`require`（要求と充足検査）とは意味論が
異なるため、混用しない。

---

## 2. 設計判断（AskUserQuestion で確定済み）

| 論点 | 確定した方針 |
|------|-------------|
| 色散在の統合方法 | **導出を維持し整理に留める**。recompute の 3 フラグ→colors 導出はそのまま。BASIC4 は検出側 colors 直接指定を上書きしないガードを追加 |
| BASIC4 の表現手段 | **colors 直接指定の経路を確保**。recompute に「detected.colors が BASIC4 なら導出で上書きしない」分岐を追加。Phase 4.5 で活用 |
| require のビット体系 | **新規 `TERSE_FEAT_*` を `uint64_t` で定義**。既存 ENABLE/DISABLE とは別レイヤ。将来の機能追加に 64bit の余裕 |

### 2.1 色散在を「導出維持」とする根拠

- 消費側は既に `colors` 一本化済み（1.1）。新たな統合は不要
- `colors` 単一フィールド化（3 フラグ削除）は破壊的変更で「追加のみ」方針に反する
- Phase 4 の主眼は要求 API の追加。色は最小整理に留める
- 3 フラグは「経路（SGR basic/extended/truecolor）」、`colors` は「段階」。前者を残すことで
  「SGR extended は通るが運用上 256 色は使わない」のような将来の細分化余地を温存できる

---

## 3. 実装計画

### 3.1 `terse_color_support_t` に BASIC4 を追加（公開ヘッダ）

`include/terse.h`:

```c
typedef enum terse_color_support {
	TERSE_COLOR_NONE = 0,
	TERSE_COLOR_BASIC4,        /* 新規: Human68k 等の 4 色（SGR 経由） */
	TERSE_COLOR_BASIC16,
	TERSE_COLOR_PALETTE256,
	TERSE_COLOR_TRUECOLOR
} terse_color_support_t;
```

**注意**: enum 中間挿入なので `BASIC16` 以降の数値がシフトする。
- ソース互換: ○（シンボル名で参照しているコードは無変更）
- ABI 互換: ×（再コンパイルが必要）。terse は静的/同梱ビルド前提なので許容範囲。
  プロポーザル §4.2.2 の「enum 末尾追加なら ABI 互換」という記述は **末尾でない**ため
  該当しない旨を計画書に明記（後述 §6 でプロポーザル追記を検討）

`terse_color_kind_t`（色**値**の種別）には **BASIC4 を追加しない**。
4 色も 16 色も同じ basic16 index 表現で扱える（プロポーザル §4.2.3 の PALETTE 統一方針とも整合）。

### 3.2 degrade / rank の BASIC4 対応（`terse_style.c`）

`terse_style_color_support_rank()`（`terse_style.c:44`）に BASIC4 を rank 1 として挿入、
以降を +1 シフト:

```c
case TERSE_COLOR_NONE:      return 0;
case TERSE_COLOR_BASIC4:    return 1;   /* 新規 */
case TERSE_COLOR_BASIC16:   return 2;
case TERSE_COLOR_PALETTE256:return 3;
case TERSE_COLOR_TRUECOLOR: return 4;
```

`terse_style_degrade_color()`（`terse_style.c:182`）の switch に `case TERSE_COLOR_BASIC4:` を追加。
16 色（およびそれ以上）→ 4 色への縮退マッピングを実装:
- 4 色の構成は Phase 4.5 で Human68k の実機色（黒/赤/緑/青 等）に合わせて確定する。
  Phase 4 では **暫定マッピング**（basic16 index を 8 色系へ丸めた上で 4 色代表色へ寄せる）を置き、
  Phase 4.5 で見直す旨をコメントで明記
- degrade 先として BASIC4 が実際に来るのは Phase 4.5 以降（Phase 4 では検出側が BASIC4 を出さない）。
  Phase 4 ではロジックの枠と単体テストでの直接呼び出し検証に留める

### 3.3 recompute の BASIC4 ガード（`terse_capabilities.c`）

`recompute_capabilities()` の色導出（L226-234）の直前に、検出側が明示した BASIC4 を尊重するガードを追加:

```c
if (handle->detected_capabilities.colors == TERSE_COLOR_BASIC4) {
	handle->capabilities.colors = TERSE_COLOR_BASIC4;
} else if (handle->capabilities.has_truecolor) {
	handle->capabilities.colors = TERSE_COLOR_TRUECOLOR;
} else if (handle->capabilities.has_sgr_extended) {
	handle->capabilities.colors = TERSE_COLOR_PALETTE256;
} else if (handle->capabilities.has_sgr_basic) {
	handle->capabilities.colors = TERSE_COLOR_BASIC16;
} else {
	handle->capabilities.colors = TERSE_COLOR_NONE;
}
```

override（enable/disable）との相互作用:
- `TERSE_CAP_DISABLE_SGR_BASIC` 等は `has_sgr_*` フラグを操作する。BASIC4 の端末では
  `has_sgr_basic=1` なので、SGR basic を disable すると 4 色も無効化される（自然な挙動）
- Phase 4.5 で Human68k の検出ロジックを書くときに、この相互作用を確認する

### 3.4 `TERSE_FEAT_*` 機能ビットの定義（公開ヘッダ）

`include/terse.h` に新規 enum を追加（`uint64_t` 互換、`#include <stdint.h>` を確認）:

```c
/*
 * terse_require() / terse_caps_missing() / terse_get_active_features() が扱う
 * 機能ビット。検出ケイパビリティ（terse_capabilities_t）を「要求可能な機能」の
 * 平坦なビットマスクとして射影したもの。ENABLE/DISABLE override とは別レイヤ。
 */
enum {
	TERSE_FEAT_BASIC_OUTPUT      = 1ull << 0,
	TERSE_FEAT_CURSOR_VISIBILITY = 1ull << 1,
	TERSE_FEAT_MOVE_ABSOLUTE     = 1ull << 2,
	TERSE_FEAT_MOVE_RELATIVE     = 1ull << 3,
	TERSE_FEAT_CLEAR_LINE        = 1ull << 4,
	TERSE_FEAT_CLEAR_SCREEN      = 1ull << 5,
	TERSE_FEAT_SIZE              = 1ull << 6,
	TERSE_FEAT_COLOR_BASIC4      = 1ull << 7,
	TERSE_FEAT_COLOR_BASIC16     = 1ull << 8,
	TERSE_FEAT_COLOR_PALETTE256  = 1ull << 9,
	TERSE_FEAT_COLOR_TRUECOLOR   = 1ull << 10,
	TERSE_FEAT_TEXT_STYLES       = 1ull << 11,
	TERSE_FEAT_MOUSE             = 1ull << 12,
	TERSE_FEAT_BRACKETED_PASTE   = 1ull << 13,
	TERSE_FEAT_TITLE             = 1ull << 14,
	TERSE_FEAT_HYPERLINK         = 1ull << 15,
	TERSE_FEAT_CURSOR_SHAPE      = 1ull << 16,
	TERSE_FEAT_CLIPBOARD_WRITE   = 1ull << 17,
	TERSE_FEAT_IMAGE_INLINE      = 1ull << 18,
	TERSE_FEAT_NOTIFICATION_BELL    = 1ull << 19,
	TERSE_FEAT_NOTIFICATION_VISUAL  = 1ull << 20,
	TERSE_FEAT_NOTIFICATION_DESKTOP = 1ull << 21
};
```

**色機能ビットの意味論（重要）**: 色は段階なので「以上」で充足する。
端末が PALETTE256 を提供するなら `TERSE_FEAT_COLOR_BASIC16` の要求も充足される。
→ ケイパビリティ→機能ビット射影時に、`colors` 段階に応じて**該当段以下の色ビットをすべて立てる**:

```c
switch (caps.colors) {
case TERSE_COLOR_TRUECOLOR:  feats |= TERSE_FEAT_COLOR_TRUECOLOR;  /* fallthrough */
case TERSE_COLOR_PALETTE256: feats |= TERSE_FEAT_COLOR_PALETTE256; /* fallthrough */
case TERSE_COLOR_BASIC16:    feats |= TERSE_FEAT_COLOR_BASIC16;    /* fallthrough */
case TERSE_COLOR_BASIC4:     feats |= TERSE_FEAT_COLOR_BASIC4;     /* fallthrough */
case TERSE_COLOR_NONE:       break;
}
```

### 3.5 ケイパビリティ→機能ビット射影関数（内部）

`terse_capabilities.c` に内部ヘルパを追加:

```c
static uint64_t features_from_capabilities(const terse_capabilities_t *caps);
```

各 `has_*` フラグ・`mouse != NONE`・`images != NONE`・`notifications` の各ビット・
`colors` 段階（§3.4 の fallthrough）を `TERSE_FEAT_*` に射影する。

### 3.6 新規公開 API の実装（`terse_capabilities.c` + `include/terse.h`）

```c
/* 要求した機能のうち、端末が提供できなかったものを返す（0 なら全充足）。
 * provided = features_from_capabilities(現在の capabilities)
 * return wanted & ~provided;
 * 注: Phase 4 では「検査のみ」。要求の記録や副作用（自動有効化）は行わない。
 *     enable は従来どおり terse_capabilities_enable() で明示的に行う。 */
uint64_t terse_caps_missing(terse_handle_t handle, uint64_t wanted);

/* require: wanted を要求し、不足分を返す。caps_missing と同義だが
 * 「宣言」の意図を明示する入口。Phase 4 では caps_missing への薄いラッパ。
 * 戻り値 0 = 全充足。利用側は missing を見て degrade 方針を決める。 */
uint64_t terse_require(terse_handle_t handle, uint64_t wanted);

/* 現在有効化されている（override 適用後の capabilities が提供する）機能ビット。
 * = features_from_capabilities(handle->capabilities) */
uint64_t terse_get_active_features(terse_handle_t handle);
```

宣言は `include/terse.h` の `terse_get_capabilities` 近傍に追加。
`terse_require` / `terse_caps_missing` は副作用を持たない検査関数とする
（自動 enable はスコープ外。意味論を単純に保ち、利用側が missing を見て判断する設計）。

**API 配置**: これらは中レベル層（セッション／ケイパビリティ管理）の API なので、
`include/terse.h` 本体に置く（`include/terse/term.h` には移さない）。

### 3.7 プロファイル API の内部ケイパビリティ展開（整理）

現状 `terse_open(profile, ...)` がプロファイルを受ける。プロポーザル §3 は
「既存プロファイル API を内部でケイパビリティ展開する形に置き換え」とするが、
調査の結果**プロファイルは既に検出ケイパビリティの上に被さる束**として機能しており、
内部表現の作り直しは不要。Phase 4 では:
- プロファイル → 機能ビット束のマッピング表（`TERSE_FEAT_*` の組み合わせ）を内部に用意し、
  `terse_require_profile()` 相当（プロファイルを機能ビットに展開して require する糖衣）を**任意で**追加検討
- ただしこれは「あると便利」レベル。Phase 4 のコア（require/missing/active + BASIC4）を
  優先し、プロファイル展開ヘルパは余力があれば追加、なければ Phase 4 完了後の去就判断（§4.1）に委ねる

→ **Phase 4 必須スコープからは外し、optional とする**（コア API 完成を優先）。

---

## 4. テスト計画

`tests/unit/` に追加（`tests/CMakeLists.txt` に登録）。
端末検出に依存するテストは `tests/test_env.h` の backup/restore ヘルパを使う（CLAUDE.md 既定）。

1. **`terse_caps_missing` 基本**: モック capabilities で TrueColor 端末に
   `TERSE_FEAT_COLOR_BASIC16 | TERSE_FEAT_MOUSE` を要求 → mouse 無し端末なら mouse だけ missing
2. **色段階の「以上」充足**: PALETTE256 端末に BASIC16 を要求 → missing=0
3. **`terse_get_active_features`**: enable/disable override 後に active が追従することを確認
4. **BASIC4 degrade**: `terse_style_degrade_color()` に BASIC16/TrueColor 色 + BASIC4 support を
   渡し、4 色枠に丸まることを確認（マッピングは暫定でよいが、決定的であること）
5. **BASIC4 recompute ガード**: detected.colors=BASIC4 + has_sgr_basic=1 の状態で recompute →
   colors が BASIC4 のまま（BASIC16 に上書きされない）ことを確認
6. **後方互換**: 既存の色・プロファイルテストが BASIC4 挿入後も通ること（enum シフトの影響確認）
7. **`features_from_capabilities` 射影**: 代表的な端末プロファイルで期待ビットが立つこと

---

## 5. 作業手順（確立したパターン）

1. `include/terse.h`: BASIC4 追加、`TERSE_FEAT_*` enum 追加、新 API 3 本の宣言（+ `<stdint.h>` 確認）
2. `terse_style.c`: rank に BASIC4、degrade に BASIC4 case（暫定マッピング + コメント）
3. `terse_capabilities.c`: recompute の BASIC4 ガード、`features_from_capabilities`、新 API 3 本実装
4. `tests/unit/` + `tests/CMakeLists.txt`: §4 のテスト追加
5. クリーンビルド（`rm -rf build && cmake ... && ninja -C build`）
6. `ctest --test-dir build --output-on-failure`
7. `include/terse.h` の差分確認（追加のみ・既存シンボル削除なしを目視）
8. clang-format フック後に再ビルド確認
9. Conventional Commits でコミット（`feat: ...`）

---

## 6. プロポーザル本体への反映（任意）

- §4.2.2 の「enum 末尾追加なら ABI 互換」は BASIC4 が**中間挿入**になるため、
  実装後に「ソース互換だが ABI は再コンパイル要」の注記を追加するか検討
- §2.1 の「色 API 経路非依存化」は「消費側は既に colors 一本化済み・入力 3 フラグは導出元として
  温存」という調査結論を反映するか検討

---

## 7. リスクと留意点

- **enum 中間挿入の ABI 影響**: terse は同梱/静的ビルド前提なので実害は小さいが、
  万一バイナリ配布する利用者がいれば再コンパイルが必要。コミットメッセージに明記
- **degrade の 4 色マッピング**: Phase 4 では暫定。Phase 4.5 で Human68k 実機色に合わせて確定する
  前提を崩さないよう、マッピングを 1 関数に閉じてコメントを残す
- **require の意味論を膨らませない**: 自動 enable や要求の永続化を入れると意味論が複雑化する。
  Phase 4 は「検査のみ」に徹し、enable は既存 override API に委ねる
- **term の handle 逆流（Phase 8 課題）**: 新 API も handle 経由でケイパビリティを読むため、
  既存の逆流構造を踏襲する。Phase 4 では解消しない（スコープ外）
