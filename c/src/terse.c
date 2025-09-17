#include "terse.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct terse_handle {
	terse_profile_t profile;
	terse_capabilities_t capabilities;
} *terse_handle_t;

terse_handle_t terse_open(terse_profile_t reqested_profile)
{
	// プロファイルの妥当性チェック
	if (reqested_profile < TERSE_P0 || reqested_profile > TERSE_P3) {
		return NULL;
	}

	// ハンドルの確保
	terse_handle_t handle = malloc(sizeof(struct terse_handle));
	if (!handle) {
		return NULL;
	}

	// ハンドルの初期化
	handle->profile = reqested_profile;
	handle->capabilities.profile = reqested_profile; // 仮の実装

	// ハンドルを返す
	return handle;
}

void terse_close(terse_handle_t handle)
{
	// ハンドルを解放
	free(handle); // free()にNULLを渡しても安全
}

terse_capabilities_t terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		terse_capabilities_t empty = { .profile = TERSE_P0 };
		return empty;
	}
	return handle->capabilities;
}
