/*
 * pty_helpers.h - PTY 実プロセステストの共通基盤（Phase 6）
 *
 * 実 tty（PTY slave）配下に terse ハンドルを作り、パイプでは出ない経路
 * （TIOCGWINSZ によるサイズ取得、リサイズ等）を検証するためのヘルパ。
 *
 * POSIX 限定（posix_openpt / grantpt / unlockpt / ptsname）。Windows は
 * ConPTY ベースの別実装が必要なため後追い（redesign-proposal.md §4.4.2）。
 * 設計の手本: tests/unit/terse_get_size_test.c の open_pty_pair / set_winsize。
 */
#ifndef PTY_HELPERS_H
#define PTY_HELPERS_H

#include "terse.h"
#include <attest/attest.h>

#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*
 * PTY の master/slave ペアを開く。成功で 0、失敗で -1（このとき fd は
 * いずれも開かれていない）。失敗時はテスト側で return してスキップ扱いに
 * するのが既存テストの慣例（CI ランナーで PTY が使えない環境への配慮）。
 */
static inline int pty_open_pair(int *master_fd, int *slave_fd)
{
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0) {
		return -1;
	}
	if (grantpt(master) != 0 || unlockpt(master) != 0) {
		close(master);
		return -1;
	}
	char *slave_name = ptsname(master);
	if (!slave_name) {
		close(master);
		return -1;
	}
	int slave = open(slave_name, O_RDWR | O_NOCTTY);
	if (slave < 0) {
		close(master);
		return -1;
	}
	*master_fd = master;
	*slave_fd = slave;
	return 0;
}

/* TIOCSWINSZ で PTY のウィンドウサイズを設定する。成功で 0。 */
static inline int pty_set_winsize(int fd, unsigned short rows, unsigned short cols)
{
	struct winsize ws;
	memset(&ws, 0, sizeof(ws));
	ws.ws_row = rows;
	ws.ws_col = cols;
	return ioctl(fd, TIOCSWINSZ, &ws);
}

#endif /* HAVE_POSIX_PIPE */

#endif /* PTY_HELPERS_H */
