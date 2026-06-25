#include "terse_codec.h"
#include "terse_platform.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#include <unistd.h>
#else
#define strcasecmp _stricmp
#endif

/* ========================================================================
 * Passthrough backend (POSIX: UTF-8 terminals only)
 * ======================================================================== */

static terse_error_t
passthrough_from_utf8(terse_codec_t *c,
                      const char *in, size_t inlen,
                      char *out, size_t *outlen)
{
	(void)c;
	size_t copy = inlen < *outlen ? inlen : *outlen;
	memcpy(out, in, copy);
	*outlen = copy;
	return TERSE_OK;
}

static terse_error_t
passthrough_to_utf8(terse_codec_t *c,
                    const char *in, size_t inlen,
                    char *out, size_t *outlen)
{
	return passthrough_from_utf8(c, in, inlen, out, outlen);
}

static void
passthrough_init(terse_codec_t *codec)
{
	codec->to_utf8 = passthrough_to_utf8;
	codec->from_utf8 = passthrough_from_utf8;
	codec->destroy = NULL;
	codec->impl = NULL;
	codec->kind = TERSE_CODEC_KIND_UTF8;
}

/* ========================================================================
 * iconv backend (POSIX with non-UTF-8 codec; Human68k uses mini_iconv
 * via the same interface)
 * ======================================================================== */

#if !defined(_WIN32)

#if defined(__human68k__)
/* Human68k has no system iconv; always use mini_iconv */
#undef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 0
#endif

#ifndef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 1
#endif

#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "mini_iconv.h"
#endif

typedef struct {
	iconv_t to_utf8;
	iconv_t from_utf8;
} iconv_impl_t;

static void
iconv_reset(iconv_t cd)
{
#if TERSE_USE_SYSTEM_ICONV
	iconv(cd, NULL, NULL, NULL, NULL);
#else
	(void)cd;
#endif
}

static terse_error_t
iconv_to_utf8_fn(terse_codec_t *c,
                 const char *in, size_t inlen,
                 char *out, size_t *outlen)
{
	iconv_impl_t *impl = (iconv_impl_t *)c->impl;
	char *in_ptr = (char *)in;
	size_t in_left = inlen;
	char *out_ptr = out;
	size_t out_left = *outlen;
	iconv_reset(impl->to_utf8);
	size_t rc = iconv(impl->to_utf8, &in_ptr, &in_left, &out_ptr, &out_left);
	*outlen = *outlen - out_left;
	if (rc == (size_t)-1) {
		if (errno == E2BIG) {
			return TERSE_ERR_BUFFER_TOO_SMALL;
		}
		return TERSE_ERR_INVALID_ENCODING;
	}
	return TERSE_OK;
}

static terse_error_t
iconv_from_utf8_fn(terse_codec_t *c,
                   const char *in, size_t inlen,
                   char *out, size_t *outlen)
{
	iconv_impl_t *impl = (iconv_impl_t *)c->impl;
	char *in_ptr = (char *)in;
	size_t in_left = inlen;
	char *out_ptr = out;
	size_t out_left = *outlen;
	iconv_reset(impl->from_utf8);
	size_t rc = iconv(impl->from_utf8, &in_ptr, &in_left, &out_ptr, &out_left);
	*outlen = *outlen - out_left;
	if (rc == (size_t)-1) {
		if (errno == E2BIG) {
			return TERSE_ERR_BUFFER_TOO_SMALL;
		}
		return TERSE_ERR_INVALID_ENCODING;
	}
	return TERSE_OK;
}

static void
iconv_destroy_fn(terse_codec_t *c)
{
	iconv_impl_t *impl = (iconv_impl_t *)c->impl;
	if (!impl) {
		return;
	}
	if (impl->to_utf8 != (iconv_t)-1) {
		iconv_close(impl->to_utf8);
	}
	if (impl->from_utf8 != (iconv_t)-1) {
		iconv_close(impl->from_utf8);
	}
	free(impl);
	c->impl = NULL;
}

static int
iconv_codec_init(terse_codec_t *codec, const char *encoding)
{
	iconv_impl_t *impl = calloc(1, sizeof(*impl));
	if (!impl) {
		return -1;
	}
	impl->to_utf8 = iconv_open("UTF-8", encoding);
	impl->from_utf8 = iconv_open(encoding, "UTF-8");
	if (impl->to_utf8 == (iconv_t)-1 || impl->from_utf8 == (iconv_t)-1) {
		if (impl->to_utf8 != (iconv_t)-1) {
			iconv_close(impl->to_utf8);
		}
		if (impl->from_utf8 != (iconv_t)-1) {
			iconv_close(impl->from_utf8);
		}
		free(impl);
		return -1;
	}
	codec->to_utf8 = iconv_to_utf8_fn;
	codec->from_utf8 = iconv_from_utf8_fn;
	codec->destroy = iconv_destroy_fn;
	codec->impl = impl;
	codec->kind = TERSE_CODEC_KIND_SHIFT_JIS;
	return 0;
}

#endif /* !_WIN32 */

/* ========================================================================
 * Win32 backend
 * ======================================================================== */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	UINT codepage;
} win32_impl_t;

static terse_error_t
win32_to_utf8_fn(terse_codec_t *c,
                 const char *in, size_t inlen,
                 char *out, size_t *outlen)
{
	win32_impl_t *impl = (win32_impl_t *)c->impl;
	if (inlen == 0) {
		*outlen = 0;
		return TERSE_OK;
	}
	/* Console encoding -> UTF-16 */
	int wlen = MultiByteToWideChar(impl->codepage, MB_ERR_INVALID_CHARS,
	                               in, (int)inlen, NULL, 0);
	if (wlen <= 0) {
		return TERSE_ERR_INVALID_ENCODING;
	}
	WCHAR *wbuf = malloc((size_t)wlen * sizeof(WCHAR));
	if (!wbuf) {
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	MultiByteToWideChar(impl->codepage, 0, in, (int)inlen, wbuf, wlen);
	/* UTF-16 -> UTF-8 */
	int utf8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen,
	                                  out, (int)*outlen, NULL, NULL);
	free(wbuf);
	if (utf8len <= 0) {
		return (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		           ? TERSE_ERR_BUFFER_TOO_SMALL
		           : TERSE_ERR_INVALID_ENCODING;
	}
	*outlen = (size_t)utf8len;
	return TERSE_OK;
}

static terse_error_t
win32_from_utf8_fn(terse_codec_t *c,
                   const char *in, size_t inlen,
                   char *out, size_t *outlen)
{
	win32_impl_t *impl = (win32_impl_t *)c->impl;
	if (inlen == 0) {
		*outlen = 0;
		return TERSE_OK;
	}
	/* UTF-8 -> UTF-16 */
	int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
	                               in, (int)inlen, NULL, 0);
	if (wlen <= 0) {
		return TERSE_ERR_INVALID_ENCODING;
	}
	WCHAR *wbuf = malloc((size_t)wlen * sizeof(WCHAR));
	if (!wbuf) {
		return TERSE_ERR_OUT_OF_MEMORY;
	}
	MultiByteToWideChar(CP_UTF8, 0, in, (int)inlen, wbuf, wlen);
	/* UTF-16 -> console encoding */
	int mblen = WideCharToMultiByte(impl->codepage, 0, wbuf, wlen,
	                                out, (int)*outlen, NULL, NULL);
	free(wbuf);
	if (mblen <= 0) {
		return (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		           ? TERSE_ERR_BUFFER_TOO_SMALL
		           : TERSE_ERR_INVALID_ENCODING;
	}
	*outlen = (size_t)mblen;
	return TERSE_OK;
}

static void
win32_destroy_fn(terse_codec_t *c)
{
	free(c->impl);
	c->impl = NULL;
}

static int
win32_codec_init(terse_codec_t *codec)
{
	win32_impl_t *impl = calloc(1, sizeof(*impl));
	if (!impl) {
		return -1;
	}
	impl->codepage = GetConsoleOutputCP();
	if (impl->codepage == CP_UTF8) {
		/* Console is already UTF-8: use passthrough */
		free(impl);
		passthrough_init(codec);
		return 0;
	}
	codec->to_utf8 = win32_to_utf8_fn;
	codec->from_utf8 = win32_from_utf8_fn;
	codec->destroy = win32_destroy_fn;
	codec->impl = impl;
	/* CP932 and related DBCS codepages share the Shift_JIS byte-length rules */
	codec->kind = (impl->codepage == 932 || impl->codepage == 936 ||
	               impl->codepage == 949 || impl->codepage == 950)
	                  ? TERSE_CODEC_KIND_SHIFT_JIS
	                  : TERSE_CODEC_KIND_UTF8;
	return 0;
}
#endif /* _WIN32 */

/* ========================================================================
 * terse_codec_init: select backend based on platform and encoding name
 * ======================================================================== */

static int is_utf8_name(const char *name)
{
	if (!name) {
		return 1;
	}
	return (strcasecmp(name, "UTF-8") == 0 || strcasecmp(name, "UTF8") == 0);
}

terse_error_t
terse_codec_init(terse_codec_t *codec, const char *codec_name)
{
	memset(codec, 0, sizeof(*codec));

#ifdef _WIN32
	(void)codec_name;
	if (win32_codec_init(codec) != 0) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	return TERSE_OK;
#elif defined(__human68k__)
	/* Human68k always uses Shift_JIS via mini_iconv */
	if (iconv_codec_init(codec, "SHIFT_JIS") != 0) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	return TERSE_OK;
#else
	if (is_utf8_name(codec_name)) {
		passthrough_init(codec);
		return TERSE_OK;
	}
	/* Non-UTF-8 encoding requested: use iconv */
	if (iconv_codec_init(codec, codec_name) != 0) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	return TERSE_OK;
#endif
}

void terse_codec_destroy(terse_codec_t *codec)
{
	if (codec && codec->destroy) {
		codec->destroy(codec);
	}
}

terse_error_t
terse_codec_from_utf8(terse_codec_t *codec,
                      const char *in, size_t inlen,
                      char *out, size_t *outlen)
{
	if (codec->from_utf8) {
		return codec->from_utf8(codec, in, inlen, out, outlen);
	}
	/* passthrough */
	size_t copy = inlen < *outlen ? inlen : *outlen;
	memcpy(out, in, copy);
	*outlen = copy;
	return TERSE_OK;
}

terse_error_t
terse_codec_to_utf8(terse_codec_t *codec,
                    const char *in, size_t inlen,
                    char *out, size_t *outlen)
{
	if (codec->to_utf8) {
		return codec->to_utf8(codec, in, inlen, out, outlen);
	}
	/* passthrough */
	size_t copy = inlen < *outlen ? inlen : *outlen;
	memcpy(out, in, copy);
	*outlen = copy;
	return TERSE_OK;
}

/* ========================================================================
 * Character decoding helpers (platform-independent)
 * ======================================================================== */

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

unsigned int
terse_convert_shift_jis_pair(terse_codec_t *codec, unsigned char lead, unsigned char trail)
{
	if (!codec || !codec->to_utf8) {
		return TERSE_SHIFT_JIS_REPLACEMENT;
	}
	char inbuf[2];
	inbuf[0] = (char)lead;
	inbuf[1] = (char)trail;
	char outbuf[8] = { 0 };
	size_t outlen = sizeof(outbuf);
	if (codec->to_utf8(codec, inbuf, sizeof(inbuf), outbuf, &outlen) != TERSE_OK) {
		return TERSE_SHIFT_JIS_REPLACEMENT;
	}
	return terse_decode_utf8_bytes((const unsigned char *)outbuf, outlen);
}

int terse_decode_shift_jis_stream(terse_codec_t *codec, int fd, unsigned char first, unsigned int *out_scalar)
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
		*out_scalar = terse_convert_shift_jis_pair(codec, first, second);
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
			return 0;
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
	return 0;
}

/* ========================================================================
 * Public stateless encoding conversion
 * ======================================================================== */

terse_error_t
terse_convert_encoding(const char *from_encoding, const char *to_encoding,
                       const char *in, size_t inlen,
                       char *out, size_t *outlen)
{
	if (!from_encoding || !to_encoding || !in || !out || !outlen) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	int from_utf8 = is_utf8_name(from_encoding);
	int to_utf8 = is_utf8_name(to_encoding);

	if (from_utf8 && to_utf8) {
		size_t copy = inlen < *outlen ? inlen : *outlen;
		memcpy(out, in, copy);
		*outlen = copy;
		return (copy < inlen) ? TERSE_ERR_BUFFER_TOO_SMALL : TERSE_OK;
	}

	const char *codec_name = from_utf8 ? to_encoding : to_utf8 ? from_encoding
	                                                           : NULL;
	if (!codec_name) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	terse_codec_t codec;
	terse_error_t err = terse_codec_init(&codec, codec_name);
	if (err != TERSE_OK) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}

	if (from_utf8) {
		err = terse_codec_from_utf8(&codec, in, inlen, out, outlen);
	} else {
		err = terse_codec_to_utf8(&codec, in, inlen, out, outlen);
	}

	terse_codec_destroy(&codec);
	return err;
}
