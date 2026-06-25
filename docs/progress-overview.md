# Terse Implementation Progress Overview

## Summary
P0のライフサイクル・出力・入力・サイズ取得・エラー返却は実装済み。環境検出により Apple Terminal / GNOME Terminal (VTE) / iTerm2 / WezTerm / kitty / Ghostty / Warp を判別し、`TERSE_PROFILE_AUTO` で適切なプロファイル能力へ自動縮退/上限クリップ。ランタイムでも `terse_capabilities_enable/disable/reset_overrides` により能力の明示上書きが可能。さらにP1（色・装飾）、P2（マウス/ブランケットペースト/タイトル・リンク）、P3（クリップボード/画像/カーソル形状/通知）の主要APIを実装し、ユニットテストとサンプルで検証済み。

入出力Codecは `UTF-8` / `Shift_JIS` の選択に対応し、多バイト復号・East Asian Width に基づくセル幅推定（結合文字=幅0/全角=幅2）を実装済み。入力正規化はASCII/制御/矢印/Resize/一部修飾に限定され、機能キー群や複合グラフェムの追加対応は今後実施予定。

## Build Options

- `-DTERSE_USE_SYSTEM_ICONV=OFF` で外部 iconv 非依存ビルドが可能に。内蔵の mini iconv が `Shift_JIS` ⇔ `UTF-8` 相互変換を提供する（背景・設計は `docs/mini-iconv-plan.md` を参照）。
- `terse_convert_encoding()`: ハンドル不要のスタンドアロンエンコーディング変換 API。UTF-8 ⇔ Shift_JIS は全プラットフォームで保証、その他はシステム iconv 利用時のみ対応。

## Progress Matrix

| 機能領域 | 対応プロファイル | ステータス | 現状メモ |
| --- | --- | --- | --- |
| ライフサイクル (`open`/`close`) | P0 | ✅ 実装済み | オプション検証・初期状態（カーソル表示/SGRリセット）・終了時の安全復帰（マウス/ペースト解除, カーソル表示, SGR 0）。|
| 能力検出/取得 (`get_capabilities`) | P0-P3 | ✅ 実装済み | 環境検出＋リクエストでクリップ（P0〜P3）。ランタイム enable/disable/リセットで上書き可。色/Effetcs/マウス/ペースト/タイトル/リンク/画像/通知/カーソル形状などを包含。|
| 能力要求 (`require`/`caps_missing`/`get_active_features`) | P0-P3 | ✅ 実装済み | `TERSE_FEAT_*` ビットマスクで必要機能を宣言し不足分を検査（副作用なし）。色段階は「以上」で充足。|
| 出力制御 (`clear_*`, `move_*`, `show_cursor`, `write_text`, `flush`) | P0 | ✅ 実装済み | ANSI送出・ノーオペ縮退・重複出力抑止（同位置 `move_to`/同可視状態 `show_cursor`）。即時モードでは `flush` はNo-op、バッファドモードでは差分出力（下記参照）。|
| バッファドモード（セル仮想バッファ＋差分描画） | P0+ | ✅ 実装済み | `terse_options_t.render_mode = TERSE_RENDER_BUFFERED` で opt-in。ダブルバッファ＋ダーティセル diff により `flush` 時に変更セルだけを `move_to`/SGR/テキストで出力（色・装飾・全角continuation対応）。既定は即時モードで振る舞い不変。|
| 矩形バッファ一般化（任意 origin の仮想画面） | P0+ | ✅ 実装済み | バッファドモードを端末上の任意矩形 `(origin, rows, cols)` に対応する仮想画面へ一般化。バッファ内座標はローカル(0起点)、`flush` で `絶対 = origin + local` に射影。`terse_buffer_set_region()` で origin 移動・サイズ変更（内容非保持・次 flush で全再描画）。`terse_options_t.buffer_origin_*/buffer_rows/buffer_cols`（全0=端末全体・既定）で初期矩形指定。origin 移動・縮小時は前回矩形の残骸を `flush` が自動消去（terse 自身が出したセルのみ）。`terse_get_cell()` で表示中の確定内容を読み出し。`terse_buffer_set_cursor()` で flush 末尾のカーソル位置をローカル座標指定（入力キャレット等の論理位置にカーソルを残す用途。バッファ寸法外も許容）。`terse_buffer_invalidate()` で前フレーム記録を破棄し次 flush を全再描画（スクロール等で矩形下の端末が terse 不知のうちに変わった場合）。`terse_buffer_forget_previous_rect()` で前回矩形の位置・サイズ記録を破棄し次 flush の残骸消去をスキップ（readline 終了後など、旧矩形領域にアプリケーション出力が書かれた場合に使用）。`terse_buffer_detach()` でバッファド描画を一時停止（現在の矩形を画面に残したまま、残骸消去と差分出力の両方を抑止。readline 終了後にアプリに制御を返し、後で再開するユースケース向け）。`terse_write_raw()` でモード無関係に生バイトを即時出力（矩形外の消去・改行など利用側の能動的端末操作のエスケープハッチ）。リサイズ追従は利用側ポーリング（`terse_get_size()`→`terse_buffer_set_region()`）方式。|
| 代替スクリーン（DEC private mode 1049） | P2+ | ✅ 実装済み | `terse_enter/leave_alt_screen`（低レベルAPI）と `terse_options_t.use_alt_screen`（open時自動進入/close時自動退出）。`has_alt_screen` ケイパビリティはP2+で真。非対応端末ではノーオペ。|
| 入力 (`read_event`) | P0 | ✅ 実装済み（部分） | ASCII/Enter/Backspace/Tab/矢印（修飾付）/Resize/ブランケットペースト/SGRマウスを正規化。未対応：多バイト復号・機能キー全面。|
| 端末サイズ (`get_size`) | P0 | ✅ 実装済み | `TIOCGWINSZ` ベース。`CSI 8 ; r ; c t` 受信で内部サイズ更新。無効化時は Unknown を返却。|
| 状態管理（キャプチャ/復元・一時保存） | P0 | ✅ 実装済み | `capture/restore_state` と `push/pop_state` を提供。カーソル位置/可視/スタイルを安全に復元。|
| 環境検出・プロファイル自動判定 | P0-P3 | ✅ 実装済み | Apple Terminal / VTE / iTerm2 / WezTerm / kitty / Ghostty / Warp を判別（環境変数＋Secondary DA）。|
| エラー分類/返却 | P0 | ✅ 実装済み（基本） | `terse_error_t` enum 返却と `terse_get_last_error()`。Argument/State/I/O/Protocol/Resource/Encoding を範囲別に区別（1-99, 100-199, 200-299, 300-399, 400-499, 500-599）。|
| P1: 色・装飾（SGR） | P1 | ✅ 実装済み | `set_style/reset_style` 完了。TrueColor→256→16→4色→既定色への縮退ロジックと効果ビット（Bold/Italic/Underline/…）。色サポートは5段階（NONE/BASIC4/BASIC16/PALETTE256/TRUECOLOR）。4色はHuman68kテキスト画面（黒/シアン/黄/白、実機パレット初期値）に対応。|
| P2: マウス追跡 | P2 | ✅ 実装済み | `enable/disable_mouse`（X10/VT200/SGR）。SGRマウスの Down/Move/Up/Scroll と修飾を正規化。|
| P2: ブランケットペースト | P2 | ✅ 実装済み | `enable/disable_bracketed_paste` と `PasteBegin/End` イベント。|
| P2: タイトル/リンク | P2 | ✅ 実装済み | `OSC 0` タイトル、`OSC 8` ハイパーリンク。妥当性最低限チェック。|
| P3: クリップボード書込 | P3 | ✅ 実装済み | `OSC 52` Base64。能力無効時はノーオペ。|
| P3: 画像（iTerm inline） | P3 | ✅ 実装済み | `OSC 1337;File=...;inline=1:` 送出。名称/データBase64化。|
| P3: カーソル形状 | P3 | ✅ 実装済み | `DECSCUSR`（ブロック/下線/バー＋点滅）切替。|
| P3: 通知 | P3 | ✅ 実装済み | ベル（BEL）/視覚（DECSCNMトグル）/デスクトップ（`OSC 9;1;...`）。環境検出でBELL/デスクトップを付与。|
| サンプル | P0-P3 | ✅ 実装済み | `samples/` に P0〜P3 デモ（色/スタイル/拡張機能/通知等）。|
| Codec/セル幅 | P0 | ✅ 実装済み | UTF-8/Shift_JIS 変換、復号エラー時の置換、East Asian Width に基づく `Char.width` 推定とテスト完了。East Asian Ambiguous Width 文字の表示幅オプション（`east_asian_ambiguous_as_wide`）を実装。|
| キーボード拡張 (modifyOtherKeys/kitty) | P0 | ✅ 実装済み | `terse_keyboard_enable/disable` で modifyOtherKeys level 2 および kitty CSI-u を opt-in（対応端末のみ）。|
| 拡張キーレポート | P2+ | ⏳ 未着手 | `Shift+Enter` などの詳細修飾検出（xterm MOK, kitty 等）の検出/抽象化は今後。|
| テストモード (API記録・モック) | - | ✅ 実装済み | `TERSE_ENABLE_TEST_MODE` ビルドオプションで有効化。API呼び出し記録（write_text/move_to/clear/set_style等）、能力・サイズ・イベントのモック機能を提供。`samples/test_mode_demo.c` 参照。|
| テスト戦略 (ゴールデン/PTY) | - | ✅ 基盤導入済み | 出力バイト列ゴールデンテスト基盤（`tests/golden_helpers.h`、ハイブリッド更新フロー `UPDATE_GOLDEN=1`）と PTY 実プロセステスト基盤（`tests/pty_helpers.h`、POSIX 限定）。代表シナリオで実証済み。既存テストの段階移行・端末別スナップショット網羅は今後（Phase 6 計画書 `docs/redesign-phase6-plan.md` 参照）。|
| POSIX 拡張ヘッダ (`<terse/posix.h>`) | - | ✅ 実装済み | `terse_posix_get_input_fd()` で端末入力 fd を露出。利用側の poll/epoll/kqueue ループに統合できる。コア API（`terse_read_event`）は OS 非依存のまま維持。POSIX 限定（非 POSIX は `#error`）。Windows 拡張は後追い（Phase 7 計画書 `docs/redesign-phase7-plan.md` 参照）。|

## Next Steps Snapshot
- バッファドモードの本格活用：terse-prompt の描画パスを `TERSE_RENDER_BUFFERED` 経由へ移行（Phase 5.5、別リポジトリ）。スクロール領域抽象化は必要が出てから検討。
- 追加Codec（例：ISO-2022-JP）検討。
- 入力正規化の拡充：機能キー/Home/End/Page/Function(n) ほか、修飾一貫性の強化、タイムアウト/合成のチューニング。
- 環境検出の強化：tmux/screen配下や追加端末のSecondary DAマッピング、VISUAL通知サポート検出の拡充。
- 画像/クリップボードの互換拡大（iTerm2以外のSixel/kitty graphics対応を含む）と能力表の詳細化。設計ノートは `docs/graphics-roadmap.md` を参照。
- ドキュメントの仕様整備とサンプルの拡充（P2/P3機能別の最小コード例）。
- kitty キーボードプロトコルは公式仕様通り `CSI > 1 u` / `CSI < u` を送出（現実装で整合済み）。実機挙動の確認が必要になった際のみ追加検証を行う。
