# Terse

Terse は POSIX 端末を中心としたテキスト UI 向けに、描画・入力・端末能力検出を一元化する C ライブラリです。端末ごとの差異を吸収しながら安全に縮退し、カラー・マウス・画像などの拡張機能を段階的に扱えるよう設計されています。

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

## 最小コード例

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

## クイックスタート
### 必要要件
- C11 対応コンパイラ (clang / GCC 等)
- CMake 3.20 以降と Ninja
- POSIX 互換環境 (macOS / Linux で動作確認済み)

### 構成とビルド
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build              # すべてのターゲットをビルド
# または
ninja -C build terse        # ライブラリのみを個別ビルド
```

### テスト
```sh
ctest --test-dir build --output-on-failure
```

### サンプルを試す
`samples/` にはプロファイル別のデモが揃っています。ライブラリをビルド済みの状態で、次のようにコンパイルしてください。
```sh
cc -I./c/include -L./build/c -lterse samples/p0_demo.c -o p0_demo
./p0_demo
```
その他のデモ:
- `p1_style_demo.c`: テキスト装飾サンプル
- `p1_color_demo.c`: 16/256/TrueColor カラーグリッド
- `p2_features_demo.c`: マウス・ブランケットペースト・タイトル・リンク
- `p3_notifications_demo.c`: ベル/ビジュアル/デスクトップ通知
- `p3_image_demo.c`: iTerm2/kitty 向けインライン画像描画
- `line_edit_demo.c`: P0 API だけで実装した簡易ラインエディタ

## 主要ディレクトリ
```
c/include        公開ヘッダ (`terse.h` など)
c/src            ライブラリ実装
c/tests/unit     ユニットテストスイート
cmake/           CMake モジュールとツールチェーン
samples/         機能別デモプログラム
docs/            仕様・設計ノート・利用ガイド
build/           CMake/Ninja ビルド成果物 (生成物のみ配置)
```

## ドキュメント
- `docs/terse-api-user.md`: アプリケーション開発者向け API ガイド
- `docs/terse-specs.md`: プロファイル仕様と縮退ルール
- `docs/progress-overview.md`: 実装状況のサマリ
- `docs/graphics-roadmap.md`: 画像系機能の計画
- `docs/terse-platform-porting.md`: 追加プラットフォーム移植の指針
- `docs/mini-iconv-plan.md`: mini iconv 実装メモ

詳細なサンプルと運用上の注意は上記ドキュメントを参照してください。

## 開発フロー
- CMake/Ninja を用いたビルドを前提とします。初回のみ `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` を実行し、以降は `ninja -C build` で増分ビルドできます。
- 変更を加えた際は `ctest --test-dir build --output-on-failure` を必ず実行し、失敗の標準出力/標準エラーを確認してください。
- コーディングスタイルは K&R ブレース・タブインデント・1 行 1 宣言です。大きめの差分では `clang-format` を利用してください。
- パブリック API は `c/include` に配置し、名前は `terse_` プレフィックスで統一します。共有構造体は必要になるまで内部 (`c/src`) に留めてください。
- コミットメッセージは Conventional Commits (`feat:`, `fix:` など) に従い、動作確認内容を明記するとレビューがスムーズです。

## ライセンス
本プロジェクトは MIT No Attribution (MIT-0) ライセンスの下で提供されています。詳細は `LICENSE` を参照してください。

## サポートとフィードバック
バグ報告や改善提案は Issue もしくは Pull Request で歓迎しています。端末固有の挙動や追加したい機能があれば、再現手順や環境情報 (端末種別、プロファイル要求、入出力エンコーディングなど) を添えていただけると助かります。
