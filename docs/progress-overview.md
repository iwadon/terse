# Terse Implementation Progress Overview

## Summary
P0のライフサイクル・出力・入力・サイズ取得・エラー返却は実装済み。環境検出により Apple Terminal / GNOME Terminal (VTE) / iTerm2 / WezTerm / kitty / Ghostty / Warp を判別し、`TERSE_PROFILE_AUTO` で適切なプロファイル能力へ自動縮退/上限クリップ。ランタイムでも `terse_capabilities_enable/disable/reset_overrides` により能力の明示上書きが可能。さらにP1（色・装飾）、P2（マウス/ブランケットペースト/タイトル・リンク）、P3（クリップボード/画像/カーソル形状/通知）の主要APIを実装し、ユニットテストとサンプルで検証済み。

入出力Codecは `UTF-8` / `Shift_JIS` の選択に対応し、多バイト復号・East Asian Width に基づくセル幅推定（結合文字=幅0/全角=幅2）を実装済み。入力正規化はASCII/制御/矢印/Resize/一部修飾に限定され、機能キー群や複合グラフェムの追加対応は今後実施予定。

## Build Options

- `-DTERSE_USE_SYSTEM_ICONV=OFF` で外部 iconv 非依存ビルドが可能に。内蔵の mini iconv が `Shift_JIS` ⇔ `UTF-8` 相互変換を提供する（背景・設計は `docs/mini-iconv-plan.md` を参照）。

## Progress Matrix

| 機能領域 | 対応プロファイル | ステータス | 現状メモ |
| --- | --- | --- | --- |
| ライフサイクル (`open`/`close`) | P0 | ✅ 実装済み | オプション検証・初期状態（カーソル表示/SGRリセット）・終了時の安全復帰（マウス/ペースト解除, カーソル表示, SGR 0）。|
| 能力検出/取得 (`get_capabilities`) | P0-P3 | ✅ 実装済み | 環境検出＋リクエストでクリップ（P0〜P3）。ランタイム enable/disable/リセットで上書き可。色/Effetcs/マウス/ペースト/タイトル/リンク/画像/通知/カーソル形状などを包含。|
| 出力制御 (`clear_*`, `move_*`, `show_cursor`, `write_text`, `flush`) | P0 | ✅ 実装済み | ANSI送出・ノーオペ縮退・重複出力抑止（同位置 `move_to`/同可視状態 `show_cursor`）。`flush` はNo-op。|
| 入力 (`read_event`) | P0 | ✅ 実装済み（部分） | ASCII/Enter/Backspace/Tab/矢印（修飾付）/Resize/ブランケットペースト/SGRマウスを正規化。未対応：多バイト復号・機能キー全面。|
| 端末サイズ (`get_size`) | P0 | ✅ 実装済み | `TIOCGWINSZ` ベース。`CSI 8 ; r ; c t` 受信で内部サイズ更新。無効化時は Unknown を返却。|
| 状態管理（キャプチャ/復元・一時保存） | P0 | ✅ 実装済み | `capture/restore_state` と `push/pop_state` を提供。カーソル位置/可視/スタイルを安全に復元。|
| 環境検出・プロファイル自動判定 | P0-P3 | ✅ 実装済み | Apple Terminal / VTE / iTerm2 / WezTerm / kitty / Ghostty / Warp を判別（環境変数＋Secondary DA）。|
| エラー分類/返却 | P0 | ✅ 実装済み（基本） | `terse_error_t` enum 返却と `terse_get_last_error()`。Argument/State/I/O/Protocol/Resource/Encoding を範囲別に区別（1-99, 100-199, 200-299, 300-399, 400-499, 500-599）。|
| P1: 色・装飾（SGR） | P1 | ✅ 実装済み | `set_style/reset_style` 完了。TrueColor→256→16→既定色への縮退ロジックと効果ビット（Bold/Italic/Underline/…）。|
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

## Next Steps Snapshot
- 追加Codec（例：ISO-2022-JP）検討。
- 入力正規化の拡充：機能キー/Home/End/Page/Function(n) ほか、修飾一貫性の強化、タイムアウト/合成のチューニング。
- 環境検出の強化：tmux/screen配下や追加端末のSecondary DAマッピング、VISUAL通知サポート検出の拡充。
- 画像/クリップボードの互換拡大（iTerm2以外のSixel/kitty graphics対応を含む）と能力表の詳細化。設計ノートは `docs/graphics-roadmap.md` を参照。
- ドキュメントの仕様整備とサンプルの拡充（P2/P3機能別の最小コード例）。
- kitty キーボードプロトコルは公式仕様通り `CSI > 1 u` / `CSI < u` を送出（現実装で整合済み）。実機挙動の確認が必要になった際のみ追加検証を行う。
