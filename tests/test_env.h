/*
 * test_env.h - 端末検出に依存するテスト向けの環境変数サニタイザ
 *
 * terse_detection.c が参照する環境変数を一括して退避・クリアし、
 * テスト終了時にホストの値へ復元するためのヘルパー。
 *
 * 新しい getenv() を terse_detection.c に追加したときは、
 * TERSE_TEST_DETECTION_ENV_NAMES に名前を 1 行追加するだけで
 * すべての検出依存テストに反映される。
 *
 * 使い方:
 *   TEST(...) {
 *       terse_test_env_backup_t env;
 *       terse_test_env_backup_detection(&env);
 *       setenv("TERM", "xterm-kitty", 1);
 *       // ... テスト本体 ...
 *       terse_test_env_restore_detection(&env);
 *   }
 */
#ifndef TERSE_TEST_ENV_H
#define TERSE_TEST_ENV_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test_compat.h"

/*
 * terse_detection.c が参照する環境変数の一覧。
 * detection.c に getenv("FOO") を追加したらここにも "FOO" を足すこと。
 */
static const char *const TERSE_TEST_DETECTION_ENV_NAMES[] = {
	"TERM",
	"TERM_PROGRAM",
	"TERM_PROGRAM_VERSION",
	"LC_TERMINAL",
	"LC_TERMINAL_VERSION",
	"COLORTERM",
	"GNOME_TERMINAL_SCREEN",
	"GNOME_TERMINAL_SERVICE",
	"VTE_VERSION",
	"TERSE_SECONDARY_DA_HINT",
	"WEZTERM_EXECUTABLE",
	"KITTY_PID",
	"WT_SESSION",
	"TMUX"
};

#define TERSE_TEST_DETECTION_ENV_COUNT \
	(sizeof(TERSE_TEST_DETECTION_ENV_NAMES) / sizeof(TERSE_TEST_DETECTION_ENV_NAMES[0]))

typedef struct terse_test_env_entry {
	char *value;
	int had_value;
} terse_test_env_entry_t;

typedef struct terse_test_env_backup {
	terse_test_env_entry_t entries[TERSE_TEST_DETECTION_ENV_COUNT];
} terse_test_env_backup_t;

static inline void
terse_test_env_backup_detection(terse_test_env_backup_t *backup)
{
	for (size_t i = 0; i < TERSE_TEST_DETECTION_ENV_COUNT; ++i) {
		const char *name = TERSE_TEST_DETECTION_ENV_NAMES[i];
		const char *current = getenv(name);
		backup->entries[i].value = NULL;
		backup->entries[i].had_value = 0;
		if (current) {
			char *copy = strdup(current);
			if (copy) {
				backup->entries[i].value = copy;
				backup->entries[i].had_value = 1;
			}
		}
		unsetenv(name);
	}
}

static inline void
terse_test_env_restore_detection(terse_test_env_backup_t *backup)
{
	for (size_t i = 0; i < TERSE_TEST_DETECTION_ENV_COUNT; ++i) {
		const char *name = TERSE_TEST_DETECTION_ENV_NAMES[i];
		if (backup->entries[i].had_value) {
			setenv(name, backup->entries[i].value, 1);
		} else {
			unsetenv(name);
		}
		free(backup->entries[i].value);
		backup->entries[i].value = NULL;
		backup->entries[i].had_value = 0;
	}
}

#endif /* TERSE_TEST_ENV_H */
