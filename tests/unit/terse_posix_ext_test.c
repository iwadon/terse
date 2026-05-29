/*
 * terse_posix_ext_test.c - POSIX 拡張 API のテスト（Phase 7）
 *
 * <terse/posix.h> の terse_posix_get_input_fd を検証する。
 *   - パイプ/PTY ハンドルで options.input_fd と一致する fd を返す
 *   - 返した fd に対し実際に poll() して入力可否が判定できる
 *   - 無効ハンドルで -1 を返す
 *
 * POSIX 限定（HAVE_POSIX_PIPE）。PTY 非対応環境では該当ケースをスキップ。
 */
#include "terse.h"
#include <attest/attest.h>

#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

#include "pty_helpers.h"
#include "terse/posix.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

/* パイプハンドルで input_fd を返すこと。 */
TEST(TersePosixExt, ReturnsInputFdForPipe)
{
	int fds[2];
	EXPECT_TRUE(pipe(fds) == 0);

	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);

	EXPECT_EQ(fds[0], terse_posix_get_input_fd(handle));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* 無効ハンドルで -1 を返すこと。 */
TEST(TersePosixExt, ReturnsMinusOneForNull)
{
	EXPECT_EQ(-1, terse_posix_get_input_fd(NULL));
}

/*
 * 露出した fd を実際に poll() して入力可否が判定できること。
 * PTY master 側に書き込み、slave（terse の入力 fd）が readable になるのを
 * poll で確認する。
 */
TEST(TersePosixExt, ExposedFdIsPollable)
{
	int master_fd = -1;
	int slave_fd = -1;
	if (pty_open_pair(&master_fd, &slave_fd) != 0) {
		return; /* PTY 非対応環境ではスキップ */
	}

	terse_options_t options = {
		.input_fd = slave_fd,
		.output_fd = slave_fd,
		.codec_name = "UTF-8",
	};
	terse_handle_t handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(handle);

	int fd = terse_posix_get_input_fd(handle);
	EXPECT_EQ(slave_fd, fd);

	/* 書き込み前: 入力なし（タイムアウト 0 で readable でない）。 */
	struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
	int ready = poll(&pfd, 1, 0);
	EXPECT_TRUE(ready == 0);

	/*
	 * master 側へ書き込むと slave が readable になる。PTY のデフォルトは
	 * canonical モードなので、行が完成する改行込みで書く（terse は raw
	 * モードを管理しない設計のため、テスト側で行を完成させる）。
	 */
	EXPECT_TRUE(write(master_fd, "x\n", 2) == 2);
	pfd.revents = 0;
	ready = poll(&pfd, 1, 1000);
	EXPECT_TRUE(ready == 1);
	EXPECT_TRUE((pfd.revents & POLLIN) != 0);

	terse_close(handle);
	close(master_fd);
	close(slave_fd);
}

#endif /* HAVE_POSIX_PIPE */
