# Phase 5.5 詳細計画: バッファドモードの「任意矩形仮想画面」一般化

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.2, §4.2.3, §4.3
> 前提: Phase 5 完了（commit `75f6311`、バッファドモード + 代替スクリーン導入済み）。
> Phase 6（テスト戦略刷新）・Phase 7（POSIX 拡張ヘッダ）は本フェーズと独立。
> 注: 本フェーズは Phase 8（ハンドル分離、プロポーザル §「Phase 8 以降」で予約）とは無関係。
> Phase 5 の正常進化なので「5.5」とする。

## 0. このフェーズの位置づけと背景

### 0.1 Phase 5 BUFFERED の現状診断

Phase 5 で導入したバッファドモード（`TERSE_RENDER_BUFFERED`）の**本来の目的は
「色付き差分描画でちらつきを抑制する」**こと（Phase 5 計画書 §3.3「terse 向け
再設計の核心」）であり、これ自体は正しい。

しかし実装は、本来の目的に対して**過剰に限定的**な形になっている:

| 想定 | 実態 | 所在 |
|------|------|------|
| 任意の描画領域に色付き差分出力 | **端末全体サイズで alloc** | `terse.c:245` が `size.rows × size.cols` を渡す |
| 任意位置に配置 | **origin=(0,0) 絶対座標固定** | `terse_buffer_flush` (terse_buffer.c:289) が `for row 0..buf_rows` を絶対行0起点で出力。origin 加算なし |
| リサイズ追従 | **未配線** | `terse_buffer_resize` (terse_buffer.c:133) はテストからしか呼ばれない。RESIZE イベント→再 alloc の配線が本番に無い |

結果として、現状 BUFFERED が安全に使えるのは「開いた時の端末サイズのまま・
リサイズしない・画面全体を毎フレーム描き直すフルスクリーン的用途」だけになっている。

### 0.2 想定利用者とのギャップ

Phase 5 計画書は設計借用元として terse-prompt を挙げていたが、terse-prompt は
**フルスクリーンでも alt screen 利用でもない「通常の端末上で普段のプロンプトの
続きに出力される普通の REPL 向けライブラリ」**である。terse-prompt の自前バッファ
（`tprompt_display.c` の `tprompt_screen_buffer_*`）は:

- **プロンプト相対**: `flush_diff` (tprompt_display.c:769) は `base_row = start_row` を
  各行に加算して絶対化。start_row は起動時にカーソル位置から1回確定。
- **可変高**: 入力が伸びるとバッファ行数を動的拡張（`tprompt_display_resize_buffers`、
  内容非保持で作り直し→次フレーム全再描画運用）。
- **col は0起点固定**（origin_col=0、カーソル用に1列予約）。

つまり terse-prompt は実質「origin_row オフセット付き矩形バッファ」を既に持っており、
**現状 terse BUFFERED はそのうち `origin=(0,0)` 版でしかない**。現状 BUFFERED の形では
terse-prompt は使えない。

### 0.3 このフェーズが目指すモデル

ユーザーの理想（2026-05-29 確認）:

> 端末上の特定の矩形上での仮想画面としてバッファを用意する。プロンプト相対は
> その一例でしかない。terse が汎用ライブラリを目指すなら。

バッファを **「端末上の任意矩形 `(origin_row, origin_col, rows, cols)` に対応する
仮想画面」** とする汎用モデルへ一般化する。

- **全画面** = 矩形が端末全体、という特殊ケース
- **プロンプト相対・可変高** = origin がプロンプト行・rows を伸縮、という特殊ケース

これは terse の「画面を支配しない・任意位置に書く低レベルプリミティブ」という
本来の思想とも整合する。現状の全画面・絶対座標決め打ちは、その思想からの逸脱でもあった。

### 0.4 スコープと境界線（重要）

方針は一貫して **「追加のみ・破壊的変更なし」**。`include/terse.h` は追加のみ、
`origin=(0,0)`・端末全体を既定にすることで現状の振る舞い（既存17テスト・Phase 5
デモ）を一切壊さない。

**terse 本体に入れるもの（案A→B）**:
- 矩形 origin によるローカル座標→絶対座標の射影（flush で `origin + local`）
- 矩形指定 API（`terse_buffer_set_region`）、options による初期矩形指定
- 可変高 resize の本番公開・配線
- 縮小時の残骸消去
- RESIZE イベントの opt-in 自動追従（全画面用途の便宜）

**terse 本体に入れないもの（案C＝利用側 = terse-prompt の責務に残す）**:
- 端末末尾でのスクロール確保（`\r\n` を書いて土台をずらす）
- カーソル位置の往復クエリによる origin 自動確定
- レイアウト計算（継続行マーカー、ステータス行、補完スクロール）

理由: スクロール確保・カーソル往復は「terse が端末状態を能動操作する」ことになり、
terse の薄いプリミティブ思想を崩す。これらは利用側（terse-prompt は既に
`tprompt_ensure_vertical_space` / `start_row_known` で持っている）に残すのが正しい境界。

---

## 1. 現状分析（調査済み事実）

### 1.1 BUFFERED の出力経路（3点 + clear）

| 関数 | BUFFERED 分岐 | 所在 |
|------|--------------|------|
| `terse_move_to` | `cursor_row/col` を更新するのみ（直接出力しない） | terse_cursor.c:32 付近 |
| `terse_write_text` | `terse_buffer_write_text(handle, cursor_row, cursor_col, ...)` | terse_output.c:281 |
| `terse_set_style` | `handle->style` 更新のみ | terse_output.c:151 |
| `terse_clear_screen` | `terse_buffer_clear` | terse_output.c:46 |
| `terse_flush` | `in_flush=1` → `terse_buffer_flush` → `in_flush=0` | terse_output.c:348 |

分岐条件はすべて `render_mode == TERSE_RENDER_BUFFERED && !in_flush`。
flush 中は `in_flush=1` で内部の move_to/write_text/set_style を即時経路へ通す。

### 1.2 現状の暗黙の座標系

現状は「端末絶対座標 = バッファ index」。これは origin=(0,0) だから偶然一致している
だけ。write_text は move_to で立てた `cursor_row/col` を境界チェック
（`>= buf_rows/buf_cols` でクリップ、buffer_set_cell terse_buffer.c:156）でセルに書く。
flush は `terse_move_to(handle, row, start)` で row をそのまま絶対行として出力。

### 1.3 ハンドルのバッファ関連フィールド（terse_handle.h:59-65）

```c
terse_render_mode_t render_mode;
terse_cell_t *cur_cells;   /* current frame (rows*cols), row-major */
terse_cell_t *prev_cells;  /* previous frame, for diff */
unsigned char *dirty;      /* per-cell dirty flags (rows*cols) */
int buf_rows;              /* allocated buffer dimensions */
int buf_cols;
int in_flush;
```

origin フィールドは無い。`cursor_row/cursor_col` (terse_handle.h:37-38) は既存。

### 1.4 互換の決定的制約（既存テスト）

`tests/unit/terse_buffer_test.c` の `FlushEmitsMoveAndText` (L295) が
`move_to(1,2)` → `\x1b[2;3H`（1-based の (2,3)）を要求。これは
**origin=(0,0) で「バッファ座標=端末絶対座標」** が成立する前提。
**origin 既定を (0,0) に保てばこのテストは無改修で通る。**

また既存17テストは内部ヘッダ直叩きで `terse_buffer_alloc(handle, rows, cols)` を呼び、
`handle->buf_rows/buf_cols`・`cell_at` を直接見る。**alloc のシグネチャを変えなければ
全テスト不変。**

### 1.5 リサイズの現状（terse_input.c:548 付近）

CSI resize 報告で `handle->size` を更新するが、`terse_buffer_resize` は呼ばれない。
`terse_get_size` はキャッシュ（terse.c:577）。RESIZE→バッファ追従の配線は存在しない。

---

## 2. 設計判断（要 AskUserQuestion 確認）

実装着手前に以下を確定する。現時点の推奨を併記。

| # | 論点 | 確定/推奨 | 備考 |
|---|------|----------|------|
| D1 | move_to/write_text の座標系 | **確定: BUFFERED ではローカル矩形座標(0起点)、flush で origin 加算**。origin 既定(0,0)で絶対と数値一致 | 代替（絶対座標据え置き）は一般化不可で却下 |
| D2 | origin 設定 API | **確定: `terse_buffer_set_region(handle, origin_row, origin_col, rows, cols)` を1本追加**。options にも初期矩形フィールド（全0=端末全体） | alloc シグネチャは不変（既存テスト維持） |
| D3 | 可変高 resize の内容保持 | **確定: 内容非保持（既存 resize 流用、拡張後は全再描画運用）** | 移植元 terse-prompt も非保持運用 |
| D4 | リサイズ追従の配線 | **確定: 利用側ポーリング方式**。RESIZE イベントは既存の `handle->size` 更新（terse_input.c:548）をそのまま使い、利用側が描画時に `terse_get_size()`→必要なら `terse_buffer_set_region()` で追従。terse 本体に新配線不要。opt-in 自動追従は入れない | terse-prompt の現状方式（calculate_layout で毎回 get_size）と一致。全画面用途も利用側が set_region すればよく、本体自動 resize は不要 |
| D5 | 残骸消去（origin 移動・縮小） | **確定（論点3）: flush で「前回矩形 − 今回矩形」を terse 自身が空白消去**。origin 移動・行/列縮小を同一ロジックに統合（§3.6） | 境界 = terse が自分で出したセルのみ。矩形外の他者描画は対象外 |
| D6 | スコープ（A/B/C） | **確定: A→B を terse に、C（スクロール・カーソル往復）は利用側に残す** | C まで取り込むと低レベル思想逸脱 |
| D7 | 移植後テストの検証手段 | **確定（論点1）: `terse_get_cell()` を公開追加**（5.5-B）。「画面に表示中の確定内容」(prev_cells)を返す一本（§3.3.1） | terse 自身の flush 前 cur 検証は内部ヘッダで賄う |

---

## 3. 設計詳細（案A→B 前提）

### 3.1 座標系の正規化（案A コア、D1）

バッファド時のバッファ内座標を **常にローカル(0..rows-1, 0..cols-1)** と定義し、
端末への射影は flush 時に **`absolute = origin + local`** で行う（単一規則）。

- `terse_buffer_flush` (terse_buffer.c:321) の
  `terse_move_to(handle, row, start)` を
  `terse_move_to(handle, buf_origin_row + row, buf_origin_col + start)` に変更。
- write_text / move_to の BUFFERED 分岐は `cursor_row/col` をローカルとして扱う
  （現状コードのまま。origin=0 なら従来どおり）。
- これは terse-prompt の `base_row + row` と同じ意味論（移植元一致）。
- **互換**: origin 既定(0,0)なら `local == absolute`。`FlushEmitsMoveAndText` 不変。

### 3.2 origin フィールドとサイズ（terse_handle.h、D2）

handle に追加（内部構造体・公開 ABI 外・破壊なし）:

```c
int buf_origin_row;   /* 矩形の端末上の開始行（既定 0）*/
int buf_origin_col;   /* 矩形の端末上の開始列（既定 0）*/
```

**前回矩形の記憶（残骸消去に必須、D5/論点3）**:

origin 移動・サイズ縮小いずれの場合も「前回 terse が端末に出した矩形」を
flush 冒頭で参照して残骸を消すため、前回矩形を覚えておく:

```c
int prev_origin_row;  /* 前フレーム flush 時の矩形 origin（残骸消去用）*/
int prev_origin_col;
int prev_buf_rows;    /* 前フレーム flush 時の矩形サイズ */
int prev_buf_cols;
int prev_valid;       /* 1 = 前回 flush 済みで上記が有効 */
```

flush 末尾でこれらを今回の origin/サイズに更新する。これにより §3.6 の
「前回矩形 − 今回矩形」差分消去が origin 移動・サイズ縮小の両方を1つの
ロジックで扱える。

### 3.3 矩形指定 API（案B、D2）

公開ヘッダ `include/terse.h` に**宣言追加のみ**:

```c
/* バッファドモードの仮想画面を端末上の矩形 (origin_row, origin_col) を
 * 左上とする rows×cols 領域に設定する。バッファドモード時のみ有効。
 * 既存バッファとサイズが異なれば再確保（内容非保持→次 flush で全再描画）。
 * origin だけの変更なら再確保せず flush の射影にのみ影響。 */
terse_error_t terse_buffer_set_region(terse_handle_t handle,
                                      int origin_row, int origin_col,
                                      int rows, int cols);
```

`terse_options_t` 末尾に初期矩形フィールド追加（**全0 = 端末全体・origin0 = 現行**）:

```c
int buffer_origin_row;   /* 既定 0 */
int buffer_origin_col;   /* 既定 0 */
int buffer_rows;         /* 既定 0 = 端末高 */
int buffer_cols;         /* 既定 0 = 端末幅 */
```

`memset(handle, 0, ...)` 済みなので全0が自然に現行動作になる（Phase 5 と同じ流儀）。

### 3.3.1 セル読み出し API（D7/論点1、5.5-B で追加）

公開ヘッダ `include/terse.h` に**宣言追加のみ**:

```c
/* バッファドモードで「現在端末に表示されている」セル内容を読み出す。
 * row/col は矩形ローカル座標（0-origin）。範囲外は TERSE_ERR_* を返す。
 * 返すのは flush で確定した内容（内部の prev_cells = 直前に出力したフレーム）。
 * バッファドモードでない / 未 flush なら TERSE_ERR_* を返す。 */
terse_error_t terse_get_cell(terse_handle_t handle, int row, int col,
                             terse_cell_t *out);
```

**契約（論点1 確定）**: `terse_get_cell()` は **「今この瞬間に画面に表示されている
確定内容」を返す一本**に絞る。flush は内部で cur↔prev を swap するため、flush 後は
`prev_cells` が「直前に画面へ出した内容」を保持する（terse-prompt の `previous_buffer`
と同じ意味論）。よって本 API は `prev_cells` を読む。

- 利用者: terse-prompt のテスト（移植後、現状 `previous_buffer.cells` 直読みの置換）、
  および利用アプリが「画面に何が出ているか」を問い合わせる用途。
- terse 自身のテストで「flush 前に cur へ積んだ内容」を見たい場合は、内部ヘッダ
  （`handle->cur_cells`）直叩きで賄う（terse 自身のテストは内部ヘッダを使える）。
  公開 API を1意味（=表示中の確定内容）に保つための線引き。
- `terse_cell_t` は Phase 5 で既に公開済みなので、追加は宣言 +実装（+30行程度）のみ。

### 3.4 可変高 resize の公開・配線（案B、D3）

- 既存 `terse_buffer_resize`（内容非保持・同寸法なら clear・異寸法なら alloc 作り直し）
  の意味論をそのまま活用。`terse_buffer_set_region` がサイズ変更を含む場合これを呼ぶ。
- `terse_buffer_grow(handle, min_rows)` は `set_region`/`resize` の薄いラッパ（任意）。

### 3.5 リサイズ追従（D4 確定: 利用側ポーリング方式）

**確定方針**: terse 本体はリサイズでバッファを追従させない。利用側が描画時に
`terse_get_size()` で最新サイズを取り、必要なら `terse_buffer_set_region()` で
矩形を追従させる。terse 本体に新しい配線は不要。

根拠（terse-prompt の現状方式に一致）:
- terse の RESIZE イベント処理（terse_input.c:545-550）は CSI 8;rows;cols t 受信時に
  `handle->size` を更新済み（`size.known=1`）。`terse_get_size()` はこれを返す
  （キャッシュ、terse.c:577）。**この既存配線をそのまま使う。変更不要。**
- terse-prompt は RESIZE をイベント駆動で処理しておらず、`tprompt_display_calculate_layout`
  （display.c:368）が**毎レンダリングで `terse_get_size()` をポーリング**して
  `terminal_width/height` を更新する方式。本確定方針はこれと完全に一致するので、
  移植（5.5-C）で terse-prompt は「描画時に get_size→必要なら set_region」を呼ぶだけ。
- **opt-in 自動追従（旧 `buffer_track_resize`）は入れない**。terse-prompt は矩形の
  origin/rows を入力内容に応じて自分で決めるため、本体が勝手に端末全体へ resize すると
  むしろ困る。全画面 TUI 用途も利用側が `set_region(0,0,size.rows,size.cols)` を呼べば
  足り、本体に自動追従ロジックを持たせる必要がない（terse の薄さを保つ）。

> これにより options の `buffer_track_resize` フィールドは不要（§3.3 から削除）。

### 3.6 残骸消去 = 「前回矩形 − 今回矩形」の自己掃除（案B、D5/論点3 確定）

**方針（論点3 確定）**: terse が**自分で端末に出した領域**は terse が責任を持って掃除する。
origin 移動・サイズ縮小いずれも「前回 terse が出した矩形のうち、今回の矩形に含まれない
端末セル」を空白で消す。これは flush で一元的に行う。

**仕組み**: §3.2 の `prev_origin_*`/`prev_buf_*`/`prev_valid` で前回 flush 時の
端末上の絶対矩形を覚えておき、今回 flush の冒頭で:

1. `prev_valid` なら、前回絶対矩形 `(prev_origin, prev_rows, prev_cols)` と
   今回絶対矩形 `(origin, rows, cols)` の**差集合**（前回にあって今回に無い端末セル）を算出。
2. その差集合セルへ `terse_move_to`（絶対座標）＋空白を即時出力して消す。
3. 続けて通常の diff 出力（今回矩形内）。
4. flush 末尾で `prev_origin_*`/`prev_buf_*` を今回値に更新、`prev_valid=1`。

これにより **origin 移動・行縮小・列縮小がすべて同一ロジック**に統合される
（個別の resize 経路消去は不要）。flush が「前回自分が出した範囲」を完全に把握して
いるので、resize/set_region 側に消去責務を持たせる必要がない。

**境界（重要・§0.4 の薄さと整合）**:
- ✅ terse が**自分で出したセル**（prev_cells に記録あり）の残骸 → terse が消す。
- ❌ terse の矩形の**外側**を他者（シェルのプロンプト、別の出力）が描いた内容 →
  terse は関与しない（prev に記録がなく、消すべきでもない）。
- これは「自分が出したものは自分で消す」管理責任の範囲内であり、スクロール等の
  能動的端末操作（案C・利用側責務）とは別物。

> 注: §3.4 の `terse_buffer_resize` 自体は従来どおり「内容非保持で再確保」のまま。
> 残骸消去は resize の中ではなく flush で行うので、resize の意味論は変えない。

### 3.7 配置（ファイル）

| 変更 | ファイル |
|------|----------|
| flush の origin 加算、flush 統合の残骸消去、resize/set_region 実装、get_cell | `src/core/terse_buffer.c` |
| origin/prev_* フィールド追加 | `src/core/terse_handle.h` |
| `terse_buffer_set_region`/`terse_get_cell` 宣言、options 矩形フィールド | `include/terse.h` |
| open 時の origin/サイズ初期化、RESIZE opt-in 配線（5.5-B） | `src/core/terse.c`（finish_render, input 連携地点） |
| move_to の BUFFERED 分岐をローカル座標として明確化 | `src/term/terse_cursor.c` |

---

## 4. 実装手順（案A→B）

### Phase 5.5-A（座標系一般化・破壊極小）

1. handle に `buf_origin_row/col` 追加（terse_handle.h）。`terse_open_finish_render` で
   origin=(0,0) に初期化（既定）。
2. `terse_buffer_flush` の move_to を `origin + local` に変更（terse_buffer.c）。
3. `terse_buffer_set_region`（**origin のみ変更版**、サイズ変更は 5.5-B）を追加・宣言
   （include/terse.h）。
4. クリーンビルド + 既存17テスト無改修通過確認（origin=0 で従来出力一致）。
5. origin≠0 の新規テスト追加（矩形を端末中央に置き、flush 出力が `origin+local` で
   絶対化されることを検証）。
6. コミット（feat: バッファドモードを任意 origin の矩形に一般化）。

> 注: 5.5-A の `set_region` は origin 変更のみ。origin を動かした後の残骸消去
> （§3.6）は前回矩形の記憶（`prev_*` フィールド）が前提なので、§3.6 の flush 統合
> 消去ロジックと `prev_*` フィールドは 5.5-A の手順2〜3と同時に入れる（origin を
> 動かせば即座に残骸消去が要るため、A と B に分割できない）。よって `prev_origin_*`/
> `prev_buf_*`/`prev_valid` の追加と flush 冒頭の差分消去は **5.5-A に含める**。
> サイズ変更を伴う resize 配線・opt-in 追従・options 矩形のみ 5.5-B。

### Phase 5.5-B（矩形API一式・リサイズ・get_cell）

7. options に矩形フィールド追加（include/terse.h）。
   finish_render で初期矩形を反映（全0=端末全体）。
8. `terse_buffer_set_region` をサイズ変更対応に拡張（resize 呼び出し）。残骸消去は
   5.5-A で入れた flush 統合ロジックがサイズ縮小もそのまま処理する。
9. `terse_get_cell()` 公開追加（§3.3.1、D7/論点1）。prev_cells を読む実装。
10. テスト: 可変高 resize、サイズ縮小消去、options 矩形指定、get_cell。
    リサイズ追従は利用側ポーリング方式（D4 確定）なので本体テストは
    「get_size 更新後に set_region で追従できる」ことの確認のみ。
11. コミット（feat: 矩形バッファのリサイズ・get_cell・options 指定を追加）。

各ステップ後にビルド確認。clang-format フックが走るので整形後に再ビルド。

### Phase 5.5-C（terse-prompt 移植・別リポジトリ）

13. terse-prompt の GIT_TAG を 5.5 完了 commit に更新。
14. `tprompt_screen_buffer_*` 一式を撤去し、terse の矩形バッファ API
    （set_region で start_row を origin に、可変高 resize）に差し替え。
15. レイアウト計算（継続行・ステータス・補完）とスクロール確保
    （`ensure_vertical_space`）は terse-prompt 側に残す（案C 境界）。
16. テスト（test_status/display/integration の `previous_buffer` 直読み）を
    5.5-B で追加した `terse_get_cell()` ベースに書き換え（D7/論点1 確定済み）。

---

## 5. テスト戦略

- **案A**: origin=0 で既存17テスト無改修通過（回帰）。origin≠0 の射影テスト追加。
- **案B**: 可変高 resize（内容非保持・全ダーティ）、縮小消去（旧領域空白化）、
  opt-in 追従、options 矩形指定。Phase 6 のゴールデン/PTY 基盤を活用可。
- 出力検証は TERSE_ENABLE_TEST_MODE の API 記録 + Phase 6 ゴールデン（実バイト列）。

---

## 6. 未決事項（実装前に詰める）

**解決済み**（2026-05-29 議論で確定）:
- ~~terse-prompt のテスト検証手段~~ → D7/論点1 で `terse_get_cell()` 公開追加に確定（§3.3.1）。
- ~~origin 移動・縮小の残骸消去~~ → 論点3 で flush 統合の自己掃除に確定（§3.6）。
- ~~矩形を実画面より小さくした時のはみ出し書き込み~~ → 論点2 で現状ルール（矩形外は
  黙って捨てる = 画面が小さくなっただけ）維持に確定。

**残る未決**:
1. **options 矩形フィールドの命名**: `buffer_*` prefix で統一するか（実装時に確定）。
2. **col 縮小の残骸消去**: §3.6 の flush 統合ロジックは行・列の差集合を同時に扱える
   ので原理的には col も同時対応。実装の優先度として行を先に検証、col は同ロジックで
   カバー（後回しの分割は不要になった）。

---

## 7. 完了条件

### Phase 5.5-A
- [ ] クリーンビルド成功（Debug, Ninja）
- [ ] 既存17バッファテスト + 全体テスト無改修通過（origin=0 振る舞い不変）
- [ ] origin≠0 射影テスト追加・通過
- [ ] `include/terse.h` 差分: 追加のみ

### Phase 5.5-B
- [ ] 可変高 resize / 縮小消去 / opt-in 追従 / options 矩形のテスト通過
- [ ] `include/terse.h` 差分: 追加のみ（options 末尾追加）
- [ ] 即時モード・全画面 BUFFERED の既存振る舞い不変
- [ ] `docs/progress-overview.md` に矩形バッファ一般化を反映
- [ ] Conventional Commits でコミット（clang-format 整形後に再ビルド確認）

### Phase 5.5-C（別リポジトリ terse-prompt）
- [ ] GIT_TAG 更新、`tprompt_screen_buffer_*` 撤去、terse 矩形 API へ移植
- [ ] terse-prompt 全12テスト通過（検証手段は §6.1 で確定した方式）
- [ ] 実 TTY で REPL 描画（複数行・補完・ステータス・リサイズ）目視確認
