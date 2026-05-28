/*
 * Phase 5: バッファドモード（TERSE_RENDER_BUFFERED）と代替スクリーンのテスト。
 *
 * セルバッファは中レベルの描画ロジックで、ハンドル状態に依存する。検出に依存する
 * 端末サイズを介さず確実に検証するため、内部ヘッダ terse_handle.h / terse_buffer.h を
 * 直接 include し、pipe ハンドルにバッファを直に確保して純粋ロジックと flush 出力を
 * 検証する（basic4_test と同じ内部直叩きの流儀）。
 *
 * alt screen の has_alt_screen は P2+ 端末でのみ真になり、pipe（非 TTY）では検出が
 * P0 に落ちるため、テストではハンドルの capabilities を直接立ててから検証する。
 */
#include "terse.h"
#include "terse_buffer.h"
#include "terse_handle.h"
#include <attest/attest.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

static void make_pipe_handle(terse_handle_t *out_handle, int fds[2])
{
	EXPECT_TRUE(pipe(fds) == 0);
	terse_options_t options = {
		.input_fd = fds[0],
		.output_fd = fds[1],
		.codec_name = "UTF-8",
		.disabled_caps = 0,
		.enabled_caps = 0,
	};
	*out_handle = terse_open(TERSE_P0, &options);
	EXPECT_NOT_NULL(*out_handle);
}

static ssize_t read_pipe(int fd, char *buffer, size_t size)
{
	int flags = fcntl(fd, F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);

	memset(buffer, 0, size);
	ssize_t n = read(fd, buffer, size);
	if (n < 0 && errno == EAGAIN) {
		struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
		nanosleep(&ts, NULL);
		n = read(fd, buffer, size);
	}
	return n;
}

static const terse_cell_t *cell_at(terse_handle_t handle, int row, int col)
{
	return &handle->cur_cells[(size_t)row * (size_t)handle->buf_cols + (size_t)col];
}

/* ====================================================================== */
/* セルバッファ単体: alloc / write_text / wide char / clear / resize       */
/* ====================================================================== */

TEST(TerseBuffer, AllocInitializesEmptyCells)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 3, 5));
	EXPECT_EQ(3, handle->buf_rows);
	EXPECT_EQ(5, handle->buf_cols);
	const terse_cell_t *c = cell_at(handle, 0, 0);
	EXPECT_EQ(0, c->char_len);
	EXPECT_EQ(1, c->display_width);
	EXPECT_EQ((int)TERSE_COLOR_KIND_DEFAULT, (int)c->fg.kind);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, AllocRejectsBadDimensions)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(-1, terse_buffer_alloc(handle, 0, 5));
	EXPECT_EQ(-1, terse_buffer_alloc(handle, 3, -1));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, WriteTextStoresAsciiCells)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 10));
	terse_buffer_write_text(handle, 0, 2, "Hi");

	const terse_cell_t *h = cell_at(handle, 0, 2);
	const terse_cell_t *i = cell_at(handle, 0, 3);
	EXPECT_EQ(1, h->char_len);
	EXPECT_TRUE(memcmp(h->utf8_char, "H", 1) == 0);
	EXPECT_EQ(1, h->display_width);
	EXPECT_EQ(1, i->char_len);
	EXPECT_TRUE(memcmp(i->utf8_char, "i", 1) == 0);
	/* カーソルは書き込み後の位置へ前進している。 */
	EXPECT_EQ(0, handle->cursor_row);
	EXPECT_EQ(4, handle->cursor_col);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, WriteWideCharMarksContinuation)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 10));
	/* 全角「あ」(U+3042, UTF-8: e3 81 82) は表示幅 2。 */
	terse_buffer_write_text(handle, 1, 0, "\xe3\x81\x82");

	const terse_cell_t *lead = cell_at(handle, 1, 0);
	const terse_cell_t *cont = cell_at(handle, 1, 1);
	EXPECT_EQ(3, lead->char_len);
	EXPECT_EQ(2, lead->display_width);
	EXPECT_EQ(0, (int)lead->is_continuation);
	EXPECT_EQ(1, (int)cont->is_continuation);
	EXPECT_EQ(0, cont->char_len);
	/* 幅 2 ぶんカーソルが進む。 */
	EXPECT_EQ(2, handle->cursor_col);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, WriteTextCarriesCurrentStyle)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 10));
	handle->style.foreground = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	handle->style.effects = TERSE_STYLE_BOLD;
	terse_buffer_write_text(handle, 0, 0, "x");

	const terse_cell_t *c = cell_at(handle, 0, 0);
	EXPECT_EQ((int)TERSE_COLOR_KIND_BASIC16, (int)c->fg.kind);
	EXPECT_EQ((int)TERSE_BASIC_COLOR_RED, (int)c->fg.data.basic16.color);
	EXPECT_EQ((unsigned)TERSE_STYLE_BOLD, (unsigned)c->effects);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, WriteTextStopsAtRightEdge)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 3));
	terse_buffer_write_text(handle, 0, 0, "ABCDE");
	/* 3 桁ぶんだけ書かれ、はみ出しは捨てられる。 */
	EXPECT_EQ(1, cell_at(handle, 0, 2)->char_len);
	EXPECT_EQ(3, handle->cursor_col);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, ClearResetsCurrentFrame)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 4));
	terse_buffer_write_text(handle, 0, 0, "abcd");
	terse_buffer_clear(handle);
	EXPECT_EQ(0, cell_at(handle, 0, 0)->char_len);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, ResizeChangesDimensions)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 4));
	EXPECT_EQ(0, terse_buffer_resize(handle, 5, 8));
	EXPECT_EQ(5, handle->buf_rows);
	EXPECT_EQ(8, handle->buf_cols);
	/* 同寸法 resize は clear 相当（成功して内容クリア）。 */
	terse_buffer_write_text(handle, 0, 0, "z");
	EXPECT_EQ(0, terse_buffer_resize(handle, 5, 8));
	EXPECT_EQ(0, cell_at(handle, 0, 0)->char_len);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* ====================================================================== */
/* diff: 文字・色・effects の差分検出、次元一致前提                          */
/* ====================================================================== */

TEST(TerseBuffer, DiffDetectsChangedCell)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 4));
	/* prev は空のまま、cur に書いてから diff。 */
	terse_buffer_write_text(handle, 0, 1, "x");
	terse_buffer_diff(handle);
	EXPECT_EQ(0, (int)handle->dirty[0]);
	EXPECT_EQ(1, (int)handle->dirty[1]);
	EXPECT_EQ(0, (int)handle->dirty[2]);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, DiffDetectsColorOnlyChange)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 2));
	/* 同じ文字を prev/cur に置き、cur 側だけ色を変える。 */
	terse_buffer_write_text(handle, 0, 0, "a");
	/* prev へコピー（swap で cur が空になるのを避け、手動で複製）。 */
	memcpy(handle->prev_cells, handle->cur_cells, sizeof(terse_cell_t) * 2);
	handle->style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 0);
	terse_buffer_write_text(handle, 0, 0, "a");
	terse_buffer_diff(handle);
	/* 文字は同じでも色が違えばダーティ。 */
	EXPECT_EQ(1, (int)handle->dirty[0]);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* ====================================================================== */
/* flush: バッファドモードで pipe へ差分出力                                */
/* ====================================================================== */

TEST(TerseBuffer, FlushEmitsMoveAndText)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	/* SGR を出せるよう色機能を立てる。 */
	(void)terse_capabilities_enable(handle, TERSE_CAP_ENABLE_SGR_BASIC);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 3, 10));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 1, 2));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "OK"));
	/* バッファドモードでは write 系は即時出力しない。 */
	char pre[64];
	ssize_t pre_n = read_pipe(fds[0], pre, sizeof(pre));
	EXPECT_TRUE(pre_n <= 0);

	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char buf[128];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* (1,2) → 端末 1-based (2,3) へ移動、テキスト出力。 */
	EXPECT_TRUE(strstr(buf, "\x1b[2;3H") != NULL);
	EXPECT_TRUE(strstr(buf, "OK") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, FlushEmitsColorSgr)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	(void)terse_capabilities_enable(handle, TERSE_CAP_ENABLE_SGR_BASIC);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 10));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	terse_style_t red = terse_style_default();
	red.foreground = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
	EXPECT_EQ(TERSE_OK, terse_set_style(handle, &red));
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "R"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[128];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[31m") != NULL);
	EXPECT_TRUE(strstr(buf, "R") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, SecondFlushEmitsOnlyDiff)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	(void)terse_capabilities_enable(handle, TERSE_CAP_ENABLE_SGR_BASIC);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 8));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abcd"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[128];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* 同一内容を再度書いて flush → 差分なしなので本質的な出力なし。 */
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abcd"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char buf[128];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	/* リセット(\x1b[0m)以外に移動やテキストが出ないこと。 */
	EXPECT_TRUE(strstr(buf, "abcd") == NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseBuffer, ImmediateModeWritesDirectly)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	/* 既定（即時モード）では write_text が直接出力する。basic output は P0 既定で有効。 */
	EXPECT_EQ(TERSE_RENDER_IMMEDIATE, handle->render_mode);
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "now"));
	char buf[64];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "now") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* ====================================================================== */
/* 代替スクリーン（DEC private mode 1049）                                   */
/* ====================================================================== */

TEST(TerseAltScreen, EnterLeaveEmitSequences)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	/* pipe は非 TTY で has_alt_screen が偽になるため、テスト用に立てる。 */
	handle->capabilities.has_alt_screen = 1;

	EXPECT_EQ(TERSE_OK, terse_enter_alt_screen(handle));
	EXPECT_EQ(1, handle->alt_screen_active);
	char enter[32];
	ssize_t en = read_pipe(fds[0], enter, sizeof(enter));
	EXPECT_TRUE(en > 0);
	EXPECT_TRUE(strstr(enter, "\x1b[?1049h") != NULL);

	EXPECT_EQ(TERSE_OK, terse_leave_alt_screen(handle));
	EXPECT_EQ(0, handle->alt_screen_active);
	char leave[32];
	ssize_t ln = read_pipe(fds[0], leave, sizeof(leave));
	EXPECT_TRUE(ln > 0);
	EXPECT_TRUE(strstr(leave, "\x1b[?1049l") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAltScreen, NoOpWhenUnsupported)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	handle->capabilities.has_alt_screen = 0;

	EXPECT_EQ(TERSE_OK, terse_enter_alt_screen(handle));
	EXPECT_EQ(0, handle->alt_screen_active);
	char buf[16];
	errno = 0;
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
	ssize_t n = read(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n <= 0); /* 何も書かれていない */

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

TEST(TerseAltScreen, EnterIsIdempotent)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);
	handle->capabilities.has_alt_screen = 1;

	EXPECT_EQ(TERSE_OK, terse_enter_alt_screen(handle));
	char drain[32];
	(void)read_pipe(fds[0], drain, sizeof(drain));
	/* 2 回目は既に active なので no-op（出力なし）。 */
	EXPECT_EQ(TERSE_OK, terse_enter_alt_screen(handle));
	char buf[16];
	int flags = fcntl(fds[0], F_GETFL);
	EXPECT_TRUE(fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == 0);
	ssize_t n = read(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n <= 0);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#endif /* HAVE_POSIX_PIPE */
