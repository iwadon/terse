#include "terse.h"
#include "terse_platform.h"

/* State history stack depth macro.  Small for sanity, yet enough for
 * typical nested UI layers.
 */
#define TERSE_STATE_STACK_MAX 8

#ifndef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 1
#endif

#include <ctype.h>
#include <errno.h>
#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "mini_iconv.h"
#endif
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

static const char TERSE_RESET_ALL_SEQ[] = "\x1b[0m";
static const char TERSE_RESET_COLOR_SEQ[] = "\x1b[39;49m";
static const char TERSE_RESET_EFFECTS_SEQ[] = "\x1b[22;23;24;27;29m";
static const char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char TERSE_MODIFY_OTHER_KEYS_ENABLE_SEQ[] = "\x1b[>4;2m";
static const char TERSE_MODIFY_OTHER_KEYS_DISABLE_SEQ[] = "\x1b[>4;0m";
static const char TERSE_KITTY_PROTOCOL_ENABLE_SEQ[] = "\x1b[>1u";
static const char TERSE_KITTY_PROTOCOL_DISABLE_SEQ[] = "\x1b[<u";

typedef struct terse_rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} terse_rgb_t;

typedef enum terse_codec_kind {
	TERSE_CODEC_UNKNOWN = 0,
	TERSE_CODEC_UTF8,
	TERSE_CODEC_SHIFT_JIS
} terse_codec_kind_t;

typedef struct unicode_interval {
	unsigned int first;
	unsigned int last;
} unicode_interval_t;

static const terse_rgb_t basic16_rgb[16] = {
	{ 0, 0, 0 },
	{ 205, 0, 0 },
	{ 0, 205, 0 },
	{ 205, 205, 0 },
	{ 0, 0, 205 },
	{ 205, 0, 205 },
	{ 0, 205, 205 },
	{ 229, 229, 229 },
	{ 127, 127, 127 },
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 255, 255, 0 },
	{ 92, 92, 255 },
	{ 255, 0, 255 },
	{ 0, 255, 255 },
	{ 255, 255, 255 },
};

static const unsigned int UTF8_REPLACEMENT = 0xfffdU;
static const unsigned int SHIFT_JIS_REPLACEMENT = '?';

static int
interval_contains(const unicode_interval_t *intervals, size_t count, unsigned int scalar)
{
	size_t low = 0;
	size_t high = count;
	while (low < high) {
		size_t mid = low + ((high - low) / 2);
		const unicode_interval_t *current = &intervals[mid];
		if (scalar < current->first) {
			high = mid;
			continue;
		}
		if (scalar > current->last) {
			low = mid + 1;
			continue;
		}
		return 1;
	}
	return 0;
}

static const unicode_interval_t combining_intervals[] = {
	{ 0x0300, 0x036f },
	{ 0x0483, 0x0489 },
	{ 0x0591, 0x05bd },
	{ 0x05bf, 0x05bf },
	{ 0x05c1, 0x05c2 },
	{ 0x05c4, 0x05c5 },
	{ 0x05c7, 0x05c7 },
	{ 0x0600, 0x0605 },
	{ 0x0610, 0x061a },
	{ 0x061c, 0x061c },
	{ 0x064b, 0x065f },
	{ 0x0670, 0x0670 },
	{ 0x06d6, 0x06dd },
	{ 0x06df, 0x06e4 },
	{ 0x06e7, 0x06e8 },
	{ 0x06ea, 0x06ed },
	{ 0x070f, 0x070f },
	{ 0x0711, 0x0711 },
	{ 0x0730, 0x074a },
	{ 0x07a6, 0x07b0 },
	{ 0x07eb, 0x07f3 },
	{ 0x07fd, 0x07fd },
	{ 0x0816, 0x0819 },
	{ 0x081b, 0x0823 },
	{ 0x0825, 0x0827 },
	{ 0x0829, 0x082d },
	{ 0x0859, 0x085b },
	{ 0x08d3, 0x08e1 },
	{ 0x08e3, 0x0902 },
	{ 0x093a, 0x093a },
	{ 0x093c, 0x093c },
	{ 0x0941, 0x0948 },
	{ 0x094d, 0x094d },
	{ 0x0951, 0x0957 },
	{ 0x0962, 0x0963 },
	{ 0x0981, 0x0981 },
	{ 0x09bc, 0x09bc },
	{ 0x09c1, 0x09c4 },
	{ 0x09cd, 0x09cd },
	{ 0x09e2, 0x09e3 },
	{ 0x09fe, 0x09fe },
	{ 0x0a01, 0x0a02 },
	{ 0x0a3c, 0x0a3c },
	{ 0x0a41, 0x0a42 },
	{ 0x0a47, 0x0a48 },
	{ 0x0a4b, 0x0a4d },
	{ 0x0a51, 0x0a51 },
	{ 0x0a70, 0x0a71 },
	{ 0x0a75, 0x0a75 },
	{ 0x0a81, 0x0a82 },
	{ 0x0abc, 0x0abc },
	{ 0x0ac1, 0x0ac5 },
	{ 0x0ac7, 0x0ac8 },
	{ 0x0acd, 0x0acd },
	{ 0x0ae2, 0x0ae3 },
	{ 0x0afa, 0x0aff },
	{ 0x0b01, 0x0b01 },
	{ 0x0b3c, 0x0b3c },
	{ 0x0b3f, 0x0b3f },
	{ 0x0b41, 0x0b44 },
	{ 0x0b4d, 0x0b4d },
	{ 0x0b56, 0x0b56 },
	{ 0x0b62, 0x0b63 },
	{ 0x0b82, 0x0b82 },
	{ 0x0bc0, 0x0bc0 },
	{ 0x0bcd, 0x0bcd },
	{ 0x0c00, 0x0c00 },
	{ 0x0c04, 0x0c04 },
	{ 0x0c3c, 0x0c3c },
	{ 0x0c3e, 0x0c40 },
	{ 0x0c46, 0x0c48 },
	{ 0x0c4a, 0x0c4d },
	{ 0x0c55, 0x0c56 },
	{ 0x0c62, 0x0c63 },
	{ 0x0c81, 0x0c81 },
	{ 0x0cbc, 0x0cbc },
	{ 0x0cbf, 0x0cbf },
	{ 0x0cc6, 0x0cc6 },
	{ 0x0ccc, 0x0ccd },
	{ 0x0ce2, 0x0ce3 },
	{ 0x0d00, 0x0d01 },
	{ 0x0d3b, 0x0d3c },
	{ 0x0d41, 0x0d44 },
	{ 0x0d4d, 0x0d4d },
	{ 0x0d62, 0x0d63 },
	{ 0x0d81, 0x0d81 },
	{ 0x0dca, 0x0dca },
	{ 0x0dd2, 0x0dd4 },
	{ 0x0dd6, 0x0dd6 },
	{ 0x0e31, 0x0e31 },
	{ 0x0e34, 0x0e3a },
	{ 0x0e47, 0x0e4e },
	{ 0x0eb1, 0x0eb1 },
	{ 0x0eb4, 0x0ebc },
	{ 0x0ec8, 0x0ecd },
	{ 0x0f18, 0x0f19 },
	{ 0x0f35, 0x0f35 },
	{ 0x0f37, 0x0f37 },
	{ 0x0f39, 0x0f39 },
	{ 0x0f71, 0x0f7e },
	{ 0x0f80, 0x0f84 },
	{ 0x0f86, 0x0f87 },
	{ 0x0f8d, 0x0f97 },
	{ 0x0f99, 0x0fbc },
	{ 0x0fc6, 0x0fc6 },
	{ 0x102d, 0x1030 },
	{ 0x1032, 0x1037 },
	{ 0x1039, 0x103a },
	{ 0x103d, 0x103e },
	{ 0x1058, 0x1059 },
	{ 0x105e, 0x1060 },
	{ 0x1071, 0x1074 },
	{ 0x1082, 0x1082 },
	{ 0x1085, 0x1086 },
	{ 0x108d, 0x108d },
	{ 0x109d, 0x109d },
	{ 0x135d, 0x135f },
	{ 0x1712, 0x1714 },
	{ 0x1732, 0x1734 },
	{ 0x1752, 0x1753 },
	{ 0x1772, 0x1773 },
	{ 0x17b4, 0x17b5 },
	{ 0x17b7, 0x17bd },
	{ 0x17c6, 0x17c6 },
	{ 0x17c9, 0x17d3 },
	{ 0x17dd, 0x17dd },
	{ 0x180b, 0x180d },
	{ 0x180f, 0x180f },
	{ 0x1885, 0x1886 },
	{ 0x18a9, 0x18a9 },
	{ 0x1920, 0x1922 },
	{ 0x1927, 0x1928 },
	{ 0x1932, 0x1932 },
	{ 0x1939, 0x193b },
	{ 0x1a17, 0x1a18 },
	{ 0x1a1b, 0x1a1b },
	{ 0x1a56, 0x1a56 },
	{ 0x1a58, 0x1a5e },
	{ 0x1a60, 0x1a60 },
	{ 0x1a62, 0x1a62 },
	{ 0x1a65, 0x1a6c },
	{ 0x1a73, 0x1a7c },
	{ 0x1a7f, 0x1a7f },
	{ 0x1ab0, 0x1ace },
	{ 0x1b00, 0x1b03 },
	{ 0x1b34, 0x1b34 },
	{ 0x1b36, 0x1b3a },
	{ 0x1b3c, 0x1b3c },
	{ 0x1b42, 0x1b42 },
	{ 0x1b6b, 0x1b73 },
	{ 0x1b80, 0x1b81 },
	{ 0x1ba2, 0x1ba5 },
	{ 0x1ba8, 0x1ba9 },
	{ 0x1bab, 0x1bad },
	{ 0x1be6, 0x1be6 },
	{ 0x1be8, 0x1be9 },
	{ 0x1bed, 0x1bed },
	{ 0x1bef, 0x1bf1 },
	{ 0x1c2c, 0x1c33 },
	{ 0x1c36, 0x1c37 },
	{ 0x1cd0, 0x1cd2 },
	{ 0x1cd4, 0x1ce0 },
	{ 0x1ce2, 0x1ce8 },
	{ 0x1ced, 0x1ced },
	{ 0x1cf4, 0x1cf4 },
	{ 0x1cf7, 0x1cf9 },
	{ 0x1dc0, 0x1dff },
	{ 0x20d0, 0x20f0 },
	{ 0x2cef, 0x2cf1 },
	{ 0x2d7f, 0x2d7f },
	{ 0x2de0, 0x2dff },
	{ 0x302a, 0x302d },
	{ 0x3099, 0x309a },
	{ 0xa66f, 0xa672 },
	{ 0xa674, 0xa67d },
	{ 0xa69e, 0xa69f },
	{ 0xa6f0, 0xa6f1 },
	{ 0xa802, 0xa802 },
	{ 0xa806, 0xa806 },
	{ 0xa80b, 0xa80b },
	{ 0xa825, 0xa826 },
	{ 0xa82c, 0xa82c },
	{ 0xa8c4, 0xa8c5 },
	{ 0xa8e0, 0xa8f1 },
	{ 0xa8ff, 0xa8ff },
	{ 0xa926, 0xa92d },
	{ 0xa947, 0xa951 },
	{ 0xa980, 0xa982 },
	{ 0xa9b3, 0xa9b3 },
	{ 0xa9b6, 0xa9b9 },
	{ 0xa9bc, 0xa9bc },
	{ 0xa9e5, 0xa9e5 },
	{ 0xaa29, 0xaa2e },
	{ 0xaa31, 0xaa32 },
	{ 0xaa35, 0xaa36 },
	{ 0xaa43, 0xaa43 },
	{ 0xaa4c, 0xaa4c },
	{ 0xaa7c, 0xaa7c },
	{ 0xaab0, 0xaab0 },
	{ 0xaab2, 0xaab4 },
	{ 0xaab7, 0xaab8 },
	{ 0xaabe, 0xaabf },
	{ 0xaac1, 0xaac1 },
	{ 0xaaec, 0xaaed },
	{ 0xaaf6, 0xaaf6 },
	{ 0xabe5, 0xabe5 },
	{ 0xabe8, 0xabe8 },
	{ 0xabed, 0xabed },
	{ 0xfb1e, 0xfb1e },
	{ 0xfe00, 0xfe0f },
	{ 0xfe20, 0xfe2f },
	{ 0x101fd, 0x101fd },
	{ 0x102e0, 0x102e0 },
	{ 0x10376, 0x1037a },
	{ 0x10a01, 0x10a03 },
	{ 0x10a05, 0x10a06 },
	{ 0x10a0c, 0x10a0f },
	{ 0x10a38, 0x10a3a },
	{ 0x10a3f, 0x10a3f },
	{ 0x10ae5, 0x10ae6 },
	{ 0x10d24, 0x10d27 },
	{ 0x10eab, 0x10eac },
	{ 0x10f46, 0x10f50 },
	{ 0x10f82, 0x10f85 },
	{ 0x11001, 0x11001 },
	{ 0x11038, 0x11046 },
	{ 0x11070, 0x11070 },
	{ 0x11073, 0x11074 },
	{ 0x1107f, 0x11081 },
	{ 0x110b3, 0x110b6 },
	{ 0x110b9, 0x110ba },
	{ 0x11100, 0x11102 },
	{ 0x11127, 0x1112b },
	{ 0x1112d, 0x11134 },
	{ 0x11173, 0x11173 },
	{ 0x11180, 0x11181 },
	{ 0x111b6, 0x111be },
	{ 0x111c9, 0x111cc },
	{ 0x111cf, 0x111cf },
	{ 0x1122f, 0x11231 },
	{ 0x11234, 0x11234 },
	{ 0x11236, 0x11237 },
	{ 0x1123e, 0x1123e },
	{ 0x112df, 0x112df },
	{ 0x112e3, 0x112ea },
	{ 0x11300, 0x11301 },
	{ 0x1133b, 0x1133c },
	{ 0x11340, 0x11340 },
	{ 0x11366, 0x1136c },
	{ 0x11370, 0x11374 },
	{ 0x11438, 0x1143f },
	{ 0x11442, 0x11444 },
	{ 0x11446, 0x11446 },
	{ 0x1145e, 0x1145e },
	{ 0x114b3, 0x114b8 },
	{ 0x114ba, 0x114ba },
	{ 0x114bf, 0x114c0 },
	{ 0x114c2, 0x114c3 },
	{ 0x115b2, 0x115b5 },
	{ 0x115bc, 0x115bd },
	{ 0x115bf, 0x115c0 },
	{ 0x115dc, 0x115dd },
	{ 0x11633, 0x1163a },
	{ 0x1163d, 0x1163d },
	{ 0x1163f, 0x11640 },
	{ 0x116ab, 0x116ab },
	{ 0x116ad, 0x116ad },
	{ 0x116b0, 0x116b5 },
	{ 0x116b7, 0x116b7 },
	{ 0x1171d, 0x1171f },
	{ 0x11722, 0x11725 },
	{ 0x11727, 0x1172b },
	{ 0x1182f, 0x11837 },
	{ 0x11839, 0x1183a },
	{ 0x1193b, 0x1193c },
	{ 0x1193e, 0x1193e },
	{ 0x11943, 0x11943 },
	{ 0x119d4, 0x119d7 },
	{ 0x119da, 0x119db },
	{ 0x119e0, 0x119e0 },
	{ 0x11a01, 0x11a0a },
	{ 0x11a33, 0x11a38 },
	{ 0x11a3b, 0x11a3e },
	{ 0x11a47, 0x11a47 },
	{ 0x11a51, 0x11a56 },
	{ 0x11a59, 0x11a5b },
	{ 0x11a8a, 0x11a96 },
	{ 0x11a98, 0x11a99 },
	{ 0x11c30, 0x11c36 },
	{ 0x11c38, 0x11c3d },
	{ 0x11c3f, 0x11c3f },
	{ 0x11c92, 0x11ca7 },
	{ 0x11caa, 0x11cb0 },
	{ 0x11cb2, 0x11cb3 },
	{ 0x11cb5, 0x11cb6 },
	{ 0x11d31, 0x11d36 },
	{ 0x11d3a, 0x11d3a },
	{ 0x11d3c, 0x11d3d },
	{ 0x11d3f, 0x11d45 },
	{ 0x11d47, 0x11d47 },
	{ 0x11d90, 0x11d91 },
	{ 0x11d95, 0x11d95 },
	{ 0x11d97, 0x11d97 },
	{ 0x11ef3, 0x11ef4 },
	{ 0x11f00, 0x11f01 },
	{ 0x11f36, 0x11f3a },
	{ 0x11f40, 0x11f40 },
	{ 0x11f42, 0x11f42 },
	{ 0x13430, 0x13438 },
	{ 0x13440, 0x13455 },
	{ 0x16af0, 0x16af4 },
	{ 0x16b30, 0x16b36 },
	{ 0x16f4f, 0x16f4f },
	{ 0x16f8f, 0x16f92 },
	{ 0x16fe4, 0x16fe4 },
	{ 0x1bc9d, 0x1bc9e },
	{ 0x1cf00, 0x1cf2d },
	{ 0x1cf30, 0x1cf46 },
	{ 0x1d167, 0x1d169 },
	{ 0x1d17b, 0x1d182 },
	{ 0x1d185, 0x1d18b },
	{ 0x1d1aa, 0x1d1ad },
	{ 0x1d242, 0x1d244 },
	{ 0x1da00, 0x1da36 },
	{ 0x1da3b, 0x1da6c },
	{ 0x1da75, 0x1da75 },
	{ 0x1da84, 0x1da84 },
	{ 0x1da9b, 0x1da9f },
	{ 0x1daa1, 0x1daaf },
	{ 0x1e000, 0x1e006 },
	{ 0x1e008, 0x1e018 },
	{ 0x1e01b, 0x1e021 },
	{ 0x1e023, 0x1e024 },
	{ 0x1e026, 0x1e02a },
	{ 0x1e08f, 0x1e08f },
	{ 0x1e130, 0x1e136 },
	{ 0x1e2ae, 0x1e2ae },
	{ 0x1e2ec, 0x1e2ef },
	{ 0x1e4ec, 0x1e4ef },
	{ 0x1e8d0, 0x1e8d6 },
	{ 0x1e944, 0x1e94a },
	{ 0xe0100, 0xe01ef }
};

static const unicode_interval_t wide_intervals[] = {
	{ 0x1100, 0x115f },
	{ 0x2329, 0x232a },
	{ 0x2e80, 0x2ffb },
	{ 0x3000, 0x303e },
	{ 0x3041, 0x33ff },
	{ 0x3400, 0x4dbf },
	{ 0x4e00, 0xa4c6 },
	{ 0xa960, 0xa97c },
	{ 0xac00, 0xd7a3 },
	{ 0xf900, 0xfaff },
	{ 0xfe10, 0xfe19 },
	{ 0xfe30, 0xfe6b },
	{ 0xff01, 0xff60 },
	{ 0xffe0, 0xffe6 },
	{ 0x16fe0, 0x16fe4 },
	{ 0x16ff0, 0x16ff1 },
	{ 0x17000, 0x187f7 },
	{ 0x18800, 0x18aff },
	{ 0x1aff0, 0x1aff3 },
	{ 0x1aff5, 0x1affb },
	{ 0x1affd, 0x1afff },
	{ 0x1b000, 0x1b122 },
	{ 0x1b132, 0x1b132 },
	{ 0x1b150, 0x1b152 },
	{ 0x1b164, 0x1b167 },
	{ 0x1b170, 0x1b2fb },
	{ 0x1f004, 0x1f004 },
	{ 0x1f0cf, 0x1f0cf },
	{ 0x1f100, 0x1f10a },
	{ 0x1f110, 0x1f12d },
	{ 0x1f130, 0x1f169 },
	{ 0x1f170, 0x1f18d },
	{ 0x1f18f, 0x1f190 },
	{ 0x1f19b, 0x1f1ac },
	{ 0x1f200, 0x1f266 },
	{ 0x1f300, 0x1f6d7 },
	{ 0x1f6dc, 0x1f6ec },
	{ 0x1f6f0, 0x1f6fc },
	{ 0x1f700, 0x1f776 },
	{ 0x1f77b, 0x1f7d9 },
	{ 0x1f7e0, 0x1f7eb },
	{ 0x1f7f0, 0x1f7f0 },
	{ 0x1f800, 0x1f80b },
	{ 0x1f810, 0x1f847 },
	{ 0x1f850, 0x1f859 },
	{ 0x1f860, 0x1f882 },
	{ 0x1f890, 0x1f8ad },
	{ 0x1f8b0, 0x1f8b1 },
	{ 0x1f900, 0x1fa53 },
	{ 0x1fa60, 0x1fa6d },
	{ 0x1fa70, 0x1fa7c },
	{ 0x1fa80, 0x1fa88 },
	{ 0x1fa90, 0x1fae7 },
	{ 0x1faf0, 0x1faf6 },
	{ 0x1fb00, 0x1fb92 },
	{ 0x1fb94, 0x1fbca }
};


static int
unicode_cell_width(unsigned int scalar)
{
	if (scalar == 0) {
		return 0;
	}
	if (scalar < 0x20 || (scalar >= 0x7f && scalar < 0xa0)) {
		return 0;
	}
	if (interval_contains(combining_intervals, sizeof(combining_intervals) / sizeof(combining_intervals[0]), scalar)) {
		return 0;
	}
	if (interval_contains(wide_intervals, sizeof(wide_intervals) / sizeof(wide_intervals[0]), scalar)) {
		return 2;
	}
	return 1;
}

static void
reset_iconv_state(iconv_t cd)
{
	if (cd == (iconv_t)-1) {
		return;
	}
	(void)iconv(cd, NULL, NULL, NULL, NULL);
}

static unsigned int
decode_utf8_bytes(const unsigned char *bytes, size_t length)
{
	if (!bytes || length == 0) {
		return UTF8_REPLACEMENT;
	}
	unsigned int scalar = 0;
	if (length == 1) {
	unsigned char b0 = bytes[0];
		if (b0 < 0x80) {
			return b0;
		}
		return UTF8_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		if ((b0 & 0xe0) != 0xc0 || (b1 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x1f) << 6) | (unsigned int)(b1 & 0x3f);
		if (scalar < 0x80) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 3) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		if ((b0 & 0xf0) != 0xe0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x0f) << 12) | ((unsigned int)(b1 & 0x3f) << 6) | (unsigned int)(b2 & 0x3f);
		if (scalar < 0x800 || (scalar >= 0xd800 && scalar <= 0xdfff)) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 4) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		unsigned char b3 = bytes[3];
		if ((b0 & 0xf8) != 0xf0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) {
			return UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x07) << 18) | ((unsigned int)(b1 & 0x3f) << 12) | ((unsigned int)(b2 & 0x3f) << 6) | (unsigned int)(b3 & 0x3f);
		if (scalar < 0x10000 || scalar > 0x10ffff) {
			return UTF8_REPLACEMENT;
		}
		return scalar;
	}
	return UTF8_REPLACEMENT;
}

static terse_codec_kind_t
codec_kind_from_name(const char *name)
{
	if (!name) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "UTF-8") == 0 || strcasecmp(name, "UTF8") == 0) {
		return TERSE_CODEC_UTF8;
	}
	if (strcasecmp(name, "SHIFT_JIS") == 0 || strcasecmp(name, "SHIFT-JIS") == 0 || strcasecmp(name, "Shift_JIS") == 0 || strcasecmp(name, "SJIS") == 0) {
		return TERSE_CODEC_SHIFT_JIS;
	}
	return TERSE_CODEC_UTF8;
}


static terse_capabilities_t make_p0_capabilities(void);

static int
color_kind_rank(terse_color_kind_t kind)
{
	switch (kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 0;
	case TERSE_COLOR_KIND_BASIC16:
		return 1;
	case TERSE_COLOR_KIND_PALETTE256:
		return 2;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return 3;
	default:
		return 0;
	}
}

static int
matches_da_prefix(const unsigned char *buffer, size_t length, const char *prefix)
{
	if (!buffer || length == 0 || !prefix) {
		return 0;
	}
	size_t prefix_len = strlen(prefix);
	if (prefix_len == 0 || length < prefix_len) {
		return 0;
	}
	return memcmp(buffer, prefix, prefix_len) == 0;
}

static terse_capabilities_t
make_terminal_app_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_p0_capabilities();
	caps.profile = TERSE_P1;
	caps.has_sgr_basic = 1;
	caps.has_sgr_extended = 1;
	caps.has_truecolor = has_truecolor ? 1 : 0;
	caps.has_text_styles = 1;
	caps.has_title = 1;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_BELL;
	return caps;
}

static terse_capabilities_t
make_vte_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P2;
	caps.mouse = TERSE_MOUSE_SGR;
	caps.has_bracketed_paste = 1;
	caps.has_hyperlinks = 1;
	caps.has_cursor_shape = 1;
	return caps;
}

static terse_capabilities_t
make_iterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_ITERM_INLINE;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	return caps;
}

static terse_capabilities_t
make_wezterm_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_KITTY;
	caps.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	return caps;
}

static terse_capabilities_t
make_kitty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	caps.images = TERSE_IMAGE_KITTY;
	return caps;
}

static terse_capabilities_t
make_ghostty_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_vte_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.has_clipboard_write = 1;
	return caps;
}

static terse_capabilities_t
make_sixel_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P3;
	caps.images = TERSE_IMAGE_SIXEL;
	return caps;
}

static terse_capabilities_t
make_warp_capabilities(int has_truecolor)
{
	terse_capabilities_t caps = make_terminal_app_capabilities(has_truecolor);
	caps.profile = TERSE_P1;
	return caps;
}

static void
clamp_capabilities_to_request(terse_capabilities_t *caps, terse_profile_t requested)
{
	if (!caps) {
		return;
	}
	if (requested == TERSE_PROFILE_AUTO) {
		return;
	}
	if (requested <= TERSE_P0) {
		*caps = make_p0_capabilities();
		return;
	}
	if (requested == TERSE_P1 && caps->profile > TERSE_P1) {
		caps->profile = TERSE_P1;
		caps->mouse = TERSE_MOUSE_NONE;
		caps->has_bracketed_paste = 0;
		caps->has_hyperlinks = 0;
		caps->has_clipboard_write = 0;
		caps->images = TERSE_IMAGE_NONE;
		caps->notifications &= TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (requested == TERSE_P2 && caps->profile > TERSE_P2) {
		caps->profile = TERSE_P2;
		caps->images = TERSE_IMAGE_NONE;
		caps->notifications &= ~(TERSE_NOTIFICATION_SUPPORT_DESKTOP);
	}
}

static int
is_multiplexer_session(const char *term)
{
	if (getenv("TMUX")) {
		return 1;
	}
	if (!term) {
		return 0;
	}
	if (strstr(term, "tmux") || strstr(term, "screen")) {
		return 1;
	}
	return 0;
}

static int
term_supports_sixel(const char *term)
{
	if (!term) {
		return 0;
	}
	if (strstr(term, "sixel")) {
		return 1;
	}
	if (strcmp(term, "mlterm") == 0 || strcmp(term, "mlterm-direct") == 0) {
		return 1;
	}
	if (strcmp(term, "contour") == 0) {
		return 1;
	}
	if (strcmp(term, "yaft-256color") == 0 || strcmp(term, "yaft-sixel") == 0) {
		return 1;
	}
	return 0;
}

static terse_capabilities_t
detect_environment_capabilities(terse_profile_t requested_profile, const terse_options_t *options)
{
	terse_capabilities_t caps = make_p0_capabilities();
	int auto_requested = requested_profile == TERSE_PROFILE_AUTO;
	if (!auto_requested && requested_profile == TERSE_P0) {
		return caps;
	}
	const char *term = getenv("TERM");
	const char *term_program = getenv("TERM_PROGRAM");
	const char *lc_terminal = getenv("LC_TERMINAL");
	const char *colorterm = getenv("COLORTERM");
	const char *gnome_screen = getenv("GNOME_TERMINAL_SCREEN");
	const char *gnome_service = getenv("GNOME_TERMINAL_SERVICE");
	const char *vte_version = getenv("VTE_VERSION");
	const char *secondary_hint = getenv("TERSE_SECONDARY_DA_HINT");
	unsigned char secondary[128];
	memset(secondary, 0, sizeof(secondary));
	size_t secondary_len = 0;
	if (secondary_hint && *secondary_hint) {
		size_t hint_len = strlen(secondary_hint);
		if (hint_len > sizeof(secondary)) {
			hint_len = sizeof(secondary);
		}
		memcpy(secondary, secondary_hint, hint_len);
		secondary_len = hint_len;
	} else if (options) {
		secondary_len = terse_platform_probe_secondary_da(options->input_fd, options->output_fd, secondary, sizeof(secondary));
	}
	int has_truecolor = (colorterm && strcasecmp(colorterm, "truecolor") == 0) ? 1 : 0;
	int is_mux = is_multiplexer_session(term);
	int is_terminal_app = 0;
	if (term_program && strcmp(term_program, "Apple_Terminal") == 0) {
		is_terminal_app = 1;
	}
	if (!is_terminal_app && lc_terminal && strcmp(lc_terminal, "Apple_Terminal") == 0) {
		is_terminal_app = 1;
	}
	if (!is_terminal_app && matches_da_prefix(secondary, secondary_len, "\x1b[>1;95;0c")) {
		is_terminal_app = 1;
	}
	if (is_terminal_app) {
		caps = make_terminal_app_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_warp = 0;
	if (term_program && strcmp(term_program, "WarpTerminal") == 0) {
		is_warp = 1;
	}
	if (!is_warp && matches_da_prefix(secondary, secondary_len, "\x1b[>0;95;0c")) {
		is_warp = 1;
	}
	if (is_warp) {
		caps = make_warp_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_iterm = 0;
	if (term_program && strcmp(term_program, "iTerm.app") == 0) {
		is_iterm = 1;
	}
	if (!is_iterm && lc_terminal && strcmp(lc_terminal, "iTerm2") == 0) {
		is_iterm = 1;
	}
	if (!is_iterm && matches_da_prefix(secondary, secondary_len, "\x1b[>64;")) {
		is_iterm = 1;
	}
	if (is_iterm) {
		caps = make_iterm_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_vte = 0;
	if ((gnome_screen && *gnome_screen) || (gnome_service && *gnome_service) || (vte_version && *vte_version)) {
		is_vte = 1;
	}
	if (!is_vte && matches_da_prefix(secondary, secondary_len, "\x1b[>61;")) {
		is_vte = 1;
	}
	if (!is_vte && matches_da_prefix(secondary, secondary_len, "\x1b[>65;")) {
		is_vte = 1;
	}
	if (is_vte) {
		caps = make_vte_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_wezterm = 0;
	if (term_program && strcmp(term_program, "WezTerm") == 0) {
		is_wezterm = 1;
	}
	if (!is_wezterm) {
		const char *wezexec = getenv("WEZTERM_EXECUTABLE");
		if (wezexec && *wezexec) {
			is_wezterm = 1;
		}
	}
	if (!is_wezterm && matches_da_prefix(secondary, secondary_len, "\x1b[>1;277;")) {
		is_wezterm = 1;
	}
	if (is_wezterm) {
		caps = make_wezterm_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_kitty = 0;
	if (term && strcmp(term, "xterm-kitty") == 0) {
		is_kitty = 1;
	}
	if (!is_kitty) {
		const char *kitty_pid = getenv("KITTY_PID");
		if (kitty_pid && *kitty_pid) {
			is_kitty = 1;
		}
	}
	if (!is_kitty && matches_da_prefix(secondary, secondary_len, "\x1b[>1;4000;")) {
		is_kitty = 1;
	}
	if (is_kitty) {
		caps = make_kitty_capabilities(has_truecolor);
		caps.keyboard_features |= TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL;
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	int is_ghostty = 0;
	if (term && strcmp(term, "xterm-ghostty") == 0) {
		is_ghostty = 1;
	}
	if (!is_ghostty && term_program && strcmp(term_program, "ghostty") == 0) {
		is_ghostty = 1;
	}
	if (!is_ghostty && matches_da_prefix(secondary, secondary_len, "\x1b[>1;10;")) {
		is_ghostty = 1;
	}
	if (is_ghostty) {
		caps = make_ghostty_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	if (term_supports_sixel(term)) {
		caps = make_sixel_capabilities(has_truecolor);
		if (is_mux) {
			caps.images = TERSE_IMAGE_NONE;
		}
		clamp_capabilities_to_request(&caps, requested_profile);
		return caps;
	}
	clamp_capabilities_to_request(&caps, requested_profile);
	return caps;
}

static int
color_support_rank(terse_color_support_t support)
{
	switch (support) {
	case TERSE_COLOR_NONE:
		return 0;
	case TERSE_COLOR_BASIC16:
		return 1;
	case TERSE_COLOR_PALETTE256:
		return 2;
	case TERSE_COLOR_TRUECOLOR:
		return 3;
	default:
		return 0;
	}
}

static unsigned char
cube_component_from_truecolor(unsigned char value)
{
	if (value < 48) {
		return 0;
	}
	if (value < 114) {
		return 1;
	}
	return (unsigned char)((value - 35) / 40);
}

static unsigned char
cube_component_to_value(unsigned char component)
{
	static const unsigned char values[6] = { 0, 95, 135, 175, 215, 255 };
	if (component > 5) {
		component = 5;
	}
	return values[component];
}

static unsigned char
truecolor_to_palette_index(unsigned char r, unsigned char g, unsigned char b)
{
	if (r == g && g == b) {
		if (r < 8) {
			return 16;
		}
		if (r > 248) {
			return 231;
		}
		return (unsigned char)(232 + (r - 8) / 10);
	}
	unsigned char rc = cube_component_from_truecolor(r);
	unsigned char gc = cube_component_from_truecolor(g);
	unsigned char bc = cube_component_from_truecolor(b);
	return (unsigned char)(16 + rc * 36 + gc * 6 + bc);
}

static void
palette_index_to_rgb(unsigned char index, unsigned char *r, unsigned char *g, unsigned char *b)
{
	if (index < 16) {
		*r = basic16_rgb[index].r;
		*g = basic16_rgb[index].g;
		*b = basic16_rgb[index].b;
		return;
	}
	if (index < 232) {
		unsigned char adj = (unsigned char)(index - 16);
		unsigned char rc = (unsigned char)(adj / 36);
		unsigned char gc = (unsigned char)((adj / 6) % 6);
		unsigned char bc = (unsigned char)(adj % 6);
		*r = cube_component_to_value(rc);
		*g = cube_component_to_value(gc);
		*b = cube_component_to_value(bc);
		return;
	}
	unsigned char level = (unsigned char)(8 + (index - 232) * 10);
	*r = level;
	*g = level;
	*b = level;
}

static unsigned char
closest_basic16_index(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned int best_distance = UINT_MAX;
	unsigned char best_index = 0;
	for (unsigned char i = 0; i < 16; ++i) {
		int dr = (int)r - (int)basic16_rgb[i].r;
		int dg = (int)g - (int)basic16_rgb[i].g;
		int db = (int)b - (int)basic16_rgb[i].b;
		unsigned int distance = (unsigned int)(dr * dr + dg * dg + db * db);
		if (distance < best_distance) {
			best_distance = distance;
			best_index = i;
		}
	}
	return best_index;
}

terse_color_t
terse_color_default(void)
{
	terse_color_t color = { .kind = TERSE_COLOR_KIND_DEFAULT };
	return color;
}

terse_color_t
terse_color_basic(terse_basic_color_t color, int bright)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_BASIC16,
		.data.basic16 = {
			.color = color,
			.bright = bright ? 1 : 0,
		},
	};
	return result;
}

terse_color_t
terse_color_palette(unsigned char index)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_PALETTE256,
		.data.palette = {
			.value = index,
		},
	};
	return result;
}

terse_color_t
terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b)
{
	terse_color_t result = {
		.kind = TERSE_COLOR_KIND_TRUECOLOR,
		.data.truecolor = {
			.r = r,
			.g = g,
			.b = b,
		},
	};
	return result;
}

terse_style_t
terse_style_default(void)
{
	terse_style_t style = {
		.foreground = terse_color_default(),
		.background = terse_color_default(),
		.effects = 0,
	};
	return style;
}

static unsigned int
mask_effects(unsigned int effects)
{
	return effects & TERSE_STYLE_ALL_SUPPORTED;
}

static int
colors_equal(const terse_color_t *a, const terse_color_t *b)
{
	if (a->kind != b->kind) {
		return 0;
	}
	switch (a->kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 1;
	case TERSE_COLOR_KIND_BASIC16:
		return a->data.basic16.color == b->data.basic16.color && a->data.basic16.bright == b->data.basic16.bright;
	case TERSE_COLOR_KIND_PALETTE256:
		return a->data.palette.value == b->data.palette.value;
	case TERSE_COLOR_KIND_TRUECOLOR:
		return a->data.truecolor.r == b->data.truecolor.r && a->data.truecolor.g == b->data.truecolor.g && a->data.truecolor.b == b->data.truecolor.b;
	default:
		return 0;
	}
}

static int
styles_equal(const terse_style_t *a, const terse_style_t *b)
{
	if (a->effects != b->effects) {
		return 0;
	}
	if (!colors_equal(&a->foreground, &b->foreground)) {
		return 0;
	}
	if (!colors_equal(&a->background, &b->background)) {
		return 0;
	}
	return 1;
}

static terse_color_t
degrade_color(terse_color_t color, terse_color_support_t support)
{
	if (color.kind == TERSE_COLOR_KIND_DEFAULT) {
		return color;
	}
	int support_level = color_support_rank(support);
	if (support_level == 0) {
		return terse_color_default();
	}
	int requested_level = color_kind_rank(color.kind);
	if (requested_level <= support_level) {
		return color;
	}
	switch (support) {
	case TERSE_COLOR_BASIC16:
		{
			unsigned char r = 0;
			unsigned char g = 0;
			unsigned char b = 0;
			if (color.kind == TERSE_COLOR_KIND_TRUECOLOR) {
				r = color.data.truecolor.r;
				g = color.data.truecolor.g;
				b = color.data.truecolor.b;
			} else if (color.kind == TERSE_COLOR_KIND_PALETTE256) {
				palette_index_to_rgb(color.data.palette.value, &r, &g, &b);
			}
			unsigned char idx = closest_basic16_index(r, g, b);
			terse_color_t basic = {
				.kind = TERSE_COLOR_KIND_BASIC16,
				.data.basic16 = {
					.color = (terse_basic_color_t)(idx % 8),
					.bright = idx >= 8,
				},
			};
			return basic;
		}
	case TERSE_COLOR_PALETTE256:
		{
			if (color.kind == TERSE_COLOR_KIND_TRUECOLOR) {
				unsigned char idx = truecolor_to_palette_index(color.data.truecolor.r, color.data.truecolor.g, color.data.truecolor.b);
				terse_color_t palette = {
					.kind = TERSE_COLOR_KIND_PALETTE256,
					.data.palette = { .value = idx },
				};
				return palette;
			}
			break;
		}
	case TERSE_COLOR_TRUECOLOR:
		break;
	default:
		break;
	}
	return terse_color_default();
}

static terse_style_t
sanitize_style_request(const terse_style_t *style)
{
	terse_style_t sanitized = *style;
	sanitized.effects = mask_effects(style->effects);
	return sanitized;
}

static terse_style_t
make_effective_style(const terse_capabilities_t *caps, const terse_style_t *requested)
{
	terse_style_t effective = *requested;
	effective.effects &= caps->effects;
	effective.foreground = degrade_color(requested->foreground, caps->colors);
	effective.background = degrade_color(requested->background, caps->colors);
	return effective;
}

struct terse_handle {
	terse_profile_t requested_profile;
	terse_capabilities_t capabilities;
	terse_capabilities_t detected_capabilities;
	terse_options_t options;
	terse_codec_kind_t codec_kind;
	iconv_t codec_to_utf8;
	iconv_t utf8_to_codec;
	terse_size_t size;
	int cursor_visible;
	int cursor_row;
	int cursor_col;
	int cursor_known;
	terse_style_t style;
	terse_style_t effective_style;
	int style_known;
	terse_mouse_mode_t mouse_mode;
	int mouse_enabled;
	terse_mouse_button_t mouse_button;
	int paste_enabled;
	terse_error_category_t last_error;
	int last_errno;
	unsigned int runtime_enabled;
	unsigned int runtime_disabled;
	unsigned int keyboard_supported;
	unsigned int keyboard_enabled;
	// State history
	terse_state_t state_stack[TERSE_STATE_STACK_MAX];
	int state_stack_top; // -1 when empty
	unsigned char pending_byte;
	int has_pending_byte;
};

static void
update_effective_style(terse_handle_t handle)
{
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 1;
}

static void set_char_event(terse_handle_t handle, terse_event_t *event, unsigned int scalar, int mods);
static void set_key_event(terse_event_t *event, terse_event_type_t type, int mods);
static void set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length);
static int write_literal(terse_handle_t handle, const char *literal);
static int write_sequence(terse_handle_t handle, const char *sequence, size_t length);
static void set_error(terse_handle_t handle, terse_error_category_t category, int code);
static int send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
static int send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
static int send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);

static int
initialize_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return -1;
	}
	handle->codec_kind = codec_kind_from_name(handle->options.codec_name);
	handle->codec_to_utf8 = (iconv_t)-1;
	handle->utf8_to_codec = (iconv_t)-1;
	if (handle->codec_kind == TERSE_CODEC_SHIFT_JIS) {
		handle->codec_to_utf8 = iconv_open("UTF-8", "SHIFT_JIS");
		if (handle->codec_to_utf8 == (iconv_t)-1) {
			return -1;
		}
		handle->utf8_to_codec = iconv_open("SHIFT_JIS", "UTF-8");
		if (handle->utf8_to_codec == (iconv_t)-1) {
			iconv_close(handle->codec_to_utf8);
			handle->codec_to_utf8 = (iconv_t)-1;
			return -1;
		}
	}
	return 0;
}

static void
destroy_codec_handles(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (handle->codec_to_utf8 != (iconv_t)-1) {
		iconv_close(handle->codec_to_utf8);
		handle->codec_to_utf8 = (iconv_t)-1;
	}
	if (handle->utf8_to_codec != (iconv_t)-1) {
		iconv_close(handle->utf8_to_codec);
		handle->utf8_to_codec = (iconv_t)-1;
	}
}

static int
decode_utf8_stream(int fd, unsigned char first, unsigned int *out_scalar)
{
	unsigned char bytes[4] = { 0 };
	bytes[0] = first;
	int expected = 0;
	if (first < 0x80) {
		*out_scalar = first;
		return 0;
	}
	if ((first & 0xe0) == 0xc0) {
		expected = 2;
		if ((first & 0x1e) == 0) {
			*out_scalar = UTF8_REPLACEMENT;
			return 0;
		}
	} else if ((first & 0xf0) == 0xe0) {
		expected = 3;
	} else if ((first & 0xf8) == 0xf0) {
		expected = 4;
		if (first > 0xf4) {
			*out_scalar = UTF8_REPLACEMENT;
			return 0;
		}
	} else {
		*out_scalar = UTF8_REPLACEMENT;
		return 0;
	}
	for (int i = 1; i < expected; ++i) {
		unsigned char next = 0;
		ssize_t n = terse_platform_read_byte(fd, &next);
		if (n <= 0) {
			if (n == 0) {
				errno = EPIPE;
				return -EPIPE;
			}
			return (int)n;
		}
		bytes[i] = next;
	}
	*out_scalar = decode_utf8_bytes(bytes, (size_t)expected);
	return 0;
}

static unsigned int
convert_shift_jis_pair(terse_handle_t handle, unsigned char lead, unsigned char trail)
{
	(void)lead;
	(void)trail;
	if (!handle || handle->codec_to_utf8 == (iconv_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	char inbuf[2];
	inbuf[0] = (char)lead;
	inbuf[1] = (char)trail;
	char *in_ptr = inbuf;
	size_t in_left = sizeof(inbuf);
	char outbuf[8] = { 0 };
	char *out_ptr = outbuf;
	size_t out_left = sizeof(outbuf);
	reset_iconv_state(handle->codec_to_utf8);
	if (iconv(handle->codec_to_utf8, &in_ptr, &in_left, &out_ptr, &out_left) == (size_t)-1) {
		return SHIFT_JIS_REPLACEMENT;
	}
	if (in_left != 0) {
		return SHIFT_JIS_REPLACEMENT;
	}
	size_t produced = (size_t)(out_ptr - outbuf);
	return decode_utf8_bytes((const unsigned char *)outbuf, produced);
}

static int
decode_shift_jis_stream(terse_handle_t handle, int fd, unsigned char first, unsigned int *out_scalar)
{
	if (first <= 0x7f) {
		*out_scalar = first;
		return 0;
	}
	if (first >= 0xa1 && first <= 0xdf) {
		*out_scalar = 0xff61u + (unsigned int)(first - 0xa1);
		return 0;
	}
	if ((first >= 0x81 && first <= 0x9f) || (first >= 0xe0 && first <= 0xfc)) {
		unsigned char second = 0;
		ssize_t n = terse_platform_read_byte(fd, &second);
		if (n <= 0) {
			if (n == 0) {
				errno = EPIPE;
				return -EPIPE;
			}
			return (int)n;
		}
		if (!((second >= 0x40 && second <= 0x7e) || (second >= 0x80 && second <= 0xfc))) {
			*out_scalar = SHIFT_JIS_REPLACEMENT;
			return 0;
		}
		unsigned int scalar = convert_shift_jis_pair(handle, first, second);
		if (scalar == UTF8_REPLACEMENT) {
			*out_scalar = SHIFT_JIS_REPLACEMENT;
		} else {
			*out_scalar = scalar;
		}
		return 0;
	}
	*out_scalar = SHIFT_JIS_REPLACEMENT;
	return 0;
}

static int
decode_stream_char(terse_handle_t handle, int fd, unsigned char first, terse_event_t *event)
{
	unsigned int scalar = 0;
	int rc = 0;
	switch (handle->codec_kind) {
	case TERSE_CODEC_UTF8:
		rc = decode_utf8_stream(fd, first, &scalar);
		if (rc < 0) {
			return rc;
		}
		if (scalar == 0) {
			scalar = UTF8_REPLACEMENT;
		}
		break;
	case TERSE_CODEC_SHIFT_JIS:
		rc = decode_shift_jis_stream(handle, fd, first, &scalar);
		if (rc < 0) {
			return rc;
		}
		break;
	default:
		scalar = first;
		break;
	}
	set_char_event(handle, event, scalar, 0);
	return 0;
}

static unsigned int
disable_mask_from_enable(unsigned int enable_mask)
{
	unsigned int mask = 0;
	if (enable_mask & TERSE_CAP_ENABLE_SGR_BASIC) {
		mask |= TERSE_CAP_DISABLE_SGR_BASIC;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TEXT_STYLES) {
		mask |= TERSE_CAP_DISABLE_TEXT_STYLES;
	}
	if (enable_mask & TERSE_CAP_ENABLE_SGR_EXTENDED) {
		mask |= TERSE_CAP_DISABLE_SGR_EXTENDED;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TRUECOLOR) {
		mask |= TERSE_CAP_DISABLE_TRUECOLOR;
	}
	if (enable_mask & TERSE_CAP_ENABLE_MOUSE) {
		mask |= TERSE_CAP_DISABLE_MOUSE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_BRACKETED_PASTE) {
		mask |= TERSE_CAP_DISABLE_BRACKETED_PASTE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_TITLE) {
		mask |= TERSE_CAP_DISABLE_TITLE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_HYPERLINK) {
		mask |= TERSE_CAP_DISABLE_HYPERLINK;
	}
	if (enable_mask & TERSE_CAP_ENABLE_CURSOR_SHAPE) {
		mask |= TERSE_CAP_DISABLE_CURSOR_SHAPE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_CLIPBOARD_WRITE) {
		mask |= TERSE_CAP_DISABLE_CLIPBOARD_WRITE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_IMAGE_INLINE) {
		mask |= TERSE_CAP_DISABLE_IMAGE_INLINE;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_BELL) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_BELL;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_VISUAL) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_VISUAL;
	}
	if (enable_mask & TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP) {
		mask |= TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP;
	}
	return mask;
}

static unsigned int
enable_mask_from_disable(unsigned int disable_mask)
{
	unsigned int mask = 0;
	if (disable_mask & TERSE_CAP_DISABLE_SGR_BASIC) {
		mask |= TERSE_CAP_ENABLE_SGR_BASIC;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TEXT_STYLES) {
		mask |= TERSE_CAP_ENABLE_TEXT_STYLES;
	}
	if (disable_mask & TERSE_CAP_DISABLE_SGR_EXTENDED) {
		mask |= TERSE_CAP_ENABLE_SGR_EXTENDED;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TRUECOLOR) {
		mask |= TERSE_CAP_ENABLE_TRUECOLOR;
	}
	if (disable_mask & TERSE_CAP_DISABLE_MOUSE) {
		mask |= TERSE_CAP_ENABLE_MOUSE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_BRACKETED_PASTE) {
		mask |= TERSE_CAP_ENABLE_BRACKETED_PASTE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_TITLE) {
		mask |= TERSE_CAP_ENABLE_TITLE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_HYPERLINK) {
		mask |= TERSE_CAP_ENABLE_HYPERLINK;
	}
	if (disable_mask & TERSE_CAP_DISABLE_CURSOR_SHAPE) {
		mask |= TERSE_CAP_ENABLE_CURSOR_SHAPE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_CLIPBOARD_WRITE) {
		mask |= TERSE_CAP_ENABLE_CLIPBOARD_WRITE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_IMAGE_INLINE) {
		mask |= TERSE_CAP_ENABLE_IMAGE_INLINE;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_BELL) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_BELL;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_VISUAL) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_VISUAL;
	}
	if (disable_mask & TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP) {
		mask |= TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP;
	}
	return mask;
}

static void
recompute_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	handle->capabilities = handle->detected_capabilities;

	unsigned int disabled = handle->options.disabled_caps | handle->runtime_disabled;
	if (disabled & TERSE_CAP_DISABLE_BASIC_OUTPUT) {
		handle->capabilities.has_basic_output = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CURSOR_VISIBILITY) {
		handle->capabilities.has_cursor_visibility = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_ABSOLUTE) {
		handle->capabilities.has_move_absolute = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOVE_RELATIVE) {
		handle->capabilities.has_move_relative = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_LINE) {
		handle->capabilities.has_clear_line = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLEAR_SCREEN) {
		handle->capabilities.has_clear_screen = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SIZE) {
		handle->capabilities.has_size = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_BASIC) {
		handle->capabilities.has_sgr_basic = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_SGR_EXTENDED) {
		handle->capabilities.has_sgr_extended = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TRUECOLOR) {
		handle->capabilities.has_truecolor = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TEXT_STYLES) {
		handle->capabilities.has_text_styles = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_MOUSE) {
		handle->capabilities.mouse = TERSE_MOUSE_NONE;
	}
	if (disabled & TERSE_CAP_DISABLE_BRACKETED_PASTE) {
		handle->capabilities.has_bracketed_paste = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_TITLE) {
		handle->capabilities.has_title = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_HYPERLINK) {
		handle->capabilities.has_hyperlinks = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CURSOR_SHAPE) {
		handle->capabilities.has_cursor_shape = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_CLIPBOARD_WRITE) {
		handle->capabilities.has_clipboard_write = 0;
	}
	if (disabled & TERSE_CAP_DISABLE_IMAGE_INLINE) {
		handle->capabilities.images = TERSE_IMAGE_NONE;
	}
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_BELL) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_VISUAL) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_VISUAL;
	}
	if (disabled & TERSE_CAP_DISABLE_NOTIFICATION_DESKTOP) {
		handle->capabilities.notifications &= ~TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	}

	unsigned int enabled = handle->options.enabled_caps | handle->runtime_enabled;
	if (enabled & TERSE_CAP_ENABLE_SGR_BASIC) {
		handle->capabilities.has_sgr_basic = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TEXT_STYLES) {
		handle->capabilities.has_text_styles = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_SGR_EXTENDED) {
		handle->capabilities.has_sgr_extended = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TRUECOLOR) {
		handle->capabilities.has_truecolor = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_MOUSE) {
		handle->capabilities.mouse = TERSE_MOUSE_SGR;
	}
	if (enabled & TERSE_CAP_ENABLE_BRACKETED_PASTE) {
		handle->capabilities.has_bracketed_paste = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_TITLE) {
		handle->capabilities.has_title = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_HYPERLINK) {
		handle->capabilities.has_hyperlinks = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_CURSOR_SHAPE) {
		handle->capabilities.has_cursor_shape = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_CLIPBOARD_WRITE) {
		handle->capabilities.has_clipboard_write = 1;
	}
	if (enabled & TERSE_CAP_ENABLE_IMAGE_INLINE) {
		if (handle->detected_capabilities.images != TERSE_IMAGE_NONE) {
			handle->capabilities.images = handle->detected_capabilities.images;
		} else if (handle->capabilities.images == TERSE_IMAGE_NONE) {
			handle->capabilities.images = TERSE_IMAGE_ITERM_INLINE;
		}
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_BELL) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_BELL;
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_VISUAL) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_VISUAL;
	}
	if (enabled & TERSE_CAP_ENABLE_NOTIFICATION_DESKTOP) {
		handle->capabilities.notifications |= TERSE_NOTIFICATION_SUPPORT_DESKTOP;
	}

	if (handle->capabilities.has_truecolor) {
		handle->capabilities.colors = TERSE_COLOR_TRUECOLOR;
	} else if (handle->capabilities.has_sgr_extended) {
		handle->capabilities.colors = TERSE_COLOR_PALETTE256;
	} else if (handle->capabilities.has_sgr_basic) {
		handle->capabilities.colors = TERSE_COLOR_BASIC16;
	} else {
		handle->capabilities.colors = TERSE_COLOR_NONE;
	}
	handle->capabilities.effects = handle->capabilities.has_text_styles ? TERSE_STYLE_ALL_SUPPORTED : 0;
	if (handle->style_known) {
		handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	}
}

static void
set_mouse_event(terse_event_t *event, terse_event_type_t type, terse_mouse_button_t button, int mods, int row, int col)
{
	event->type = type;
	event->data.mouse.button = button;
	event->data.mouse.mods = mods;
	event->data.mouse.row = row;
	event->data.mouse.col = col;
}

static int mouse_modifiers_from_param(int param);
static void clear_error(terse_handle_t handle);

static char *
base64_encode(const unsigned char *data, size_t length, size_t *out_len)
{
	if (!data || length == 0) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t encoded = ((length + 2) / 3) * 4;
	char *output = malloc(encoded + 1);
	if (!output) {
		if (out_len) {
			*out_len = 0;
		}
		return NULL;
	}
	size_t out_index = 0;
	for (size_t i = 0; i < length; i += 3) {
		unsigned int triple = data[i] << 16;
		if (i + 1 < length) {
			triple |= data[i + 1] << 8;
		}
		if (i + 2 < length) {
			triple |= data[i + 2];
		}
		output[out_index++] = BASE64_ALPHABET[(triple >> 18) & 0x3f];
		output[out_index++] = BASE64_ALPHABET[(triple >> 12) & 0x3f];
		if (i + 1 < length) {
			output[out_index++] = BASE64_ALPHABET[(triple >> 6) & 0x3f];
		} else {
			output[out_index++] = '=';
		}
		if (i + 2 < length) {
			output[out_index++] = BASE64_ALPHABET[triple & 0x3f];
		} else {
			output[out_index++] = '=';
		}
	}
	output[out_index] = '\0';
	if (out_len) {
		*out_len = out_index;
	}
	return output;
}

static int
send_iterm_inline_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	size_t name_len = 0;
	char *name_encoded = base64_encode((const unsigned char *)name, strlen(name), &name_len);
	size_t data_len = 0;
	char *data_encoded = base64_encode(data, size, &data_len);
	if (!name_encoded || !data_encoded) {
		free(name_encoded);
		free(data_encoded);
		errno = ENOMEM;
		set_error(handle, TERSE_ERROR_RESOURCE, ENOMEM);
		return -ENOMEM;
	}
	char header[256];
	int header_len = snprintf(header,
		sizeof(header),
		"\x1b]1337;File=name=%s;size=%zu;inline=1:",
		name_encoded,
		size);
	free(name_encoded);
	if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
		free(data_encoded);
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	if (write_sequence(handle, header, (size_t)header_len) < 0) {
		free(data_encoded);
		return -handle->last_errno;
	}
	if (write_sequence(handle, data_encoded, data_len) < 0) {
		free(data_encoded);
		return -handle->last_errno;
	}
	free(data_encoded);
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

static int
send_sixel_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	static const char prefix[] = "\x1bPq";
	static const char suffix[] = "\x1b\\";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) < 0) {
		return -handle->last_errno;
	}
	const size_t chunk_size = 1024;
	size_t offset = 0;
	while (offset < size) {
		size_t remaining = size - offset;
		size_t to_write = remaining > chunk_size ? chunk_size : remaining;
		if (write_sequence(handle, (const char *)data + offset, to_write) < 0) {
			return -handle->last_errno;
		}
		offset += to_write;
	}
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

static int
send_kitty_image(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	(void)name;
	size_t encoded_len = 0;
	char *encoded = base64_encode(data, size, &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERROR_RESOURCE, ENOMEM);
		return -ENOMEM;
	}
	const char prefix[] = "\x1b_Ga=T,f=100,m=1;";
	if (write_sequence(handle, prefix, sizeof(prefix) - 1) < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	if (write_sequence(handle, encoded, encoded_len) < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	free(encoded);
	const char suffix[] = "\x1b\\";
	if (write_sequence(handle, suffix, sizeof(suffix) - 1) < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

static int
handle_sgr_mouse_sequence(terse_handle_t handle, terse_event_t *out_event, const int *values, size_t value_count, char final)
{
	if (!handle || value_count < 3) {
		return 0;
	}
	if (!handle->mouse_enabled || handle->mouse_mode == TERSE_MOUSE_NONE) {
		return 0;
	}
	int raw_cb = values[0];
	int col = values[1];
	int row = values[2];
	if (col < 0 || row < 0) {
		return 0;
	}
	int mods = mouse_modifiers_from_param(raw_cb);
	int cb = raw_cb & ~(4 | 8 | 16);
	int is_motion = cb & 32;
	int is_wheel = cb & 64;
	int base = cb & 3;
	terse_mouse_button_t button = TERSE_MOUSE_BUTTON_NONE;
	terse_event_type_t type = TERSE_EVENT_MOUSE_MOVE;
	if (is_wheel) {
		button = (base == 0) ? TERSE_MOUSE_BUTTON_SCROLL_UP : TERSE_MOUSE_BUTTON_SCROLL_DOWN;
		type = TERSE_EVENT_MOUSE_SCROLL;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (final == 'm') {
		if (base <= 2) {
			button = (base == 0) ? TERSE_MOUSE_BUTTON_LEFT : (base == 1 ? TERSE_MOUSE_BUTTON_MIDDLE : TERSE_MOUSE_BUTTON_RIGHT);
		} else {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_UP;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	} else if (is_motion) {
		if (handle->mouse_button != TERSE_MOUSE_BUTTON_NONE) {
			button = handle->mouse_button;
		}
		type = TERSE_EVENT_MOUSE_MOVE;
	} else {
		if (base == 0) {
			button = TERSE_MOUSE_BUTTON_LEFT;
		} else if (base == 1) {
			button = TERSE_MOUSE_BUTTON_MIDDLE;
		} else if (base == 2) {
			button = TERSE_MOUSE_BUTTON_RIGHT;
		}
		type = TERSE_EVENT_MOUSE_DOWN;
		handle->mouse_button = button;
	}
	set_mouse_event(out_event, type, button, mods, row, col);
	clear_error(handle);
	return 1;
}

static void
set_error(terse_handle_t handle, terse_error_category_t category, int code)
{
	if (!handle) {
		return;
	}
	handle->last_error = category;
	handle->last_errno = code;
}

static void
clear_error(terse_handle_t handle)
{
	set_error(handle, TERSE_ERROR_NONE, 0);
}

static terse_capabilities_t
make_p0_capabilities(void)
{
	terse_capabilities_t caps = {
		.profile = TERSE_P0,
		.has_basic_output = 1,
		.has_cursor_visibility = 1,
		.has_move_absolute = 1,
		.has_move_relative = 1,
		.has_clear_line = 1,
		.has_clear_screen = 1,
		.has_size = 0,
		.has_sgr_basic = 0,
		.has_sgr_extended = 0,
		.has_truecolor = 0,
		.has_text_styles = 0,
		.mouse = TERSE_MOUSE_NONE,
		.has_bracketed_paste = 0,
		.has_title = 0,
		.has_hyperlinks = 0,
		.has_cursor_shape = 0,
		.colors = TERSE_COLOR_NONE,
		.effects = 0,
		.has_clipboard_write = 0,
		.images = TERSE_IMAGE_NONE,
		.notifications = 0,
		.keyboard_features = 0,
	};
	return caps;
}

static void emit_reset_sequences(terse_handle_t handle);

int terse_validate_options(const terse_options_t *options)
{
	if (!options) {
		return 0;
	}
	if (options->input_fd < 0 || options->output_fd < 0) {
		errno = EBADF;
		return -EBADF;
	}
	return 0;
}

static terse_size_t
make_unknown_size(void)
{
	terse_size_t size = {
		.rows = 0,
		.cols = 0,
		.known = 0,
	};
	return size;
}

static void
refresh_size(terse_handle_t handle)
{
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		handle->size = make_unknown_size();
		return;
	}
	terse_size_t size = terse_platform_query_fd_size(handle->options.output_fd);
	if (!size.known && handle->options.input_fd != handle->options.output_fd) {
		size = terse_platform_query_fd_size(handle->options.input_fd);
	}
	if (size.known || !handle->size.known) {
		handle->size = size;
	}
}

terse_handle_t
terse_open(terse_profile_t requested_profile, const terse_options_t *options)
{
	if (requested_profile != TERSE_PROFILE_AUTO && (requested_profile < TERSE_P0 || requested_profile > TERSE_P3)) {
		return NULL;
	}
	if (terse_validate_options(options) < 0) {
		return NULL;
	}

	terse_handle_t handle = malloc(sizeof(*handle));
	if (!handle) {
		return NULL;
	}
	memset(handle, 0, sizeof(*handle));

	// Initialize state stack
	handle->state_stack_top = -1;

	handle->requested_profile = requested_profile;
	handle->capabilities = make_p0_capabilities();

	terse_options_t defaults = terse_platform_default_options();
	if (options) {
		handle->options = *options;
		if (!handle->options.codec_name) {
			handle->options.codec_name = defaults.codec_name;
		}
	} else {
		handle->options = defaults;
	}
	if (initialize_codec_handles(handle) < 0) {
		int err = errno ? errno : EINVAL;
		destroy_codec_handles(handle);
		free(handle);
		errno = err;
		return NULL;
	}
	handle->size = make_unknown_size();
	handle->capabilities = detect_environment_capabilities(handle->requested_profile, &handle->options);
	handle->detected_capabilities = handle->capabilities;
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	handle->keyboard_supported = handle->capabilities.keyboard_features;
	handle->keyboard_enabled = 0;
	recompute_capabilities(handle);
	handle->cursor_visible = 1;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_known = 0;
	handle->style = terse_style_default();
	update_effective_style(handle);
	handle->mouse_mode = TERSE_MOUSE_NONE;
	handle->mouse_enabled = 0;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	handle->paste_enabled = 0;
	clear_error(handle);
	refresh_size(handle);
	handle->has_pending_byte = 0;

	return handle;
}

void terse_close(terse_handle_t handle)
{
	if (handle) {
		if (handle->keyboard_enabled) {
			(void)terse_keyboard_disable(handle, handle->keyboard_enabled);
		}
	}
	emit_reset_sequences(handle);
	destroy_codec_handles(handle);
	free(handle);
}

terse_capabilities_t
terse_get_capabilities(terse_handle_t handle)
{
	if (!handle) {
		return make_p0_capabilities();
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	clear_error(handle);
	return handle->capabilities;
}

static int
ensure_handle(terse_handle_t handle)
{
	if (!handle) {
		errno = EINVAL;
		return -EINVAL;
	}
	return 0;
}

static void
apply_runtime_overrides(terse_handle_t handle)
{
	recompute_capabilities(handle);
	clear_error(handle);
}

int terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (enable_mask == 0) {
		clear_error(handle);
		return 0;
	}
	handle->runtime_enabled |= enable_mask;
	handle->runtime_disabled &= ~disable_mask_from_enable(enable_mask);
	apply_runtime_overrides(handle);
	return 0;
}

int terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (disable_mask == 0) {
		clear_error(handle);
		return 0;
	}
	handle->runtime_disabled |= disable_mask;
	handle->runtime_enabled &= ~enable_mask_from_disable(disable_mask);
	apply_runtime_overrides(handle);
	return 0;
}

int terse_capabilities_reset_overrides(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	handle->runtime_enabled = 0;
	handle->runtime_disabled = 0;
	apply_runtime_overrides(handle);
	return 0;
}

int terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (feature_mask == 0) {
		clear_error(handle);
		return 0;
	}
	unsigned int supported = handle->keyboard_supported & feature_mask;
	unsigned int to_enable = supported & ~handle->keyboard_enabled;
	if (to_enable == 0) {
		clear_error(handle);
		return 0;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (to_enable & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
		rc = write_literal(handle, TERSE_MODIFY_OTHER_KEYS_ENABLE_SEQ);
		if (rc < 0) {
			return rc;
		}
	}
	if (to_enable & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
		rc = write_literal(handle, TERSE_KITTY_PROTOCOL_ENABLE_SEQ);
		if (rc < 0) {
			return rc;
		}
	}
	handle->keyboard_enabled |= to_enable;
	clear_error(handle);
	return 0;
}

int terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (feature_mask == 0) {
		clear_error(handle);
		return 0;
	}
	unsigned int to_disable = feature_mask & handle->keyboard_enabled;
	if (to_disable == 0) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (to_disable & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
			rc = write_literal(handle, TERSE_MODIFY_OTHER_KEYS_DISABLE_SEQ);
			if (rc < 0) {
				return rc;
			}
		}
		if (to_disable & TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL) {
			rc = write_literal(handle, TERSE_KITTY_PROTOCOL_DISABLE_SEQ);
			if (rc < 0) {
				return rc;
			}
		}
	}
	handle->keyboard_enabled &= ~to_disable;
	clear_error(handle);
	return 0;
}

unsigned int terse_keyboard_get_enabled(terse_handle_t handle)
{
	if (!handle) {
		return 0;
	}
	return handle->keyboard_enabled;
}

unsigned int terse_keyboard_get_supported(terse_handle_t handle)
{
	if (!handle) {
		return 0;
	}
	return handle->keyboard_supported;
}

int terse_state_override(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (state->cursor_known) {
		handle->cursor_known = 1;
		handle->cursor_row = state->cursor_row > 0 ? state->cursor_row : 1;
		handle->cursor_col = state->cursor_col > 0 ? state->cursor_col : 1;
	} else {
		handle->cursor_known = 0;
		if (state->cursor_row > 0) {
			handle->cursor_row = state->cursor_row;
		}
		if (state->cursor_col > 0) {
			handle->cursor_col = state->cursor_col;
		}
	}
	handle->cursor_visible = state->cursor_visible ? 1 : 0;
	if (state->style_known) {
		terse_style_t sanitized = sanitize_style_request(&state->style);
		handle->style = sanitized;
		handle->effective_style = make_effective_style(&handle->capabilities, &sanitized);
		handle->style_known = 1;
	} else {
		handle->style_known = 0;
	}
	clear_error(handle);
	return 0;
}

int terse_state_clear(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	handle->cursor_known = 0;
	handle->cursor_row = 0;
	handle->cursor_col = 0;
	handle->cursor_visible = 1;
	handle->style = terse_style_default();
	handle->effective_style = make_effective_style(&handle->capabilities, &handle->style);
	handle->style_known = 0;
	clear_error(handle);
	return 0;
}

// ---------- State history helpers ----------
int terse_push_state(terse_handle_t handle)
{
	if (!handle) {
		return -EINVAL;
	}
	if (handle->state_stack_top >= TERSE_STATE_STACK_MAX - 1) {
		set_error(handle, TERSE_ERROR_STACK_OVERFLOW, EINVAL);
		return -EINVAL;
	}
	terse_state_t *stack_state = &handle->state_stack[handle->state_stack_top + 1];
	stack_state->cursor_known = handle->cursor_known;
	stack_state->cursor_visible = handle->cursor_visible;
	stack_state->cursor_row = handle->cursor_row;
	stack_state->cursor_col = handle->cursor_col;
	stack_state->style_known = handle->style_known;
	stack_state->style = handle->style;
	handle->state_stack_top++;
	return 0;
}

int terse_pop_state(terse_handle_t handle)
{
	if (!handle) {
		return -EINVAL;
	}
	if (handle->state_stack_top < 0) {
		set_error(handle, TERSE_ERROR_STACK_UNDERFLOW, EINVAL);
		return -EINVAL;
	}
	const terse_state_t *state = &handle->state_stack[handle->state_stack_top];
	handle->cursor_known = state->cursor_known;
	handle->cursor_visible = state->cursor_visible;
	handle->cursor_row = state->cursor_row;
	handle->cursor_col = state->cursor_col;
	handle->style_known = state->style_known;
	handle->style = state->style;
	if (state->style_known) {
		handle->effective_style = make_effective_style(&handle->capabilities, &state->style);
	}
	handle->state_stack_top--;
	return 0;
}

static int
write_literal(terse_handle_t handle, const char *literal)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!literal) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int out = terse_platform_write_bytes(handle->options.output_fd, literal, strlen(literal));
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static int
write_sequence(terse_handle_t handle, const char *sequence, size_t length)
{
	int out = terse_platform_write_bytes(handle->options.output_fd, sequence, length);
	if (out < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -out);
	} else {
		clear_error(handle);
	}
	return out;
}

static int
payload_has_disallowed_chars(const char *payload)
{
	if (!payload) {
		return 0;
	}
	for (const unsigned char *p = (const unsigned char *)payload; *p; ++p) {
		if (*p == 0x07 || *p == 0x1b) {
			return 1;
		}
	}
	return 0;
}

static void
emit_reset_sequences(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	if (!handle->capabilities.has_basic_output) {
		return;
	}
	if (handle->mouse_enabled) {
		(void)terse_disable_mouse(handle);
	}
	if (handle->paste_enabled) {
		(void)terse_disable_bracketed_paste(handle);
	}
	static const char *const cursor_on_seq = "\x1b[?25h";
	if (!handle->cursor_visible) {
		if (write_sequence(handle, cursor_on_seq, strlen(cursor_on_seq)) == 0) {
			handle->cursor_visible = 1;
		}
	}
	if (write_sequence(handle, TERSE_RESET_ALL_SEQ, sizeof(TERSE_RESET_ALL_SEQ) - 1) == 0) {
		handle->style = terse_style_default();
		update_effective_style(handle);
	}
}

static int
parse_csi_sequence(const unsigned char *seq, size_t len, int *values, size_t max_values, size_t *value_count, char *final)
{
	if (len < 2 || seq[0] != 0x1b || seq[1] != '[') {
		return -1;
	}
	if (len < 3) {
		return -1;
	}
	char terminator = (char)seq[len - 1];
	if (terminator < '@' || terminator > '~') {
		return -1;
	}
	size_t count = 0;
	size_t index = 2;
	while (index < len - 1) {
		if (seq[index] == '<') {
			++index;
			continue;
		}
		int value = 0;
		int has_digit = 0;
		while (index < len - 1 && isdigit((unsigned char)seq[index])) {
			has_digit = 1;
			value = (value * 10) + (seq[index] - '0');
			++index;
		}
		if (has_digit) {
			if (count < max_values) {
				values[count] = value;
			}
			++count;
		}
		if (index >= len - 1) {
			break;
		}
		if (seq[index] == ';') {
			++index;
			continue;
		}
		break;
	}
	*value_count = count;
	*final = terminator;
	return 0;
}

static int
modifier_bits_from_param(int param)
{
	int mods = 0;
	switch (param) {
	case 2:
		mods = TERSE_MOD_SHIFT;
		break;
	case 3:
		mods = TERSE_MOD_ALT;
		break;
	case 4:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT;
		break;
	case 5:
		mods = TERSE_MOD_CTRL;
		break;
	case 6:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_CTRL;
		break;
	case 7:
		mods = TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	case 8:
		mods = TERSE_MOD_SHIFT | TERSE_MOD_ALT | TERSE_MOD_CTRL;
		break;
	default:
		break;
	}
	return mods;
}

static int
mods_from_kitty_param(int param)
{
	if (param <= 0) {
		return 0;
	}
	int bits = param - 1;
	int mods = 0;
	if (bits & 0x01) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (bits & 0x02) {
		mods |= TERSE_MOD_ALT;
	}
	if (bits & 0x04) {
		mods |= TERSE_MOD_CTRL;
	}
	if (bits & 0x08) {
		mods |= TERSE_MOD_META;
	}
	return mods;
}

static size_t
expected_bytes_for_codec(terse_codec_kind_t kind, unsigned char first)
{
	switch (kind) {
	case TERSE_CODEC_UTF8:
		if (first < 0x80) {
			return 1;
		}
		if ((first & 0xe0u) == 0xc0u) {
			return 2;
		}
		if ((first & 0xf0u) == 0xe0u) {
			return 3;
		}
		if ((first & 0xf8u) == 0xf0u) {
			return 4;
		}
		return 0;
	case TERSE_CODEC_SHIFT_JIS:
		if (first <= 0x7f) {
			return 1;
		}
		if (first >= 0xa1 && first <= 0xdf) {
			return 1;
		}
		if ((first >= 0x81 && first <= 0x9f) || (first >= 0xe0 && first <= 0xfc)) {
			return 2;
		}
		return 0;
	case TERSE_CODEC_UNKNOWN:
	default:
		return 1;
	}
}

static unsigned int
decode_shift_jis_bytes(terse_handle_t handle, const unsigned char *bytes, size_t length)
{
	if (length == 0 || !bytes) {
		return SHIFT_JIS_REPLACEMENT;
	}
	unsigned char first = bytes[0];
	if (length == 1) {
		if (first <= 0x7f) {
			return first;
		}
		if (first >= 0xa1 && first <= 0xdf) {
			return 0xff61u + (unsigned int)(first - 0xa1);
		}
		return SHIFT_JIS_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char lead = bytes[0];
		unsigned char trail = bytes[1];
		if (!((trail >= 0x40 && trail <= 0x7e) || (trail >= 0x80 && trail <= 0xfc))) {
			return SHIFT_JIS_REPLACEMENT;
		}
		if (!((lead >= 0x81 && lead <= 0x9f) || (lead >= 0xe0 && lead <= 0xfc))) {
			return SHIFT_JIS_REPLACEMENT;
		}
		unsigned int scalar = convert_shift_jis_pair(handle, lead, trail);
		if (scalar == UTF8_REPLACEMENT) {
			return SHIFT_JIS_REPLACEMENT;
		}
		return scalar;
	}
	return SHIFT_JIS_REPLACEMENT;
}

static int
function_number_from_code(int code)
{
	switch (code) {
	case 11:
		return 1;
	case 12:
		return 2;
	case 13:
		return 3;
	case 14:
		return 4;
	case 15:
		return 5;
	case 17:
		return 6;
	case 18:
		return 7;
	case 19:
		return 8;
	case 20:
		return 9;
	case 21:
		return 10;
	case 23:
		return 11;
	case 24:
		return 12;
	case 25:
		return 13;
	case 26:
		return 14;
	case 28:
		return 15;
	case 29:
		return 16;
	case 31:
		return 17;
	case 32:
		return 18;
	case 33:
		return 19;
	case 34:
		return 20;
	case 35:
		return 21;
	case 36:
		return 22;
	case 37:
		return 23;
	case 38:
		return 24;
	default:
		return 0;
	}
}

int terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_screen) {
		clear_error(handle);
		return 0;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[J";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1J";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2J";
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_clear_line) {
		clear_error(handle);
		return 0;
	}

	const char *sequence = NULL;
	switch (mode) {
	case TERSE_CLEAR_AFTER:
		sequence = "\x1b[K";
		break;
	case TERSE_CLEAR_BEFORE:
		sequence = "\x1b[1K";
		break;
	case TERSE_CLEAR_ALL:
		sequence = "\x1b[2K";
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	return write_literal(handle, sequence);
}

int terse_move_to(terse_handle_t handle, int row, int col)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_absolute) {
		clear_error(handle);
		return 0;
	}

	if (row < 1) {
		row = 1;
	}
	if (col < 1) {
		col = 1;
	}
	if (row == handle->cursor_row && col == handle->cursor_col) {
		clear_error(handle);
		return 0;
	}

	char sequence[32];
	int written = snprintf(sequence, sizeof(sequence), "\x1b[%d;%dH", row, col);
	if (written <= 0 || (size_t)written >= sizeof(sequence)) {
		errno = EINVAL;
		return -EINVAL;
	}

	int out = write_sequence(handle, sequence, (size_t)written);
	if (out == 0) {
		handle->cursor_row = row;
		handle->cursor_col = col;
		handle->cursor_known = 1;
	}
	return out;
}

int terse_move_by(terse_handle_t handle, int drow, int dcol)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_move_relative) {
		clear_error(handle);
		return 0;
	}
	if (drow == 0 && dcol == 0) {
		clear_error(handle);
		return 0;
	}

	int new_row = handle->cursor_row;
	int new_col = handle->cursor_col;

	if (drow < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dA", -drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	} else if (drow > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dB", drow);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_row += drow;
	}

	if (dcol < 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dD", -dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	} else if (dcol > 0) {
		char seq[32];
		int len = snprintf(seq, sizeof(seq), "\x1b[%dC", dcol);
		if (len <= 0) {
			errno = EINVAL;
			return -EINVAL;
		}
		int w = write_sequence(handle, seq, (size_t)len);
		if (w < 0) {
			return w;
		}
		new_col += dcol;
	}

	if (new_row < 1) {
		new_row = 1;
	}
	if (new_col < 1) {
		new_col = 1;
	}
	handle->cursor_row = new_row;
	handle->cursor_col = new_col;
	handle->cursor_known = 1;
	clear_error(handle);
	return 0;
}

int terse_show_cursor(terse_handle_t handle, int visible)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_cursor_visibility) {
		clear_error(handle);
		return 0;
	}
	int target = visible ? 1 : 0;
	if (handle->cursor_visible == target) {
		clear_error(handle);
		return 0;
	}
	int result = write_literal(handle, target ? "\x1b[?25h" : "\x1b[?25l");
	if (result == 0) {
		handle->cursor_visible = target;
	}
	return result;
}

static int
append_param(char *seq, size_t size, size_t *pos, int *first, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int written = vsnprintf(seq + *pos, size - *pos, fmt, ap);
	va_end(ap);
	if (written < 0) {
		return -EINVAL;
	}
	if ((size_t)written >= size - *pos) {
		errno = EOVERFLOW;
		return -EOVERFLOW;
	}
	*pos += (size_t)written;
	*first = 0;
	return 0;
}

static int
mouse_modifiers_from_param(int param)
{
	int mods = 0;
	if (param & 4) {
		mods |= TERSE_MOD_SHIFT;
	}
	if (param & 8) {
		mods |= TERSE_MOD_ALT;
	}
	if (param & 16) {
		mods |= TERSE_MOD_CTRL;
	}
	return mods;
}

static int
append_effects(char *seq, size_t size, size_t *pos, int *first, unsigned int effects)
{
	if (effects & TERSE_STYLE_BOLD) {
		int rc = append_param(seq, size, pos, first, *first ? "1" : ";1");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_FAINT) {
		int rc = append_param(seq, size, pos, first, *first ? "2" : ";2");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_ITALIC) {
		int rc = append_param(seq, size, pos, first, *first ? "3" : ";3");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_UNDERLINE) {
		int rc = append_param(seq, size, pos, first, *first ? "4" : ";4");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_INVERSE) {
		int rc = append_param(seq, size, pos, first, *first ? "7" : ";7");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_BLINK) {
		int rc = append_param(seq, size, pos, first, *first ? "5" : ";5");
		if (rc < 0) {
			return rc;
		}
	}
	if (effects & TERSE_STYLE_STRIKE) {
		int rc = append_param(seq, size, pos, first, *first ? "9" : ";9");
		if (rc < 0) {
			return rc;
		}
	}
	return 0;
}

static int
append_basic16_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, terse_basic_color_t color, int bright)
{
	int base = is_foreground ? 30 : 40;
	int hi_base = is_foreground ? 90 : 100;
	int code = bright ? (hi_base + color) : (base + color);
	return append_param(seq, size, pos, first, *first ? "%d" : ";%d", code);
}

static int
append_palette_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, unsigned int index)
{
	const char *prefix = is_foreground ? "38;5;" : "48;5;";
	return append_param(seq, size, pos, first, *first ? "%s%u" : ";%s%u", prefix, index);
}

static int
append_truecolor(char *seq, size_t size, size_t *pos, int *first, int is_foreground, unsigned char r, unsigned char g, unsigned char b)
{
	const char *prefix = is_foreground ? "38;2;" : "48;2;";
	return append_param(seq, size, pos, first, *first ? "%s%u;%u;%u" : ";%s%u;%u;%u", prefix, r, g, b);
}

static int
append_color(char *seq, size_t size, size_t *pos, int *first, int is_foreground, const terse_color_t *color)
{
	switch (color->kind) {
	case TERSE_COLOR_KIND_DEFAULT:
		return 0;
	case TERSE_COLOR_KIND_BASIC16:
		return append_basic16_color(seq, size, pos, first, is_foreground, color->data.basic16.color, color->data.basic16.bright);
	case TERSE_COLOR_KIND_PALETTE256:
		return append_palette_color(seq, size, pos, first, is_foreground, color->data.palette.value);
	case TERSE_COLOR_KIND_TRUECOLOR:
		return append_truecolor(seq, size, pos, first, is_foreground, color->data.truecolor.r, color->data.truecolor.g, color->data.truecolor.b);
	default:
		return 0;
	}
}

static int
emit_style_sequence(terse_handle_t handle, const terse_style_t *style)
{
	int reset = write_literal(handle, "\x1b[0m");
	if (reset < 0) {
		return reset;
	}
	if (style->effects == 0 && style->foreground.kind == TERSE_COLOR_KIND_DEFAULT && style->background.kind == TERSE_COLOR_KIND_DEFAULT) {
		return 0;
	}
	char seq[128];
	int first = 1;
	int prefix = snprintf(seq, sizeof(seq), "\x1b[");
	if (prefix < 0 || (size_t)prefix >= sizeof(seq)) {
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	size_t pos = (size_t)prefix;
	int rc = append_effects(seq, sizeof(seq), &pos, &first, style->effects);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 1, &style->foreground);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	rc = append_color(seq, sizeof(seq), &pos, &first, 0, &style->background);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_CONFIG, -rc);
		return rc;
	}
	if (first) {
		return 0;
	}
	if (pos >= sizeof(seq) - 1) {
		errno = EOVERFLOW;
		set_error(handle, TERSE_ERROR_CONFIG, EOVERFLOW);
		return -EOVERFLOW;
	}
	seq[pos++] = 'm';
	return write_sequence(handle, seq, pos);
}

int terse_set_style(terse_handle_t handle, const terse_style_t *style)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!style) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	terse_style_t requested = sanitize_style_request(style);
	handle->style = requested;
	terse_style_t effective = make_effective_style(&handle->capabilities, &requested);
	if (handle->style_known && styles_equal(&effective, &handle->effective_style)) {
		clear_error(handle);
		return 0;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.effects == 0 && handle->capabilities.colors == TERSE_COLOR_NONE)) {
		handle->effective_style = effective;
		handle->style_known = 1;
		clear_error(handle);
		return 0;
	}
	int result = emit_style_sequence(handle, &effective);
	if (result == 0) {
		handle->effective_style = effective;
		handle->style_known = 1;
	}
	return result;
}

static int
write_reset_sequence(terse_handle_t handle, terse_reset_scope_t scope)
{
	switch (scope) {
	case TERSE_RESET_ALL:
		return write_sequence(handle, TERSE_RESET_ALL_SEQ, sizeof(TERSE_RESET_ALL_SEQ) - 1);
	case TERSE_RESET_COLOR_ONLY:
		return write_sequence(handle, TERSE_RESET_COLOR_SEQ, sizeof(TERSE_RESET_COLOR_SEQ) - 1);
	case TERSE_RESET_EFFECTS_ONLY:
		return write_sequence(handle, TERSE_RESET_EFFECTS_SEQ, sizeof(TERSE_RESET_EFFECTS_SEQ) - 1);
	default:
		return 0;
	}
}

int terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (scope < TERSE_RESET_ALL || scope > TERSE_RESET_EFFECTS_ONLY) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	int result = 0;
	if (handle->capabilities.has_basic_output) {
		int emit = 0;
		switch (scope) {
		case TERSE_RESET_ALL:
			emit = (handle->capabilities.colors != TERSE_COLOR_NONE || handle->capabilities.effects != 0);
			break;
		case TERSE_RESET_COLOR_ONLY:
			emit = (handle->capabilities.colors != TERSE_COLOR_NONE);
			break;
		case TERSE_RESET_EFFECTS_ONLY:
			emit = (handle->capabilities.effects != 0);
			break;
		default:
			emit = 0;
			break;
		}
		if (emit) {
			result = write_reset_sequence(handle, scope);
			if (result < 0) {
				return result;
			}
		}
	}
	switch (scope) {
	case TERSE_RESET_ALL:
		handle->style = terse_style_default();
		break;
	case TERSE_RESET_COLOR_ONLY:
		handle->style.foreground = terse_color_default();
		handle->style.background = terse_color_default();
		handle->style.effects = mask_effects(handle->style.effects);
		break;
	case TERSE_RESET_EFFECTS_ONLY:
		handle->style.effects = 0;
		break;
	default:
		break;
	}
	update_effective_style(handle);
	clear_error(handle);
	return result;
}

static int
set_mouse_mode(terse_handle_t handle, terse_mouse_mode_t mode, int enable)
{
	static const char *const enable_seqs[][2] = {
		{ "\x1b[?1000h", NULL },		  // X10
		{ "\x1b[?1002h", NULL },		  // VT200
		{ "\x1b[?1002h", "\x1b[?1006h" }, // SGR
	};
	static const char *const disable_seqs[][2] = {
		{ "\x1b[?1000l", NULL },
		{ "\x1b[?1002l", NULL },
		{ "\x1b[?1002l", "\x1b[?1006l" },
	};
	int index = 0;
	switch (mode) {
	case TERSE_MOUSE_X10:
		index = 0;
		break;
	case TERSE_MOUSE_VT200:
		index = 1;
		break;
	case TERSE_MOUSE_SGR:
		index = 2;
		break;
	default:
		return 0;
	}
	const char *const *seqs = enable ? enable_seqs[index] : disable_seqs[index];
	for (int i = 0; i < 2 && seqs[i]; ++i) {
		if (write_literal(handle, seqs[i]) < 0) {
			return -handle->last_errno;
		}
	}
	return 0;
}

static terse_mouse_mode_t
clamp_mouse_mode(terse_mouse_mode_t requested, terse_mouse_mode_t available)
{
	if (requested > available) {
		return available;
	}
	return requested;
}

int terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (mode <= TERSE_MOUSE_NONE || mode > TERSE_MOUSE_SGR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (handle->capabilities.mouse == TERSE_MOUSE_NONE || !handle->capabilities.has_basic_output) {
		handle->mouse_mode = TERSE_MOUSE_NONE;
		handle->mouse_enabled = 0;
		handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
		clear_error(handle);
		return 0;
	}
	terse_mouse_mode_t actual = clamp_mouse_mode(mode, handle->capabilities.mouse);
	if (handle->mouse_enabled && handle->mouse_mode == actual) {
		clear_error(handle);
		return 0;
	}
	if (handle->mouse_enabled) {
		int disable_rc = terse_disable_mouse(handle);
		if (disable_rc < 0) {
			return disable_rc;
		}
	}
	if (set_mouse_mode(handle, actual, 1) < 0) {
		return -handle->last_errno;
	}
	handle->mouse_mode = actual;
	handle->mouse_enabled = 1;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	clear_error(handle);
	return 0;
}

int terse_disable_mouse(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->mouse_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_mouse_mode(handle, handle->mouse_mode, 0) < 0) {
			return -handle->last_errno;
		}
	}
	handle->mouse_enabled = 0;
	handle->mouse_mode = TERSE_MOUSE_NONE;
	handle->mouse_button = TERSE_MOUSE_BUTTON_NONE;
	clear_error(handle);
	return 0;
}

static int
set_bracketed_paste(terse_handle_t handle, int enable)
{
	const char *seq = enable ? "\x1b[?2004h" : "\x1b[?2004l";
	return write_literal(handle, seq);
}

int terse_enable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->capabilities.has_bracketed_paste || !handle->capabilities.has_basic_output) {
		handle->paste_enabled = 0;
		clear_error(handle);
		return 0;
	}
	if (handle->paste_enabled) {
		clear_error(handle);
		return 0;
	}
	if (set_bracketed_paste(handle, 1) < 0) {
		return -handle->last_errno;
	}
	handle->paste_enabled = 1;
	clear_error(handle);
	return 0;
}

int terse_disable_bracketed_paste(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!handle->paste_enabled) {
		clear_error(handle);
		return 0;
	}
	if (handle->capabilities.has_basic_output) {
		if (set_bracketed_paste(handle, 0) < 0) {
			return -handle->last_errno;
		}
	}
	handle->paste_enabled = 0;
	clear_error(handle);
	return 0;
}

int terse_set_title(terse_handle_t handle, const char *title)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!title) {
		title = "";
	}
	if (!handle->capabilities.has_title || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x1b]0;") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, title, strlen(title)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!url) {
		url = "";
	}
	if (!label) {
		label = "";
	}
	if (!handle->capabilities.has_hyperlinks || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x1b]8;;") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, url, strlen(url)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	if (write_sequence(handle, label, strlen(label)) < 0) {
		return -handle->last_errno;
	}
	if (write_literal(handle, "\x1b]8;;\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (shape < TERSE_CURSOR_SHAPE_DEFAULT || shape > TERSE_CURSOR_SHAPE_BAR) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_cursor_shape || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	int value = 0;
	switch (shape) {
	case TERSE_CURSOR_SHAPE_DEFAULT:
		value = blinking ? 1 : 2;
		break;
	case TERSE_CURSOR_SHAPE_BLOCK:
		value = blinking ? 1 : 2;
		break;
	case TERSE_CURSOR_SHAPE_UNDERLINE:
		value = blinking ? 3 : 4;
		break;
	case TERSE_CURSOR_SHAPE_BAR:
		value = blinking ? 5 : 6;
		break;
	default:
		value = 1;
		break;
	}
	char seq[16];
	int len = snprintf(seq, sizeof(seq), "\x1b[%d q", value);
	if (len <= 0 || len >= (int)sizeof(seq)) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	return write_literal(handle, seq);
}

int terse_set_clipboard(terse_handle_t handle, const char *data)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!data) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_clipboard_write || !handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	size_t encoded_len = 0;
	char *encoded = base64_encode((const unsigned char *)data, strlen(data), &encoded_len);
	if (!encoded) {
		errno = ENOMEM;
		set_error(handle, TERSE_ERROR_RESOURCE, ENOMEM);
		return -ENOMEM;
	}
	if (write_literal(handle, "\x1b]52;;") < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	if (write_sequence(handle, encoded, encoded_len) < 0) {
		free(encoded);
		return -handle->last_errno;
	}
	free(encoded);
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name)
{
	terse_image_request_t request = {
		.data = data,
		.size = size,
		.name = name,
		.format = TERSE_IMAGE_FORMAT_AUTO,
		.width = 0,
		.height = 0,
		.flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE,
	};
	return terse_display_image(handle, &request);
}

int terse_display_image(terse_handle_t handle, const terse_image_request_t *request)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!request) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!request->data || request->size == 0) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	unsigned int flags = request->flags;
	if (flags == 0) {
		flags = TERSE_IMAGE_FLAG_ALLOW_DEGRADE | TERSE_IMAGE_FLAG_INLINE;
	}
	int degrade_allowed = (flags & TERSE_IMAGE_FLAG_ALLOW_DEGRADE) != 0;
	if (!handle->capabilities.has_basic_output) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	}
	terse_image_support_t available = handle->capabilities.images;
	if (available == TERSE_IMAGE_NONE) {
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	}
	terse_image_support_t target = available;
	switch (request->format) {
	case TERSE_IMAGE_FORMAT_AUTO:
	case TERSE_IMAGE_FORMAT_PNG:
	case TERSE_IMAGE_FORMAT_JPEG:
		break;
	case TERSE_IMAGE_FORMAT_SIXEL:
		if (available == TERSE_IMAGE_SIXEL) {
			target = TERSE_IMAGE_SIXEL;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	case TERSE_IMAGE_FORMAT_KITTY:
		if (available == TERSE_IMAGE_KITTY) {
			target = TERSE_IMAGE_KITTY;
			break;
		}
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	default:
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	}
	const char *name = request->name;
	if (!name || !*name) {
		name = "image";
	}
	(void)request->width;
	(void)request->height;
	switch (target) {
	case TERSE_IMAGE_ITERM_INLINE:
		return send_iterm_inline_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_SIXEL:
		return send_sixel_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_KITTY:
		return send_kitty_image(handle, request->data, request->size, name);
	case TERSE_IMAGE_NONE:
	default:
		if (degrade_allowed) {
			clear_error(handle);
			return 0;
		}
		errno = ENOTSUP;
		set_error(handle, TERSE_ERROR_CONFIG, ENOTSUP);
		return -ENOTSUP;
	}
}

int terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	unsigned int required = 0;
	switch (kind) {
	case TERSE_NOTIFICATION_KIND_BELL:
		required = TERSE_NOTIFICATION_SUPPORT_BELL;
		break;
	case TERSE_NOTIFICATION_KIND_VISUAL:
		required = TERSE_NOTIFICATION_SUPPORT_VISUAL;
		break;
	case TERSE_NOTIFICATION_KIND_DESKTOP:
		required = TERSE_NOTIFICATION_SUPPORT_DESKTOP;
		break;
	default:
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_basic_output || (handle->capabilities.notifications & required) == 0) {
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_DESKTOP) {
		if (!payload || payload_has_disallowed_chars(payload)) {
			errno = EINVAL;
			set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
			return -EINVAL;
		}
		if (write_literal(handle, "\x1b]9;1;") < 0) {
			return -handle->last_errno;
		}
		if (write_sequence(handle, payload, strlen(payload)) < 0) {
			return -handle->last_errno;
		}
		if (write_literal(handle, "\x07") < 0) {
			return -handle->last_errno;
		}
		clear_error(handle);
		return 0;
	}
	if (kind == TERSE_NOTIFICATION_KIND_VISUAL) {
		if (write_literal(handle, "\x1b[?5h\x1b[?5l") < 0) {
			return -handle->last_errno;
		}
		clear_error(handle);
		return 0;
	}
	if (write_literal(handle, "\x07") < 0) {
		return -handle->last_errno;
	}
	clear_error(handle);
	return 0;
}

int terse_write_text(terse_handle_t handle, const char *graphemes)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!graphemes) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return 0;
	}
	if (handle->codec_kind == TERSE_CODEC_UTF8) {
		return write_literal(handle, graphemes);
	}
	if (handle->codec_kind == TERSE_CODEC_SHIFT_JIS) {
		if (handle->utf8_to_codec == (iconv_t)-1) {
			errno = EINVAL;
			set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
			return -EINVAL;
		}
		const char *input = graphemes;
		size_t in_left = strlen(graphemes);
		unsigned char outbuf[256];
		reset_iconv_state(handle->utf8_to_codec);
		while (in_left > 0) {
			char *in_ptr = (char *)input;
			size_t local_in_left = in_left;
			char *out_ptr = (char *)outbuf;
			size_t out_left = sizeof(outbuf);
			size_t iconv_rc = iconv(handle->utf8_to_codec, &in_ptr, &local_in_left, &out_ptr, &out_left);
			size_t produced = (size_t)(out_ptr - (char *)outbuf);
			if (produced > 0) {
				if (write_sequence(handle, (const char *)outbuf, produced) < 0) {
					return -handle->last_errno;
				}
			}
			input = (const char *)in_ptr;
			in_left = local_in_left;
			if (iconv_rc == (size_t)-1) {
				if (errno == E2BIG) {
					continue;
				}
				if (errno == EILSEQ || errno == EINVAL) {
					if (in_left > 0) {
						input++;
						in_left--;
					}
					const char replacement = '?';
					if (write_sequence(handle, &replacement, 1) < 0) {
						return -handle->last_errno;
					}
					reset_iconv_state(handle->utf8_to_codec);
					continue;
				}
				set_error(handle, TERSE_ERROR_CONFIG, errno);
				return -errno;
			}
		}
		reset_iconv_state(handle->utf8_to_codec);
		clear_error(handle);
		return 0;
	}
	return write_literal(handle, graphemes);
}

int terse_flush(terse_handle_t handle)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	clear_error(handle);
	return 0;
}

static void
set_key_event(terse_event_t *event, terse_event_type_t type, int mods)
{
	event->type = type;
	event->data.key.mods = mods;
}

static void
set_function_event(terse_event_t *event, int fn_number, int mods)
{
	event->type = TERSE_EVENT_FUNCTION;
	event->data.function.number = fn_number;
	event->data.function.mods = mods;
}

static void
set_char_event(terse_handle_t handle, terse_event_t *event, unsigned int scalar, int mods)
{
	int width = unicode_cell_width(scalar);
	(void)handle;
	event->type = TERSE_EVENT_CHAR;
	event->data.ch.scalar = scalar;
	event->data.ch.width = width;
	event->data.ch.mods = mods;
}

static void
set_raw_event(terse_event_t *event, const unsigned char *bytes, size_t length)
{
	event->type = TERSE_EVENT_RAW_SEQUENCE;
	if (length > TERSE_EVENT_RAW_MAX) {
		length = TERSE_EVENT_RAW_MAX;
	}
	event->data.raw.length = length;
	memset(event->data.raw.bytes, 0, TERSE_EVENT_RAW_MAX);
	memcpy(event->data.raw.bytes, bytes, length);
}

static void
set_resize_event(terse_event_t *event, int rows, int cols)
{
	event->type = TERSE_EVENT_RESIZE;
	event->data.resize.rows = rows;
	event->data.resize.cols = cols;
}

static int
read_input_byte(terse_handle_t handle, int timeout_ms, unsigned char *out)
{
	if (!handle || !out) {
		return -EINVAL;
	}
	if (handle->has_pending_byte) {
		*out = handle->pending_byte;
		handle->has_pending_byte = 0;
		return 1;
	}
	int fd = handle->options.input_fd;
	int ready = terse_platform_wait_for_input(fd, timeout_ms);
	if (ready == 0) {
		return 0;
	}
	if (ready < 0) {
		return ready;
	}
	ssize_t n = terse_platform_read_byte(fd, out);
	if (n == 0) {
		errno = EPIPE;
		return -EPIPE;
	}
	if (n < 0) {
		return (int)n;
	}
	return 1;
}

static int
handle_escape_prefixed_char(terse_handle_t handle, terse_event_t *event, const unsigned char *seq, size_t len)
{
	if (len < 2 || !handle) {
		return 0;
	}
	size_t payload_len = len - 1;
	if (payload_len == 0) {
		return 0;
	}
	const unsigned char *payload = seq + 1;
	unsigned char first = payload[0];
	if (first == 0x1b) {
		return 0;
	}
	size_t expected = expected_bytes_for_codec(handle->codec_kind, first);
	if (expected == 0 || expected != payload_len) {
		return 0;
	}
	int mods = TERSE_MOD_ALT;
	if (payload_len == 1) {
		if (payload[0] == '\r' || payload[0] == '\n') {
			set_key_event(event, TERSE_EVENT_ENTER, mods);
			return 1;
		}
		if (payload[0] == '\t') {
			set_key_event(event, TERSE_EVENT_TAB, mods);
			return 1;
		}
		if (payload[0] == '\b' || payload[0] == 0x7f) {
			set_key_event(event, TERSE_EVENT_BACKSPACE, mods);
			return 1;
		}
		if (payload[0] >= 0x01 && payload[0] <= 0x1a) {
			unsigned int scalar = 'A' + (unsigned int)(payload[0] - 1);
			set_char_event(handle, event, scalar, mods | TERSE_MOD_CTRL);
			return 1;
		}
	}
	unsigned int scalar = 0;
	switch (handle->codec_kind) {
	case TERSE_CODEC_UTF8:
		scalar = decode_utf8_bytes(payload, payload_len);
		if (scalar == UTF8_REPLACEMENT) {
			return 0;
		}
		break;
	case TERSE_CODEC_SHIFT_JIS:
		scalar = decode_shift_jis_bytes(handle, payload, payload_len);
		if (scalar == SHIFT_JIS_REPLACEMENT) {
			return 0;
		}
		break;
	case TERSE_CODEC_UNKNOWN:
	default:
		scalar = payload[0];
		break;
	}
	set_char_event(handle, event, scalar, mods);
	return 1;
}

int terse_read_event(terse_handle_t handle, int timeout_ms, terse_event_t *out_event)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_event) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}

	int fd = handle->options.input_fd;

	unsigned char first = 0;
	rc = read_input_byte(handle, timeout_ms, &first);
	if (rc == 0) {
		clear_error(handle);
		return TERSE_EVENT_NONE;
	}
	if (rc < 0) {
		int err = -rc;
		set_error(handle, TERSE_ERROR_TRANSPORT, err);
		return rc;
	}

	switch (first) {
	case '\r': {
		unsigned char next = 0;
		int peek = read_input_byte(handle, 0, &next);
		if (peek < 0) {
			int err = -peek;
			set_error(handle, TERSE_ERROR_TRANSPORT, err);
			return peek;
		}
		if (peek > 0 && next == '\n') {
			set_key_event(out_event, TERSE_EVENT_ENTER, 0);
			clear_error(handle);
			return TERSE_EVENT_OK;
		}
		if (peek > 0) {
			handle->pending_byte = next;
			handle->has_pending_byte = 1;
		}
		unsigned char raw_bytes[1] = { '\r' };
		set_raw_event(out_event, raw_bytes, sizeof(raw_bytes));
		clear_error(handle);
		return TERSE_EVENT_OK;
	}
	case '\n':
		set_key_event(out_event, TERSE_EVENT_ENTER, TERSE_MOD_CTRL);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\b':
	case 0x7f:
		set_key_event(out_event, TERSE_EVENT_BACKSPACE, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	case '\t':
		set_key_event(out_event, TERSE_EVENT_TAB, 0);
		clear_error(handle);
		return TERSE_EVENT_OK;
	default:
		break;
	}

	if (first >= 0x01 && first <= 0x1a) {
		unsigned int scalar = 'A' + (first - 1);
		set_char_event(handle, out_event, scalar, TERSE_MOD_CTRL);
		return TERSE_EVENT_OK;
	}

	if (first == 0x1b) {
		unsigned char seq[TERSE_EVENT_RAW_MAX] = { 0 };
		seq[0] = first;
		size_t len = terse_platform_drain_escape_sequence(fd, seq, TERSE_EVENT_RAW_MAX);
		int values[8] = { 0 };
		size_t value_count = 0;
		char final = 0;
		if (parse_csi_sequence(seq, len, values, 8, &value_count, &final) == 0) {
			if ((final == 'M' || final == 'm') && len > 2 && seq[2] == '<') {
				if (handle_sgr_mouse_sequence(handle, out_event, values, value_count, final)) {
					return TERSE_EVENT_OK;
				}
			}
			if (final == '~' && value_count >= 1 && handle->paste_enabled) {
				if (values[0] == 200) {
					out_event->type = TERSE_EVENT_PASTE_BEGIN;
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (values[0] == 201) {
					out_event->type = TERSE_EVENT_PASTE_END;
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
			}
			if (final == 'u' && value_count >= 1) {
				unsigned int code = (unsigned int)values[0];
				int mods = 0;
				if (value_count >= 2) {
					mods = mods_from_kitty_param(values[1]);
				}
				if (code == 13) {
					set_key_event(out_event, TERSE_EVENT_ENTER, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (code == 9) {
					set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (code == 127) {
					set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (code >= 0x20 && code <= 0x10ffff) {
					set_char_event(handle, out_event, code, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
			}
			if (final == 'Z') {
				int mods = TERSE_MOD_SHIFT;
				if (value_count > 0) {
					mods = modifier_bits_from_param(values[value_count - 1]);
					if (!(mods & TERSE_MOD_SHIFT)) {
						mods |= TERSE_MOD_SHIFT;
					}
				}
				set_key_event(out_event, TERSE_EVENT_TAB, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			}
			if (final == 't' && value_count >= 3 && values[0] == 8) {
				set_resize_event(out_event, values[1], values[2]);
				handle->size.rows = values[1];
				handle->size.cols = values[2];
				handle->size.known = 1;
				handle->capabilities.has_size = 1;
				clear_error(handle);
				return TERSE_EVENT_OK;
			}
			if (final == 'H' || final == 'F') {
				int mods = 0;
				if (value_count > 0) {
					mods = modifier_bits_from_param(values[value_count - 1]);
				}
				if (final == 'H') {
					set_key_event(out_event, TERSE_EVENT_HOME, mods);
				} else {
					set_key_event(out_event, TERSE_EVENT_END, mods);
				}
				clear_error(handle);
				return TERSE_EVENT_OK;
			}
			if (final == '~') {
				if (value_count == 0) {
					set_raw_event(out_event, seq, len);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				int mods_param = 0;
				int key_code = values[0];
				if (key_code == 27 && value_count >= 3) {
					mods_param = values[1];
					key_code = values[2];
				} else if (value_count > 1) {
					mods_param = values[value_count - 1];
				}
				int mods = modifier_bits_from_param(mods_param);
				switch (key_code) {
				case 1:
				case 7:
					set_key_event(out_event, TERSE_EVENT_HOME, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 4:
				case 8:
					set_key_event(out_event, TERSE_EVENT_END, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 2:
					set_key_event(out_event, TERSE_EVENT_INSERT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 3:
					set_key_event(out_event, TERSE_EVENT_DELETE, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 5:
					set_key_event(out_event, TERSE_EVENT_PAGE_UP, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 6:
					set_key_event(out_event, TERSE_EVENT_PAGE_DOWN, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 9:
					set_key_event(out_event, TERSE_EVENT_TAB, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 13:
					if (values[0] == 27 && value_count >= 3) {
						set_key_event(out_event, TERSE_EVENT_ENTER, modifier_bits_from_param(values[1]));
						clear_error(handle);
						return TERSE_EVENT_OK;
					}
					break;
				default:
					break;
				}
				int fn = function_number_from_code(key_code);
				if (fn > 0) {
					set_function_event(out_event, fn, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
				if (key_code >= 0 && key_code <= 0x10ffff) {
					if (key_code == 9) {
						set_key_event(out_event, TERSE_EVENT_TAB, mods);
						clear_error(handle);
						return TERSE_EVENT_OK;
					}
					if (key_code == 8 || key_code == 127) {
						set_key_event(out_event, TERSE_EVENT_BACKSPACE, mods);
						clear_error(handle);
						return TERSE_EVENT_OK;
					}
					set_char_event(handle, out_event, (unsigned int)key_code, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				}
			}
			if (final == 'A' || final == 'B' || final == 'C' || final == 'D') {
				int mods = 0;
				if (value_count > 0) {
					mods = modifier_bits_from_param(values[value_count - 1]);
				}
				switch (final) {
				case 'A':
					set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'B':
					set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'C':
					set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				case 'D':
					set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
					clear_error(handle);
					return TERSE_EVENT_OK;
				default:
					break;
				}
			}
		}
		if (len >= 3 && seq[1] == 'O') {
			int mods = 0;
			char code = (char)seq[2];
			switch (code) {
			case 'A':
				set_key_event(out_event, TERSE_EVENT_ARROW_UP, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'B':
				set_key_event(out_event, TERSE_EVENT_ARROW_DOWN, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'C':
				set_key_event(out_event, TERSE_EVENT_ARROW_RIGHT, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'D':
				set_key_event(out_event, TERSE_EVENT_ARROW_LEFT, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'H':
				set_key_event(out_event, TERSE_EVENT_HOME, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'F':
				set_key_event(out_event, TERSE_EVENT_END, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'P':
				set_function_event(out_event, 1, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'Q':
				set_function_event(out_event, 2, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'R':
				set_function_event(out_event, 3, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			case 'S':
				set_function_event(out_event, 4, mods);
				clear_error(handle);
				return TERSE_EVENT_OK;
			default:
				break;
			}
		}
		if (handle_escape_prefixed_char(handle, out_event, seq, len)) {
			clear_error(handle);
			return TERSE_EVENT_OK;
		}
		set_raw_event(out_event, seq, len);
		clear_error(handle);
		return TERSE_EVENT_OK;
	}

	if (first >= 0x20 || (handle->codec_kind == TERSE_CODEC_SHIFT_JIS && first >= 0x80)) {
		int decode_rc = decode_stream_char(handle, fd, first, out_event);
		if (decode_rc == 0) {
			clear_error(handle);
			return TERSE_EVENT_OK;
		}
		if (decode_rc < 0) {
			return decode_rc;
		}
	}

	unsigned char raw_bytes[1] = { first };
	set_raw_event(out_event, raw_bytes, 1);
	clear_error(handle);
	return TERSE_EVENT_OK;
}

terse_size_t
terse_get_size(terse_handle_t handle)
{
	terse_size_t unknown = make_unknown_size();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
	if (handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE) {
		clear_error(handle);
		return unknown;
	}
	if (!handle->size.known) {
		refresh_size(handle);
	}
	if (!(handle->options.disabled_caps & TERSE_CAP_DISABLE_SIZE)) {
		handle->capabilities.has_size = handle->size.known;
	}
	clear_error(handle);
	return handle->size;
}

static terse_cursor_position_t
make_unknown_cursor_position(void)
{
	terse_cursor_position_t pos = {0, 0, 0};
	return pos;
}

static int
query_cursor_position(terse_handle_t handle, int *out_row, int *out_col)
{
	if (!handle || !out_row || !out_col) {
		return -EINVAL;
	}
	int input_fd = handle->options.input_fd;
	int output_fd = handle->options.output_fd;
	if (input_fd < 0 || output_fd < 0) {
		return -EBADF;
	}
	if (!isatty(input_fd) || !isatty(output_fd)) {
		return -ENOTTY;
	}

	// Save current terminal settings and switch to raw mode
	struct termios original;
	if (tcgetattr(input_fd, &original) != 0) {
		return -errno;
	}
	struct termios raw = original;
	raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= CS8;
	raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(input_fd, TCSANOW, &raw) != 0) {
		return -errno;
	}

	// Send CPR (Cursor Position Report) request: CSI 6 n
	const char request[] = "\x1b[6n";
	if (write(output_fd, request, sizeof(request) - 1) < 0) {
		(void)tcsetattr(input_fd, TCSANOW, &original);
		return -errno;
	}

	// Read response: CSI row ; col R
	unsigned char buffer[32];
	size_t length = 0;
	struct pollfd pfd = {
		.fd = input_fd,
		.events = POLLIN,
	};
	const int timeout_ms = 200;
	const int slice = 25;
	int remaining = timeout_ms;
	while (length < sizeof(buffer)) {
		int poll_timeout = (remaining < slice) ? remaining : slice;
		int ready = poll(&pfd, 1, poll_timeout);
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			(void)tcsetattr(input_fd, TCSANOW, &original);
			return -errno;
		}
		if (ready == 0) {
			remaining -= poll_timeout;
			if (remaining <= 0) {
				break;
			}
			continue;
		}
		ssize_t n = read(input_fd, buffer + length, sizeof(buffer) - length);
		if (n <= 0) {
			break;
		}
		length += (size_t)n;
		remaining -= poll_timeout;

		// Check if we have a complete response (ends with 'R')
		if (length > 0 && buffer[length - 1] == 'R') {
			break;
		}
	}

	// Restore terminal settings
	(void)tcsetattr(input_fd, TCSANOW, &original);

	// Parse response: ESC [ row ; col R
	if (length < 6 || buffer[0] != 0x1b || buffer[1] != '[' || buffer[length - 1] != 'R') {
		return -EPROTO;
	}

	// Parse row and col
	int row = 0, col = 0;
	size_t i = 2;
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		row = row * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != ';') {
		return -EPROTO;
	}
	i++; // skip ';'
	while (i < length && buffer[i] >= '0' && buffer[i] <= '9') {
		col = col * 10 + (buffer[i] - '0');
		i++;
	}
	if (i >= length || buffer[i] != 'R') {
		return -EPROTO;
	}

	*out_row = row;
	*out_col = col;
	return 0;
}

terse_cursor_position_t
terse_get_cursor_position(terse_handle_t handle)
{
	terse_cursor_position_t unknown = make_unknown_cursor_position();
	if (ensure_handle(handle) < 0) {
		return unknown;
	}
	if (!handle->capabilities.has_basic_output) {
		clear_error(handle);
		return unknown;
	}

	int row, col;
	int rc = query_cursor_position(handle, &row, &col);
	if (rc < 0) {
		set_error(handle, TERSE_ERROR_TRANSPORT, -rc);
		return unknown;
	}

	terse_cursor_position_t pos = {row, col, 1};
	clear_error(handle);
	return pos;
}

int terse_get_options(terse_handle_t handle, terse_options_t *out_options)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_options) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	*out_options = handle->options;
	clear_error(handle);
	return 0;
}

terse_error_info_t
terse_get_last_error(terse_handle_t handle)
{
	terse_error_info_t info = { TERSE_ERROR_NONE, 0 };
	if (!handle) {
		info.category = TERSE_ERROR_STATE;
		info.code = EINVAL;
		return info;
	}
	info.category = handle->last_error;
	info.code = handle->last_errno;
	return info;
}

int terse_capture_state(terse_handle_t handle, terse_state_t *out_state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!out_state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	out_state->cursor_known = handle->cursor_known;
	out_state->cursor_visible = handle->cursor_visible;
	out_state->cursor_row = handle->cursor_row;
	out_state->cursor_col = handle->cursor_col;
	out_state->style_known = handle->style_known;
	out_state->style = handle->style;
	clear_error(handle);
	return 0;
}

int terse_restore_state(terse_handle_t handle, const terse_state_t *state)
{
	int rc = ensure_handle(handle);
	if (rc < 0) {
		return rc;
	}
	if (!state) {
		errno = EINVAL;
		set_error(handle, TERSE_ERROR_CONFIG, EINVAL);
		return -EINVAL;
	}
	terse_state_t local = *state;
	if (local.cursor_known) {
		if (local.cursor_row < 1) {
			local.cursor_row = 1;
		}
		if (local.cursor_col < 1) {
			local.cursor_col = 1;
		}
	}
	local.cursor_visible = state->cursor_visible ? 1 : 0;
	if (local.style_known) {
		local.style = sanitize_style_request(&state->style);
	}

	int result = 0;

	// Apply outputs BEFORE updating internal state to avoid duplicate skipping
	if (local.cursor_known && handle->capabilities.has_move_absolute && local.cursor_row > 0 && local.cursor_col > 0) {
		int move_rc = terse_move_to(handle, local.cursor_row, local.cursor_col);
		if (move_rc < 0 && result == 0) {
			result = move_rc;
		}
	}
	if (handle->capabilities.has_cursor_visibility) {
		int vis_rc = terse_show_cursor(handle, local.cursor_visible);
		if (vis_rc < 0 && result == 0) {
			result = vis_rc;
		}
	}
	if (local.style_known) {
		int style_rc = terse_set_style(handle, &local.style);
		if (style_rc < 0 && result == 0) {
			result = style_rc;
		}
	}

	// Update internal state after outputs to keep state synchronized
	(void)terse_state_override(handle, &local);

	return result;
}
