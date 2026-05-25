# Terse

[English](README.md) | **日本語**

Terse はテキスト UI 向けに、描画・入力・端末能力検出を一元化する C ライブラリです。端末ごとの差異を吸収しながら安全に縮退し、カラー・マウス・画像などの拡張機能を段階的に扱えるよう設計されています。

## プロファイル

Terse は端末の能力を 4 段階のプロファイルで管理し、利用可能な機能を段階的に拡張します。

| プロファイル | 機能 |
|-------------|------|
| **P0** | カーソル移動、画面クリア、テキスト出力、サイズ取得、入力イベント |
| **P1** | P0 + 16/256/TrueColor、テキスト装飾（太字・斜体・下線など） |
| **P2** | P1 + マウス追跡、ブランケットペースト、ウィンドウタイトル、ハイパーリンク |
| **P3** | P2 + クリップボード、インライン画像、カーソル形状、デスクトップ通知 |

`TERSE_PROFILE_AUTO` を指定すると、端末の能力を自動検出して適切なプロファイルで動作します。

## 特徴
- プロファイル制御: `TERSE_PROFILE_AUTO` と P0〜P3 プロファイルで、端末能力に応じた自動縮退と上限クリップを提供。
- 端末検出: Apple Terminal / GNOME Terminal (VTE) / iTerm2 / WezTerm / kitty / Ghostty / Warp などを判別し、利用可能な機能セットを推定。
- 描画 API: カーソル移動、画面/行消去、文字列・スタイル・カラー出力、インライン画像表示、通知送出を網羅。
- 入力正規化: `terse_read_event` がキー・マウス・リサイズ・ブランケットペーストなどを抽象化し、修飾キーも一貫して扱えるようにする。
- 符号化サポート: UTF-8 を既定としつつ、Shift_JIS 変換や mini iconv フォールバックで多バイト入出力に対応。
- 一貫した状態管理: `terse_capture_state` / `terse_restore_state` や push/pop API でカーソルやスタイルを安全に退避・復元。

## クイックスタート

```c
#include "terse.h"
#include <unistd.h>

int main(void)
{
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8"
    };

    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) return 1;

    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_move_to(handle, 0, 0);
    terse_write_text(handle, "Hello, Terse!");

    terse_close(handle);
    return 0;
}
```

## 必要要件

- C11 対応コンパイラ (clang / GCC 等)
- CMake 3.14 以降
- CMake対応のビルドツール（Ninja / Make など）

## 構成とビルド

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## テスト

```sh
ctest --test-dir build --output-on-failure
```

## 動作サンプル

`samples/` にはプロファイル別のデモが揃っています。ビルドするとサンプルもビルドされます。もしサンプルがビルドされていない場合は cmake で `TERSE_BUILD_SAMPLES` を有効にしてからビルドしてください。

- `p0_demo`: P0のデモ
- `p1_style_demo.c`: テキスト装飾サンプル
- `p1_color_demo.c`: 16/256/TrueColor カラーグリッド
- `p2_features_demo.c`: マウス・ブランケットペースト・タイトル・リンク
- `p3_notifications_demo.c`: ベル/ビジュアル/デスクトップ通知
- `p3_image_demo.c`: iTerm2/kitty 向けインライン画像描画
- `event_logger_demo`: イベントロガーデモ
- `line_edit_demo.c`: P0 API だけで実装した簡易ラインエディタ

## 動作確認済み環境

- macOS 26.5 (arm64)
- Ubuntu 24.04.4 LTS (x86_64 WSL2)
- Ubuntu 24.04 LTS (aarch64)
- Debian 13 trixie (aarch64)
- Windows 11 25H2 (x86_64)

## ドキュメント
- `docs/terse-api-user.md`: アプリケーション開発者向け API ガイド
- `docs/terse-specs.md`: プロファイル仕様と縮退ルール
- `docs/progress-overview.md`: 実装状況のサマリ
- `docs/graphics-roadmap.md`: 画像系機能の計画
- `docs/terse-platform-porting.md`: 追加プラットフォーム移植の指針
- `docs/mini-iconv-plan.md`: mini iconv 実装メモ

詳細なサンプルと運用上の注意は上記ドキュメントを参照してください。

## ライセンス
MIT No Attribution (MIT-0)。詳細は `LICENSE` を参照してください。
