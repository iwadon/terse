/*
 * terse_posix_ext.c - POSIX 拡張 API の実装（Phase 7）。
 *
 * <terse/posix.h> で宣言する POSIX 固有のアクセサを実装する。コア
 * （terse.c）はプラットフォーム非依存に保ちたいため、POSIX 専用コードは
 * 本ファイルに分離する。CMake で UNIX のときのみコンパイルされる。
 *
 * ハンドルの不透明性は維持する: 公開ヘッダ <terse/posix.h> は struct
 * terse_handle の中身に一切触れず、本ファイル（内部ヘッダを見られる core
 * 層）が fd を取り出して返す。
 */
#include "terse/posix.h"

#include "terse_handle.h"

int terse_posix_get_input_fd(terse_handle_t handle)
{
	if (ensure_handle(handle) != 0) {
		return -1;
	}
	return handle->options.input_fd;
}
