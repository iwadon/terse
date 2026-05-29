/*
 * golden_helpers.h - バイト列ゴールデンテストの共通基盤（Phase 6）
 *
 * terse の「最終的に端末へ出力したバイト列」を検証するためのヘルパ。
 * パイプハンドルで出力をキャプチャし、制御文字を可視化したテキストとして
 * ゴールデンファイル（tests/golden/<name>.txt）と比較する。
 *
 * 更新フロー（ハイブリッド。redesign-proposal.md §4.4.1）:
 *   - 通常実行: ゴールデンを読み込み、一致のみを検証（書き込まない）
 *   - UPDATE_GOLDEN=1: ゴールデンを上書き（git diff で確認してから commit）
 *   - 未設定かつファイル不在: 明示失敗（暗黙生成しない）
 *
 * POSIX 限定（pipe() ベース）。Windows ではこのヘルパは無効。
 * 設計の手本: tests/unit/terse_output_test.c（create_pipe_handle / read_pipe）
 */
#ifndef GOLDEN_HELPERS_H
#define GOLDEN_HELPERS_H

#include "terse.h"
#include <attest/attest.h>

#include "test_compat.h"

#ifdef HAVE_POSIX_PIPE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef TERSE_GOLDEN_DIR
#error "TERSE_GOLDEN_DIR must be defined by the build system"
#endif

/* 出力キャプチャのコンテキスト。golden_begin で初期化し golden_read_all で破棄する。 */
typedef struct golden_capture {
	terse_handle_t handle;
	int fds[2]; /* fds[0]=read 側, fds[1]=write 側 */
} golden_capture_t;

/*
 * パイプを張り terse ハンドルを開く。options の input_fd/output_fd は
 * 内部で上書きするので、呼び出し側はケイパビリティ（enabled_caps 等）と
 * codec_name のみ設定すればよい。
 */
static inline void golden_begin(golden_capture_t *cap, terse_profile_t profile,
                                terse_options_t *options)
{
	EXPECT_TRUE(pipe(cap->fds) == 0);
	options->input_fd = cap->fds[0];
	options->output_fd = cap->fds[1];
	cap->handle = terse_open(profile, options);
	EXPECT_NOT_NULL(cap->handle);
}

/*
 * ハンドルを閉じ、書き込み側を閉じてから読み出し側を全 read する。
 * 返すのは NUL 終端の生バイト列（malloc 済み。呼び出し側で free）。
 * len_out が非 NULL なら読み取りバイト数を返す（NUL を含む出力にも対応）。
 */
static inline char *golden_read_all(golden_capture_t *cap, size_t *len_out)
{
	terse_close(cap->handle);
	close(cap->fds[1]);

	int flags = fcntl(cap->fds[0], F_GETFL);
	EXPECT_TRUE(flags != -1);
	EXPECT_TRUE(fcntl(cap->fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

	size_t cap_size = 1024;
	size_t pos = 0;
	char *buf = malloc(cap_size);
	EXPECT_NOT_NULL(buf);

	for (;;) {
		if (pos + 1 >= cap_size) {
			cap_size *= 2;
			buf = realloc(buf, cap_size);
			EXPECT_NOT_NULL(buf);
		}
		ssize_t n = read(cap->fds[0], buf + pos, cap_size - pos - 1);
		if (n > 0) {
			pos += (size_t)n;
			continue;
		}
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			/* write 側がまだ flush 中の可能性。少し待って再 read。 */
			struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
			nanosleep(&ts, NULL);
			n = read(cap->fds[0], buf + pos, cap_size - pos - 1);
			if (n > 0) {
				pos += (size_t)n;
				continue;
			}
		}
		break; /* EOF または再試行後も読めず */
	}

	close(cap->fds[0]);
	buf[pos] = '\0';
	if (len_out) {
		*len_out = pos;
	}
	return buf;
}

/*
 * 生バイト列を可視化テキスト化する。git diff の読みやすさを優先し、
 * 次の規則に固定する:
 *   ESC(0x1b)         -> "\x1b"
 *   改行(0x0a)        -> "\n" のあと実際の LF（複数シーケンスを行で分ける）
 *   復帰(0x0d)        -> "\r"
 *   タブ(0x09)        -> "\t"
 *   その他制御/非ASCII -> "\xNN"（小文字 hex）
 *   印字可能 ASCII     -> そのまま
 * 返すのは malloc 済み NUL 終端文字列（呼び出し側で free）。
 */
static inline char *golden_escape(const char *raw, size_t len)
{
	/* 最悪ケースで 1 バイト -> "\x1b" の 4 文字 + LF。余裕を持って確保。 */
	size_t out_cap = len * 5 + 1;
	char *out = malloc(out_cap);
	EXPECT_NOT_NULL(out);
	size_t o = 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)raw[i];
		const char *seq = NULL;
		char tmp[8];

		if (c == 0x1b) {
			seq = "\\x1b";
		} else if (c == 0x0a) {
			seq = "\\n\n"; /* 可視化 + 実際の改行で行を分ける */
		} else if (c == 0x0d) {
			seq = "\\r";
		} else if (c == 0x09) {
			seq = "\\t";
		} else if (c < 0x20 || c >= 0x7f) {
			snprintf(tmp, sizeof(tmp), "\\x%02x", c);
			seq = tmp;
		} else {
			out[o++] = (char)c;
			continue;
		}

		size_t sl = strlen(seq);
		while (o + sl + 1 >= out_cap) {
			out_cap *= 2;
			out = realloc(out, out_cap);
			EXPECT_NOT_NULL(out);
		}
		memcpy(out + o, seq, sl);
		o += sl;
	}
	out[o] = '\0';
	return out;
}

/* ゴールデンファイルのフルパスを out に組み立てる。 */
static inline void golden_path(const char *name, char *out, size_t outsz)
{
	snprintf(out, outsz, "%s/%s.txt", TERSE_GOLDEN_DIR, name);
}

/*
 * 可視化済みテキストをゴールデンと比較（または更新）する。
 * UPDATE_GOLDEN=1 のときは書き込み、それ以外は読み込んで一致検証する。
 */
static inline void golden_assert(const char *name, const char *visualized)
{
	char path[1024];
	golden_path(name, path, sizeof(path));

	const char *update = getenv("UPDATE_GOLDEN");
	if (update && update[0] == '1' && update[1] == '\0') {
		FILE *fp = fopen(path, "wb");
		EXPECT_NOT_NULL(fp);
		if (fp) {
			size_t vlen = strlen(visualized);
			EXPECT_TRUE(fwrite(visualized, 1, vlen, fp) == vlen);
			fclose(fp);
		}
		return;
	}

	FILE *fp = fopen(path, "rb");
	if (!fp) {
		/* 未設定かつファイル不在は明示失敗（暗黙生成しない）。 */
		fprintf(stderr,
		        "golden file missing: %s\n"
		        "  run with UPDATE_GOLDEN=1 to create it, then review with git diff\n",
		        path);
		EXPECT_TRUE(fp != NULL);
		return;
	}

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	EXPECT_TRUE(fsize >= 0);

	char *expected = malloc((size_t)fsize + 1);
	EXPECT_NOT_NULL(expected);
	size_t rd = fread(expected, 1, (size_t)fsize, fp);
	expected[rd] = '\0';
	fclose(fp);

	if (strcmp(expected, visualized) != 0) {
		fprintf(stderr,
		        "golden mismatch: %s\n--- expected ---\n%s\n--- actual ---\n%s\n",
		        path, expected, visualized);
	}
	EXPECT_EQ(0, strcmp(expected, visualized));
	free(expected);
}

/* よく使う一連の流れ: read -> escape -> assert -> free。 */
static inline void golden_capture_assert(golden_capture_t *cap, const char *name)
{
	size_t len = 0;
	char *raw = golden_read_all(cap, &len);
	char *vis = golden_escape(raw, len);
	golden_assert(name, vis);
	free(vis);
	free(raw);
}

#endif /* HAVE_POSIX_PIPE */

#endif /* GOLDEN_HELPERS_H */
