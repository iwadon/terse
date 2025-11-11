# Terse プラットフォーム移植ガイド

本書は、`terse` ライブラリを POSIX 以外の環境（Windows, Human68k など）へ移植する際に実装すべきプラットフォーム層 API と、設計上の制約をまとめたものです。アプリケーション開発者向けの API 説明は `docs/terse-api-user.md` を参照してください。

## 1. 構造の概要

`c/src/terse.c` は、端末制御のコアロジックを保持する移植性の高いモジュールです。入出力・端末設定・イベント待機といった OS 依存部分は `terse_platform.h` で抽象化され、現在は以下の実装が提供されています。

| ファイル | 対象 | 備考 |
| --- | --- | --- |
| `c/src/terse_posix.c` | Unix/POSIX | `termios`, `poll`, `ioctl` を使用 |
| `c/src/terse_platform_stub.c` | 非 POSIX（仮実装） | `ENOTSUP` を返し、移植ポイントを明示 |

新しいプラットフォームをサポートする場合は、`terse_platform_stub.c` を置き換えるか、対応するビルド条件を追加して専用ソースを組み込みます。

## 2. 必須エントリポイント

`terse_platform.h` で宣言される各関数は以下の責務を持ちます。

### 2.1 `terse_platform_default_options`

- 既定の入出力ハンドル・エンコーディングを返します。
- POSIX 実装では `stdin/stdout` を前提にしていますが、GUI ターミナル等が標準入出力と独立している環境では、適切なデバイス識別子を提供する必要があります。

### 2.2 `terse_platform_query_fd_size`

- 画面サイズ（行/列）を問い合わせ、取得できた場合は `known=1` に設定します。
- Human68k など、画面サイズ API が非同期の場合はキャッシュ戦略を検討してください。

### 2.3 `terse_platform_probe_secondary_da`

- 端末の Secondary Device Attributes (DA2) をリクエストし、応答を `buffer` に格納します。
- 端末が非同期応答する場合に備え、200ms 程度でタイムアウトする設計になっています。必要に応じ調整できます。
- 要求文字列（`ESC [ > 0 c`）を出力し、端末モードを変更した場合は確実に元へ戻してください。

### 2.4 `terse_platform_wait_for_input`

- 入力が到達するまで `timeout_ms`（ms）待機します。負数は無期限待機、0 は即時判定です。
- 返り値は「`TERSE_OK`=タイムアウト、正=入力あり、`terse_error_t`=エラーコード」です。
- `select`/`poll` 相当機構がない場合、ビジーループは避け、OS が提供するイベント待機 API に委譲してください。

### 2.5 `terse_platform_read_byte`

- 1 バイト読み取り、EINTR 等は内部でリトライします。
- 非ブロッキングモードのデバイスでは、入力待機 (`terse_platform_wait_for_input`) を組み合わせて利用する前提です。

### 2.6 `terse_platform_drain_escape_sequence`

- すでに `buffer[0] == ESC` が格納された状態から開始し、CSI 系列等の終端（`@`〜`~`）まで読み進めます。
- `TERSE_EVENT_RAW_MAX` を超えない範囲で読み込み、タイムアウトになったら読み込みを中断します。

### 2.7 `terse_platform_write_bytes`

- 指定バイト列を完全に書き込みます。短い書き込みや EINTR を内部で再試行してください。
- 出力デバイスがバッファリングを持つ場合、`terse_flush` が期待通り動作するよう考慮が必要です。

## 3. エラーハンドリング規約

- 失敗時は **必ず `terse_error_t` 列挙型の適切なエラーコード** を返します。`terse.c` 側はこの規約に依存しています。
- サポート対象外機能は `TERSE_ERR_UNSUPPORTED`/`TERSE_ERR_INVALID_ARGUMENT` を用い、呼び出し元が適切に縮退できるようにします。
- タイムアウト (`wait_for_input` 他) では `TERSE_OK` を返すのが既定です。
- エラーコードの範囲: 引数エラー (1-99)、状態エラー (100-199)、I/Oエラー (200-299)、プロトコルエラー (300-399)、リソースエラー (400-499)、エンコーディングエラー (500-599)

## 4. ビルド統合

CMake では `UNIX` 判定で POSIX 実装を組み込んでいます。新プラットフォームを追加するには、下記のように条件分岐を拡張してください。

```cmake
if(WIN32)
  target_sources(terse PRIVATE src/terse_windows.c)
elseif(UNIX)
  target_sources(terse PRIVATE src/terse_posix.c)
else()
  target_sources(terse PRIVATE src/terse_platform_stub.c)
endif()
```

プラットフォーム固有の依存ライブラリ（例: Win32 API, Human68k BIOS 呼び出し）が必要な場合は、`target_link_libraries` や `target_compile_definitions` で追加します。

## 5. テスト戦略

1. **ユニットテスト再利用**: `ctest --test-dir build --output-on-failure` を新環境でも実行できるようにするのが理想です。必要ならば端末 API をモックするバックエンドを提供します。
2. **端末の実測検証**: 実機で `samples/p0_demo.c` を動かし、カーソル移動・色・入力が正しく動作するか確認してください。
3. **プロファイル検出の調整**: 新環境では `detect_environment_capabilities` が想定外の環境変数を読む可能性があります。必要に応じて条件分岐を追加し、`TERSE_PROFILE_AUTO` の挙動を安定化させます。

## 6. よくある課題と対策

| 課題 | 典型原因 | 対策 |
| --- | --- | --- |
| `terse_read_event` が常に `TERSE_ERR_UNSUPPORTED` を返す | `terse_platform_wait_for_input` / `read_byte` 未実装 | 該当関数を完全実装し、適切な `terse_error_t` を返却 |
| 端末終了後にカーソルが戻らない | `terse_platform_write_bytes` が部分書き込みを無視 | ループで全量書き込みを保証 |
| サイズ取得が `known=0` のまま | `terse_platform_query_fd_size` 未対応 | OS のコンソール API やウィンドウマネージャ API を利用 |
| DA2 応答が欠落する | 非同期読み取りが不足 | タイムアウトやリトライを調整し、必要なら OS イベントで待機 |

## 7. 今後の拡張指針

- **コードページ変換**: `terse_write_text` は Shift_JIS 変換を内部で扱います。非 POSIX 環境では iconv 以外の変換 API を採用する場合があるため、`initialize_codec_handles` 周辺の互換性を検討してください。
- **非同期入出力**: Windows の `ReadConsoleInputW` 等を利用する際は、文字列解析コードとの整合性を保つため、UTF-8 への正規化を行ってから `terse_event_t` へ変換します。
- **ビルドオプション**: CMake フラグで実装有無を切り替えられるようにすると、CI で複数プラットフォームを並行検証しやすくなります。

---

Terse のプラットフォーム層は薄いラッパーである一方、端末振る舞いの再現性を左右する重要なパーツです。本ガイドを踏まえ、対象 OS の API に沿った堅牢な実装を行ってください。実装が揃ったら、ユニットテストとサンプル実行でフィードバックを収集し、`docs/progress-overview.md` 等に実績を追記することを推奨します。
