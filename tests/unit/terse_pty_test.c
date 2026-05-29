/*
 * terse_pty_test.c - PTY 実プロセステストの代表シナリオ（Phase 6）
 *
 * 実 tty（PTY slave）配下に terse ハンドルを作り、パイプでは検証できない
 * 経路を確認する。基盤の詳細は tests/pty_helpers.h を参照。
 *
 * PTY が使えない CI 環境では各テストは早期 return でスキップ扱いになる
 * （既存 terse_get_size_test.c と同じ慣例）。
 */
#include "pty_helpers.h"

#ifdef HAVE_POSIX_PIPE

#include <errno.h>
#include <string.h>
#include <time.h>

/* PTY 上では TIOCGWINSZ 経路でサイズ取得が成功すること。 */
TEST(TersePty, GetSizeReturnsWinsize)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (pty_open_pair(&master_fd, &slave_fd) != 0) {
		return; /* PTY 非対応環境ではスキップ */
	}
	if (pty_set_winsize(slave_fd, 40, 100) != 0) {
		close(master_fd);
		close(slave_fd);
		return;
	}

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);

	terse_size_t size = terse_get_size(handle);
	EXPECT_TRUE(size.known == 1);
	EXPECT_EQ(40, size.rows);
	EXPECT_EQ(100, size.cols);

	terse_capabilities_t caps = terse_get_capabilities(handle);
	EXPECT_TRUE(caps.has_size == 1);

	terse_close(handle);
	close(master_fd);
	close(slave_fd);
}

/*
 * 実 tty 配下でも出力バイト列が正しいこと。slave へ書いた SGR を
 * master 側で read して検証する（パイプでなく実 tty 経由の I/O 実証）。
 */
TEST(TersePty, SgrOutputReachesMaster)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (pty_open_pair(&master_fd, &slave_fd) != 0) {
		return;
	}

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
		.enabled_caps = TERSE_CAP_ENABLE_SGR_BASIC,
	};
	terse_handle_t handle = terse_open(TERSE_P1, &options);
	EXPECT_NOT_NULL(handle);

	terse_style_t style = terse_style_default();
	style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 0);
	EXPECT_EQ(0, terse_set_style(handle, &style));
	terse_close(handle);

	/*
	 * master 側を非ブロッキングにして読み出す。PTY では 1 回の read で全
	 * バイトが揃うとは限らない（Linux と macOS で挙動が異なる）。EAGAIN を
	 * 短く待ちつつ累積し、期待シーケンスが揃うまで読む（リトライ上限あり）。
	 */
	int flags = fcntl(master_fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(master_fd, F_SETFL, flags | O_NONBLOCK) == 0);

	char buf[64];
	memset(buf, 0, sizeof(buf));
	size_t pos = 0;
	for (int attempt = 0; attempt < 50 && pos < sizeof(buf) - 1; attempt++) {
		ssize_t n = read(master_fd, buf + pos, sizeof(buf) - 1 - pos);
		if (n > 0) {
			pos += (size_t)n;
			if (strstr(buf, "\x1b[0m\x1b[32m") != NULL) {
				break; /* 期待シーケンスが揃った */
			}
			continue;
		}
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
			nanosleep(&ts, NULL);
			continue;
		}
		break; /* EOF またはエラー */
	}
	EXPECT_TRUE(pos > 0);
	/* 緑前景の SGR（32）が含まれること。 */
	EXPECT_TRUE(strstr(buf, "\x1b[0m\x1b[32m") != NULL);

	close(master_fd);
	close(slave_fd);
}

#endif /* HAVE_POSIX_PIPE */
