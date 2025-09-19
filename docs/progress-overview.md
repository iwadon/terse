# Terse Implementation Progress Overview

## Summary
P0のライフサイクル・出力・入力が一通り整い、ANSIシーケンス送出・イベント正規化・`-errno`返却・`get_size` まで実装済み。オプションでは機能無効化フラグとアクセサを備え、縮退パスも確認済み。最新版では Apple Terminal / GNOME Terminal / iTerm2 / WezTerm / kitty / Ghostty / Warp を環境シグネチャで判別し、`TERSE_PROFILE_AUTO` によって対応プロファイルへ自動移行できるようになった。さらに `terse_capabilities_enable/disable/reset_overrides` で検出結果を後から上書き可能。残作業は状態保持や上位プロファイル機能の実装拡張。以下の表で仕様領域ごとの状態を俯瞰できます。

## Progress Matrix

| 機能領域 | 対応プロファイル | ステータス | 現状メモ |
| --- | --- | --- | --- |
| ライフサイクル (`open`/`close`) | P0 | ✅ 実装済み | オプション受け取りとP0縮退を含む。リソース開放のみ。 |
| 能力取得 (`get_capabilities`) | P0 | ✅ 実装済み | P0能力に加え、P1以降向けフィールド（SGR/マウス等）をプレースホルダーとして公開。 |
| 出力制御 (`clear_*`, `move_*`, `show_cursor`, `write_text`, `flush`) | P0 | ✅ 実装済み | ANSI/UTF-8送出・`-errno`返却・機能無効化時のノーオペ対応済み。`flush` はハンドル検証のみ。 |
| 入力 (`read_event`) | P0 | ✅ 実装済み | ASCII/改行/矢印/リサイズ・修飾キーを正規化。タイムアウトは `TERSE_EVENT_NONE`。 |
| 端末サイズ (`get_size`) | P0 | ✅ 実装済み | `TIOCGWINSZ` と Resize イベントを同期。無効化フラグで Unknown を強制可能。 |
| 環境検出・プロファイル自動判定 | P0-P3 | ⚠️ 部分 | Apple Terminal, VTE (GNOME Terminal), iTerm2, WezTerm, kitty, Ghostty, Warp を判別し `TERSE_PROFILE_AUTO` から P1〜P3 capability に自動移行。API での後付け enable/disable をサポートしたので細かな上書きが可能。Warp 以外の未対応端末や追加検証が残課題。 |
| エラー分類と縮退ハンドリング | P0 | ⚠️ 部分 | `-errno` 返却と機能フラグによる縮退は完了。状態復旧や詳細エラー分類は今後。 |
| 内部状態管理 (カーソル/スタイル保持) | P0 | ⚠️ 部分 | `terse_state_override` / `terse_state_clear` を追加し、能力が無い環境でもカーソル位置・表示・スタイルを追跡可能に。今後は履歴保持や更なる状態復元を検討。 |
| オプション/設定拡張 | 全般 | ✅ 実装済み | `disabled_caps` とバリデーション/アクセサを提供。追加設定は今後検討。 |
| テスト: ライフサイクル | P0 | ✅ 実装済み | オープン成功/失敗と縮退挙動を確認。 |
| テスト: 出力 | P0 | ✅ 実装済み | パイプでシーケンスと異常系を検証。 |
| テスト: 入力/エラー | P0 | ✅ 実装済み | 文字/修飾/リサイズ/EOF/引数エラーをカバー。 |
| 上位プロファイル (P1-P3) | P1-P3 | ⏳ 未着手 | 仕様定義のみ。実装/テストともに未着手。 |

## Next Steps Snapshot
- 状態保持API（カーソル位置・スタイル・他の復帰処理）をP0仕様に沿って設計実装。
- 拡張エラー分類（Transport/Protocol等）と状態復旧フローをコード／ドキュメント化。
- P1以降に向けた能力フィールドの拡張とサンプルアプリ／ドキュメントの整備。
- 追加端末（Warp、tmux 内座標差分など）の DA 収集とマッピング拡張。
