# Phase 6 詳細計画: テスト戦略の刷新

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.4, §3 Phase 6, §4.4
> 前提: Phase 1〜5 完了（commit `75f6311` まで、全テスト 253 件通過、クリーンビルド確認済み）
> Phase 5.5（terse-prompt 本格移行・別リポジトリ）とは技術的依存なし。先に Phase 6 を進める。

## 0. このフェーズの位置づけ

現状のテストは `TERSE_ENABLE_TEST_MODE` の API call recording が中心で、
「terse が正しい API を呼んだか」までは検証できるが
「最終的に端末へ何バイト出力したか」「実端末でどう見えるか」は保証されない（プロポーザル §2.4）。

Phase 6 はこの空白を埋める **テスト基盤の追加** を行う。三段構えのテスト階層
（ユニット / バイト列ゴールデン / PTY 実プロセス）のうち、本フェーズでは
**ゴールデンテスト基盤と PTY テスト基盤を最小導入** し、各々を代表シナリオ
1〜2 件で実証する。既存テストの大量移行は行わない。

方針は再設計の他フェーズと一貫して **「追加のみ・破壊的変更なし」**:
既存 253 件のテストは無変更でビルド・通過すること。`include/terse.h` は変更しない
（テスト基盤はテスト側の変更のみで完結し、公開 API には触れない）。

### スコープ（プロポーザル §3 Phase 6 / §4.4）
1. **バイト列ゴールデンテスト基盤**: パイプハンドルで出力をキャプチャ → ゴールデンと比較。
   ハイブリッド更新フロー（通常は比較のみ、`UPDATE_GOLDEN=1` 時のみ上書き）。
2. ゴールデンの代表シナリオを 1〜2 件導入（ケイパビリティ組み合わせベース。
   例: `colors=BASIC4` / `colors=TRUECOLOR` の SGR 出力）。
3. **PTY 実プロセステスト基盤**: POSIX の `posix_openpt`/`openpty` 系で
   PTY 配下に terse ハンドルを作り、入出力を検証するヘルパを導入。代表 1 件で実証。
4. ドキュメント更新（`docs/progress-overview.md`、必要なら本計画書の確定事項節）。

### スコープ外
- 既存 253 件のゴールデン化・移行（段階移行は将来。一括書き換えはしない。§4.3.3）
- Windows 版 PTY テスト（ConPTY。POSIX のみ提供、Windows は後追い。§4.4.2）
- 端末別スナップショットの網羅（iTerm2 OSC1337 / kitty / Sixel 等の専用テストは
  必要になってから個別追加。基盤だけ用意する）
- 仮想セルバッファのスナップショットテスト（プロポーザル §2.4 (4)。基盤が固まってから）

---

## 1. 現状分析（調査結果）

### 1.1 既存のバイト列検証パターン

既存テストには、すでに **パイプハンドルで出力バイト列を read して検証する**
パターンが存在する（`tests/unit/terse_output_test.c`）。

| ヘルパ | 役割 | 所在 |
|--------|------|------|
| `create_pipe_handle()` | `pipe()` で fd ペアを作り `terse_open()` | `terse_output_test.c:20` |
| `read_pipe()` | 非ブロッキングで出力 fd から read（EAGAIN 時は短時間 sleep して再 read） | `terse_output_test.c:35` |
| `expect_no_bytes_available()` | 出力が無いことの検証 | `terse_output_test.c:52` |

これらは `#ifdef HAVE_POSIX_PIPE`（`tests/test_compat.h`）で囲まれ、Windows では
`SKIP_ON_WINDOWS()` でスキップされる。**ゴールデンテスト基盤はこのパターンを
共通ヘルパへ昇格させる形で構築する**（新規発明でなく既存資産の整理）。

### 1.2 既存の PTY 利用パターン

`tests/unit/terse_get_size_test.c` は既に PTY を使っている。

| 呼び出し | 用途 | 所在 |
|----------|------|------|
| `posix_openpt(O_RDWR\|O_NOCTTY)` | master 取得 | `terse_get_size_test.c:62` |
| `grantpt` / `unlockpt` / `ptsname` | slave 準備 | `:66`〜`:70` |

CMake 側で POSIX 拡張（`_POSIX_C_SOURCE=200809L` / `_XOPEN_SOURCE=700`）は
`if(UNIX)` 下で既に定義済み（`tests/CMakeLists.txt`）。**PTY テスト基盤も
この既存設定の上に乗せられ、新たなビルド構成変更は不要**。

### 1.3 テストビルドの現状

- 全テストは単一バイナリ `terse_unit_test` に統合（`tests/CMakeLists.txt`）。
- C11 でコンパイル（terse 本体は C99）、attest フレームワーク使用。
- 新規テストは `add_executable` のソース列に追加 → 自動的に統合バイナリへ。

---

## 2. 設計判断（AskUserQuestion で確定 — 2026-05-29）

| 論点 | 確定 | 根拠 |
|------|------|------|
| **ゴールデン保存形式** | **エスケープ可視化テキスト**（`.txt`） | `\x1b[1m` のように制御文字をエスケープ表記。`git diff` で人間がレビューでき、バイナリ差分の事故に気づける。比較時はキャプチャ側も同じ可視化を通して比較。プロポーザル §2.4 の「保守コスト」懸念を最小化 |
| **導入順序/範囲** | **ゴールデン基盤を先に最小導入**（基盤+ヘルパ+代表1〜2シナリオを1コミット、PTYは次コミット） | 小さく検証しながら積む。確立パターン（1フェーズ複数コミット）と整合 |
| **既存移行範囲** | **基盤導入のみ・既存253件は不変** | プロポーザル「一括書き換えはしない」「移行は段階的」に最も忠実 |
| **PTY ビルド構成** | **統合バイナリ内・POSIX限定で条件コンパイル**（`HAVE_POSIX_PIPE` で囲み Windows はスキップ） | `terse_get_size_test.c` と同方式。ctest 構成を変えず最小差分で済む |

### 2.1 プロポーザル既確定事項（再掲）

- ゴールデン更新フロー: ハイブリッド（§4.4.1）。通常は比較のみ、`UPDATE_GOLDEN=1`
  環境変数指定時のみ上書き。CI では絶対に上書きしない。
- PTY の Windows 対応: POSIX のみ、Windows 後追い（§4.4.2）。
- スナップショット粒度: ケイパビリティ組み合わせベース + 特定端末は専用テスト（§4.4.3）。

---

## 3. 実装計画

### 3.1 コミット 1: ゴールデンテスト基盤 + 代表シナリオ

**新規ファイル**

- `tests/golden_helpers.h`（POSIX 限定、`HAVE_POSIX_PIPE` で囲む）
  - `golden_capture_t`: パイプハンドル + 出力 fd を束ねるキャプチャコンテキスト
  - `golden_begin(terse_profile_t, terse_options_t *)`: pipe を張り `terse_open` → capture を返す
  - `golden_read_all(golden_capture_t *, char *buf, size_t)`: 出力 fd を非ブロッキングで全 read
    （`terse_output_test.c` の `read_pipe` を昇格・一般化）
  - `golden_escape(const char *raw, size_t len, char *out, size_t outsz)`:
    バイト列を可視化テキスト化（`\x1b` → `\x1b`、改行 → `\n` 等、決め打ちのエスケープ規則）
  - `golden_assert(const char *name, const char *visualized)`:
    `tests/golden/<name>.txt` と比較。`UPDATE_GOLDEN=1` 時は書き込み、
    それ以外は読み込んで `EXPECT_*` で一致検証。ファイル不在かつ非更新時は明示失敗
  - パスは CMake が渡すマクロ（後述）でソースツリー内 `tests/golden/` を指す
- `tests/unit/terse_golden_test.c`（代表シナリオ 1〜2 件）
  - 例 1: `colors=TRUECOLOR` のとき `terse_set_style` が RGB SGR を吐く → ゴールデン比較
  - 例 2: `colors=BASIC4` のとき 4 色 SGR にマップされる（Phase 4 の純粋ロジックを実出力で）
  - ケイパビリティは `terse_capabilities_enable/disable` で固定（端末名に依存しない。§4.4.3）
- `tests/golden/`（ゴールデン格納ディレクトリ。初回は `UPDATE_GOLDEN=1` で生成しコミット）

**CMake 変更**（`tests/CMakeLists.txt`）

- `terse_golden_test.c` をソース列へ追加
- ゴールデンディレクトリパスをコンパイル定義で渡す:
  `target_compile_definitions(terse_unit_test PRIVATE TERSE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")`
  （CI で読み取り専用、ローカルで `UPDATE_GOLDEN=1` 更新の双方に対応）

**更新フローの実装**

```
UPDATE_GOLDEN 未設定: golden/<name>.txt を読む → 無ければ FAIL（"run with UPDATE_GOLDEN=1"）→ 比較
UPDATE_GOLDEN=1:      golden/<name>.txt へ書き込み（テストは PASS 扱い）→ git diff で目視確認後コミット
```

### 3.2 コミット 2: PTY テスト基盤 + 代表シナリオ

**新規ファイル**

- `tests/pty_helpers.h`（`HAVE_POSIX_PIPE` で囲む。Windows は中身を提供しない）
  - `pty_open_pair(int *master, int *slave)`:
    `posix_openpt`/`grantpt`/`unlockpt`/`ptsname`/`open` で master/slave を確保
    （`terse_get_size_test.c` の手順を共通ヘルパ化）
  - `pty_set_winsize(int fd, unsigned short rows, unsigned short cols)`:
    `TIOCSWINSZ` でサイズ設定（リサイズ挙動検証用）
  - 必要なら `pty_read_all` / クリーンアップ
- `tests/unit/terse_pty_test.c`（代表 1 件）
  - 例: PTY slave を terse の input/output に割り当て、`terse_get_size` が
    `TIOCSWINSZ` で設定したサイズを返す（実 tty 上でしか出ない経路の検証）
  - 余力があればリサイズ後のサイズ再取得を 1 ケース追加

**CMake 変更**

- `terse_pty_test.c` をソース列へ追加（POSIX 拡張定義は既存の `if(UNIX)` 分岐で充足）
- Windows では `SKIP_ON_WINDOWS()` によりテスト本体が空 return（ビルドは通る）

### 3.3 コミット 3（任意・余力次第）: ドキュメント反映

- `docs/progress-overview.md` にテスト戦略の現状（ゴールデン/PTY 基盤導入済み）を追記
- 本計画書末尾に「確定事項・実装結果」節を追記（他フェーズ計画書の慣例）

> コミット 1・2 でも各々ドキュメントの該当行を更新する場合は 3 を省略してよい。

---

## 4. 検証手順（各コミット共通）

確立パターン通り:

1. クリーンビルド: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build`
2. 全テスト: `ctest --test-dir build --output-on-failure`（既存 253 + 新規が全通過）
3. ゴールデン更新フロー検証: `UPDATE_GOLDEN=1 ./build/tests/terse_unit_test` で生成 →
   未設定で再実行し一致を確認 → `git diff tests/golden/` で内容目視
4. `git diff include/terse.h` が **空** であること（公開 API 不変の確認）
5. clang-format フック適用後に再ビルド確認
6. Conventional Commits でコミット

### 4.1 Windows 非対応の確認方針

実機 Windows は手元に無いため、`HAVE_POSIX_PIPE` 分岐の構文崩れを避けることに留める
（`SKIP_ON_WINDOWS()` / `#ifdef` の対称性を既存テストと揃える）。必要なら
`cc -fsyntax-only` 相当の確認に倣う（Phase 4.5 の `-D__human68k__` 構文チェックと同様の発想）。

---

## 5. リスクと留意点

- **ゴールデンの環境依存**: ケイパビリティを明示固定するため端末名・環境変数に依存しない。
  ただし `terse_detection.c` を経由するシナリオは `tests/test_env.h` の
  backup/restore（`terse_test_env_backup_detection`）でホスト環境変数をサニタイズすること
  （`docs/testing-terminal-detection.md` の既往インシデント参照）。
- **改行・パス区切り**: ゴールデンはテキストでも LF 固定で保存（`.gitattributes` で
  `text eol=lf` 指定を検討）。Windows で CRLF に化けると比較が壊れるため。
- **非ブロッキング read の取りこぼし**: 既存 `read_pipe` は EAGAIN 時に 1ms sleep して
  再 read している。出力量が増えるシナリオではバッファサイズと read ループに注意。
- **`UPDATE_GOLDEN` の事故防止**: CI では環境変数を設定しない運用を明記（§4.4.1）。
  ヘルパ側で「未設定かつファイル不在 = FAIL」を徹底し、暗黙生成しない。

---

## 6. 完了条件

- [ ] ゴールデンテスト基盤（ヘルパ + ハイブリッド更新フロー）導入、代表 1〜2 シナリオ通過
- [ ] PTY テスト基盤（POSIX 限定ヘルパ）導入、代表 1 シナリオ通過
- [ ] 既存 253 件が無変更で全通過
- [ ] `include/terse.h` の差分が空（破壊的変更なし・公開 API 不変）
- [ ] `UPDATE_GOLDEN=1` 更新 → 未設定比較のラウンドトリップが機能
- [ ] ドキュメント反映（progress-overview / 本計画書の確定事項）
- [ ] Conventional Commits でコミット（push は Phase 6 完了後に一括 → Phase 5.5 へ）
