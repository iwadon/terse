# 端末検出に依存するテストの書き方

`terse_detection.c` は `TERM`, `TERM_PROGRAM`, `WT_SESSION`, `KITTY_PID`,
`TMUX` などの環境変数を読んで動作する端末を識別する。

ユニットテストでこの検出ロジックを叩く場合、**ホストにセットされている
環境変数がそのままテストプロセスに継承される**ため、以下のような事故が
起きやすい。

* WSL 上で実行すると `WT_SESSION` が設定されていて、WezTerm/kitty の
  検出を期待していたテストが Windows Terminal 分岐に流れてしまう。
* tmux セッション内で実行すると `TMUX` が設定されていて、画像機能が
  no-op に降格される。

このドキュメントは「検出依存テストの書き方」と「環境変数を新規追加した
ときに何をすればよいか」を一箇所にまとめる。

## 共通ヘルパー

`tests/test_env.h` に検出依存テスト用のヘルパーを置いている。
中身は 2 つの関数とリストだけ:

```c
/* 端末検出に関わる全環境変数の名前リスト (Single Source of Truth) */
static const char *const TERSE_TEST_DETECTION_ENV_NAMES[] = { ... };
#define TERSE_TEST_DETECTION_ENV_COUNT  (...)

typedef struct terse_test_env_backup { ... } terse_test_env_backup_t;

/* 全 env を退避してから unset する */
void terse_test_env_backup_detection(terse_test_env_backup_t *backup);

/* 退避した値を元に戻す */
void terse_test_env_restore_detection(terse_test_env_backup_t *backup);
```

## 使い方

```c
#include "test_env.h"

TEST(MyArea, DetectsKitty) {
    terse_test_env_backup_t env_backup;
    terse_test_env_backup_detection(&env_backup);   /* 全 env を退避＆unset */

    setenv("TERM", "xterm-kitty", 1);               /* 必要なものだけ setenv */

    /* ... テスト本体 ... */

    terse_test_env_restore_detection(&env_backup);  /* 末尾で必ず復元 */
}
```

ポイント:

* テストの最初に `terse_test_env_backup_detection` を呼ぶと、
  検出に関わる全環境変数が **退避され、その上で unset される**。
  以後、必要な変数だけ `setenv` で立てればよい。
* テストごとに「自分が触る変数だけ save/restore」を手書きしない。
  抜け漏れの温床になる(過去にこれで `WT_SESSION` が漏れて WSL 上で
  3 テストが落ちた)。

## 新しい環境変数を `terse_detection.c` に追加したら

`terse_detection.c` で新たに `getenv("FOO")` を呼ぶようになったら、
**必ず** `tests/test_env.h` の
`TERSE_TEST_DETECTION_ENV_NAMES` 配列に `"FOO"` を 1 行追加する。

これ 1 ヶ所だけで、

* `terse_test_env_backup_detection` / `terse_test_env_restore_detection`
  を使うすべてのテスト
* `terse_open_test.c` の `clear_detection_environment()`

の挙動に反映される。テスト個別の env リスト更新は不要。

漏らすと、ホストにその env が設定されている環境で CI/ローカルが落ちる。

## 既存テストの状況

| テストファイル | 採用パターン |
|----------------|--------------|
| `tests/unit/terse_image_test.c` | `terse_test_env_backup_detection` |
| `tests/unit/terse_keyboard_features_test.c` | `terse_test_env_backup_detection` |
| `tests/unit/terse_open_test.c` | テスト個別の `names[]` + `clear_detection_environment()`(後者は `TERSE_TEST_DETECTION_ENV_NAMES` を参照) |

`terse_open_test.c` の各テストは「自分が触る変数だけ手書きでリストして
backup する」既存スタイルを維持しているが、`clear_detection_environment()`
の unset 対象は共通配列に集約されているため、SoT は保たれる。
新規テストを書く場合は `terse_test_env_backup_detection` のほうを
使うことを推奨する。
