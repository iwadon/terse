# Phase 5 詳細計画: バッファドモードの導入

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.2, §3 Phase 5, §4.2.3, §4.2.4, §4.3.1
> 前提: Phase 1〜4.5 完了（commit `947ee9c` まで、全テスト 236 件通過、クリーンビルド確認済み）

## 0. このフェーズの位置づけ

セルベース仮想バッファ（ダブルバッファ + diff + ダーティセル）による
**バッファドモード**を opt-in で追加する。即時モードは現状維持。再設計で初めて
`terse_options_t` を拡張するフェーズであり、規模が大きい。

方針は一貫して **「追加のみ・破壊的変更なし」**:
既存利用側コード（即時モード）は無変更でビルド・動作すること。
バッファドモードはすべて opt-in（`render_mode` を明示しない限り即時モード）。

### スコープ（プロポーザル §3 Phase 5）
1. セルベース仮想バッファの実装（ダブルバッファ + diff + ダーティセル配列）
   - terse-prompt の `tprompt_display.c` を **設計借用で再実装**（コピーでなく再設計。§4.3.1）
2. `terse_open()` の opts に `render_mode`（即時 / バッファド）を追加。即時モードは現状維持
3. セル構造体: 文字 + `terse_color_t`(fg/bg) + effects（§4.2.3）
4. `terse_options_t` に `use_alt_screen` フラグ、低レベル API
   `terse_enter_alt_screen()` / `terse_leave_alt_screen()` を追加（§4.2.4）
5. terse-prompt 側で最小限のプロトタイプ適用して API 検証（本格移行は Phase 5.5）

### スコープ外
- terse-prompt の本格リファクタリング（Phase 5.5、別リポジトリの作業）
- スクロール領域の抽象化（プロポーザル §2.2 末尾、必要が出てから検討）
- ウィンドウ透明度 API（§4.2.3、将来検討）
- `terse_color_kind_t` の PALETTE 統一（後述 §3.1 で先送り判断）

---

## 1. 現状分析（調査結果）

### 1.1 即時出力モデルの現状

| 関数 | 現状の振る舞い | 所在 |
|------|---------------|------|
| `terse_write_text` | codec 変換して fd へ直接 write、`cursor_known=0` にする | `src/core/terse_output.c:237` |
| `terse_set_style` | SGR を fd へ直接 write | `src/core/terse_output.c:123` |
| `terse_move_to` | カーソル移動エスケープを直接 write | `src/term/terse_cursor.c:20` |
| `terse_flush` | **現状 no-op**（`clear_error` して 0 を返すだけ） | `src/core/terse_output.c:321` |

→ バッファドモードでは write 系を「セルバッファへの書き込み」に切り替え、
`terse_flush` に diff 計算と差分出力を載せる。即時モードは現状の直接 write を維持。

### 1.2 既存の型（変更しないもの）

```c
/* include/terse.h */
typedef struct terse_color {           /* L109: kind は 4 種（DEFAULT/BASIC16/PALETTE256/TRUECOLOR）*/
	terse_color_kind_t kind;
	union { ... } data;
} terse_color_t;

typedef struct terse_style {           /* L127 */
	terse_color_t foreground;
	terse_color_t background;
	unsigned int effects;              /* TERSE_STYLE_*（L274-280、7 ビット）*/
} terse_style_t;

enum { TERSE_STYLE_BOLD = 1u<<0, ... TERSE_STYLE_STRIKE = 1u<<6 };
```

### 1.3 terse_options_t の現状（末尾追加で拡張）

```c
typedef struct terse_options {         /* include/terse.h:243 */
	int input_fd;
	int output_fd;
	const char *codec_name;
	unsigned int disabled_caps;
	unsigned int enabled_caps;
	int east_asian_ambiguous_as_wide;
} terse_options_t;
```

`terse_open()`（`src/core/terse.c:230`）は `options` を `handle->options` にコピーし、
未指定時はプラットフォーム既定にフォールバック。`memset(handle, 0, ...)` 済みなので
**新フィールドが 0 のとき = 即時モード / alt screen 無効** という既定が自然に成立する。

### 1.4 terse-prompt 参考実装（設計借用元）

`terse-prompt/src/tprompt_display.c`（約1500行）、`tprompt_internal.h`:

| 借用する要素（ロジック） | 関数 |
|--------------------------|------|
| セル書き込み（wide char の continuation 処理含む） | `tprompt_screen_buffer_write_char` (L628) |
| バッファ確保 / 解放 / クリア / リサイズ | `tprompt_screen_buffer_init/free/clear/resize` (L533-) |
| diff（次元不一致なら全ダーティ、それ以外はセル比較） | `tprompt_screen_buffer_diff` (L716) |
| ダブルバッファ swap | `tprompt_screen_buffer_swap` (L748) |
| 行内連続ダーティ領域での `move_to`→`write_text` 出力 | `tprompt_screen_buffer_flush_diff` (L769) |

terse 向けに**再設計**する要素（§4.3.1）:
- セル構造体: reserved×4 → `terse_color_t fg/bg` + `uint16_t effects`
- 端末ハンドル依存を `terse_handle_t` に変更（terse-prompt 固有 handle を排除）
- **diff/flush に SGR 出力を追加**（terse-prompt は色未実装だったため、ここが本質的な新規実装）

> 注: terse-prompt のセルの `is_continuation` / `display_width` ベースの wide char 処理、
> 連続ダーティ領域のまとめ出力ロジックは細部までそのまま参考にできる。

---

## 2. 確定した設計判断（AskUserQuestion 2026-05-29）

| 論点 | 確定 | 根拠 |
|------|------|------|
| **セル色 kind** | 既存 4 kind をそのまま流用 | §3.1。PALETTE 統一は破壊的変更を伴うため Phase 5 では行わず、独立フェーズに先送り。Phase 5 を純粋な追加に保つ |
| **セル SGR 出力** | fg/bg/effects を完全出力 | §3.4。terse-prompt は色未実装だったので、これが terse 向け再設計の核心。バッファドモードの主目的（色付き TUI のちらつき抑制）を達成 |
| **書き込み API 経路** | 既存 API をモードで分岐 | §3.2。`terse_write_text`/`terse_set_style`/`terse_move_to` がモードを見て分岐。利用側は `render_mode` を変えるだけで同一 API。§2.2「オプション切り替えで吸収」方針 |
| **alt screen** | 低レベル API + opts フラグ両方 | §3.5。§4.2.4 の確定事項を完全実装。`has_alt_screen` ケイパビリティも追加 |

---

## 3. 設計詳細

### 3.1 セル構造体（§4.2.3、確定）

公開ヘッダ `include/terse.h` に追加する（バッファ API を公開するため公開型）:

```c
typedef struct terse_cell {
	/* 文字情報 */
	char     utf8_char[5];    /* UTF-8 1 文字（最大 4 バイト + NUL）*/
	uint8_t  char_len;        /* バイト長（0 = 空セル）*/
	uint8_t  display_width;   /* 表示幅（1 or 2）*/
	uint8_t  is_continuation; /* wide char の 2 桁目なら 1 */

	/* 色（既存 terse_color_t をそのまま使用）*/
	terse_color_t fg;         /* kind=DEFAULT で未指定（背景透過）*/
	terse_color_t bg;

	/* 装飾 */
	uint16_t effects;         /* TERSE_STYLE_* のビットマスク */
} terse_cell_t;
```

- `terse_color_kind_t` は**変更しない**（DEFAULT/BASIC16/PALETTE256/TRUECOLOR の 4 種のまま）。
  PALETTE 統一（§4.2.3 の理想形）は破壊的変更を伴うため、移行戦略付きの独立フェーズへ先送り。
- effects は `terse_style_t.effects`（`unsigned int`）の下位を `uint16_t` に格納。
  現状 7 ビットしか使わないので欠落なし。
- `is_continuation` は terse-prompt が `bool`（`<stdbool.h>`）だったが、公開ヘッダの C89 互換性を保つため
  `uint8_t` とする（既存 capabilities の `int` フラグと整合）。

> セルサイズは `terse_color_t` の実サイズ（タグ + ユニオン、概ね 4 バイト）に依存。
> セル ≒ 8 + 4×2 + 2 = 18 バイト前後 + アライメント。

### 3.2 render_mode と書き込み API のモード分岐（§3.2 確定）

```c
typedef enum terse_render_mode {
	TERSE_RENDER_IMMEDIATE = 0,   /* 既定: 即時モード（現状維持）*/
	TERSE_RENDER_BUFFERED          /* バッファドモード */
} terse_render_mode_t;
```

`terse_options_t` 末尾に追加（ABI: 末尾追加、`memset` 0 → IMMEDIATE が既定）:

```c
typedef struct terse_options {
	int input_fd;
	int output_fd;
	const char *codec_name;
	unsigned int disabled_caps;
	unsigned int enabled_caps;
	int east_asian_ambiguous_as_wide;
	terse_render_mode_t render_mode;  /* 追加: 既定 IMMEDIATE */
	int use_alt_screen;               /* 追加: 既定 0（§3.5）*/
} terse_options_t;
```

書き込み系の分岐方針（既存 API のシグネチャは不変）:

| 関数 | 即時モード（現状） | バッファドモード（新規） |
|------|-------------------|------------------------|
| `terse_write_text` | codec 変換して直接 write | カレントカーソル位置からセルバッファへ書き込み（codec 変換せず UTF-8 のままセルに格納）、幅分カーソル前進 |
| `terse_set_style` | SGR を直接 write | `handle->style` を更新するのみ（以降の write_text がこの色でセルに書く）。直接出力しない |
| `terse_move_to` | 移動エスケープを直接 write | `handle->cursor_row/col` を更新するのみ。直接出力しない |
| `terse_clear_screen` | 直接 write | セルバッファ全クリア（空セル化） |
| `terse_flush` | no-op | **diff 計算 → 差分セルを SGR 付きで出力 → swap**（§3.3）|

> 分岐は各関数冒頭で `handle->options.render_mode == TERSE_RENDER_BUFFERED` を判定し、
> バッファド経路へ早期分岐する形にする（即時経路は一切触らず振る舞い不変を担保）。

### 3.3 flush の diff + SGR 出力（terse 向け再設計の核心、§3.4 確定）

terse-prompt の `flush_diff`（行内連続ダーティ領域のまとめ出力）を借用しつつ、
**セルの色/effects 差分に応じた SGR 出力を追加**する。

擬似コード:

```
terse_flush(handle):              # バッファドモード時
    diff(previous, current) -> dirty[]          # 借用: 次元不一致なら全ダーティ
    last_emitted_style = UNKNOWN
    for each row:
        for each 連続ダーティ領域 [start, end):  # 借用: 連続領域のまとめ
            move_to(row, start)                  # 即時経路で実 write
            for c in [start, end):
                cell = current[row][c]
                if cell.is_continuation: continue
                style = {cell.fg, cell.bg, cell.effects}
                if style != last_emitted_style:  # 新規: セル単位 SGR
                    emit_sgr(style)              # terse_set_style 相当の実 write
                    last_emitted_style = style
                emit text(cell.utf8_char or " ") # 即時経路で実 write（codec 変換）
            # 領域末尾で実カーソル位置を同期（借用）
    emit_reset_style()                           # 行末/フラッシュ末で SGR リセット
    swap(previous, current)                      # 借用
    clear dirty[]
```

- SGR 出力は既存の `terse_set_style` の内部実装（degrade 込み）を再利用する。
  バッファドモードの分岐に入る前の「実出力ヘルパ」を切り出して flush から直接叩く設計にする。
- `last_emitted_style` の追跡で、同一スタイルが続く区間は SGR を重複出力しない最適化。
- diff は terse-prompt と同様、文字（`char_len`/`utf8_char`/`display_width`/`is_continuation`）に
  **加えて fg/bg/effects も比較対象に含める**（色だけ変わったセルもダーティ扱い）。

### 3.4 バッファ管理とリサイズ

- ダブルバッファ（`current` / `previous`）+ `dirty` 配列を `struct terse_handle` に追加。
  即時モードでは未確保（NULL）。バッファドモードの `terse_open` 時に端末サイズで確保。
- リサイズ（`terse_get_size` の変化検出または resize イベント）時は両バッファを再確保し全ダーティ化
  （terse-prompt の `screen_buffer_resize` を借用）。
- `terse_close` でバッファ解放。

> 既存 handle は `memset(handle, 0, ...)`（terse.c:243）されるので、新ポインタ群は NULL 初期化される。
> 即時モードでは確保処理に入らないため、メモリ増は完全に opt-in。

### 3.5 alt screen（§4.2.4、§3.5 確定）

低レベル API（公開）:

```c
terse_error_t terse_enter_alt_screen(terse_handle_t handle);  /* ^[[?1049h */
terse_error_t terse_leave_alt_screen(terse_handle_t handle);  /* ^[[?1049l */
```

- ケイパビリティ: `terse_capabilities_t` に `int has_alt_screen` を追加（DEC Private Mode 1049 可否）。
  検出は P2 以上の VT 端末で概ね真、Human68k 等は偽。
  → **既存 struct 末尾でなく論理位置に挿入すると ABI 変動**するため、**末尾に追加**する（再コンパイル要だが既存フィールドのオフセット不変）。
- モード化: `terse_options_t.use_alt_screen` が真なら `terse_open` で自動 enter、`terse_close` で自動 leave。
- フォールバック: `1049` が基本。`1047`/`47` 系フォールバックは §4.2.4 で「検討」とされており、
  Phase 5 では `1049` のみ実装（フォールバックは将来必要時）。`has_alt_screen` が偽なら no-op。
- alt screen はバッファドモードと**独立**して使える（即時モードでも有効）。

### 3.6 配置（ファイル構成）

- セルバッファ実装: 新規 `src/core/terse_buffer.c`（+ 内部ヘッダ `src/core/terse_buffer.h`）。
  core 層に置く（diff/flush は中レベルの描画ロジック）。
- alt screen: 低レベル制御なので `src/term/terse_device.c` か `terse_output.c` 系に追加（既存配置に合わせて実装時確定）。
- handle 拡張: `src/core/terse_handle.h` にバッファポインタ群を追加。
- 公開 API 宣言: `terse_enter/leave_alt_screen` は低レベルなので `include/terse/term.h`、
  バッファ/render_mode 型は中レベルなので `include/terse.h`（実装時に分類確定）。

---

## 4. 実装手順

1. **公開型の追加**（`include/terse.h`）: `terse_render_mode_t`、`terse_cell_t`、
   `terse_options_t` に `render_mode`/`use_alt_screen`、`terse_capabilities_t` に `has_alt_screen`。
2. **handle 拡張**（`terse_handle.h`）: `current/previous` バッファ、`dirty` 配列、関連サイズ。
3. **セルバッファ実装**（`terse_buffer.c`）: init/free/clear/resize/write_char/diff/swap を
   terse-prompt から設計借用で移植（handle 依存に変更、セル構造体を terse 版に）。
4. **flush の diff + SGR 出力**: §3.3 の核心ロジック。`terse_set_style` の実出力部を切り出して再利用。
5. **write 系のモード分岐**: `terse_write_text`/`terse_set_style`/`terse_move_to`/`terse_clear_screen`。
6. **alt screen**: 低レベル API + `terse_open`/`terse_close` のモード化配線 + `has_alt_screen` 検出。
7. **terse_open/close**: バッファドモード時のバッファ確保/解放、alt screen 自動進入/退出。
8. **テスト追加**（`tests/unit/`）: §5。
9. **terse-prompt プロトタイプ適用**: 最小限の利用で API 検証（§6）。

各ステップ後にビルド確認。clang-format フックが走るので整形後に再ビルド。

---

## 5. テスト戦略

新規 `tests/unit/terse_buffer_test.c`（`tests/CMakeLists.txt` に登録）:

- **セルバッファ単体**: write_char（wide char の continuation 含む）、resize、clear、swap の純粋ロジック。
- **diff**: 同次元での文字/色/effects 差分検出、次元不一致での全ダーティ。
- **flush 出力**: TERSE_ENABLE_TEST_MODE の API 記録で、バッファドモードの `terse_flush` が
  期待する `move_to`/`set_style`/`write_text` 列を出すことを検証
  （即時モードの記録と比較し、同じ最終画面が異なる呼び出し列で達成されることを確認）。
- **モード等価性**: 同一の write 列を即時/バッファド両モードで実行し、最終画面状態が一致すること。
- **alt screen**: enter/leave のエスケープ出力、`use_alt_screen` opts での自動進入/退出。
- **回帰**: 既存テスト 236 件が無変更で通過（即時モードの振る舞い不変）。

> 検出側の `has_alt_screen` はホスト環境依存になるため、`tests/test_env.h` の
> 環境変数サニタイズ方針に従う（新 getenv 追加時は `TERSE_TEST_DETECTION_ENV_NAMES` に追記）。

---

## 6. terse-prompt プロトタイプ適用（API 検証、§4.3.2）

Phase 5 のスコープは **最小限のプロトタイプ適用**に留める（本格移行は Phase 5.5）。

- terse-prompt 側の一部描画パス（例: ステータスライン 1 行）を terse バッファ API 経由に置き換え、
  `render_mode = TERSE_RENDER_BUFFERED` で動作・ちらつき抑制を確認。
- 既存の `tprompt_display.c` 全体の削除は行わない（Phase 5.5）。
- API の使い勝手で問題が出たら Phase 5 内で API を調整（実利用者なしで FIX する危険を回避）。
- これは別リポジトリ `~/src/terse-prompt` の作業。terse 本体のコミットとは分離する。

---

## 7. 完了条件

- [ ] クリーンビルド成功（Debug、Ninja）
- [ ] `ctest --test-dir build --output-on-failure` 全通過（既存 236 件 + 新規バッファテスト）
- [ ] `include/terse.h` 差分確認: 追加のみ（既存型のフィールド削除/並べ替えなし）
- [ ] 即時モードの既存サンプル/テストが無変更で動作（振る舞い不変）
- [ ] `cc -fsyntax-only -D__human68k__` 相当のクロス構文チェック（alt screen 検出分岐）
- [ ] `docs/progress-overview.md` にバッファドモード/alt screen の実装状況を反映
- [ ] terse-prompt プロトタイプで API 検証（別リポジトリ、コミット分離）
- [ ] Conventional Commits でコミット（clang-format 整形後に再ビルド確認）
