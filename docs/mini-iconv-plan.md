# Mini iconv Implementation Plan

## Motivation

- Human68k ビルドではシステム側の文字コードが Shift_JIS 固定で、外部 `iconv` をリンクできないケースがある。
- `TERSE_ENABLE_ICONV=OFF` では現在 Shift_JIS を選ぶと `ENOSYS` を返して終了するため、Human68k では実用にならない。
- そこで `iconv` の API 互換シェルだけを TERSE 内部に実装し、Shift_JIS ⇔ UTF-8 の相互変換を最小コストで提供する。

## 目標と非目標

- **目標**
  - `iconv_open("UTF-8", "SHIFT_JIS")` と `iconv_open("SHIFT_JIS", "UTF-8")` を正しく処理。
  - `iconv`・`iconv_close`・リセット (`iconv(cd, NULL, NULL, NULL, NULL)`) を API 互換で提供。
  - ASCII, 半角カナ, JIS 第1・第2水準漢字、および既存ユニットテストが検証する範囲をサポート。
  - 依存ライブラリを追加せず、ビルド時に `TERSE_ENABLE_ICONV=OFF` でも Shift_JIS が動作する。
- **非目標**
  - 多言語コードページや ISO-2022-JP など他の charset への拡張。
  - 大規模なエンコーディング検証や完全な Unicode 正規化。

## 期待する差し込みポイント

1. `TERSE_ENABLE_ICONV=OFF` のとき `TERSE_HAVE_ICONV` を 0 ではなく "mini iconv" 実装に向ける。
2. 既存の `initialize_codec_handles` / `destroy_codec_handles` はそのまま利用し、`iconv_open` などでラッパーが呼ばれる。
3. 失敗時は `errno` に `EINVAL` / `EILSEQ` / `E2BIG` を設定して戻す。

## 実装の骨子

### 型とハンドル
- `typedef struct mini_iconv *iconv_t;` とし、Shift_JIS→UTF-8 / UTF-8→Shift_JIS で方向を持たせる。
- 方向・内部状態（未処理バイト数）をハンドルに保持。
- リセット呼び出しで内部状態をクリア。

### 変換ルーチン
- **Shift_JIS → UTF-8**
  - 先頭バイトで 1バイト / 2バイト / 半角カナを判別。
  - 2バイト目の範囲チェック後、マッピングテーブルで Unicode scalar に変換。
  - UTF-8 エンコードを行い、出力バッファに書き込む。
- **UTF-8 → Shift_JIS**
  - UTF-8 マルチバイトを最長 4 バイトまで読み取り、UTF-32 scalar を得る。
  - 代表的な変換範囲（ASCII, 半角カナ, JIS 第1・第2水準, NEC/IBM 拡張など）をマッピング表で逆引き。
  - 見つからない場合は `?` (`0x3f`) を出力し、`errno` を `EILSEQ` にして処理継続（現行挙動と揃える）。

### マッピングデータ
- 片方向テーブルを静的に埋め込み：
  - ASCII は直写。
  - 半角カナ (0xA1–0xDF) は公式表をベタ書き。
  - 第1水準・第2水準はJISコード順の配列を 2バイト番号 → Unicode の形で格納。
  - 逆方向はハッシュテーブルではなく、JISグループ + 線形探索、または Unicode → Shift_JIS のソート済み配列 + 二分探索で対応（メモリ <32KB 目標）。

### エラー処理
- 入力が途中で終わった場合は `EILSEQ` とし、`-1` を返す。
- 出力バッファが不足なら `E2BIG`。
- 未サポートの変換ペア（例：`iconv_open("UTF-8", "UTF-16")`）は `EINVAL`。

## 組み込み手順

1. `c/src/mini_iconv.c`（仮称）に実装。
2. `c/CMakeLists.txt` で `TERSE_ENABLE_ICONV` が OFF のとき、このファイルをビルドへ追加し `TERSE_HAVE_ICONV` を 1 のままにする。
3. ヘッダー (`mini_iconv.h`) で既存の `<iconv.h>` シンボルを上書きし、ベアメタルでもビルドできるようにする。
4. 現在の `#if TERSE_HAVE_ICONV` ガードは多くが不要になり、`TERSE_HAVE_ICONV` は常に 1 扱いが可能（アイコンブのあり/なしを透過化）。

## テスト計画

- 既存の `terse_write_text_test`・`terse_read_event_test` の Shift_JIS ケースを `TERSE_ENABLE_ICONV=OFF` でビルドし、継続的に回す。
- 追加で、エラーケース（不正バイト列／バッファ不足）を網羅する単体テストを新設。
- 端末上での実機確認：Human68k 版の TERSE を用いて、簡単な入出力デモで正しい表示とイベント復号を確認。

## 工数とリスク

- 実装量：約 500–800 行（テーブル含む）。
- 主なリスクはマッピング漏れ。段階的に ASCII→かな→頻出漢字→全表と広げる方針にする。
- 既存の `unicode_cell_width` やテスト群が UTF-8 ベースであるため、ミニ iconv でも UTF-8 に正しく変換されることが最重要。

## 次のアクション

1. マッピングソースを確定（既存の Shift_JIS テーブル、もしくは JIS X 0208 公式表から生成）。
2. プロトタイプで Shift_JIS→UTF-8 のみを先に実装し、`terse_read_event` のテストを通す。
3. UTF-8→Shift_JIS を追加し、`terse_write_text` テストを通す。
4. 変換テーブルを最適化（圧縮・静的初期化）。
5. ドキュメントを成約のある形（利用可能な文字範囲など）で更新。

この方針で進めれば、Human68k 向けの Shift_JIS 環境でもアイコンブへ依存せず TERSE を配布できる見込みです。
