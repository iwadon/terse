#include "terse_codec.h"
#include "terse_internal.h"
#include "terse_platform.h"
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* iconv reset helper (from terse.c) */
void terse_codec_reset_iconv_state(iconv_t cd)
{
#if TERSE_USE_SYSTEM_ICONV
	/* Reset iconv state by calling with NULL input */
	iconv(cd, NULL, NULL, NULL, NULL);
#else
	(void)cd;
#endif
}

int terse_is_shift_jis_lead_byte(unsigned char ch)
{
	return (ch >= 0x81 && ch <= 0x9f) || (ch >= 0xe0 && ch <= 0xfc);
}

unsigned int
terse_decode_utf8_bytes(const unsigned char *bytes, size_t length)
{
	if (!bytes || length == 0) {
		return TERSE_UTF8_REPLACEMENT;
	}
	unsigned int scalar = 0;
	if (length == 1) {
		unsigned char b0 = bytes[0];
		if (b0 < 0x80) {
			return b0;
		}
		return TERSE_UTF8_REPLACEMENT;
	}
	if (length == 2) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		if ((b0 & 0xe0) != 0xc0 || (b1 & 0xc0) != 0x80) {
			return TERSE_UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x1f) << 6) | (unsigned int)(b1 & 0x3f);
		if (scalar < 0x80) {
			return TERSE_UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 3) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		if ((b0 & 0xf0) != 0xe0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) {
			return TERSE_UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x0f) << 12) | ((unsigned int)(b1 & 0x3f) << 6) | (unsigned int)(b2 & 0x3f);
		if (scalar < 0x800 || (scalar >= 0xd800 && scalar <= 0xdfff)) {
			return TERSE_UTF8_REPLACEMENT;
		}
		return scalar;
	}
	if (length == 4) {
		unsigned char b0 = bytes[0];
		unsigned char b1 = bytes[1];
		unsigned char b2 = bytes[2];
		unsigned char b3 = bytes[3];
		if ((b0 & 0xf8) != 0xf0 || (b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) {
			return TERSE_UTF8_REPLACEMENT;
		}
		scalar = ((unsigned int)(b0 & 0x07) << 18) | ((unsigned int)(b1 & 0x3f) << 12) | ((unsigned int)(b2 & 0x3f) << 6) | (unsigned int)(b3 & 0x3f);
		if (scalar < 0x10000 || scalar > 0x10ffff) {
			return TERSE_UTF8_REPLACEMENT;
		}
		return scalar;
	}
	return TERSE_UTF8_REPLACEMENT;
}

int terse_decode_utf8_stream(int fd, unsigned char first, unsigned int *out_scalar)
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
			*out_scalar = TERSE_UTF8_REPLACEMENT;
			return 0;
		}
	} else if ((first & 0xf0) == 0xe0) {
		expected = 3;
	} else if ((first & 0xf8) == 0xf0) {
		expected = 4;
		if (first > 0xf4) {
			*out_scalar = TERSE_UTF8_REPLACEMENT;
			return 0;
		}
	} else {
		*out_scalar = TERSE_UTF8_REPLACEMENT;
		return 0;
	}
	for (int i = 1; i < expected; ++i) {
		unsigned char next = 0;
		ssize_t n = terse_platform_read_byte(fd, &next);
		if (n <= 0) {
			if (n == 0) {
				errno = EPIPE;
				return TERSE_ERR_IO;
			}
			return (int)n;
		}
		bytes[i] = next;
	}
	*out_scalar = terse_decode_utf8_bytes(bytes, (size_t)expected);
	return 0;
}

/* Convert Shift_JIS pair to UTF-8 using iconv */
unsigned int
terse_convert_shift_jis_pair(terse_handle_t handle, unsigned char lead, unsigned char trail)
{
	(void)lead;
	(void)trail;
	if (!handle || handle->codec_to_utf8 == (iconv_t)-1) {
		return TERSE_SHIFT_JIS_REPLACEMENT;
	}
	char inbuf[2];
	inbuf[0] = (char)lead;
	inbuf[1] = (char)trail;
	char *in_ptr = inbuf;
	size_t in_left = sizeof(inbuf);
	char outbuf[8] = { 0 };
	char *out_ptr = outbuf;
	size_t out_left = sizeof(outbuf);
	terse_codec_reset_iconv_state(handle->codec_to_utf8);
	if (iconv(handle->codec_to_utf8, &in_ptr, &in_left, &out_ptr, &out_left) == (size_t)-1) {
		return TERSE_SHIFT_JIS_REPLACEMENT;
	}
	if (in_left != 0) {
		return TERSE_SHIFT_JIS_REPLACEMENT;
	}
	size_t produced = (size_t)(out_ptr - outbuf);
	return terse_decode_utf8_bytes((const unsigned char *)outbuf, produced);
}

int terse_decode_shift_jis_stream(terse_handle_t handle, int fd, unsigned char first, unsigned int *out_scalar)
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
				return TERSE_ERR_IO;
			}
			return (int)n;
		}
		*out_scalar = terse_convert_shift_jis_pair(handle, first, second);
		return 0;
	}
	*out_scalar = TERSE_SHIFT_JIS_REPLACEMENT;
	return 0;
}

int terse_encode_utf8(unsigned int scalar, unsigned char *out)
{
	if (!out) {
		return 0;
	}
	if (scalar < 0x80) {
		out[0] = (unsigned char)scalar;
		return 1;
	}
	if (scalar < 0x800) {
		out[0] = (unsigned char)(0xc0 | (scalar >> 6));
		out[1] = (unsigned char)(0x80 | (scalar & 0x3f));
		return 2;
	}
	if (scalar < 0x10000) {
		if (scalar >= 0xd800 && scalar <= 0xdfff) {
			return 0; /* surrogate pair - invalid */
		}
		out[0] = (unsigned char)(0xe0 | (scalar >> 12));
		out[1] = (unsigned char)(0x80 | ((scalar >> 6) & 0x3f));
		out[2] = (unsigned char)(0x80 | (scalar & 0x3f));
		return 3;
	}
	if (scalar <= 0x10ffff) {
		out[0] = (unsigned char)(0xf0 | (scalar >> 18));
		out[1] = (unsigned char)(0x80 | ((scalar >> 12) & 0x3f));
		out[2] = (unsigned char)(0x80 | ((scalar >> 6) & 0x3f));
		out[3] = (unsigned char)(0x80 | (scalar & 0x3f));
		return 4;
	}
	return 0; /* out of range */
}
