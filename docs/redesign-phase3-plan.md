# Phase 3: 公開ヘッダの分離 詳細計画

> **Status: Draft（方針確定済み、着手前）**
> 親ドキュメント: [redesign-proposal.md](redesign-proposal.md) Section 3 「Phase 3」
> 前フェーズ: [redesign-phase1-plan.md](redesign-phase1-plan.md) / [redesign-phase2-plan.md](redesign-phase2-plan.md)（完了）
> Last updated: 2026-05-29

## 0. 目的とスコープ

Phase 1-2 で内部実装の層分離（ディレクトリ + 集約ヘッダ）が完了した。
Phase 3 では **公開ヘッダ**を分離し、ユーザーが初めて「低レベル層 API だけを
意識する」選択肢を持てるようにする。

### このフェーズでやること
- `include/terse/term.h` を新設し、低レベル層 API の**関数宣言**を切り出す
- `include/terse.h` は `terse/term.h` を include する形にして既存利用側との互換を保つ
- 型定義（enum/struct）は共有資産として `terse.h` に残す（§2 の確定方針）

### このフェーズでやらないこと
- 型定義の分割（共有型が多く、分割は費用対効果が低い。§2）
- API シグネチャ・ロジックの変更
- 関数の実装ファイル間移動（Phase 1 で確定した配置を維持）
- core 専用ヘッダ `terse/core.h` の新設（今回は term のみ切り出す）

### 互換性方針
- **既存利用側は `#include <terse.h>` のまま無変更で動く**ことを最優先。
- `terse/term.h` は新たな選択肢の追加であり、必須化しない。
- プロポーザル §3 は「include path の見直しが必要な利用側があるかもしれない」と
  予告しているが、本計画では `terse.h` が `terse/term.h` を include することで
  **既存利用側の include path 変更を不要にする**（破壊的変更を回避）。

---

## 1. 確定した設計方針

| 論点 | 結論（2026-05-29 確定） | 根拠 |
|------|------------------------|------|
| `terse/term.h` の粒度 | **関数宣言のみ分離**。型定義は terse.h に残す | §2。型は term/core で広く共有されており、型の分割は錦の御旗を避け低リスク |

---

## 2. なぜ「関数宣言のみ分離」か

公開ヘッダの型定義（enum/struct）の多くは term / core 双方で共有される:

- `terse_handle_t`: 全 API の第 1 引数
- `terse_color_t` / `terse_style_t`: term（色構築・SGR 生成）と core（状態保持）双方
- `terse_capabilities_t`: term（検出）と core（参照）双方の境界型
- `terse_event_t` / `terse_size_t` / `terse_error_t`: 全層横断
- `terse_mouse_mode_t` / `terse_cursor_shape_t` / `terse_image_*`: term 寄りだが
  `terse_capabilities_t` のメンバとして core からも参照される

これらを term / core に振り分けようとすると、`terse_capabilities_t` のような
**境界型**をどちらに置くかで循環 include や重複定義のリスクが生じる。

→ 型は**共有資産として `terse.h` に集約したまま**、関数宣言だけを層別に
分離する。これがリスク最小で Phase 3 の目的（低レベル API の可視化）を達成する。

---

## 3. 関数の層分類

実装ファイル（Phase 1-2 で確定した配置）を基準に、各公開関数を分類する。

### 3.1 term（低レベル）→ `include/terse/term.h` へ

| 関数 | 実装ファイル |
|------|--------------|
| `terse_capabilities_enable` | term/terse_capabilities.c |
| `terse_capabilities_disable` | term/terse_capabilities.c |
| `terse_capabilities_reset_overrides` | term/terse_capabilities.c |
| `terse_move_to` | term/terse_cursor.c |
| `terse_move_by` | term/terse_cursor.c |
| `terse_show_cursor` | term/terse_cursor.c |
| `terse_set_cursor_shape` | term/terse_cursor.c |
| `terse_enable_mouse` | term/terse_device.c |
| `terse_disable_mouse` | term/terse_device.c |
| `terse_enable_bracketed_paste` | term/terse_device.c |
| `terse_disable_bracketed_paste` | term/terse_device.c |
| `terse_set_title` | term/terse_device.c |
| `terse_set_hyperlink` | term/terse_device.c |
| `terse_set_clipboard` | term/terse_graphics.c |
| `terse_display_image` | term/terse_graphics.c |
| `terse_display_image_inline` | term/terse_graphics.c |
| `terse_notify` | term/terse_graphics.c |
| `terse_keyboard_enable` | term/terse_keyboard.c |
| `terse_keyboard_disable` | term/terse_keyboard.c |
| `terse_keyboard_get_enabled` | term/terse_keyboard.c |
| `terse_keyboard_get_supported` | term/terse_keyboard.c |
| `terse_encode_utf8` | term/terse_codec.c |
| `terse_char_width` | term/terse_unicode.c |
| `terse_color_default` | core/terse.c（※論理的には term、§3.3） |
| `terse_color_basic` | core/terse.c（※同上） |
| `terse_color_palette` | core/terse.c（※同上） |
| `terse_color_truecolor` | core/terse.c（※同上） |
| `terse_style_default` | core/terse.c（※同上） |

### 3.2 core（中レベル）→ `include/terse.h` に残す

| 関数 | 実装ファイル |
|------|--------------|
| `terse_open` / `terse_close` | core/terse.c |
| `terse_get_capabilities` | core/terse.c |
| `terse_get_size` | core/terse.c |
| `terse_get_cursor_position` | core/terse.c |
| `terse_get_options` | core/terse.c |
| `terse_validate_options` | core/terse.c |
| `terse_get_last_error` | core/terse.c |
| `terse_read_event` | core/terse.c |
| `terse_clear_screen` / `terse_clear_line` | core/terse_output.c |
| `terse_write_text` / `terse_flush` | core/terse_output.c |
| `terse_set_style` / `terse_reset_style` | core/terse_output.c |
| `terse_state_override` / `terse_state_clear` | core/terse_state.c |
| `terse_push_state` / `terse_pop_state` | core/terse_state.c |
| `terse_capture_state` / `terse_restore_state` | core/terse_state.c |

### 3.3 判断が要る関数: 色 / style コンストラクタ

`terse_color_default/basic/palette/truecolor` と `terse_style_default` は
**実装は core/terse.c** にあるが（Phase 1 で「terse.c は分割せず core へ」とした
副作用で同居）、**API の性質は term（色値の構築、SGR 生成の前段）**である。

- **方針**: ヘッダ上は **term 分類**（`terse/term.h` へ宣言を移す）。
  - 根拠: これらは端末への色表現を組み立てる低レベルユーティリティであり、
    セッション状態（ハンドル）に依存しない純粋関数。論理的に term に属する。
  - 実装ファイルの物理的所在（core/terse.c）とヘッダ分類が一致しないが、
    これは Phase 1 の「terse.c 非分割」判断の既知の帰結。実装の物理移動は
    将来 Phase（terse.c の解体）で扱う。Phase 3 はヘッダ上の論理分類を優先する。
- この不一致は §3.1 の表に「※」付きで明示済み。

> 代替案（実装位置に合わせて core 分類）も検討したが、ハンドル非依存の純粋な
> 色構築関数を中レベル API とするのは利用者の直感に反するため採らない。

---

## 4. ヘッダの構成（Phase 3 完了後）

```
include/
├── terse.h            # 型定義（全共有）+ core 関数宣言 + #include <terse/term.h>
├── terse_test.h       # （変更なし）
└── terse/
    └── term.h         # 低レベル層の関数宣言のみ（型は terse.h に依存）
```

### 4.1 `include/terse/term.h`（新設）

```c
#ifndef TERSE_TERM_H_INCLUDED
#define TERSE_TERM_H_INCLUDED

#include <terse.h>   /* 型定義（terse_handle_t, terse_color_t 等）を取得 */

#ifdef __cplusplus
extern "C" {
#endif

/* 色・スタイル構築（ハンドル非依存の低レベルユーティリティ）*/
terse_style_t terse_style_default(void);
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);

/* ケイパビリティのランタイム上書き */
terse_error_t terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask);
terse_error_t terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask);
terse_error_t terse_capabilities_reset_overrides(terse_handle_t handle);

/* カーソル制御 */
terse_error_t terse_move_to(terse_handle_t handle, int row, int col);
terse_error_t terse_move_by(terse_handle_t handle, int drow, int dcol);
terse_error_t terse_show_cursor(terse_handle_t handle, int visible);
terse_error_t terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking);

/* デバイス制御（マウス・ペースト・タイトル・ハイパーリンク）*/
terse_error_t terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode);
terse_error_t terse_disable_mouse(terse_handle_t handle);
terse_error_t terse_enable_bracketed_paste(terse_handle_t handle);
terse_error_t terse_disable_bracketed_paste(terse_handle_t handle);
terse_error_t terse_set_title(terse_handle_t handle, const char *title);
terse_error_t terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label);

/* グラフィクス・クリップボード・通知 */
terse_error_t terse_set_clipboard(terse_handle_t handle, const char *data);
terse_error_t terse_display_image(terse_handle_t handle, const terse_image_request_t *request);
terse_error_t terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
terse_error_t terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload);

/* キーボードプロトコル */
terse_error_t terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask);
terse_error_t terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask);
unsigned int terse_keyboard_get_enabled(terse_handle_t handle);
unsigned int terse_keyboard_get_supported(terse_handle_t handle);

/* エンコーディング・文字幅ユーティリティ */
int terse_encode_utf8(unsigned int scalar, unsigned char *out);
int terse_char_width(terse_handle_t handle, unsigned int scalar);

#ifdef __cplusplus
}
#endif

#endif /* TERSE_TERM_H_INCLUDED */
```

### 4.2 `include/terse.h`（改変）

- 上記 term 関数の宣言を**削除**し、代わりにファイル末尾近くで
  `#include <terse/term.h>` する。
- 型定義・core 関数宣言はそのまま残す。
- include 位置の注意: `terse/term.h` は `terse.h` の型定義を必要とするため、
  全型定義の**後**に include する（インクルードガードで二重 include は安全だが、
  型が見えている位置で include することが必須）。

```c
/* terse.h 末尾、core 関数宣言の後 */
...
int terse_char_width(terse_handle_t handle, unsigned int scalar);  /* ← これは term.h へ移動 */

/* 低レベル層 API（関数宣言）*/
#include <terse/term.h>

#ifdef __cplusplus
}
#endif
#endif
```

> **include ガードの相互作用に注意**: `terse/term.h` は `<terse.h>` を include し、
> `terse.h` は `<terse/term.h>` を include する循環構造になる。両者の
> インクルードガード（`TERSE_H_INCLUDED` / `TERSE_TERM_H_INCLUDED`）により
> 無限再帰は防がれるが、**include 順序で見える宣言が変わる**ため §5 で検証する。
> - `terse.h` 経由: 型 → term.h（型は見えている）→ OK
> - `terse/term.h` 単独 include: `<terse.h>` を引き込み、その中で型定義 →
>   末尾で `<terse/term.h>` を再帰 include するがガードで弾かれる → term.h の
>   残りの宣言が処理される → OK
> この構造の健全性を §5 のビルドで両経路から確認する。

---

## 5. 実施手順（チェックリスト）

1. [ ] `include/terse/` ディレクトリ作成
2. [ ] `include/terse/term.h` を新設（§4.1）
3. [ ] `include/terse.h` から term 関数宣言を削除し、末尾で `#include <terse/term.h>`（§4.2）
4. [ ] CMake の install(DIRECTORY include/) が `terse/term.h` も配置することを確認
   （`FILES_MATCHING PATTERN "*.h"` は再帰的なので追加設定不要のはず → 要確認）
5. [ ] クリーンビルド（全ターゲット）
6. [ ] **両 include 経路の検証**:
   - 既存の `#include <terse.h>` だけで全 API が見える（テスト・samples で確認）
   - `#include <terse/term.h>` 単独でも term API が見える（検証用の小さな .c を一時作成 or 既存 samples で確認）
7. [ ] `ctest --test-dir build --output-on-failure` が前後で一致
8. [ ] samples / tools がすべて無変更でビルドできる（include path 変更不要の確認）

---

## 6. 影響範囲

- **公開 API**: 関数の宣言場所が変わるが、`terse.h` を include する限り
  すべて従来通り見える。**シグネチャ不変**。
- **利用側**: `#include <terse.h>` のままで無変更。新たに低レベルのみ使いたい
  利用者は `#include <terse/term.h>` を選べる。
- **インストール**: ヘッダのインストール先に `terse/term.h` が増える。
- **CMake**: ソースリスト変更なし（ヘッダのみ）。install ルールは要確認（手順 4）。

---

## 7. オープン論点（着手時に確認）

| ID | 論点 | 暫定方針 |
|----|------|----------|
| A | `terse_get_capabilities` の分類 | **core**（ハンドル状態の参照、§3.2）。検出系 term API とは別物 |
| B | install ルールが terse/ サブディレクトリを拾うか | `FILES_MATCHING PATTERN "*.h"` は再帰想定。手順 4 で実地確認 |
| C | term.h の include 表記 | 既存実装の大半が `"terse.h"`（引用符）。それに合わせ term.h 内も `"terse.h"`、terse.h 内は `"terse/term.h"` を使う |
