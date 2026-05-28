# Phase 2: 内部ヘッダの分離 詳細計画

> **Status: Draft（方針確定済み、着手前）**
> 親ドキュメント: [redesign-proposal.md](redesign-proposal.md) Section 3 「Phase 2」
> 前フェーズ: [redesign-phase1-plan.md](redesign-phase1-plan.md)（完了）
> Last updated: 2026-05-29

## 0. 目的とスコープ

Phase 1 でディレクトリは `src/term/`（低レベル）と `src/core/`（中レベル）に
分離した。Phase 2 では **層間の依存を内部ヘッダで明示**し、「中レベルが
低レベルを呼ぶ経路」を 1 つの集約ヘッダに集約する。

### このフェーズでやること
- `src/term/terse_term_internal.h`（ファサード集約ヘッダ）を新設
- core（中レベル）が term（低レベル）を呼ぶときは、個別ヘッダではなく
  この集約ヘッダ 1 本を include する形に整理
- term → core の逆流依存（`terse_handle.h` 依存）の現状を**記録**する（§4）

### このフェーズでやらないこと
- 公開ヘッダ `include/terse.h` の変更（Phase 3 の領分）
- 個別ヘッダ（`terse_codec.h` 等）の廃止（ファサード型なので個別ヘッダは残す）
- 逆流依存（term → `struct terse_handle`）の解消（Phase 8 の本質的課題）
- API シグネチャ・ロジックの変更

> **原則**: Phase 1 同様、振る舞いは変えない。テスト結果は前後で完全一致。

---

## 1. 確定した設計方針

| 論点 | 結論（2026-05-29 確定） | 根拠 |
|------|------------------------|------|
| 集約ヘッダの形 | **ファサード集約型** | §2。core は集約ヘッダ 1 本を include。個別ヘッダは残すので diff 最小・低リスク |
| 逆流依存の扱い | **記録のみ、解消は Phase 8** | §4。`struct terse_handle` 依存はハンドル分離（スコープ外）の本質的課題 |

---

## 2. ファサード集約ヘッダの設計

### 2.1 現状の依存（Phase 1 完了時点、実地調査済み）

core の各ファイルが直接 include している term ヘッダ:

| core ファイル | include している term ヘッダ |
|---------------|------------------------------|
| `core/terse.c` | capabilities, codec, detection, event_helpers, input, keyboard, style, unicode（8 種） |
| `core/terse_output.c` | codec, style |
| `core/terse_state.c` | style |
| `core/terse_handle.h` | codec（`terse_codec_t` を struct メンバに持つため） |

### 2.2 新設する `src/term/terse_term_internal.h`

term の公開内部ヘッダを束ねるファサード:

```c
#ifndef TERSE_TERM_INTERNAL_H
#define TERSE_TERM_INTERNAL_H

/*
 * terse-term (低レベル層) が中レベル層に公開する内部 API の集約ヘッダ。
 * core 層はこのヘッダ 1 本を include することで term 層の機能にアクセスする。
 * 個別ヘッダ（terse_codec.h 等）は term 層内部での利用のために残す。
 */

#include "terse_capabilities.h"
#include "terse_codec.h"
#include "terse_detection.h"
#include "terse_device.h"
#include "terse_cursor.h"
#include "terse_event_helpers.h"
#include "terse_graphics.h"
#include "terse_input.h"
#include "terse_keyboard.h"
#include "terse_style.h"
#include "terse_unicode.h"

#endif /* TERSE_TERM_INTERNAL_H */
```

> 注: `mini_iconv.h` は codec の実装内部の下請けであり、core から直接呼ばないため
> 集約ヘッダには含めない（term 内部でのみ `terse_codec.c` が include する）。
> `terse_cursor.h` / `terse_device.h` / `terse_graphics.h` は現状 core の各 .c から
> 直接 include されていないが、term が公開する低レベル API として論理的に
> 集約対象に含める（将来 core から呼ぶ余地を残す）。

### 2.3 core 側の整理

各 core ファイルの個別 term include を集約ヘッダ 1 本に置き換える:

```c
/* before (core/terse.c) */
#include "terse_capabilities.h"
#include "terse_codec.h"
#include "terse_detection.h"
#include "terse_event_helpers.h"
#include "terse_input.h"
#include "terse_keyboard.h"
#include "terse_style.h"
#include "terse_unicode.h"

/* after */
#include "terse_term_internal.h"
```

**例外: `core/terse_handle.h`**
- `terse_handle.h` は `struct terse_handle` のメンバに `terse_codec_t codec` を
  持つため、`terse_codec.h` の型定義が必要。
- ここで集約ヘッダを include すると、term の全公開 API が core の基盤ヘッダに
  漏れて依存が広がる。**`terse_handle.h` は `terse_codec.h` の直接 include を維持**する
  （型定義のための最小依存に留める）。
- この例外は §4 の逆流依存と表裏一体の構造的事情であり、Phase 2 では現状維持とする。

### 2.4 整理後の include 方針（まとめ）

| ファイル | term への include 方針 |
|----------|------------------------|
| `core/terse.c` | `terse_term_internal.h` のみ |
| `core/terse_output.c` | `terse_term_internal.h` のみ |
| `core/terse_state.c` | `terse_term_internal.h` のみ |
| `core/terse_handle.h` | `terse_codec.h` のみ（型定義の最小依存、例外） |
| term 内部の .c / .h | 従来通り個別ヘッダを直接 include（変更なし） |

---

## 3. 「中レベルは内部ヘッダ経由でのみ低レベルを呼ぶ」の担保

プロポーザル Phase 2 の眼目は「中レベル層は内部ヘッダ経由でしか低レベル層を
呼ばない」整理である。ファサード型では以下で担保する:

- core の各 .c は term の個別ヘッダを **直接 include しない**
  （`terse_handle.h` の codec 例外を除く）
- term へのアクセスは必ず `terse_term_internal.h` を経由する
- これにより「core が term の何を使っているか」が集約ヘッダの include リストに
  一望できる

> 将来、集約ヘッダに載せていない term 内部関数を core が呼ぼうとすると、
> 個別ヘッダの直接 include が必要になり「集約ヘッダ経由」原則の違反として
> レビューで検知できる。

---

## 4. 逆流依存の記録（term → core の `struct terse_handle` 依存）

Phase 1 完了時点の実地調査で判明した**層をまたぐ密結合**を記録する。
**Phase 2 では解消しない**（Phase 8 ハンドル分離の本質的課題）。

### 4.1 現状

term の以下のファイルが core 所属の `terse_handle.h`（`struct terse_handle` 定義）
を include し、`handle->...` でメンバに直接アクセスしている:

```
term/terse_capabilities.c / .h
term/terse_cursor.c
term/terse_device.c
term/terse_graphics.c
term/terse_input.c
term/terse_keyboard.c / .h
term/terse_style.c
term/terse_unicode.c
term/mini_iconv.c
term/terse_detection.c
```

### 4.2 なぜ Phase 2 で解消しないか

- `struct terse_handle` は「セッション状態の本体」であり core に属する。
  term がこれに依存しているのは、低レベル関数が `handle->options.output_fd` 等の
  セッション状態を引数ではなくハンドル経由で受け取る設計になっているため。
- これを解消するには、低レベル関数のシグネチャを「ハンドルではなく必要な値を
  引数で受け取る」形に変える必要があり、これは **Phase 8（ハンドル分離 =
  `terse_term_t` / `terse_t` の分割）の本質的作業**。
- プロポーザル §3 で Phase 8 は明確にスコープ外と宣言されている。

### 4.3 Phase 2 での扱い

- 集約ヘッダ `terse_term_internal.h` は「core → term」方向のみを整理する。
- 「term → core（handle）」の逆流は触らず、この §4 に現状として記録するに留める。
- 将来 Phase 8 を検討する際の出発点として、4.1 のファイル一覧が依存解消の
  作業対象リストになる。

---

## 5. 実施手順（チェックリスト）

1. [ ] `src/term/terse_term_internal.h` を新設（§2.2）
2. [ ] `core/terse.c` の個別 term include を集約ヘッダに置換
3. [ ] `core/terse_output.c` の個別 term include を集約ヘッダに置換
4. [ ] `core/terse_state.c` の個別 term include を集約ヘッダに置換
5. [ ] `core/terse_handle.h` は `terse_codec.h` 直接 include を維持（変更なし、§2.3 例外）
6. [ ] クリーンビルド（`cmake -S . -B build -G Ninja && ninja -C build`）
7. [ ] `ctest --test-dir build --output-on-failure` が前後で一致
8. [ ] `git diff include/terse.h` が空であることを確認
9. [ ] term 内部ファイルの include が変わっていないこと（core 側のみの変更）を確認

---

## 6. 影響範囲

- **ビルドシステム**: `terse_term_internal.h` はヘッダなので CMake のソースリスト
  変更は不要。PRIVATE include ディレクトリ（`src/term`）は Phase 1 で登録済みなので
  追加設定不要。
- **tests / samples / tools**: core の .c が include するヘッダが変わるだけで、
  公開 API・内部 API のシグネチャは不変。影響なし。
- **公開ヘッダ**: `include/terse.h` 無変更。
