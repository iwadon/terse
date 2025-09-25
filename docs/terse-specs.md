# Terse: 端末制御ライブラリ仕様書（ドラフト）

## 用語集

- **セル（Cell）**: 端末画面の最小単位。幅は1または2。
- **グラフェム（Grapheme）**: 出力対象の文字単位。結合文字を含む場合もあり。
- **プロファイル（Profile）**: 端末が提供する機能水準を段階的に区分したもの。P0が最小核。
- **能力（Capability）**: 実際に利用可能な個別の端末機能。
- **縮退（Degrade）**: 要求機能が未対応の場合に、安全な代替または無効果に置き換える動作。

---

## プロファイル定義

### P0（最小核）

- 文字出力（符号化変換を伴う）
- カーソル移動（絶対/相対）
- 行・画面消去（部分/全体）
- カーソル表示切替
- 端末サイズ取得（未知可）
- 基本入力（文字/Enter/Backspace/Tab/矢印キー/Resize）

### P1（色・装飾）

- SGRによる色指定（16色/256色/TrueColorに縮退）
- 装飾属性：Bold, Italic, Underline, Inverse, Blink, Strike等
- スタイルリセット

### P2（入出力拡張）

- マウス追跡（x10, vt200, SGR）
- ブランケットペースト通知
- ターミナルタイトル設定
- ハイパーリンク埋め込み（OSC 8）

### P3（高度拡張）

- 画像描画 (Sixel, iTerm2, kitty graphics)
- クリップボード操作（OSC 52等）
- カーソル形状制御（DECSCUSR）
- 詳細キーボード報告（xterm MOK, kitty protocol等）
- 通知・ベル・フォーカスイベント

---

## プロファイル互換性とライフサイクル

### プロファイル交渉

#### 初期交渉（open時）
```pseudo
handle = open(requested_profile, options)
actual_capabilities = get_capabilities(handle)
actual_profile = actual_capabilities.profile
```

#### 交渉結果
| 要求プロファイル | 端末能力 | 結果プロファイル | 利用可能機能 |
|--------------|----------|--------------|------------|
| P3 | 全対応 | P3 | 画像・クリップボード・詳細キーボード等 |
| P3 | P2相当 | P2 | マウス・ペースト・タイトル・リンク |
| P2 | P1相当 | P1 | 色・装飾のみ |
| P1 | P0相当 | P0 | 基本機能のみ |
| P0 | 最小限 | P0 | 基本機能（一部縮退可能） |

### プロファイル移行

#### アップグレード（P0→P1→P2→P3）
プロファイルのアップグレードは**新しいopen()呼び出し**が必要：

```pseudo
// 段階的アップグレード例
handle_p0 = open(P0, options);
// P0機能を使用...
close(handle_p0);

handle_p1 = open(P1, options);
if (get_capabilities(handle_p1).profile >= P1) {
    // P1機能（色・装飾）を使用可能
  set_style(handle_p1, foreground=RED, bold=true);
}
```

#### ダウングレード理由
- **一時的制限**: tmux/screen環境での機能制限
- **互換性確保**: 古い端末での動作保証
- **リソース節約**: 低メモリ環境での軽量動作
- **デバッグ**: 問題切り分けのための機能制限

### 状態管理と移行

#### 状態保持対象
| 状態カテゴリ | P0→P1 | P1→P2 | P2→P3 | ダウングレード |
|------------|-------|-------|-------|------------|
| **カーソル位置** | 保持 | 保持 | 保持 | 保持 |
| **画面内容** | 保持 | 保持 | 保持 | 保持 |
| **文字スタイル** | リセット | 保持 | 保持 | リセット→既定 |
| **マウス追跡** | - | 無効→設定可 | 保持 | 無効化 |
| **ペースト通知** | - | 無効→設定可 | 保持 | 無効化 |
| **タイトル** | - | 既定→設定可 | 保持 | 保持 |
| **画像** | - | - | 無効→設定可 | 無効化・消去 |

#### 安全な移行手順
```pseudo
function safe_profile_transition(current_handle, target_profile) {
    // 1. 現在状態の記録
    current_state = capture_state(current_handle);

    // 2. 安全な終了
    safe_close(current_handle);

    // 3. 新プロファイルで再開
    new_handle = open(target_profile, options);
    actual_profile = get_capabilities(new_handle).profile;

    // 4. 状態復元（対応範囲内で）
    restore_compatible_state(new_handle, current_state, actual_profile);

    return new_handle;
}
```

### 動的プロファイル変更

#### 制限事項
- **ランタイム変更不可**: 同一ハンドルでのプロファイル変更は未対応
- **再接続必須**: 必ず `close()` → `open()` のサイクル
- **端末能力固定**: 物理端末の能力は実行中に変化しない前提

#### 実行時プロファイル検出
```pseudo
// アプリケーション適応パターン
handle = open(P3, options);  // 最高レベルを要求
actual = get_capabilities(handle);

switch (actual.profile) {
    case P3:
  enable_full_features(handle);
        break;
    case P2:
  enable_basic_interaction(handle);
        break;
    case P1:
  enable_visual_enhancement(handle);
        break;
    case P0:
  enable_text_only_mode(handle);
        break;
}
```

### 互換性保証

#### 前方互換性
- **P0コードのP1実行**: P0用に書かれたコードはP1環境で正常動作
- **API安定性**: 下位プロファイルのAPIは上位で必ず利用可能
- **縮退動作**: 未対応機能は安全に無効化または代替動作

#### 後方互換性
- **機能制限**: 上位プロファイル専用機能は下位では利用不可
- **エラー処理**: 未対応機能の呼び出しは明確なエラー報告
- **状態整合性**: ダウングレード時の状態は安全な既定値にリセット

#### プロファイル検出推奨パターン
```pseudo
// 防御的プログラミング
function use_color_if_available(text, color) {
  caps = get_capabilities(handle);
    if (caps.colors != "none") {
    set_style(handle, foreground=color);
    write_text(handle, text);
    reset_style(handle, all);
    } else {
    write_text(handle, text);  // 色なしで出力
    }
}

// 機能別分岐
function setup_mouse_if_supported() {
  caps = get_capabilities(handle);
    if (caps.mouse != "none") {
    enable_mouse(handle, "sgr");
    register_mouse_handler(handle, on_mouse_event);
    }
}
```

---

## API一覧（P0）

### ライフサイクル

| API                                             | 概要           | 備考           |
| ----------------------------------------------- | ------------ | ------------ |
| `open(requested_profile, options) -> Handle`    | ハンドルを生成し利用開始 | 実際の能力を交渉し返す。端末能力に応じてプロファイル縮退あり |
| `close(handle)`                                 | 利用終了、画面状態を復元 | カーソル表示・スタイルリセット・拡張機能無効化 |

### 出力

| API                              | 概要                     | 縮退時の挙動          |
| -------------------------------- | ---------------------- | --------------- |
| `clear_screen(handle, mode)`     | 画面消去（after/before/all） | 未対応時は無効果        |
| `clear_line(handle, mode)`       | 行消去（after/before/all）  | 未対応時は無効果        |
| `move_to(handle, row, col)`      | 絶対カーソル移動               | 範囲外は端末内でクランプ    |
| `move_by(handle, drow, dcol)`    | 相対カーソル移動               | 未対応時は無効果        |
| `show_cursor(handle, bool)`      | カーソル表示／非表示             | 未対応時は常に表示       |
| `write_text(handle, graphemes)`  | 文字列出力（符号化を経由）          | 符号化不可文字は代替文字に変換 |
| `flush(handle)`                  | 明示的なバッファー送出（P0では即時送出のためNo-op） | 無効果（エラー状態リセットのみ） |

### スタイル・色制御（P0拡張）

- **有効化**: スタイル関連を利用する場合は `terse_options_t.enabled_caps` に `TERSE_CAP_ENABLE_TEXT_STYLES` を指定する。色は `TERSE_CAP_ENABLE_SGR_BASIC` / `TERSE_CAP_ENABLE_SGR_EXTENDED` / `TERSE_CAP_ENABLE_TRUECOLOR` で段階的に有効化。
- **スタイル設定**: `terse_style_t style = terse_style_default();` で初期化し、`style.effects` に `TERSE_STYLE_*` ビットを立てる。色を指定する場合は `style.foreground` / `style.background` に `kind={BASIC16,PALETTE256,TRUECOLOR}` を設定して `terse_set_style(handle, &style)` を呼ぶ。
- **色ヘルパ**: `terse_color_basic(color, bright)` / `terse_color_palette(index)` / `terse_color_truecolor(r,g,b)` を使うと `terse_color_t` を安全に構築できる。
- **縮退動作**: 対応していない色/装飾は自動で下位互換に縮退（例: TrueColor→256→16→既定色）。機能無効時は No-op で成功し、`terse_get_last_error` に値が残ることはない。
- **状態復元**: `terse_capture_state` / `terse_restore_state` でカーソル位置・表示と同時にスタイル状態を保存/復元できる。
  - スタイルが未知の場合は `restore` 時にリセット（`0m`）および指定スタイルを再適用。
- **リセット**: `terse_reset_style(handle, scope)` を利用する。`scope=all` は `SGR 0`、`scope=color_only` は `39;49`、`scope=effects_only` は `22;23;24;27;29` を送出する。`terse_close` も安全のため同様のリセットを行う。

### 入力

| API                                 | 概要            | 縮退時の挙動                       |
| ----------------------------------- | ------------- | ---------------------------- |
| `read_event(handle, timeout_ms)`    | 入力を抽象イベントに正規化 | 未知シーケンスは `RawSequence` として返す |

### 能力

| API                           | 概要           | 備考             |
| ----------------------------- | ------------ | -------------- |
| `get_size(handle)`            | 行数・列数を返す     | 取得不可なら Unknown |
| `get_capabilities(handle)`    | 色・マウスなどの能力情報 | P0では最小限（色なし）   |

---

## 入力イベント仕様表（P0）

### Event種別

| 種別                                                   | 説明            | フィールド                                       |
| ---------------------------------------------------- | ------------- | ------------------------------------------- |
| `Char`                                               | 単一の文字入力       | `scalar`（Unicodeスカラー値）、`mods`（修飾キー）、`width`（セル幅） |
| `Enter`                                              | 改行キー          | `mods`                                      |
| `Backspace`                                          | バックスペースキー     | `mods`                                      |
| `Tab`                                                | タブキー          | `mods`                                      |
| `ArrowUp` / `ArrowDown` / `ArrowLeft` / `ArrowRight` | 矢印キー          | `mods`                                      |
| `Resize`                                             | 端末サイズ変更       | `rows`, `cols`                              |
| `RawSequence`                                        | 未知のエスケープシーケンス | `bytes[]`                                   |

### フィールド定義

- **scalar**: 入力文字のUnicodeスカラー値。復号不可の場合は置換文字。
- **width**: 表示セル幅の推定値（結合/制御は0、半角1、全角2）。
- **mods**: 修飾キー。`{Shift, Ctrl, Alt, Meta}` のビット集合。未対応修飾は無視（0扱い）。
- **rows, cols**: リサイズイベントでの新しい行数・列数。取得不可ならUnknownを返す。
- **bytes[]**: 未知シーケンスの生バイト列。アプリケーションが解釈可能。

---

## 縮退表（P0）

| 要求機能                          | 能力未対応時の縮退動作      |
| ----------------------------- | ---------------- |
| `clear_screen` / `clear_line` | 無効果（画面に何も起きない）   |
| `move_to` / `move_by`         | 無効果（カーソル位置固定）    |
| `show_cursor(false)`          | 無効果（常に表示されたまま）   |
| `write_text`（符号化不可文字）         | 代替文字（例: `?`）に置換  |
| `get_size`                    | Unknownを返す       |
| `read_event`（未知のシーケンス）        | RawSequenceとして返す |

---

## エラーと安全規約

### エラー分類

ライブラリで扱うエラーは以下のカテゴリに分類される。C実装では `terse_get_last_error()` で `terse_error_info_t` を取得できる。

1. **Transport層エラー**：物理的な入出力の失敗（例：`write(2)` の `EBADF`、`read(2)` の `EPIPE`）
2. **プロトコルエラー**：端末との通信プロトコルの問題（P0では未使用）
3. **リソースエラー**：メモリ不足、サイズ制限等（P0では未使用）
4. **設定エラー**：不正な引数、未対応能力の要求等（`EINVAL` など）
5. **状態エラー**：ライブラリの不正な状態での操作（`terse_get_last_error(NULL)` など）

> **補足:** APIが成功した場合（`0`または正値）は `category=TERSE_ERROR_NONE` が保証される。機能無効化によるNo-opでもエラーはクリアされる。

> **補足**: 戻り値が成功 (`0`/正値) の場合、`category=TERSE_ERROR_NONE` が保証される。機能無効化によるノーオペ時にもエラー状態はクリアされる。

### Transport層エラー詳細

| エラー種別 | 発生API | 原因 | 処理方針 |
|-----------|---------|------|----------|
| **送信失敗** | `write_text`, `move_to` 等の出力API | 接続切断、バッファー満杯、権限不足 | `TERSE_ERROR_TRANSPORT` を設定しエラー返却。部分送信は未定義動作 |
| **受信失敗** | `read_event` | 接続切断、読み込みタイムアウト、信号割り込み | 信号割り込みは再試行、致命的な場合は `TERSE_ERROR_TRANSPORT` を設定して返却。EOF (`EPIPE`) と区別する |
| **初期化失敗** | `open` | デバイスアクセス不可、端末モード設定失敗 | エラー返却。リソース確保前なので後始末不要 |
| **終了処理失敗** | `close` | 復帰処理の部分的失敗 | 可能な限り継続実行。致命的な場合のみ `TERSE_ERROR_TRANSPORT` |

### 例外処理 vs エラーコード

言語の慣習に従い、以下の使い分けを推奨：

#### 例外を使用する場合（C++、Python、Rust等）
- **例外**: Transport層エラー、プロトコルエラー、リソースエラー
- **戻り値**: 正常な結果（イベント、能力情報等）
- **None/Option**: タイムアウト、未検出状態（非エラー）

#### エラーコードを使用する場合（C等）
- **負値**: エラーコード（POSIX errno互換推奨）
- **0**: 成功
- **正値**: 有効なデータ、ハンドル等

### エラー復旧処理

#### 自動復旧
以下のエラーでは自動復旧を試みる：
- **一時的送信失敗**: 短時間リトライ（最大3回、100ms間隔）
- **信号割り込み**: システムコール再実行
- **部分的終了処理失敗**: 残り処理を継続実行

#### 復旧不可能
以下のエラーでは復旧せずエラー報告：
- **接続切断**: アプリケーションレベルでの再接続が必要
- **権限不足**: 環境修正が必要
- **致命的プロトコルエラー**: 端末互換性問題

### 状態一貫性保証

エラー発生時の状態保証：
- **出力API失敗**: 端末状態は不定。アプリケーションで `reset_style(handle, all)` 等による復旧推奨
- **入力API失敗**: 内部状態は一貫性を保持。`read_event` 再呼出し可能
- **close() 失敗**: 部分的復旧済み。プロセス終了時の最終手段として機能

### 安全規約

- `close()` は可能な場合「カーソル表示ON」「スタイルリセット」を試みる（出力無効時はノーオペ）。
- 部分的失敗でも可能な限り復帰処理を継続し、最終的なエラー状態は `terse_get_last_error()` で参照できる。
- 入力から得た生シーケンスをログへ出力する際は、制御コードのエスケープを推奨。

---

## 今後の拡張ポイント（P1以降）

- 色・装飾（SGR）
- マウス追跡
- ブランケットペースト
- タイトル／リンク／画像\
  （詳細は別途定義する）

---

## 入力イベント仕様（P0）

### イベントモデル

- **同期取得**：`read_event(handle, timeout_ms) -> Event | None`。
  - `timeout_ms < 0`：無期限待ち。`0`：即時ポーリング。正数：指定ミリ秒。
- **正規化**：受信バイト列を既知パターンへ最長一致で正規化し、抽象イベントに変換する。
- **順序保証**：受信順序を保持。端末/OSのバッファリングにより到着順が入替る場合は到着順を優先。
- **合成（コアレッシング）**：`Resize` は短時間に複数発生しても **最新1件のみ** を返す実装を許容（アプリは連続`Resize`を想定）。

### イベント種別

| 種別                              | 説明                                | 主フィールド                    |
| ------------------------------- | --------------------------------- | ------------------------- |
| `Char`                          | 文字入力（表示可能グラフェム）                   | `scalar`, `mods`, `width` |
| `Enter`                         | 改行キー（修飾付きも含む）                      | `mods`                    |
| `Backspace`                     | 後退キー                              | `mods`                    |
| `Tab`                           | 水平タブ（`Shift+Tab` など）               | `mods`                    |
| `Arrow{Up,Down,Left,Right}`     | 矢印キー                              | `mods`                    |
| `Home` / `End`                  | 行頭/行末                             | `mods`                    |
| `Page{Up,Down}`                 | ページ移動                             | `mods`                    |
| `Insert` / `Delete`             | 挿入／削除キー                          | `mods`                    |
| `Function(n)`                   | F1–F24（実装拡張可。n=1..24 目安）          | `n`, `mods`               |
| `Resize`                        | 端末サイズ変更                           | `rows`, `cols`            |
| `PasteBegin` / `PasteEnd`       | ブランケットペースト開始/終了（**P0では未対応、P2で有効化**） | なし                        |
| `RawSequence`                   | 未知のエスケープ列または制御列                   | `bytes[]`                 |

> 注：P0では `PasteBegin/End` は**未対応**のため生成されない。P2以降で有効化される。ただし、将来互換のためイベント種別として予約定義されている。

### フィールド定義

| フィールド         | 型/値域                             | 説明                                      |
| ------------- | -------------------------------- | --------------------------------------- |
| `scalar`      | Unicode スカラー値（U+0000..U+10FFFF）   | `Char` の文字。復号不可の場合は **置換ポリシー**（7.4）に従う。 |
| `width`       | {0,1,2}                          | 表示セル幅の推定値（結合/制御は0、半角1、全角2）。             |
| `mods`        | ビット集合 `{Shift, Ctrl, Alt, Meta}` | 未対応修飾は無視（0扱い）。                          |
| `rows`/`cols` | 正整数                              | `Resize` の新しいサイズ（1オリジン座標系の行/列数）。        |
| `n`           | 正整数                              | `Function(n)` の番号。                      |
| `bytes[]`     | バイト列                             | 未知シーケンスそのもの（**ログ用途**。直接実行しないこと）。        |

### 文字デコード規約（Codec）

- **入力Codec** は `open(..., options.codec)` で選択（例：`UTF-8`, `Shift_JIS`）。
- 復号時の規約：
  1. **完全一致**できる並びは `Char` とする。
  2. 不正シーケンスは **置換** または **RawSequence** のいずれかを選択可能（実装オプション）。
     - 置換は既定で `U+FFFD`、Shift_JISでは `?` を許容（仕様ポリシーで固定可能）。
  3. 制御文字（C0/C1）は `Char` とせず、既知のキー（`Enter`/`Tab`/`Backspace` 等）に正規化、未知は `RawSequence`。
- **セル幅決定**：実装はEast Asian WidthとCodecに基づき `width` を算出。未知は1。

### 修飾キー正規化

- `Ctrl` はASCII英字の0x40マスクによる衝突と区別して保持（例：`Ctrl-J` は `Enter` に正規化可。アプリの方針に依存）。
- `Alt` はESC前置/8bit拡張の差異を吸収して `mods.Alt` を立てる。
- `Meta` は端末依存のため多くの環境で未使用（0）。

### リピートと長押し

- 端末/OSの **キーリピート** により同一イベントが連続発生しうる。ライブラリは特別扱いしない（そのまま列挙）。
- 長押しの押下/離上イベントは **扱わない**（抽象化の対象外）。

### タイムアウトと None

- `read_event(handle, t)` は、tミリ秒以内にイベントがない場合 **`None`** を返す。
- 例外/エラーがない限り、`None` は正常動作を意味する。

### エラー条件

#### Transport受信エラー
以下の状況でエラーを返す（例外/エラーコードは言語規約に従う）：

| 状況 | エラーの意味 | アプリケーション対応 |
|------|-------------|-------------------|
| **接続切断** | 端末プロセス終了、SSH切断等 | 再接続またはアプリケーション終了 |
| **デバイスエラー** | ハードウェア障害、権限喪失 | エラー報告後の適切な終了処理 |
| **バッファーオーバーフロー** | 大量データ受信でメモリ不足 | リトライまたは設定調整 |
| **プロトコル異常** | 解析不能な制御シーケンス | 警告ログ後の処理継続 |

#### 正常な非エラー状態
- **タイムアウト**: `None` を返す（エラーではない）
- **Unknown サイズ**: `get_size()` が `Unknown` を返す（エラーではない）
- **RawSequence**: 未知シーケンスをそのまま返す（エラーではない）

### ロギングと監視

- アプリケーションは API 呼び出し失敗時に `terse_get_last_error()` を取得し、`category` と `code` をログ出力する。例: `Transport` かつ `code=EBADF` ならハンドル再初期化を検討する。
- エラー発生直後に `terse_get_last_error()` を再評価すると、成功時に `TERSE_ERROR_NONE` にクリアされる点に注意。
- 無効化された機能がノーオペで成功した場合は `TERSE_ERROR_NONE` のままなので、特別なログは不要。
- 信号割り込みなど一時的なエラーは API 内でリトライ済み。繰り返し失敗する場合のみ `TERSE_ERROR_TRANSPORT` が設定される。

#### Resize取得の特別扱い
- `Resize` の取得失敗時はイベントを生成しない
- `get_size()` は別途Unknownになりうる
- これはエラーではなく、能力不足として扱う

### 安全とログ指針

- `RawSequence.bytes[]` は **そのまま再送しない**。ログに出力する際は制御コードを可視化/エスケープすること。
- 端末へ戻す場合はアプリ側で検証/再解釈すること（インジェクション防止）。

### キーボード拡張 API

TERSE は端末に対して修飾キー拡張を opt-in で交渉する関数を提供する。

```c
unsigned int terse_keyboard_get_supported(terse_handle_t handle);
unsigned int terse_keyboard_get_enabled(terse_handle_t handle);
int terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask);
int terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask);
```

- `terse_keyboard_get_supported` は、環境検出の結果「安全に有効化できると推定される機能」をビット集合で返す。現在は `TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS`（xterm / WezTerm 等）と `TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL`（kitty / iTerm2 などの CSI-u 対応端末）を判定対象としている。
- `terse_keyboard_enable` / `disable` は冪等で、すでに対象がオン/オフの場合も 0 を返す。送信が必要な場合のみ制御シーケンスを出し、エラーが発生したときに負の値を返す。
- `TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS` は `CSI > 4 ; 2 m` / `CSI > 4 ; 0 m` を送信し、`Shift+Enter` や `Ctrl+Tab` などが `CSI 27;…~` 形式で届くようになる。
- `TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL` は `CSI ? 2026 h` / `CSI ? 2026 l` を送信し、kitty 互換の `CSI number;mods u` フォーマットで修飾付きキーが届くようになる。
- サポートされない環境では縮退（シーケンスを送信せず成功扱い、`get_enabled` にも反映されない）。

### 例：抽象イベント（非規範・参考）

- `Char(scalar=U+3042, width=2, mods=0)` … ひらがな「あ」入力。
- `ArrowLeft(mods=Ctrl)` … `Ctrl+←`。
- `Function(n=5, mods=Alt)` … `Alt+F5`。
- `Resize(rows=48, cols=160)` … 端末サイズ変更。
- `RawSequence(bytes=[0x1B,0x5B,0x3F,0x7E])` … 未知CSI。

---

## 能力表（Capabilities）

### 能力取得

- API: `get_capabilities()`
- 戻り値: 能力情報の構造体または辞書形式。
- P0では最小限の定義にとどめ、拡張はP1以降で追加可能。

### 共通フィールド（P0コア能力）

| フィールド               | 値域                                     | 説明            | P0での既定     |
| ------------------- | -------------------------------------- | ------------- | ---------- |
| `profile`           | {P0,P1,P2,P3}                          | 実効プロファイル      | P0         |
| `rows`              | 正整数または Unknown                         | 端末の行数         | Unknown可   |
| `cols`              | 正整数または Unknown                         | 端末の列数         | Unknown可   |
| `cursor_visibility` | {toggle, always_on}                   | カーソル表示切替可能性   | always_on |

### 縮退規則（P0コア能力）

| 要求          | 能力未対応時の挙動                                                 |
| ----------- | --------------------------------------------------------- |
| カーソル非表示     | `cursor_visibility=always_on` の場合は無効果。                    |

### プロファイル拡張

- **P1以降**で追加される能力フィールド：
  - P1: `colors`, `effects`（色・装飾）
  - P2: `mouse`, `paste_bracketed`, `title`, `hyperlink`（入出力拡張）
  - P3: `images`, `clipboard`, `cursor_shape`, `keyboard_reporting`, `notifications` 等（高度拡張）
- Human68k環境では多くが未対応となり、P0相当の返却となることを想定。

---

## セル幅規約

### 基本方針

- 画面は等幅セルのグリッドで構成される。
- 各グラフェムは0, 1, 2のいずれかのセル幅を占有する。
- セル幅は **符号化 (Codec)** と **East Asian Width 規則**を基準に決定する。
- 実装はセル幅を推定し、`Event.width` または `write_text` の内部処理で反映する。

### 値の意味

| 値 | 意味                  | 例                         |
| - | ------------------- | ------------------------- |
| 0 | 幅を消費しない（結合文字、制御コード） | U+0301 結合アクセント、U+0007 BEL |
| 1 | 半角幅                 | ASCII文字、半角カナ              |
| 2 | 全角幅                 | CJK統合漢字、かな類、全角記号          |

### Codec別規則

#### UTF-8

- Unicode East Asian Width = WideまたはFullwidth → 幅2。
- Neutral / Ambiguous → 既定は幅1。ただしオプションでEastAsianAmbiguous=Wideを有効化可。
- Combining Marks → 幅0。

#### Shift_JIS

- 半角カナ (0xA1–0xDF) → 幅1。
- その他2バイト文字 → 幅2。
- ASCII (0x00–0x7F) → 幅1。
- 不正バイト列 → 代替文字 `?` とし幅1。

### 不明文字・代替

- 符号化不可または幅判定不可の場合 → 幅1を既定とする。
- 代替文字（例: `?`, U+FFFD）は常に幅1。

### 一貫性の保証

- 同一文字は常に同じ幅で扱う（実行時変更しない）。
- アプリケーションは「実幅」との差異が出うることを考慮すべき。とくにEastAsianAmbiguous設定が異なる場合。

### Human68k環境の想定

- Shift_JISを既定とし、半角/全角を上記規則で区別。
- 合成文字は存在しない前提で幅0を利用しない（未知コードは幅1）。

---

## エンコーディング規約

### 概要

- ライブラリは**入出力の符号化**を抽象化し、`open(..., options.codec)` で選択可能。
- P0の必須実装: `UTF-8`, `Shift_JIS`。その他（`ISO-2022-JP` 等）は任意拡張。

### 入力（受信→抽象グラフェム）

1. **復号の基本**
   - 受信バイト列を `codec` にしたがって復号し、**抽象Unicodeスカラー**へ写像する。
   - 復号不能（不正並び等）は **置換** もしくは **RawSequence** として扱う（後述の「置換・RawSequence方針」参照）。
2. **制御文字の扱い**
   - C0/C1のうち、P0で意味を持つものは**イベント化**（例：`LF`→`Enter`、`TAB`→`Tab`、`BS`→`Backspace`）。
   - それ以外は `RawSequence` として保持するか **破棄**（実装オプション、既定は保持）。
3. **合成文字**
   - UTF-8では結合文字を許容し、`width=0` を付与（セル幅規約参照）。
   - Shift_JISでは結合の想定なし。未知は置換し幅1。

### 出力（抽象グラフェム→送信）

1. **再符号化**
   - 抽象文字を `codec` に従いエンコードし送信。
   - 非対応文字は **代替表現** に置換（後述の「置換・RawSequence方針」参照）。
2. **改行**
   - `write_text("
     ")` は **LF（0x0A）のみ** 送出。CRLF変換は行わない（アプリが必要に応じて送る）。
3. **制御の安全**
   - テキスト出力は原則として **制御シーケンスを含まない** 前提。必要な制御は出力API（`move_to` 等）で表現する。

### オプション（推奨）

| オプション                         | 既定        | 説明                                   |
| ----------------------------- | --------- | ------------------------------------ |
| `codec`                       | `UTF-8`   | 入出力符号化。`Shift_JIS` を選択可能。            |
| `invalid_input_policy`        | `replace` | `replace`（置換）/`raw`（RawSequenceで保持）。 |
| `replacement_char_utf8`       | `U+FFFD`  | UTF-8復号不能の置換。                        |
| `replacement_char_sjis`       | `?`       | Shift_JIS復号不能の置換。                   |
| `east_asian_ambiguous_wide`   | `false`   | Ambiguous を幅2として扱う。                  |
| `escape_control_when_logging` | `true`    | ログ出力時に制御コードをエスケープ。                   |

### 置換・RawSequence 方針

- **置換（replace）**
  - 入力：不正並びは置換文字へ。
  - 出力：エンコード不能は近似（半角代替等）→無ければ置換文字。
- **RawSequence（raw）**
  - 入力：不正並び/未知制御は `RawSequence(bytes[])` としてイベント化。
  - 出力：**RawSequenceをそのまま送るAPIは提供しない**（インジェクション回避）。必要なら上位で検査・変換する。

### Shift_JIS 特記事項

- 半角カナ（0xA1–0xDF）はそのままスカラーへ写像し幅1（セル幅規約参照）。
- ベンダー拡張／未定義コードポイントは置換。
- JIS互換記号の丸め等、厳密互換性は要求しない（実装差を許容）。

### Human68k環境の想定

- 既定Codec: `Shift_JIS`。
- 半角/全角規則はセル幅規約に準拠。
- 不正バイトは `?` に置換。

### Unicode 正規化

- 入出力において**正規化（NFC/NFKC 等）は実施しない**のを既定とする。
- 正規化が必要な場合はアプリケーション層で実施する。

### 一貫性と可搬性

- 同一 `codec` とオプション設定下では、同一入力に対して**同一の抽象イベント列**と**同一の送信列**を生成すること。
- 異なる端末間の差異は**能力・セル幅規約・縮退規則**で吸収する。

### ログと安全

- ログ出力時には不正シーケンスを必ずエスケープすること。
- 生バイト列をそのまま端末に戻すことは推奨されない。

---

## 制御コードと改行規約

### 制御コードの幅（Shift_JIS含む）

- 制御コード（0x00–0x1F, 0x7F, C1領域など）は **幅0** とする。
- 既知のキーに正規化される場合（例: 0x08 → Backspace, 0x09 → Tab, 0x0A → Enter）はイベント化され、幅の概念を持たない。
- 未知の制御コードを `RawSequence` として扱う場合も幅0とする（表示セルを占有しない）。

### 改行（LF / CRLF）の扱い

#### 出力側

- `write_text(handle, "\n")` は常に **LF (0x0A)** を送出する。
- CRLF変換は行わない。必要に応じて `"\r\n"` を明示的に送るのはアプリケーションの責務。

#### 入力側

- **LF単独 (0x0A)** → `Enter` イベントに正規化。
- **CRLF (0x0D 0x0A)** → 1つの `Enter` イベントに正規化（CRは吸収）。
- **CR単独 (0x0D)** → 未知制御として `RawSequence` 扱い（幅0）。

### 互換性の意図

- Unix系（LF）、Windows/DOS/Human68k（CRLF）の両方をサポート。
- 内部表現はLFに統一し、外部からの入力でCRを伴う場合は正規化で吸収。
- これによりアプリケーションは常に「Enter = LF」として扱える。

---

## 対話入力における改行と送信の分離（REPL向け指針）

### 目的

- 生成AIチャット／REPL等で求められる「**改行（InsertNewline）** と **送信（Submit）** の分離」を、端末依存性を最小にして実現するための指針。
- ライブラリは **キー→抽象イベント** までを担当し、最終的な動作（改行or送信）は **アプリ側のキー割り当て**で決める。

### 抽象イベントの扱い（P0）

- `Enter` は基本イベント。修飾の扱いは以下：
  - **通常の Enter**（多くの端末でCRまたはCRLF）→ `Enter(mods=0)` に正規化。
  - **Ctrl+J**（ASCII 0x0A = LF）→ `Enter(mods=Ctrl)` として正規化することを**推奨**。
    - *注*: 7.5の規約に従い、Ctrlマスクを保持して `Enter` に正規化。
  - **Shift+Enter**：多くの端末では**区別できない**（Shift情報が来ない）。よって `Enter(mods=0)` と区別不能。
    - ただしP2以降の **拡張キーボード報告**（例：xterm `modifyOtherKeys` / kitty keyboard protocol）を能力として検出できる場合は `Enter(mods=Shift)` を報告可能。

### 推奨キー割り当て（アプリ指針）

- 互換性重視の既定：
  - `Enter(mods=0)` → **Submit**（送信）
  - `Enter(mods=Ctrl)` → **InsertNewline**（改行）
  - `Enter(mods=Shift)` → **InsertNewline**（ただし能力 `keyboard_reporting` が有効なときのみ有効）
- 代替：`Alt+Enter` が区別可能な端末では `Enter(mods=Alt)` をInsertNewlineに割り当ててもよい（端末によりAltはESC前置として到来）。

### 能力拡張（P2予定）

- `capabilities.keyboard_reporting` を追加：
  - 値域 `{none, xterm_mok, kitty, wezterm, other}` など。
  - `none` の場合、Shift情報や細かい修飾は取得不能。
  - `xterm_mok/kitty` 等では `Enter` を含む多くのキーで修飾が検出可能。

### ブランケットペーストとの相互作用

- 大量の改行を含む貼り付けで意図せずSubmitされるのを防ぐため、P2の `paste_bracketed` を有効化し、
  - `PasteBegin`〜`PasteEnd` の区間では `Enter(mods=0)` を **InsertNewline** として扱う、等のアプリ側ポリシーを推奨。

### 互換性メモ

- 本仕様のコアはP0でも成立（`Enter` と `Ctrl+J` の区別）。
- `Shift+Enter` を確実に使いたい場合はP2で `keyboard_reporting` 能力が必要。

---

## リサイズイベントと初期状態規約

### リサイズイベント

- **目的**: 端末の行数・列数が変更されたときに `Resize(rows, cols)` イベントを発火する。
- **Human68k**: 画面解像度（例: 768x512, 512x512, 256x256）が変更可能であり、必ずイベントが必要。
  - イベントトリガーの検出方法は環境依存（IOCS呼び出し等）。
- **POSIX系**: 通常は `SIGWINCH` を契機にサイズを取得し直し、イベント生成する。
- **保証**: ライブラリは「サイズが変わった」と判定できた場合のみイベントを生成する。判定不可の場合は `get_size()` に依存。
- **コアレッシング**: 短時間に複数回のサイズ変化があっても、最新の1件のみを返してよい。

### 初期状態

- `open()` 直後の状態規定:
  - **カーソル表示**: ON。
  - **スタイル**: リセット状態（色なし、装飾なし）。
  - **画面内容**: 既存内容を維持（クリアしない）。
  - **端末サイズ**:
    - Human68k: 必ず取得可能。
    - POSIX: `ioctl(TIOCGWINSZ)` で取得。OSによってはUnknownを返す場合がある。

### 安全終了

- `close()` を呼ぶことで必ず安全に復帰することを仕様とする。
- ただしアプリケーションが呼び忘れる可能性があるため、**最終的な責任はアプリケーション**にある。
- 言語ラッパ（C++ RAII, Rubyブロック等）で自動的に `close()` を呼ぶ設計を推奨。

---

## リサイズイベント規約（P0 詳細）

### 目的

- 画面サイズの変更（列数・行数の変化）を **Resize(rows, cols)** イベントで通知する。Human68kでも768×512 / 512×512 / 256×256等の切替を想定。

### 生成要件

- **Resize イベントは、実サイズが変化したときにのみ生成**する（重複通知防止）。
- 連続検出時のコアレッシング：短時間に複数回検知した場合、**最新の1件のみ**をアプリに返してよい（7.1参照）。

### トリガー（環境別の検知手段）

- 本仕様では **検知手段は実装依存**。代表例：
  - **信号・割り込み・フック**：POSIXの `SIGWINCH` 相当、Human68kの表示モード切替フック等。
  - **プロトコル問合せ**：カーソル位置報告／属性問い合わせなどの副作用からの検知（P1+想定）。
  - **ポーリング**：一定間隔または `read_event` 呼出時に前回値と比較（P0で許容）。

### サイズ取得と一貫性

- `get_size()` が返す `(rows, cols)` と、`Resize` イベントの `rows, cols` は**一致**しなければならない。
- 取得に失敗する環境では `Unknown`。この場合、`Resize` イベントは生成しない（サイズが観測不可能なため）。

### Human68k 特記事項

- Human68kでは **サイズ取得が常に可能**であることを前提とする（本仕様の前提）。
- 表示モード変更（例：768×512 → 512×512）では、**行数・列数の定義**はバックエンドが決定し一貫して返す（例：フォント矩形に基づくキャラクタセル数）。

---

## 初期状態の規定（P0）

### `open()` 直後の画面状態

- **カーソル**：表示状態（`show_cursor(true)` 相当）。
- **スタイル**：リセット済み（装飾なし、色なし）。
- **スクリーン**：オルタネートスクリーンは未使用（P0）。

### 初期サイズ

- Human68k：`get_size()` は **必ず有効な** `(rows, cols)` を返す。
- 近代端末（POSIX等）：`get_size()` は **取得可能なことが多い**が、端末でない入力（パイプ等）の場合は `Unknown` を返しうる。
- 取得が `Unknown` のままでも、P0では動作可能（アプリは自前のレイアウト推定や、初回描画時に `get_size()` 再取得を行う）。

---

## 安全終了とクリーンアップ（P0）

### 原則

- **安全終了はアプリケーションの責務**：原則として `close()` を明示呼出する。言語機能（RAII/ファイナライザー等）で `close()` を自動化してもよいが、仕様上はアプリ側での管理を推奨。

### 最低保証（ライブラリ側）

- `close()` は可能な範囲で **カーソル表示ON**、**スタイルリセット**、必要ならスクリーン復帰（P1+）等を行う。
- `close()` 呼出し忘れに対する挙動は **未規定**（実装依存）。可能ならプロセス終了時のクリーンアップフックを用いて安全側に復帰することが望ましい。

### EOF/切断処理

#### EOF検出
- 入力ストリームの **EOF/切断** を検出した場合、`read_event()` は以下のように動作：
  - **例外言語**: `EOFError`, `ConnectionError` 等の専用例外をスロー
  - **エラーコード言語**: 負のエラーコードを返却（例：`-ECONNRESET`、`-EPIPE`）
  - **Result型言語**: `Err(ErrorKind::UnexpectedEof)` 等を返却

#### EOFとタイムアウトの区別
- **EOF**: 入力ストリームの終端。復旧不可能
- **タイムアウト**: 指定時間内にデータなし。`None` を返し、後続の `read_event` 呼び出し可能
- **信号割り込み**: システムコール中断。内部で再試行またはエラー報告

#### アプリケーション推奨対応
```pseudo
try {
    while (running) {
        event = read_event(100);  // 100ms タイムアウト
        if (event == None) {
            // タイムアウト：継続可能
            continue;
        }
        handle_event(event);
    }
} catch (EOFError) {
    // 入力終了：アプリケーション終了処理
    cleanup_and_exit();
} catch (TransportError) {
    // 通信エラー：再試行または終了
    retry_or_exit();
}
```

---

## プロファイル P1（色・装飾）仕様

### 目的

- P0の基本機能に加え、文字スタイル（装飾）と色指定を可能にする。
- 実端末能力に応じて、16色／256色／24bit TrueColorのいずれかに縮退。

### API 拡張

| API                               | 概要                    | 備考                                    |
| --------------------------------- | --------------------- | ------------------------------------- |
| `set_style(handle, style)`        | 前景・背景色と装飾ビット集合をまとめて指定 | 状態ベース。部分更新も許容。                        |
| `reset_style(handle, scope)`      | スタイルのリセット             | `scope={all,color_only,effects_only}` |

### スタイル要素

- **色**：
  - `None`（デフォルト色）
  - `Basic16`：8色×明暗 = 16色。
  - `Palette256(idx)`：拡張256色パレット。
  - `TrueColor(r,g,b)`：24bit色（0–255）。
- **装飾（Effects）**：ビット集合。
  - `Bold`, `Faint`, `Italic`, `Underline`, `Inverse`, `Blink`, `Strike`。
  - 未対応ビットは無視。

### 縮退規則

| 要求            | 能力未対応時の縮退動作                              |
| ------------- | ---------------------------------------- |
| TrueColor 指定  | Palette256 対応なら近似変換、無ければ Basic16、最終的に無視。 |
| Palette256 指定 | Basic16 の近似色に縮退。                         |
| 装飾（未対応）       | 無視（スタイル変更なし）。                            |

### 能力表の拡張（P1で追加）

- `capabilities.colors`：{none, basic16, palette256, truecolor}
- `capabilities.effects`：サポートする装飾ビット集合

### 縮退規則（P1追加）

| 要求          | 能力未対応時の挙動                                                 |
| ----------- | --------------------------------------------------------- |
| 色指定         | もっとも近い下位へ縮退（例: truecolor→256→16→無色）。最終的に無色なら無視。             |
| 装飾（未対応）       | 無視（スタイル変更なし）。                                           |

### 初期状態

- `open()` 直後は **リセット状態**：
  - 前景・背景 = デフォルト。
  - 装飾 = 無し。

### Human68k特記事項

- Human68k標準のテキスト画面では色能力が限定的。通常は **basic16 未満**、または無色。
- 可能な場合のみ色をサポートし、未対応要素はすべて縮退。
- 装飾（下線等）は利用環境次第でサポートが異なるため、既定では未対応扱い。

### 安全規約

- `reset_style(handle, all)` を呼ぶことで、常に既定の安全状態（色なし・装飾なし）へ戻せる。
- 端末によっては色リセットが完全でない場合があるため、`reset_style(handle, all)` は **SGR 0** を送出することを推奨。

---

## プロファイル P2（入出力拡張）仕様

### 目的

- P1の色・装飾に加え、**マウス入力追跡**、**ブランケットペースト**、**タイトル/リンク制御**をサポートする。
- 端末依存度が高いため、`capabilities` で利用可否を判定することを必須とする。

### マウス追跡

- API:
  - `enable_mouse(handle, mode)` / `disable_mouse(handle)`
- モード：
  - `x10`：最小限（座標・ボタン）。
  - `vt200`：押下/離上を含む。
  - `sgr`：SGR拡張（推奨、座標>223対応）。
- イベント種別追加：
  - `Mouse{Down,Up,Move}` with fields: `button`, `mods`, `row`, `col`。
- 縮退：
  - `mouse=none` の場合、マウス関連イベントは一切生成されない。

### ブランケットペースト

- 機能：ペースト開始/終了をイベント化。
- API:
  - `enable_bracketed_paste(handle)` / `disable_bracketed_paste(handle)`
- イベント：
  - `PasteBegin`, `PasteEnd`
- 縮退：
  - `paste_bracketed=false` の場合、ペーストは通常の文字入力として処理され、イベント化されない。

### タイトル・リンク制御

- API:
  - `set_title(handle, string)`：端末ウィンドウ/タブタイトルを設定。
  - `set_hyperlink(handle, url, label)`：ハイパーリンク埋め込み（OSC 8準拠）。
  - `set_cursor_shape(handle, shape, blinking)`：カーソル形状（ブロック/アンダーライン/バー）と点滅を切り替える。
- 縮退：
  - `title=false` または `hyperlink=false` の場合は無効果。

### 能力表の拡張（P2で追加）

- `capabilities.mouse`：{none,x10,vt200,sgr}
- `capabilities.paste_bracketed`：{true,false}
- `capabilities.title`：{true,false}
- `capabilities.hyperlink`：{true,false}
- `capabilities.cursor_shape`：{true,false}

### 縮退規則（P2追加）

| 要求          | 能力未対応時の挙動                                                 |
| ----------- | --------------------------------------------------------- |
| マウス追跡       | `mouse=none` の場合はイベント生成されない。                              |
| ブランケットペースト  | `paste_bracketed=false` の場合は `PasteBegin/End` イベントを生成しない。 |
| タイトル変更      | `title=false` の場合は無効果。                                    |
| ハイパーリンク     | `hyperlink=false` の場合は無効果。                                |

### 初期状態

- `open()` 直後はマウス追跡とブランケットペーストは無効。
- タイトル・リンクは端末既定状態（未設定）。

### Human68k 特記事項

- Human68k標準環境ではマウス入力やタイトル/リンクは存在しないため、通常はすべて未対応。
- ただしGUI環境や拡張ツールが存在する場合は実装依存でサポート可能。

### 安全規約

- 無効化API（`disable_mouse`, `disable_bracketed_paste`）を必ず用意し、アプリ終了時に元の状態へ戻せるようにする。
- `set_hyperlink` は安全側で閉じ忘れを防ぐため、自動で終了シーケンスを送る実装を推奨。

---

## プロファイル P3（高度端末拡張）仕様

### 目的

- P2の入出力拡張に加え、**画像表示**や**端末固有の高度機能**を取り扱う。
- 高い互換性は保証されず、tmux/screen等の中継環境では透過されない可能性がある。
- 利用前に `capabilities` を必ず確認すること。

### 画像表示

- API候補：
  - `draw_image(handle, data, format, row, col, options)`
- サポート形式：
  - `sixel`（古典的、DEC・一部xterm系）
  - `iterm2` プロトコル（OSC 1337）
  - `kitty` graphicsプロトコル
- 縮退：
  - 非対応環境では無効果。

### クリップボード操作

- API候補：
  - `set_clipboard(data)`（OSC 52）
  - `get_clipboard()`（応答を期待できる環境は限定的）
- 縮退：
  - 非対応環境では無効果。

### カーソル形状制御

- API候補：
  - `set_cursor_shape(shape)`（block, underline, barなど）
  - `set_clipboard(data)`（OSC 52）
  - `get_clipboard()`（応答を期待できる環境は限定的）
- 縮退：
  - 非対応環境では無効果。

### その他拡張

- `notifications`：端末内通知表示（OSC 9等）。
- `focus_in/out`：フォーカスイベント通知。

### 能力表の拡張（P3で追加）

- `capabilities.images`：{none, sixel, iterm2, kitty}（複数対応の場合は配列）
- `capabilities.image_limits`：最大サイズ・ピクセル数・埋め込み数・透過サポート等
- `capabilities.clipboard`：{none, osc52_write, osc52_rw}
- `capabilities.cursor_shape`：{true,false}
- `capabilities.keyboard_reporting`：{none, xterm_mok, kitty, wezterm, other}
- `capabilities.notifications`：{none, bell, visual, desktop}（複合可）
- `capabilities.focus_events`：{true,false}
- `capabilities.multiplexer`：{none, tmux, screen, other}

### 縮退規則（P3追加）

| 要求          | 能力未対応時の挙動                                                 |
| ----------- | --------------------------------------------------------- |
| 画像描画        | 未対応時は無効果。代替テキスト表示はアプリ判断。                               |
| クリップボード操作   | 未対応時は無効果。読み出しはエラーまたは未対応を返す。                             |
| カーソル形状変更    | 未対応時は無効果。                                               |
| 詳細キーボード報告   | 未対応時は通常のP0/P2相当のイベントに留まる。                               |
| 通知・ベル・フラッシュ | 未対応時は無効果。                                               |

### 初期状態

- 画像表示・クリップボード・通知・フォーカスイベントはすべて無効。
- アプリケーションが明示的に有効化しなければ利用されない。

### Human68k 特記事項

- Human68k標準環境ではすべて未対応。
- GUIエミュレーターが拡張実装を提供する場合にのみ利用可能。

### 安全規約

- P3の機能は端末やエミュレーター依存が強いため、利用時は必ず `capabilities` を確認し、未対応環境では無効果とする。
- 画像・クリップボード・通知等は**セキュリティ上の影響**を考慮し、既定では無効。アプリケーションが明示的に有効化した場合のみ動作すること。

---

## プロファイル P3（画像・高度拡張）仕様

### 目的

- P2までの機能に加え、**画像描画**や**高度な端末拡張**（クリップボード、カーソル形状、詳細キーボード報告など）を扱う。
- 互換性の幅が大きく安全性にも影響するため、**既定は全無効**。能力で明示的に可否判定し、アプリからのオプトインでのみ有効化する。

### 画像描画

- API（抽象）：
  - `draw_image(handle, image, geom)`：画像を描画。`geom` は `{row,col,width,height,placement}` を含む。
  - `erase_image(handle, region)`：領域内の画像を消去。
- バックエンド写像（代表例）：
  - **Sixel**（DEC/Sixel対応端末）
  - **iTerm2 Image Protocol**
  - **kitty Graphics Protocol**
- ライブラリ実装では `terse_display_image_inline(handle, data, name)` が iTerm2 inline 形式を送出
- 縮退：
  - 未対応時は**無効果**。必要なら代替として **代替テキスト** を `write_text` によって表示可能（アプリ判断）。
- 能力：
  - `capabilities.images`：`{none, sixel, iterm2, kitty}`（複数可の場合は配列）
  - `capabilities.image_limits`：最大サイズ・ピクセル数・埋め込み数・透過サポート等。

### クリップボード

- API：
  - `clipboard_write(handle, selection, data)`：`selection={clipboard, primary}` 等（端末依存）。
  - `clipboard_read(handle, selection)`（**読み出しは未対応の端末が多い**）
  - ライブラリ実装では `terse_set_clipboard(handle, data)` が書き込みのみ対応
- バックエンド写像：**OSC 52**（Base64）など。tmux/screen下ではブロックされることがある。
- 縮退：
  - 未対応時は無効果。`clipboard_read` はエラーまたは未対応を返す。
- 能力：
  - `capabilities.clipboard`：`{none, osc52_write, osc52_rw}`

### カーソル形状／可視属性

- API：
  - `set_cursor(handle, shape, blink)`：`shape={block, underline, bar}`, `blink={true,false}`
- バックエンド写像：**DECSCUSR**（`CSI SP q`）。
- 縮退：未対応時は無効果。
- 能力：
  - `capabilities.cursor_shape`：`{true,false}`

### 詳細キーボード報告

- 目的：`Shift+Enter` 等、P0で区別不能だった修飾キー情報を取得。
- API：
  - `enable_keyboard_reporting(handle, kind)` / `disable_keyboard_reporting(handle)`
- バックエンド写像：
  - **xterm modifyOtherKeys**（MOK）
  - **kitty keyboard protocol**
  - **wezterm extensions**
- イベント：P0の `Enter` などに `mods` を追加で反映可能。
- 縮退：未対応時は通常のP0/P2相当のイベントに留まる。
- 能力：
  - `capabilities.keyboard_reporting`：`{none, xterm_mok, kitty, wezterm, other}`

### 通知・ベル・フラッシュ

- API：`notify(handle, kind, payload)`
  - `kind={bell, visual_bell, desktop_notification}`（対応端末・統合環境でのみ有効）
- 縮退：未対応時は無効果。
- 能力：`capabilities.notifications`：`{none, bell, visual, desktop}`（複合可）
- 実装想定（macOS）：
  - `bell` は BEL (`\x07`) 送出。
  - `visual_bell` は `CSI ?5h` → `CSI ?5l` で画面反転フラッシュを誘発。
  - `desktop_notification` は `OSC 9;1;{payload}` + BEL で通知センター連携を狙う。
- ペイロードは BEL (`\x07`)・ESC (`\x1b`) を含めないこと（制御列崩壊防止）。
- macOS版ライブラリは P3 の通知系オプションを明示オプトイン (`TERSE_CAP_ENABLE_NOTIFICATION_*`) した場合に限り上記制御列を送出する。

### tmux/screen 等の多重化対策

- `capabilities.multiplexer`：`{none, tmux, screen, other}` を追加。
- これらの環境では画像・OSC・キーボード拡張が**無効化**または**ラップ**されることがあるため、バックエンドは透過可否を能力で明示。
- kitty画像のパススルーやtmuxの `allow-passthrough` など、個別の緩和策は**実装依存**。

### 安全規約（強化）

- デフォルトは **全拡張無効**。API呼出時に能力を確認し、**アプリの明示的オプトイン**がある場合のみ有効化する。
- `set_hyperlink` と同様、画像やクリップボード操作は**注入・リークリスク**があるため、ライブラリ内でサイズ/転送量/回数の**上限**を設けることを推奨。
- `clipboard_read` は秘匿情報取得の可能性があるため、アプリ側の明示フラグ無しには許可しない。

### 初期状態

- 画像・クリップボード・詳細キーボード報告・通知はいずれも **無効**。
- 有効化後は `close()` 時に**確実に元へ戻す**こと（disable APIをペアで用意）。

### Human68k 特記事項

- 標準的なHuman68kテキスト環境ではP3機能は基本未対応。
- GUI拡張や専用端末/通信ソフトを介する場合のみ、実装依存で部分的に対応しうる（Sixel等）。

### テスト指針（概要）

- **ゴールデンテスト**：抽象操作 → バックエンドの制御列（Sixel/OSC/kitty）を記録し比較。
- **互換行列**：`images × multiplexer × terminal` の可否マトリクスを用意して期待縮退を固定化。
- **安全枠**：画像サイズ上限・Base64長の検証、無効化時の無効果確認。

---

## テスト指針

### 方針

- 抽象APIの安定性を最重視し、端末依存の差異はゴールデンテストと縮退規則で吸収する。
- 双方向（出力→制御列、制御列→入力）の往復可能性を確認する。

### テスト層と種別

- **単体テスト**：抽象API呼び出しに対し、期待される制御列が生成されるか。
- **逆方向テスト**：制御列入力から期待される抽象イベントが得られるか。
- **互換テスト**：
  - **プロファイル交渉**: 要求P3→実際P1等の適切な縮退
  - **アップグレード移行**: P0→P1→P2→P3の段階的移行テスト
  - **ダウングレード移行**: 状態保持とリセットの適切な動作
  - **機能縮退**: 各プロファイルで縮退が正しく働くか
  - **API互換性**: 下位プロファイルコードの上位環境実行
- **ゴールデンテスト**：抽象操作シナリオを用意し、出力シーケンスを固定。バックエンドごとに保持し比較。
- **入力正規化テスト**：代表的なキーシーケンスを抽象イベントに正規化できるか。
- **往復（ラウンドトリップ）テスト**：イベント→文字列→イベントで情報損失が規約通りであるか。
- **Codec・セル幅テスト**：UTF-8/Shift_JISでの幅判定と置換規則、CR/LF/CRLFの正規化。
- **リサイズ・安全終了テスト**：Resizeイベントとget_size()の整合、close()忘れ時の挙動。
- **安全試験**：画像/クリップボード/リンク等の無効時無効果、サイズ上限の検証。
- **性能テスト**：
  - **スループット**: 文字出力（目標: >1MB/s）、イベント処理（目標: >1000events/s）
  - **レイテンシ**: API応答時間（目標: <5ms）、バッファーフラッシュ（目標: <20ms）
  - **メモリ効率**: ピーク使用量（<344KB）、メモリリーク検出
  - **低速環境**: Human68k（68000 10MHz）での基本動作確認

### テストデータ形式

- 記述形式（例：YAML/JSON）：`profile`, `capabilities`, `codec`, `input_events` or `output_ops`, `expected_bytes` or `expected_events`。
- 差分許容：同等な複数列が存在する場合は正規表現または候補集合で許容。

### テスト環境

- 端末実機：iTerm2, Terminal.app, GNOME Terminal, WezTerm, kitty, Alacritty, tmux/screen下。
- Human68k実機/エミュレーター。
- CI：擬似端末（pty）とキャプチャモックで再現可能にする。

---

## 互換行列

### 概要

- プロファイルごとに端末環境での対応状況を一覧化し、実装/テストの基盤とする。

### 能力×端末マトリクス（例）

| 端末/環境      | colors        | mouse | paste | title | hyperlink | images      | keyboard_reporting | notes                |
| ---------- | ------------- | ----- | ----- | ----- | --------- | ----------- | ------------------- | -------------------- |
| Human68k   | none/limited  | none  | no    | no    | no        | none/sixel? | none                | 実装依存                 |
| POSIX VT   | truecolor     | sgr   | yes   | yes   | yes       | none/sixel  | mok                 |                      |
| iTerm2     | truecolor     | sgr   | yes   | yes   | yes       | iterm2      | xterm_mok          | tmux 下で制限            |
| kitty      | truecolor     | sgr   | yes   | yes   | yes       | kitty       | kitty               | 独自拡張有                |
| WezTerm    | truecolor     | sgr   | yes   | yes   | yes       | kitty       | xterm_mok          |                      |
| GNOME Term | truecolor     | sgr   | yes   | yes   | yes       | none        | none                |                      |
| Alacritty  | truecolor     | sgr   | yes   | yes   | yes       | none        | none                |                      |
| tmux       | passthrough依存 | 端末依存  | 端末依存  | 限定    | 限定        | 限定          | 限定                  | allow-passthroughで緩和 |

凡例：○=通常対応、△=一部対応/環境依存、×=未対応

### プロファイル移行マトリクス

| 端末環境 | P0→P1 | P1→P2 | P2→P3 | P3→P2 | P2→P1 | P1→P0 | 移行時注意事項 |
|---------|-------|-------|-------|-------|-------|-------|------------|
| **Human68k** | 色制限 | 大部分無効 | 全無効 | 安全 | 安全 | 安全 | ほとんどP0相当 |
| **POSIX VT** | 完全対応 | 完全対応 | 画像制限 | 画像消去 | マウス無効 | 色リセット | 標準的動作 |
| **iTerm2** | 完全対応 | 完全対応 | 完全対応 | 画像消去 | マウス無効 | 色リセット | 高機能環境 |
| **kitty** | 完全対応 | 完全対応 | 完全対応 | 画像消去 | マウス無効 | 色リセット | 最高機能 |
| **tmux下** | 端末依存 | 制限多 | 大部分無効 | 安全 | 安全 | 安全 | パススルー制限 |

### 移行安全性保証

| 移行方向 | 状態保持 | データ損失 | 復旧可能性 | リスク評価 |
|---------|----------|------------|------------|------------|
| **アップグレード** | 基本状態のみ | なし | 完全 | 低リスク |
| **ダウングレード** | 基本状態のみ | 拡張機能失効 | 部分的 | 中リスク |
| **同等移行** | 完全保持 | なし | 完全 | 最低リスク |
| **強制P0** | 位置のみ | スタイル全消失 | 最小限 | 高リスク |

### 縮退期待表

| 要求        | 期待縮退      | 備考          |
| --------- | --------- | ----------- |
| TrueColor | 256→16→無色 | 標準パレットに丸め   |
| SGRマウス    | x10       | 取得不可なら無イベント |
| ハイパーリンク   | 無効果       | 表示テキストのみ    |
| 画像(kitty) | 無効        | 代替テキスト表示    |

---

## エラー処理実装例（参考）

### 言語別エラー処理パターン

#### Rust
```rust
pub enum TerseError {
    Transport(io::Error),
    Protocol(String),
    InvalidArgument(String),
    Unsupported(String),
}

pub type Result<T> = std::result::Result<T, TerseError>;

impl Terminal {
    pub fn read_event(&mut self, timeout_ms: i32) -> Result<Option<Event>> {
        match self.transport.read_with_timeout(timeout_ms) {
            Ok(Some(bytes)) => Ok(Some(self.parse_event(bytes)?)),
            Ok(None) => Ok(None), // タイムアウト
            Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => {
                Err(TerseError::Transport(e))
            }
            Err(e) => Err(TerseError::Transport(e)),
        }
    }
}
```

#### Python
```python
class TerseError(Exception):
    """ベース例外クラス"""
    pass

class TransportError(TerseError):
    """Transport層エラー"""
    pass

class ProtocolError(TerseError):
    """プロトコルエラー"""
    pass

class Terminal:
    def read_event(self, timeout_ms: int) -> Optional[Event]:
        try:
            data = self.transport.read_with_timeout(timeout_ms)
            if data is None:
                return None  # タイムアウト
            return self.parse_event(data)
        except EOFError as e:
            raise TransportError("Connection closed") from e
        except OSError as e:
            raise TransportError(f"I/O error: {e}") from e
```

#### C
```c
typedef enum {
    TERSE_OK = 0,
    TERSE_ERROR_TRANSPORT = -1,
    TERSE_ERROR_PROTOCOL = -2,
    TERSE_ERROR_INVALID_ARG = -3,
    TERSE_ERROR_UNSUPPORTED = -4,
    TERSE_ERROR_EOF = -5,
} terse_error_t;

terse_error_t terse_read_event(terse_t* term, int timeout_ms, terse_event_t* event) {
    char buffer[BUFFER_SIZE];
    ssize_t n = read_with_timeout(term->fd, buffer, sizeof(buffer), timeout_ms);

    if (n == 0) {
        return TERSE_ERROR_EOF;
    } else if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return TERSE_OK;  // タイムアウト、event は NULL
        }
        return TERSE_ERROR_TRANSPORT;
    }

    return parse_event(buffer, n, event);
}
```

### 復旧処理実装例

#### 自動リトライ機能
```pseudo
function write_with_retry(data, max_retries=3) {
    for (attempt = 0; attempt < max_retries; attempt++) {
        try {
            return transport.write(data);
        } catch (TemporaryError e) {
            if (attempt == max_retries - 1) throw e;
            sleep(100ms * (attempt + 1));  // 指数バックオフ
        }
    }
}
```

#### 安全終了処理
```pseudo
function safe_close() {
    errors = [];

    // カーソル表示復帰（失敗しても継続）
    try {
        send_cursor_show();
    } catch (Error e) {
        errors.append(e);
    }

    // スタイルリセット（失敗しても継続）
    try {
        send_style_reset();
    } catch (Error e) {
        errors.append(e);
    }

    // 端末モード復帰（失敗しても継続）
    try {
        restore_terminal_mode();
    } catch (Error e) {
        errors.append(e);
    }

    // 最終的なエラー報告
    if (!errors.empty()) {
        log_warnings(errors);
        return PARTIAL_FAILURE;
    }
    return SUCCESS;
}
```

---

## パフォーマンス仕様

### バッファリング仕様

#### 出力バッファー
| パラメーター | 推奨値 | 最小値 | 最大値 | 備考 |
|------------|-------|-------|-------|------|
| **バッファーサイズ** | 8KB | 1KB | 64KB | 通常の画面操作に十分 |
| **フラッシュ閾値** | 4KB | 512B | 32KB | バッファーサイズの50% |
| **自動フラッシュ間隔** | 16ms | 1ms | 100ms | 60FPS相当の応答性 |
| **強制フラッシュタイムアウト** | 100ms | 10ms | 1000ms | 画面更新の応答性確保 |

#### フラッシュタイミング
以下の条件でバッファーを自動フラッシュ：
1. **バッファー満杯時**: フラッシュ閾値に達した場合
2. **タイマー駆動**: 最後の書き込みから自動フラッシュ間隔経過
3. **特定API呼び出し**: `flush()` の明示的呼び出し
4. **ブロッキング操作前**: `read_event()` 等の入力待ち前
5. **重要制御シーケンス**: カーソル移動、画面クリア等

#### 入力バッファー
| パラメーター | 推奨値 | 最小値 | 最大値 | 備考 |
|------------|-------|-------|-------|------|
| **受信バッファーサイズ** | 4KB | 256B | 16KB | マウス・ペースト対応 |
| **イベントキューサイズ** | 128イベント | 16イベント | 1024イベント | バースト入力対応 |
| **RawSequence最大長** | 1KB | 64B | 4KB | 未知制御シーケンス保持 |

### メモリ使用量仕様

#### 基本メモリ要件
| コンポーネント | 基本メモリ | 最大メモリ | 備考 |
|--------------|------------|------------|------|
| **ライブラリコア** | 32KB | 128KB | 状態管理・制御テーブル |
| **出力バッファー** | 8KB | 64KB | 設定可能 |
| **入力バッファー** | 4KB | 16KB | 受信・解析用 |
| **イベントキュー** | 16KB | 128KB | イベント × サイズ |
| **能力情報** | 4KB | 8KB | プロファイル・端末情報 |
| **総メモリ使用量** | **64KB** | **344KB** | 通常運用での上限 |

#### メモリ制限と制御
- **ハードリミット**: 総メモリ使用量が最大値を超えた場合はエラー
- **ソフトリミット**: 推奨値を超えた場合は警告ログ
- **動的調整**: 低メモリ環境では自動的にバッファーサイズを縮小
- **GC配慮**: GC言語では大きなオブジェクトの再利用を推奨

### レスポンス時間要件

#### 出力応答性
| 操作カテゴリ | 目標応答時間 | 最大許容時間 | 測定条件 |
|------------|-------------|-------------|----------|
| **文字出力** | < 1ms | < 5ms | 1KB文字列 |
| **カーソル移動** | < 0.5ms | < 2ms | 絶対・相対移動 |
| **画面クリア** | < 2ms | < 10ms | 全画面クリア |
| **スタイル変更** | < 0.5ms | < 2ms | 色・装飾変更 |
| **バッファーフラッシュ** | < 5ms | < 20ms | 8KB出力 |

#### 入力応答性
| 操作カテゴリ | 目標応答時間 | 最大許容時間 | 測定条件 |
|------------|-------------|-------------|----------|
| **キーイベント解析** | < 0.1ms | < 1ms | 基本キー入力 |
| **マウスイベント解析** | < 0.5ms | < 2ms | 座標・ボタン情報 |
| **リサイズ検出** | < 10ms | < 50ms | サイズ変更通知 |
| **ペーストイベント** | < 1ms/KB | < 5ms/KB | 大量テキスト |

#### 環境別性能目標
| 環境カテゴリ | CPU | メモリ | ディスク | 応答性目標 |
|------------|-----|-------|----------|----------|
| **Human68k** | 68000 10MHz | 2MB | フロッピー | 基本動作のみ |
| **組み込み** | ARM 100MHz | 16MB | Flash | 標準応答性 |
| **デスクトップ** | x86_64 1GHz+ | 512MB+ | SSD | 高応答性 |
| **サーバー** | 制限なし | 制限なし | 制限なし | 最高性能 |

### 性能最適化指針

#### 高速化技法
1. **バッチ処理**: 連続する同種操作をまとめて実行
2. **遅延評価**: 不要な計算を回避
3. **キャッシュ活用**: 端末能力・文字幅情報等
4. **ゼロコピー**: 可能な限り文字列コピーを回避
5. **プールリング**: 頻繁なメモリ確保を回避

#### 低リソース環境対応
- **段階的縮退**: メモリ不足時の機能制限
- **圧縮保存**: 履歴・ログの圧縮保存
- **遅延初期化**: 使用時まで重いリソースの初期化を延期
- **リソース解放**: 未使用時の積極的なリソース解放

### 性能測定・監視

#### 測定指標
- **スループット**: 文字/秒、イベント/秒
- **レイテンシ**: API呼び出しからバッファー送出まで
- **メモリ効率**: ピーク使用量、GC回数
- **CPU使用率**: プロファイル・最適化前後比較

#### 性能ベンチマーク
```pseudo
// 基本出力性能テスト
benchmark_output() {
    text = "Hello, World! 日本語テスト\n" * 1000;
    start_time = now();
    for (i = 0; i < 100; i++) {
        write_text(text);
    }
    flush();
    end_time = now();

    throughput = length(text) * 100 / (end_time - start_time);
    return throughput; // 文字/秒
}

// 入力処理性能テスト
benchmark_input() {
    events = simulate_key_sequence(1000); // 1000キーイベント
    start_time = now();

    for (event in events) {
        inject_raw_input(event.bytes);
        parsed = read_event(0); // ポーリング
        assert(parsed == event.expected);
    }

    end_time = now();
    return 1000 / (end_time - start_time); // イベント/秒
}
```

---

## 参考実装方針（非規範）

### レイヤー構成

- Transport：双方向バイトストリーム。
- Codec：UTF-8 / Shift_JISの復号・再符号化。
- Terminal Control：抽象API ↔ 制御列写像。
- Input Normalization：シーケンスをイベント化。未知はRawSequence。
- Screen Model（任意）：セルグリッド管理。

### バックエンド分岐

- `vt_ansi`（近代端末）、`human68k`、`multiplexer-aware`（tmux/screen）。
- 機能は `capabilities` に基づき分岐。テーブル駆動写像でテスト容易性を確保。

### 入力デコーダー設計

- 最長一致オートマトン（Trie/状態機械）でCSI/OSC/マウス等を正規化。
- タイムアウト時はRawSequenceにフォールバック。

### 性能・バッファリング

- 出力：リングバッファー + `flush()`。低速環境ではまとめ書きで効率化。
- 入力：イベントキュー（ロスレス）。Resizeはコアレッシング可。

### ロギングとデバッグ

- 可視化モード（制御列のエスケープ表示）とバイナリモードを切替可能。
- テストではキャプチャのハッシュと可視化を併用。

### Human68k 実装メモ

- Codec=Shift_JISを既定に。文字幅・サイズ取得を常に可能とする。
- 色・装飾・マウス等は未対応縮退を徹底。
- 画面モード切替フックを利用しResizeイベント生成。

---

## APIデザイン方針（ガイドライン）

- コアAPIはすべて`handle`を明示的に受け取り、再入可能性・並行性・複数端末同時制御・テスト容易性を担保する。
- 言語ラッパ（C++/Rust/Python等）ではインスタンスメソッド化して `handle` を隠蔽してもよい。
- 便宜的なスレッドローカルcurrentコンテキストAPI（例：`set_current(handle)`→`clear_screen(mode)`）を用意することは許容するが、
  - current未設定時は明確にエラーとすること、
  - マルチスレッド環境ではスレッドローカルであること、
  - コアの `...(..., handle, ...)` 版を必ず提供すること、
  を条件とする。

この方針に沿い、本仕様書の関数シグネチャは「handle明示渡し」に統一して記述する。
- **出力スタイル（SGR）**：`terse_style_t` に色・装飾を詰めて `terse_set_style()` を呼び出す。対応機能が無効な場合は無効果で成功扱い。
- **状態キャプチャ/復元**：`terse_capture_state()` と `terse_restore_state()` でカーソル位置・表示・スタイルを保存/適用できる。
