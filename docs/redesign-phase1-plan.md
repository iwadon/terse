# Phase 1: ディレクトリ分離 詳細計画

> **Status: Draft（レビュー待ち）**
> 親ドキュメント: [redesign-proposal.md](redesign-proposal.md) Section 3 「Phase 1」
> Last updated: 2026-05-29

## 0. 目的とスコープ

`src/` 直下にフラットに置かれている実装ファイルを、再設計プロポーザルの三層分離
（terse-term / terse）に沿って `src/term/` と `src/core/` の 2 ディレクトリに
物理的に振り分ける。

### このフェーズでやること
- ファイルの移動（`git mv`）
- `#include` パスの調整（移動に伴う相対パスの修正のみ）
- `CMakeLists.txt` のソースパス更新
- 必要なら `terse.c` の **関数単位での分割**（後述、§3 で論点として明示）

### このフェーズでやらないこと（重要）
- **公開ヘッダ `include/terse.h` は一切変更しない**（§5 で確認）
- 内部ヘッダの再編・統合（Phase 2 の領分）
- API シグネチャの変更、関数のリネーム
- ロジックの変更（純粋な移動・分割のみ）

> **原則**: Phase 1 は「どのコードがどちらの層か」を**可視化**するだけ。
> 振る舞いは 1 ビットも変えない。テスト結果は移動前後で完全一致すること。

---

## 1. 層の判定基準

プロポーザル §1.1 の定義を判定基準に落とし込む:

| 層 | 責務 | 判定キーワード |
|----|------|----------------|
| **terse-term**（低レベル） | 端末ケイパビリティ検出、エスケープシーケンス生成、入力イベントのバイト列パース、デバイス問い合わせ | 「端末は何ができるか」「このバイト列は何を意味するか」「この機能のエスケープは何か」 |
| **terse**（中レベル） | セッション状態管理、カーソル/スタイルの論理状態追跡、イベントループ統合、（将来）描画バッファ | 「今の状態は何か」「状態をどう保存/復元するか」「ハンドルのライフサイクル」 |
| **terse-sys**（プラットフォーム抽象） | OS 固有 I/O。**今回は移動しない**（§4 参照） | termios / Win32 Console / IOCS |

判断に迷うファイルは §2 の表で個別に根拠を示す。

---

## 2. ファイル単位のマッピング

### 2.1 `src/term/` へ（低レベル: terse-term）

| ファイル | 行数 | 根拠 |
|----------|------|------|
| `terse_detection.c` / `.h` | 572 | ケイパビリティ検出そのもの。terse-term の中核 |
| `terse_capabilities.c` / `.h` | 281 | ケイパビリティの recompute / プロファイル展開。検出結果の加工 |
| `terse_style.c` / `.h` | 405 | 色の degrade（端末能力に応じた変換）と **SGR エスケープ生成**。`terse_style_emit_sequence` は典型的な低レベル出力 |
| `terse_cursor.c` / `.h` | 221 | カーソル移動・可視性・形状の**エスケープ生成**（CUP/CUU 等） |
| `terse_device.c` / `.h` | 218 | マウストラッキング・bracketed paste・タイトル等のデバイス制御エスケープ |
| `terse_graphics.c` / `.h` | 354 | 画像プロトコル（iTerm2/Sixel/Kitty）のエスケープ生成 |
| `terse_input.c` / `.h` | 714 | 入力エスケープシーケンスの**パース**（CSI/SS3/Alt 等）。terse-term の中核 |
| `terse_keyboard.c` / `.h` | 97 | Kitty keyboard protocol の enable/disable エスケープ |
| `terse_event_helpers.c` / `.h` | 57 | `terse_event_t` 構築ヘルパー。入力パースの下請け |
| `terse_codec.c` / `.h` | 551 | 端末ネイティブエンコーディング ⇔ UTF-8 変換。端末境界の処理 |
| `terse_unicode.c` / `.h` | 713 | East Asian Width によるセル幅推定。「端末上で何セルか」の判定 |
| `mini_iconv.c` / `.h` / `mini_iconv_tables.h` | 379+ | codec の下請け（Shift_JIS フォールバック） |

**補足: codec / unicode の所属について**
- どちらも「端末という外部境界をどう解釈するか」に属するため terse-term と判定。
- ただしプロポーザル §2.5 では「内部エンコーディングは現状維持」とあり、将来
  terse-ui 寄りに再配置する可能性は残る。Phase 1 では低レベル扱いで確定し、
  必要なら後続 Phase で見直す（要レビュー判断、§6 論点 C）。

### 2.2 `src/core/` へ（中レベル: terse）

| ファイル | 行数 | 根拠 |
|----------|------|------|
| `terse_state.c` / `.h` | 168 | push/pop/capture/restore。論理状態スタックの管理。中レベルの中核 |
| `terse_output.c` / `.h` | 327 | `terse_write_text` / `terse_set_style` / `terse_flush` 等の**統合 API**。内部で低レベルのエスケープ生成を呼び出しつつ、ハンドル状態を更新する「束ね役」 |
| `terse_handle.h` | 132 | `struct terse_handle` 定義。セッション状態の本体。中レベルに属する |

**補足: `terse_output.c` の所属について**
- `terse_write_text` は「codec で変換 → unicode で幅計算 → カーソル状態更新 →
  バイト出力」という低レベル機能の**オーケストレーション**を行う。
- 個々のエスケープ生成は低レベルだが、状態追跡と束ねは中レベルの責務。
- よって `terse_output.c` 全体は core に置く。将来バッファドモード（Phase 5）が
  入ると、この束ね役がバッファ層に置き換わる中心地点になる。

### 2.3 プラットフォーム層（今回は移動しない、§4 参照）

| ファイル | 扱い |
|----------|------|
| `terse_platform.h` | terse-sys 相当。Phase 1 では `src/` 直下に残す |
| `terse_posix.c` | 同上 |
| `terse_windows.c` | 同上 |
| `terse_human68k.c` | 同上 |
| `terse_platform_stub.c` | 同上 |

### 2.4 テスト用（今回は移動しない）

| ファイル | 扱い |
|----------|------|
| `terse_test.c` / `terse_test_internal.h` | test mode 専用。`src/` 直下に残す（横断的関心事） |

### 2.5 混在ファイル: `terse.c`（最重要論点 → §3）

`terse.c`（619 行）は低レベルと中レベルが混在しているため、単純移動できない。
§3 で専用に扱う。

---

## 3. `terse.c` の扱い（Phase 1 最大の論点）

`terse.c` には以下が混在している:

| 関数 / 定義 | 層 | 備考 |
|-------------|----|----|
| `terse_color_default/basic/palette/truecolor` | term | 色値コンストラクタ（公開 API 実装） |
| `terse_style_default` | term | スタイル初期化 |
| `TERSE_RESET_*_SEQ`（文字列定数） | term | エスケープシーケンス定数 |
| `base64_encode` | term | graphics の下請け（画像 base64） |
| `write_literal` / `write_sequence` | term/sys 境界 | プラットフォーム I/O の薄いラッパー |
| `emit_reset_sequences` | term | リセットエスケープ出力 |
| `terse_read_event` 本体 | **term** | 入力バイト列パースの統合（POSIX 経路） |
| `terse_open` / `terse_close` | **core** | ハンドルのライフサイクル管理 |
| `terse_get_capabilities` | core | ハンドル状態の参照 |
| `terse_get_size` / `refresh_size` | core | サイズ状態の管理 |
| `terse_get_cursor_position` | core | カーソル状態の参照 |
| `terse_get_options` / `terse_get_last_error` | core | ハンドル状態の参照 |
| `update_effective_style` | core | ハンドルのスタイルキャッシュ更新 |
| `set_error` / `clear_error` / `ensure_handle` | core 共通基盤 | 全層が使う |

### 3.1 方針案（要レビュー判断 → §6 論点 A）

Phase 1 の「破壊的変更なし・移動のみ」の原則をどこまで厳格に守るかで 2 案ある。

#### 案 A-1: `terse.c` は分割せず core に置く（保守的・推奨）
- `terse.c` をそのまま `src/core/terse.c` へ移動。
- 混在は残るが、**Phase 1 では物理移動のみ**という原則を最も厳格に守れる。
- `terse_read_event` 等の term 寄りコードの切り出しは **Phase 2**（内部ヘッダ分離）
  以降に持ち越す。
- (+) リスク最小。diff が `git mv` + include 修正のみで完結
- (−) `terse.c` 内の層混在は可視化されない（Phase 1 の主目的が一部未達）

#### 案 A-2: `terse.c` を関数単位で 2 分割する
- term 寄り関数 → `src/term/terse_term_core.c`（仮称）に切り出し
- core 寄り関数 → `src/core/terse.c` に残す
- (+) Phase 1 の「層の可視化」目的を最大限達成
- (−) 関数移動に伴い `static` の解除や前方宣言の追加が必要になり、
      「ロジック不変」を保ちつつも diff が大きくなる
- (−) `write_literal`/`write_sequence` 等の共有関数の置き場所を決める必要がある

**推奨**: **案 A-1**。理由:
- Phase 1 の影響範囲を「ビルドシステムのみ」に厳密に閉じられる（プロポーザル §3 の宣言通り）
- `terse.c` の層分離は本質的に Phase 2（内部ヘッダ分離）の作業と一体で行う方が自然
- まず安全にディレクトリ構造を立ち上げ、混在ファイルの解体は次フェーズに回す

> この判断はユーザーレビューで確定させる（§6 論点 A）。

---

## 4. プラットフォーム層を移動しない理由

プロポーザル §1.1 では terse-sys（`src/terse_platform.h` 相当）を最下層として
定義しているが、Phase 1 では `src/` 直下に残す。理由:

- terse-sys の分離（`src/sys/` 新設等）はプロポーザルの Phase ロードマップに
  明示されていない。三層分離の主眼は term / core の分離にある。
- プラットフォーム層は CMake で条件付きビルド（POSIX/Windows/Human68k/stub）
  されており、移動するとビルド条件分岐との二重管理になりやすい。
- term / core の両方から参照されるため、どちらのディレクトリにも属さない。
  `src/` 直下に置くことで「共通の土台」という位置づけが明確になる。

> 将来 terse-sys を独立させる場合は別 Phase として扱う（プロポーザル §1.1 の
> 図には登場するが、本プロポーザルの段階的ロードマップ Phase 1-8 には含まれない）。

---

## 5. 公開ヘッダ非変更の確認

Phase 1 で `include/terse.h` を変更しないことを担保する条件:

1. **`#include "terse.h"` の解決**
   各実装ファイルは `#include "terse.h"`（または `"../include/terse.h"`）で
   公開ヘッダを参照している。移動後は CMake の
   `target_include_directories(... include)` が効いているため、
   `#include "terse.h"` は引き続き解決される（include パスはディレクトリ非依存）。
   - 例外: `terse_codec.h` は `#include "../include/terse.h"` という相対パスを
     使っている。`src/term/` へ移動すると `../../include/terse.h` になるため、
     **この相対 include は `"terse.h"` に統一して修正する**（include ディレクトリ
     経由で解決させる）。他に相対パス include がないか移動時に grep で確認する。

2. **内部ヘッダ間の相対 include**
   `terse_handle.h` 等の内部ヘッダ同士は `#include "terse_xxx.h"` で参照し合う。
   同一ディレクトリにある前提のものと、term↔core をまたぐものが出る。
   - 対策: `src/`, `src/term/`, `src/core/` の 3 つを**内部用 include ディレクトリ**
     として CMake に登録する（`target_include_directories(terse PRIVATE ...)`）。
     これにより `#include "terse_handle.h"` 等はディレクトリをまたいでも解決される。
   - これは公開ヘッダの変更ではない（PRIVATE スコープなので利用側に漏れない）。

3. **検証**
   - ビルドが通ること（全プラットフォーム条件）
   - `ctest` の結果が移動前と完全一致すること
   - `include/terse.h` の `git diff` が空であること

---

## 6. CMakeLists.txt の調整方針

### 6.1 ソースパスの更新

`add_library(terse ...)` と `target_sources(...)` 内のパスを新ディレクトリに更新:

```cmake
add_library(terse
  src/core/terse.c            # ← 案 A-1 の場合
  src/term/terse_unicode.c
  src/term/terse_codec.c
  src/term/terse_detection.c
  src/term/terse_event_helpers.c
  src/term/terse_input.c
  src/term/terse_style.c
  src/core/terse_output.c
  src/term/terse_graphics.c
  src/term/terse_cursor.c
  src/term/terse_device.c
  src/core/terse_state.c
  src/term/terse_capabilities.c
  src/term/terse_keyboard.c
  src/terse_test.c            # ← 直下のまま
)
```

プラットフォーム条件分岐（`terse_human68k.c` / `terse_windows.c` /
`terse_posix.c` / `terse_platform_stub.c`）と `mini_iconv.c` のパスは
`src/` 直下のまま（§2.3）。ただし `mini_iconv.c` は codec の下請けなので
`src/term/mini_iconv.c` へ移動する（§2.1 で term 判定済み）。

### 6.2 内部 include ディレクトリの登録

```cmake
target_include_directories(terse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/term
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
```

### 6.3 tests / samples への影響（実地調査済み 2026-05-29）

調査結果:
- tests は `src/` のソースを直接コンパイルせず、`terse` ライブラリに
  リンクしている（`target_link_libraries(terse_unit_test PRIVATE terse attest)`）。
  → ソースパス更新の必要なし。
- **ただし** `tests/unit/terse_event_helpers_test.c` が内部ヘッダ
  `#include "terse_event_helpers.h"` を直接 include している。このヘッダは
  `src/term/` へ移動するため、tests 側の include パスに `../src/term` の追加が必要。
- tests の現状 include 設定:
  ```cmake
  target_include_directories(terse_unit_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)
  ```
  → ここに `../src/term` `../src/core` を追加する:
  ```cmake
  target_include_directories(terse_unit_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/term
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core)
  ```
- tools は内部ヘッダを include していない（影響なし）。
- samples は公開ヘッダのみ使用（影響なし）。移動後にビルド確認する。

---

## 7. 実施手順（チェックリスト）

1. [ ] **事前調査**: `tests/CMakeLists.txt` と各テストが `src/` 内ファイルを
   直接参照していないか grep（`../src/` や `src/terse_` 等）
2. [ ] **事前調査**: 全 `.c`/`.h` の相対パス include を grep
   （`#include "../` パターン）して、移動で壊れるものを洗い出す
3. [ ] `src/term/` `src/core/` ディレクトリ作成
4. [ ] `git mv` でファイル移動（§2 の表に従う）
5. [ ] 相対 include の修正（§5.1 の `terse_codec.h` 等）
6. [ ] `CMakeLists.txt` のパス更新 + PRIVATE include ディレクトリ登録（§6）
7. [ ] クリーンビルド（`cmake -S . -B build -G Ninja && ninja -C build`）
8. [ ] `ctest --test-dir build --output-on-failure` が移動前と一致
9. [ ] `git diff include/terse.h` が空であることを確認
10. [ ] （可能なら）Windows / Human68k のビルド条件も確認

---

## 8. 想定されるディレクトリ構成（Phase 1 完了後）

```
src/
├── terse_platform.h          # terse-sys（共通土台、直下に残す）
├── terse_posix.c
├── terse_windows.c
├── terse_human68k.c
├── terse_platform_stub.c
├── terse_test.c              # test mode（横断的、直下に残す）
├── terse_test_internal.h
├── term/                     # terse-term（低レベル）
│   ├── terse_detection.{c,h}
│   ├── terse_capabilities.{c,h}
│   ├── terse_style.{c,h}
│   ├── terse_cursor.{c,h}
│   ├── terse_device.{c,h}
│   ├── terse_graphics.{c,h}
│   ├── terse_input.{c,h}
│   ├── terse_keyboard.{c,h}
│   ├── terse_event_helpers.{c,h}
│   ├── terse_codec.{c,h}
│   ├── terse_unicode.{c,h}
│   ├── mini_iconv.{c,h}
│   └── mini_iconv_tables.h
└── core/                     # terse（中レベル）
    ├── terse.c               # 案 A-1: 分割せず core へ
    ├── terse_output.{c,h}
    ├── terse_state.{c,h}
    └── terse_handle.h
```

---

## 9. レビューが必要な判断

| ID | 論点 | 結論 | 根拠 |
|----|------|------|------|
| A | `terse.c` を分割するか | **A-1: 分割せず core へ**（確定 2026-05-29） | §3.1。Phase 1 を「移動のみ」に厳密に閉じる。混在解体は Phase 2 へ |
| B | プラットフォーム層を `src/sys/` に移すか | **移さない**（確定） | §4。共通土台として `src/` 直下に残す |
| C | codec / unicode を term に置くか | **term**（確定 2026-05-29） | §2.1 補足。端末境界の解釈として低レベル扱い |
| D | `terse_test.c` の置き場所 | **直下**（確定） | 横断的関心事のため移動しない |

全論点が確定済み。§7 のチェックリストに従って着手してよい。

> **着手前の残作業**: §7 のチェックリスト 1〜2（tests の依存確認・相対 include の
> 洗い出し）は実地調査が必要。ここで判明した事項に応じて §5.1 / §6.3 を更新する。
