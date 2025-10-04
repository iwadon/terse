# Terse API ガイド（アプリケーション開発者向け）

Terse は POSIX 端末をはじめとするテキスト UI 環境で、描画・入力・能力検出を一貫して扱うための C ライブラリです。本書では、`terse.h` が公開する API の使い方を概観し、典型的な制御フローと、エラー処理・移植性に関する注意点をまとめます。詳細な仕様は `docs/terse-specs.md` を参照してください。

## 1. 基本概念

- **ハンドル (`terse_handle_t`)**: 端末とのセッション状態を保持する不透明ポインタ。`terse_open` で生成し、`terse_close` で破棄します。
- **プロファイル (`terse_profile_t`)**: 端末機能を段階的に整理したターゲット水準。`TERSE_PROFILE_AUTO` を指定すると Terse が環境を診断し、自動で最適化と縮退を行います。
- **能力 (`terse_capabilities_t`)**: カーソル移動、色、マウス等の機能有無を示す構造体。検出結果の確認や、ランタイムでの有効・無効化に利用します。
- **状態 (`terse_state_t`)**: カーソル位置やスタイル等の現在値。プッシュ/ポップ API で簡易的に退避・復帰が可能です。

## 2. 初期化とクリーンアップ

```c
terse_options_t options = {
    .input_fd = STDIN_FILENO,
    .output_fd = STDOUT_FILENO,
    .codec_name = "UTF-8",
};
terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
if (!handle) {
    perror("terse_open");
    return 1;
}

/* ... 利用 ... */

terse_close(handle);
```

- `terse_open` は `NULL` を返す可能性があります。errno が保持されるため、`perror` 等で診断できます。
- `terse_validate_options` で事前検証が可能です。`terse_get_options` を使えば、実際に使用中のファイルディスクリプタや設定を取得できます。
- `terse_close` は能力を可能な限り端末初期状態に戻し、内部リソースを解放します。ハンドルは 1 度だけ閉じてください。

## 3. 能力検出とランタイム制御

```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (!caps.has_basic_output) {
    /* 最低限の描画も不可なので処理を中断するなど */
}
```

- `terse_capabilities_enable` / `terse_capabilities_disable` で能力を強制的に上書き可能です（例: 色付き出力を明示的に禁止する等）。
- マスク値は `TERSE_CAP_ENABLE_*`／`TERSE_CAP_DISABLE_*` を組み合わせます。`terse_capabilities_reset_overrides` で検出結果に戻せます。

## 4. 描画系 API

| 機能 | 主な関数 |
| --- | --- |
| カーソル操作 | `terse_move_to`, `terse_move_by`, `terse_show_cursor` |
| 消去 | `terse_clear_screen`, `terse_clear_line` |
| 文字列出力 | `terse_write_text`（Shift_JIS 変換を含む） |
| 画像表示 | `terse_display_image`（新API） / `terse_display_image_inline`（互換） |
| フラッシュ | `terse_flush`（必要に応じバッファ送出） |
| スタイル設定 | `terse_set_style`, `terse_reset_style`, `terse_style_default` |
| 色ユーティリティ | `terse_color_default`, `terse_color_basic`, `terse_color_palette`, `terse_color_truecolor` |

スタイルを適用する際は、現在の能力に応じて自動縮退が行われます。`terse_set_style` は `TERSE_ERROR_TRANSPORT` 等を返すことがあるため、戻り値を必ず確認してください。

### 4.1 画像表示

`terse_display_image(handle, const terse_image_request_t *request)` を使うと、利用可能な画像プロトコル（現状は iTerm2 inline を使用、Sixel/kitty は順次対応予定）に基づいて最適な送出が行われます。`request->flags` を省略（0）すると安全なノーオペ縮退が既定で有効になります。既存の `terse_display_image_inline` は互換用ラッパーであり、内部的に新APIを呼び出します。

### 4.2 状態スタック

一時的にカーソルやスタイルを変更する場合は、`terse_push_state` / `terse_pop_state` を利用します。スタック上限 (`TERSE_STATE_STACK_MAX`) を超えると `-EINVAL` で失敗し、`terse_get_last_error` から `TERSE_ERROR_STACK_OVERFLOW` が取得できます。

## 5. 入力とイベント処理

`terse_read_event(handle, timeout_ms, &event)` は端末からの入力を解析し、`terse_event_t` として返します。

- `timeout_ms` に 0 を指定するとノンブロッキング、負数で待機無制限です。
- 戻り値が `TERSE_EVENT_NONE` の場合はタイムアウト、負数の場合は `-errno` です。
- `event.type` に応じて、文字 (`TERSE_EVENT_CHAR`)、特殊キー、マウス、貼り付け、ウィンドウサイズ等を区別します。
- 低レベルの未解析列は `TERSE_EVENT_RAW_SEQUENCE` として受け取れます。

マウスやブランケットペースト等を利用する場合は、事前に `terse_enable_mouse`, `terse_enable_bracketed_paste` を呼び出してください。対応状況は `caps.mouse` や `caps.has_bracketed_paste` で確認可能です。

## 6. キーボード拡張

```c
unsigned int supported = terse_keyboard_get_supported(handle);
if (supported & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
    terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL);
}
```

- 利用可能な機能は `terse_keyboard_get_supported`、現在有効な機能は `terse_keyboard_get_enabled` で確認できます。
- `terse_keyboard_enable/disable` は一部端末では失敗するため、戻り値のチェックと `terse_get_last_error` の参照が必要です。

## 7. 端末付加機能

| 能力 | 関数 | 備考 |
| --- | --- | --- |
| ブランケットペースト | `terse_enable_bracketed_paste`, `terse_disable_bracketed_paste` | `caps.has_bracketed_paste` が必要 |
| ウィンドウタイトル | `terse_set_title` | `TERSE_CAP_DISABLE_TITLE` を避ける |
| ハイパーリンク | `terse_set_hyperlink` | 対応端末のみ描画 |
| カーソル形状 | `terse_set_cursor_shape` | ブリンク有無を指定可能 |
| クリップボード書き込み | `terse_set_clipboard` | iTerm2 など一部実装 |
| インライン画像 | `terse_display_image_inline` | iTerm2/kitty 系列 |
| 通知 (ベル/ビジュアル/デスクトップ) | `terse_notify` | `caps.notifications` を参照 |

## 8. エラーハンドリング

直近のエラーは `terse_get_last_error` で取得できます。

```c
terse_error_info_t info = terse_get_last_error(handle);
if (info.category != TERSE_ERROR_NONE) {
    fprintf(stderr, "terse error: category=%d code=%d\n", info.category, info.code);
}
```

- `category` は論理的な分類、`code` は `errno` 相当の詳細コードです。
- 高頻度で呼び出す API は成功時にエラー情報をクリアします。

## 9. ベストプラクティス

1. **必ず戻り値を検査する**: 端末との I/O は失敗し得ます。負値の場合は `-errno` です。
2. **検出情報をキャッシュする**: `terse_get_capabilities` は軽量ですが、上位層での条件分岐を共通化すると可読性が上がります。
3. **非同期入力には select/poll 相当を併用**: `terse_read_event` のタイムアウトは単純待機なので、大規模アプリでは高水準イベントループに組み込む設計が必要です。
4. **UTF-8 以外を使う場合は iconv の有無を確認**: `cmake -DTERSE_ENABLE_ICONV=ON` でシステム iconv を利用します。未サポート環境では `mini_iconv` にフォールバックします。
5. **終了時に状態復元を徹底する**: カーソル非表示やマウス有効化を放置すると、端末の操作性に影響します。

## 10. 参考資料

- `docs/terse-specs.md`: プロファイル定義と縮退ルール
- `docs/progress-overview.md`: 実装済み能力の進捗
- `samples/p0_demo.c`: 代表的な API 利用例

Terse の API は、端末固有機能が不足する場合に自動縮退し、利用者コードの複雑さを抑えるよう設計されています。本ガイドを活用し、シンプルかつ堅牢なテキスト UI を構築してください。
