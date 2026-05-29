#ifndef TERSE_POSIX_H_INCLUDED
#define TERSE_POSIX_H_INCLUDED

/*
 * terse-posix: POSIX 環境向けの拡張 API（関数宣言）。
 *
 * コア API（<terse.h>）は OS 非依存を保つ。本ヘッダは POSIX 固有の統合手段
 * ——端末入力のファイルディスクリプタを露出し、利用側の poll/epoll/kqueue
 * ループに terse を組み込めるようにする——を提供する。
 *
 * 必要な利用側だけが本ヘッダを include する。コア API のみで足りる利用側
 * （terse_read_event() で待機する場合など）は include する必要はない。
 *
 * 本ヘッダは POSIX 専用。Windows では WaitForMultipleObjects 等、別形式の
 * 統合手段が必要になるため、対応は将来の <terse/win32.h> 等に分離する
 * （redesign-proposal.md §4.2.5）。
 *
 * 詳細は docs/redesign-phase7-plan.md を参照。
 */

#if defined(_WIN32) || defined(__human68k__)
#error "<terse/posix.h> is POSIX-only. Use terse_read_event() on this platform; a platform-specific extension header is planned for the future."
#endif

#include "terse.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 端末入力に使われているファイルディスクリプタを返す。
 *
 * 戻り値はハンドル生成時に terse_options_t.input_fd で渡した fd。利用側は
 * これを自前の poll()/select()/epoll/kqueue ループに組み込み、入力可能に
 * なってから terse_read_event(handle, 0, ...) を呼ぶ、といった統合ができる。
 *
 * 返した fd は terse が所有しており、terse_close() で閉じられる。利用側で
 * close() してはならない（poll 等の監視に使うのは構わない）。
 *
 * handle が無効な場合は -1 を返す。
 */
int terse_posix_get_input_fd(terse_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* TERSE_POSIX_H_INCLUDED */
