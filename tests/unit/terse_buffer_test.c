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

/* Phase 5.5-A: 任意 origin の矩形への射影。origin≠0 では flush 出力が
 * absolute = origin + local で絶対化される。 */
TEST(TerseBuffer, FlushProjectsThroughOrigin)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 3, 5));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 矩形を端末上の (5, 10) へ置く（origin のみ変更）。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 5, 10, 3, 5));
	EXPECT_EQ(5, handle->buf_origin_row);
	EXPECT_EQ(10, handle->buf_origin_col);

	/* ローカル (1, 2) に書く → 端末絶対 (6, 12) = 1-based (7, 13)。 */
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 1, 2));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "X"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[128];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	EXPECT_TRUE(strstr(buf, "\x1b[7;13H") != NULL);
	EXPECT_TRUE(strstr(buf, "X") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5-A: origin を動かすと、前回矩形のうち今回矩形に含まれない端末セルが
 * 空白で消去される（自己掃除）。 */
TEST(TerseBuffer, FlushErasesResidueOnOriginMove)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 3));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 1 回目: origin (0,0) に "abc" を出して flush。 */
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abc"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[128];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* 2 回目: 矩形を (0,5) へずらして flush。前回 (0,0)-(0,2) は今回矩形外なので
	 * その 3 セルが空白消去され、新しい位置 (0,5)= 1-based (1,6) に再描画される。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 0, 5, 1, 3));
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abc"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[256];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* 残骸消去: 前回 origin (0,0)= 1-based (1,1) へ移動して空白を出す。 */
	EXPECT_TRUE(strstr(buf, "\x1b[1;1H") != NULL);
	/* 新しい位置 (0,5)= 1-based (1,6) へ移動して再描画。 */
	EXPECT_TRUE(strstr(buf, "\x1b[1;6H") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* set_region の引数検証。バッファドモードでなければ NOT_SUPPORTED、
 * 不正引数は INVALID_ARGUMENT。origin/サイズ変更は受理（5.5-B でサイズ対応）。 */
TEST(TerseBuffer, SetRegionValidatesArguments)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	/* バッファ未確保（即時モード）では NOT_SUPPORTED。 */
	EXPECT_EQ(TERSE_ERR_NOT_SUPPORTED, terse_buffer_set_region(handle, 0, 0, 4, 4));

	EXPECT_EQ(0, terse_buffer_alloc(handle, 4, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 不正な origin / サイズ。 */
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_buffer_set_region(handle, -1, 0, 4, 4));
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_buffer_set_region(handle, 0, 0, 0, 4));

	/* origin のみの変更は成功し、サイズ一致なら受理。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 2, 3, 4, 4));
	EXPECT_EQ(2, handle->buf_origin_row);
	EXPECT_EQ(3, handle->buf_origin_col);
	EXPECT_EQ(4, handle->buf_rows);
	EXPECT_EQ(4, handle->buf_cols);

	/* サイズ変更（5.5-B）も受理し、バッファ寸法が更新される。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 0, 0, 5, 6));
	EXPECT_EQ(5, handle->buf_rows);
	EXPECT_EQ(6, handle->buf_cols);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5-B: set_region によるサイズ拡大（可変高）。内容非保持で再確保され、
 * 次 flush で新サイズに全描画される。 */
TEST(TerseBuffer, SetRegionGrowsBufferAndRedraws)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "ab"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[128];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* 3 行へ拡大。再確保で内容は失われる。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 0, 0, 3, 4));
	EXPECT_EQ(3, handle->buf_rows);
	EXPECT_EQ(4, handle->buf_cols);

	/* 2 行目に書いて flush → 新領域に描画される。 */
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 2, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "cd"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[128];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* ローカル (2,0) = 1-based (3,1)。 */
	EXPECT_TRUE(strstr(buf, "\x1b[3;1H") != NULL);
	EXPECT_TRUE(strstr(buf, "cd") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5-B: set_region で縮小すると、前回矩形のうち今回矩形外の端末セルが
 * flush 冒頭で空白消去される（残骸消去が縮小もカバー）。 */
TEST(TerseBuffer, SetRegionShrinkErasesResidue)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 3, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 3 行を埋めて flush。 */
	int r;
	for (r = 0; r < 3; r++) {
		EXPECT_EQ(TERSE_OK, terse_move_to(handle, r, 0));
		EXPECT_EQ(TERSE_OK, terse_write_text(handle, "xy"));
	}
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[256];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* 1 行に縮小して flush。前回の行 1,2（ローカル）= 端末絶対行 1,2 が今回矩形外
	 * となり空白消去される。1-based では行 2,3。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 0, 0, 1, 4));
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "xy"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[256];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* 旧行 1,2（1-based 2,3）への移動＝残骸消去が出る。 */
	EXPECT_TRUE(strstr(buf, "\x1b[2;1H") != NULL);
	EXPECT_TRUE(strstr(buf, "\x1b[3;1H") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5-B: terse_get_cell は flush で確定した「画面表示中」の内容を返す。 */
TEST(TerseBuffer, GetCellReturnsDisplayedContent)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	terse_cell_t cell;

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 未 flush では表示中フレームが無いので NOT_SUPPORTED。 */
	EXPECT_EQ(TERSE_ERR_NOT_SUPPORTED, terse_get_cell(handle, 0, 0, &cell));

	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "Hi"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[64];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* flush 後は表示中フレーム = "Hi" を読める。 */
	EXPECT_EQ(TERSE_OK, terse_get_cell(handle, 0, 0, &cell));
	EXPECT_EQ(1, (int)cell.char_len);
	EXPECT_TRUE(memcmp(cell.utf8_char, "H", 1) == 0);
	EXPECT_EQ(TERSE_OK, terse_get_cell(handle, 0, 1, &cell));
	EXPECT_TRUE(memcmp(cell.utf8_char, "i", 1) == 0);
	/* 書いていないセルは空（char_len 0）。 */
	EXPECT_EQ(TERSE_OK, terse_get_cell(handle, 1, 0, &cell));
	EXPECT_EQ(0, (int)cell.char_len);

	/* 範囲外 / null は INVALID_ARGUMENT。 */
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_get_cell(handle, 2, 0, &cell));
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_get_cell(handle, 0, 0, NULL));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5-B: resize 後・未 flush の過渡状態では get_cell は NOT_SUPPORTED
 * （prev_cells が作り直され表示中フレームを失うため）。 */
TEST(TerseBuffer, GetCellAfterResizeIsNotSupportedUntilFlush)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	terse_cell_t cell;

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "z"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[64];
	(void)read_pipe(fds[0], drain, sizeof(drain));
	EXPECT_EQ(TERSE_OK, terse_get_cell(handle, 0, 0, &cell));

	/* サイズ変更で表示中フレーム情報が失われる。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 0, 0, 2, 4));
	EXPECT_EQ(TERSE_ERR_NOT_SUPPORTED, terse_get_cell(handle, 0, 0, &cell));

	/* 次 flush 後は再び読める。 */
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	(void)read_pipe(fds[0], drain, sizeof(drain));
	EXPECT_EQ(TERSE_OK, terse_get_cell(handle, 0, 0, &cell));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5: terse_buffer_set_cursor で指定したローカル位置へ flush 末尾で
 * カーソルが移動する（origin 加算・1-based 変換込み）。 */
TEST(TerseBuffer, SetCursorMovesCursorAfterFlush)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	EXPECT_EQ(0, terse_buffer_alloc(handle, 3, 10));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 矩形を端末上の (4, 2) へ置く。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_region(handle, 4, 2, 3, 10));

	/* ローカル (1, 5) にカーソルを要求 → 端末絶対 (5, 7) = 1-based (6, 8)。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_cursor(handle, 1, 5));
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "hi"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[256];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* flush 末尾でカーソルが (6, 8) へ移動している。 */
	EXPECT_TRUE(strstr(buf, "\x1b[6;8H") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* set_cursor の引数検証: バッファドモードでなければ NOT_SUPPORTED、
 * 負座標は INVALID_ARGUMENT。位置はバッファ寸法外でも受理（カーソル予約列）。 */
TEST(TerseBuffer, SetCursorValidatesArguments)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	/* 即時モードでは NOT_SUPPORTED。 */
	EXPECT_EQ(TERSE_ERR_NOT_SUPPORTED, terse_buffer_set_cursor(handle, 0, 0));

	EXPECT_EQ(0, terse_buffer_alloc(handle, 2, 4));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_buffer_set_cursor(handle, -1, 0));
	EXPECT_EQ(TERSE_ERR_INVALID_ARGUMENT, terse_buffer_set_cursor(handle, 0, -1));

	/* バッファ (2x4) の外側（行2・列4）でも受理される。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_set_cursor(handle, 2, 4));

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

/* Phase 5.5: terse_buffer_invalidate は prev を破棄し、同一内容でも次 flush で
 * 全再描画させる。即時モードでは NOT_SUPPORTED。 */
TEST(TerseBuffer, InvalidateForcesFullRedraw)
{
	int fds[2];
	terse_handle_t handle;
	make_pipe_handle(&handle, fds);

	/* 即時モードでは NOT_SUPPORTED。 */
	EXPECT_EQ(TERSE_ERR_NOT_SUPPORTED, terse_buffer_invalidate(handle));

	EXPECT_EQ(0, terse_buffer_alloc(handle, 1, 3));
	handle->render_mode = TERSE_RENDER_BUFFERED;

	/* 1 回目: "abc" を出して flush。 */
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abc"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));
	char drain[128];
	(void)read_pipe(fds[0], drain, sizeof(drain));

	/* 2 回目: 同じ "abc" を書く。invalidate しなければ差分ゼロで何も出ない。 */
	EXPECT_EQ(TERSE_OK, terse_buffer_invalidate(handle));
	EXPECT_EQ(TERSE_OK, terse_move_to(handle, 0, 0));
	EXPECT_EQ(TERSE_OK, terse_write_text(handle, "abc"));
	EXPECT_EQ(TERSE_OK, terse_flush(handle));

	char buf[256];
	ssize_t n = read_pipe(fds[0], buf, sizeof(buf));
	EXPECT_TRUE(n > 0);
	/* 全再描画なので origin (0,0)= 1-based (1,1) へ移動して "abc" を再出力。 */
	EXPECT_TRUE(strstr(buf, "\x1b[1;1H") != NULL);
	EXPECT_TRUE(strstr(buf, "abc") != NULL);

	terse_close(handle);
	close(fds[0]);
	close(fds[1]);
}

#endif /* HAVE_POSIX_PIPE */
