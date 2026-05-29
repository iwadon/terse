# Phase 7 詳細計画: POSIX 拡張ヘッダの追加

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.3, §3 Phase 7, §4.2.5
> 前提: Phase 1〜6 完了（commit `87d9d3f` まで、全テスト 257 件通過、クリーンビルド確認済み）
> 本フェーズ完了で redesign 本体側のゴール（Phase 1〜7）に到達。Phase 8（ハンドル分離）は
> プロポーザルでスコープ外。完了後に一括 push → terse-prompt の GIT_TAG 更新 → Phase 5.5。

## 0. このフェーズの位置づけ

現状、端末入力の待機は `terse_read_event(handle, timeout_ms, out)` のみ。複数 fd
（端末入力 + ソケット + タイマー）を一つの poll/epoll/kqueue ループで待ちたい
利用側は、terse の入力 fd を取り出せないため統合できない（プロポーザル §2.3）。

Phase 7 は **POSIX 拡張ヘッダ `<terse/posix.h>` を新設** し、ハンドルから
入力 fd を取り出す手段を提供する。コア API（OS 非依存）はそのまま、必要な利用側
だけが拡張ヘッダを include する構成。

方針は再設計の他フェーズと一貫して **「追加のみ・破壊的変更なし」**:
既存利用側コードは無変更でビルド・動作すること。`include/terse.h` は変更しない
（新 API はすべて新ヘッダ `include/terse/posix.h` に閉じる）。

### スコープ（プロポーザル §3 Phase 7 / §4.2.5）
1. `include/terse/posix.h` を新設。POSIX 環境で入力 fd を露出する関数を宣言。
2. 実装を core 層に追加（`handle->options.input_fd` を返すだけの薄いアクセサ）。
3. ユニットテスト追加（パイプ/PTY ハンドルで fd が取れ、実際に poll できること）。
4. ドキュメント反映（progress-overview、API ユーザーガイド、本計画書の確定事項）。
5. 必要なら `samples/` に poll ループ統合の最小例。

### スコープ外
- Windows 拡張ヘッダ `<terse/win32.h>`（後追い。POSIX `pollfd` と
  `WaitForMultipleObjects` は形式が大きく異なる。§4.2.5）
- Human68k 拡張（同上）
- `terse_wait_event()` の新規実装（§2.3 は「維持」と書くが実在しない。後述 §2.2 で判断）
- ハンドル分離（Phase 8、プロポーザルでスコープ外）

---

## 1. 現状分析（調査結果）

### 1.1 入力待機の現状

| 事実 | 詳細 | 所在 |
|------|------|------|
| 公開イベント API は 1 本 | `terse_read_event(handle, timeout_ms, out)` のみ | `include/terse.h:443` |
| `terse_wait_event` は**未実装** | プロポーザル §2.3 は「維持」と記すが実在しない（提案時点の想定 API 名） | — |
| 入力 fd はハンドル内に保持 | `handle->options.input_fd` | `src/core/terse_handle.h:33`（`terse_options_t`） |
| 内部では既に poll/select 済み | POSIX 層が `input_fd` に対し `poll()`/`select()` | `src/terse_posix.c:21` ほか |

### 1.2 ハンドルの不透明性

- `terse_handle_t` は不透明ポインタ（`typedef struct terse_handle *terse_handle_t;`、`terse.h:11`）。
- `struct terse_handle` の実体は内部ヘッダ `src/core/terse_handle.h` にあり、公開していない。
- したがって `terse_posix_get_input_fd` の実装は **core 層の実装ファイル**（内部ヘッダを
  include できる場所）に置く必要がある。公開ヘッダからはハンドル内部に触れない。

### 1.3 既存の include/terse/ レイアウト（Phase 3 の先例）

- `include/terse/term.h` が既存（Phase 3 で新設）。冒頭で `#include "terse.h"` し、
  型は terse.h 集約・関数宣言のみ分離、という流儀。`extern "C"` ガードと
  インクルードガード（`TERSE_TERM_H_INCLUDED`）あり。
- `<terse/posix.h>` もこの流儀に揃える（`#include "terse.h"` → 宣言 → C++ ガード）。

---

## 2. 設計判断（要 AskUserQuestion 確認）

以下を確認してから実装に入る。

### 2.1 露出する API セット

- **案 A（最小）**: `int terse_posix_get_input_fd(terse_handle_t)` のみ。
  利用側が自前で `pollfd` を組み立てる。プロポーザル §2.3 の例そのまま。
- **案 B（fd 2 本）**: `get_input_fd` + `get_output_fd`。出力多重化（ノンブロッキング
  書き込みの完了待ち）にも対応。
- **案 C（pollfd ヘルパ込み）**: 案 A/B に加え `terse_posix_fill_pollfd(handle, struct pollfd *)`
  のような補助。`<poll.h>` を拡張ヘッダが引き込むことになる。

> プロポーザルの記述（§2.3 のコード例）は **案 A** に最も忠実。fd を返すだけなら
> 拡張ヘッダは `<poll.h>` 等の OS ヘッダを引き込まずに済み、依存が最小。

### 2.2 `terse_wait_event` の扱い

プロポーザル §2.3 は「コア API は `terse_wait_event(handle, timeout_ms)` を維持」と
書くが、実際には存在せず `terse_read_event` がその役割。

- **案 X（現状維持）**: 新規実装しない。コア待機は `terse_read_event` のまま。
  Phase 7 は fd 露出のみに集中（最小・追加のみ）。
- **案 Y（別名追加）**: `terse_wait_event` を `terse_read_event` の薄い別名として追加。
  プロポーザル記述との字面整合は取れるが、API が二重化する。

> redesign の「追加のみ・最小」方針からは **案 X** が素直。プロポーザルの記述は
> 提案時点の想定であり、実装は `terse_read_event` に集約済みという事実を計画書に明記する。

### 2.3 実装ファイルの所在

- **案 P（core/terse.c に追記）**: 既存の core 実装に数行追加。新ファイル不要。
- **案 Q（新規 src/core/terse_posix_ext.c）**: POSIX 拡張専用の実装ファイルを新設。
  CMake で UNIX のときのみコンパイル。責務が明快。

> アクセサは数行だが、「POSIX 拡張」という論理的まとまりと、将来 fd 以外の POSIX 固有
> API が増える余地を考えると **案 Q** が見通しがよい。ただし terse.c は元々 platform
> 非依存なので、POSIX 専用コードを core/terse.c に混ぜるのは避けたい（案 P の難点）。

### 2.4 非 POSIX で `<terse/posix.h>` を include したときの扱い

- **案 1（ガードで空）**: `#if defined(_WIN32)` 等で宣言を `#error` か空にする。
  Windows で誤って include したらコンパイルエラー or 何も出さない。
- **案 2（宣言は常に見せる）**: 宣言は出すが、非 POSIX では実装がリンクされない
  → リンクエラーで気づく。

> 親プロポーザル §4.2.5 は「初期版は POSIX のみ提供」。明示性の観点から **案 1 の
> `#error`**（非 POSIX で include したら「このヘッダは POSIX 専用」と即座に分かる）が
> 親切。ただしクロス利用の柔軟性を取るなら案 2。要確認。

---

## 3. 実装計画（確認後に確定）

おおむね次の構成を想定（AskUserQuestion の結果で微調整）:

- `include/terse/posix.h`（新規）
  - `#include "terse.h"` → `extern "C"` ガード → 関数宣言
  - 非 POSIX ガード（§2.4 の確定に従う）
  - 宣言: `terse_posix_get_input_fd`（+ §2.1 の確定により追加分）
- 実装（§2.3 の確定に従い `src/core/terse_posix_ext.c` か `terse.c`）
  - `handle->options.input_fd` を返す薄いアクセサ。`ensure_handle` 相当のガードを通す。
- `tests/unit/terse_posix_ext_test.c`（新規）
  - パイプ/PTY ハンドルで `terse_posix_get_input_fd` が options で渡した fd を返す
  - 取得した fd に対し実際に `poll()` して入力可否が判定できる（PTY で 1 ケース）
  - POSIX 限定（`HAVE_POSIX_PIPE`）。既存 Phase 6 ヘルパ（pty_helpers.h）を流用可
- `tests/CMakeLists.txt`: テスト登録（UNIX のときのみ実装ファイルをリンク）
- `samples/`（任意）: poll ループに端末入力 + もう一つの fd を混ぜる最小例
- ドキュメント: `docs/progress-overview.md`、`docs/terse-api-user.md`（+ .ja）

---

## 4. 検証手順（確立パターン）

1. クリーンビルド: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build`
2. 全テスト: `ctest --test-dir build --output-on-failure`（既存 257 + 新規が全通過）
3. `git diff include/terse.h` が **空**（新 API は `include/terse/posix.h` に閉じる）
4. C/C++ 両方で `<terse/posix.h>` が include できることを確認（`extern "C"` ガード）
5. Windows 非対応の扱いが §2.4 の確定通りか（`#error` なら誤 include で即エラー）を
   `cc -fsyntax-only` 等で確認（Phase 4.5 の構文チェックに倣う）
6. clang-format フック適用後に再ビルド確認
7. Conventional Commits でコミット

---

## 5. リスクと留意点

- **ハンドル不透明性の維持**: アクセサ実装は内部ヘッダを見るが、公開ヘッダ
  `<terse/posix.h>` からは `struct terse_handle` の中身に一切触れない（fd を返す関数のみ）。
- **fd の所有権**: 返す fd は terse が `terse_close` で閉じる対象。利用側が poll に
  使うのは構わないが close してはいけない旨をドキュメントに明記する。
- **`<poll.h>` 依存**: 案 A（fd のみ）なら拡張ヘッダは OS ヘッダを引き込まない。
  案 C を選ぶ場合は `<poll.h>` 依存が増えるので影響を意識する。
- **テストの fd ライフサイクル**: パイプ/PTY テストで fd の close 順序に注意
  （Phase 6 ヘルパの慣例に合わせる）。

---

## 6. 完了条件

- [x] `<terse/posix.h>` 新設、入力 fd 露出 API を提供（API セットは §2.1 の確定通り）
- [x] 実装は core 層（ハンドル不透明性を保ったまま）
- [x] ユニットテスト追加・全通過（既存 257 件は無変更で通過）
- [x] `include/terse.h` の差分が空（破壊的変更なし・公開コア API 不変）
- [x] C/C++ 両方で include 可能
- [x] Windows 非対応の扱いが §2.4 の確定通り
- [x] ドキュメント反映（progress-overview / API ユーザーガイド / 本計画書の確定事項）
- [x] Conventional Commits でコミット（push は Phase 7 完了後に一括 → Phase 5.5 へ）

---

## 7. 実装結果（2026-05-29）

AskUserQuestion で 4 論点とも推奨案に確定し、実装した。全テスト 260 件通過
（既存 257 + 新規 3）、`include/terse.h` 不変。

### 確定した設計判断

| 論点 | 確定 |
|------|------|
| API セット（§2.1） | **最小: `terse_posix_get_input_fd` のみ**。拡張ヘッダは `<poll.h>` 等を引き込まず依存最小 |
| `terse_wait_event`（§2.2） | **現状維持（追加しない）**。コア待機は `terse_read_event`。プロポーザル §2.3 の記述は提案時点の想定名で実在しないことを明記 |
| 実装ファイル（§2.3） | **新規 `src/core/terse_posix_ext.c`**（CMake で UNIX のみコンパイル）。core/terse.c の platform 非依存性を保つ |
| 非 POSIX 扱い（§2.4） | **`#error` で即エラー**（`_WIN32` / `__human68k__`）。誤 include を即座に検出 |

### 成果物

- `include/terse/posix.h`: `terse_posix_get_input_fd(terse_handle_t)` を宣言。
  `#include "terse.h"` → 非 POSIX `#error` ガード → `extern "C"` → 宣言。fd は terse 所有・
  利用側で close 禁止の旨を明記。
- `src/core/terse_posix_ext.c`: `ensure_handle` ガードを通して `handle->options.input_fd`
  を返す薄いアクセサ。無効ハンドルは -1。ハンドル不透明性を維持（公開ヘッダは struct に
  触れず、内部ヘッダを見られる core 層が fd を取り出す）。
- `tests/unit/terse_posix_ext_test.c`: 3 ケース。パイプで input_fd 一致 / NULL で -1 /
  PTY 上で露出 fd を実 `poll()` して入力可否判定。
- `CMakeLists.txt`: `elseif(UNIX)` に `terse_posix_ext.c` 追加。
- `tests/CMakeLists.txt`: テスト登録。

### 検証で確定した事実

- C / C++ 両方で `<terse/posix.h>` を include できる（`fsyntax-only` で確認）。
- `_WIN32` / `__human68k__` 定義時に `#error` が発火（誤 include の即時検出）。
- **PTY ラインディシプリンの注意点**: PTY のデフォルトは canonical モードのため、
  master へ 1 バイト書いても改行が来るまで slave 側は readable にならない（macOS で
  実際に poll が 0 を返すことを最小再現で確認）。テストは改行込み `"x\n"` で書いて
  行を完成させた。terse は raw モードを管理しない設計（既知の API 境界）であり、
  raw 化はテスト/利用側の責務という事実と整合。

### redesign 本体のゴール到達

Phase 7 完了をもって、プロポーザル §3 の Phase 1〜7（層の可視化・ケイパビリティ中心・
バッファド描画・テスト刷新・POSIX 拡張）が完了。Phase 8（ハンドル分離）はプロポーザルで
スコープ外（別プロポーザル送り）。次は Phase 1〜7 を一括 push → terse-prompt の GIT_TAG
更新 → Phase 5.5 本格移行 + 保留デモのコミット。
