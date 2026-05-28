#ifndef TERSE_CODEC_H_INCLUDED
#define TERSE_CODEC_H_INCLUDED

#include "terse.h"
#include <stddef.h>

/*
 * Codec abstraction: converts between the terminal's native encoding and
 * the library's internal UTF-8 representation.
 *
 * Backends:
 *   - POSIX:    passthrough (UTF-8 terminals only)
 *   - Windows:  MultiByteToWideChar / WideCharToMultiByte
 *   - Human68k: mini_iconv (Shift_JIS <-> UTF-8)
 *
 * All functions return TERSE_OK on success or a terse_error_t on failure.
 * NULL function pointers mean passthrough (no conversion needed).
 */
typedef enum terse_codec_kind {
	TERSE_CODEC_KIND_UTF8 = 0, /* passthrough / UTF-8 terminal */
	TERSE_CODEC_KIND_SHIFT_JIS /* Shift_JIS / CP932 */
} terse_codec_kind_t;

typedef struct terse_codec {
	terse_error_t (*to_utf8)(struct terse_codec *c,
	                         const char *in, size_t inlen,
	                         char *out, size_t *outlen);
	terse_error_t (*from_utf8)(struct terse_codec *c,
	                           const char *in, size_t inlen,
	                           char *out, size_t *outlen);
	void (*destroy)(struct terse_codec *c);
	void *impl;              /* backend-specific data */
	terse_codec_kind_t kind; /* encoding family for byte-length heuristics */
} terse_codec_t;

/* Initialize the codec field of a handle based on the platform and options. */
terse_error_t terse_codec_init(terse_codec_t *codec, const char *codec_name);

/* Release resources held by a codec (calls codec->destroy if non-NULL). */
void terse_codec_destroy(terse_codec_t *codec);

/*
 * Convenience wrappers around codec->from_utf8 / codec->to_utf8.
 * When the relevant function pointer is NULL they act as passthrough:
 * they copy at most *outlen bytes from in to out and set *outlen to the
 * number of bytes written.
 */
terse_error_t terse_codec_from_utf8(terse_codec_t *codec,
                                    const char *in, size_t inlen,
                                    char *out, size_t *outlen);

terse_error_t terse_codec_to_utf8(terse_codec_t *codec,
                                  const char *in, size_t inlen,
                                  char *out, size_t *outlen);

/*
 * Internal codec helper functions for character decoding.
 * These functions are used by platform-specific code for text input processing.
 */

/* Check if a byte is a Shift_JIS lead byte (2-byte character indicator) */
int terse_is_shift_jis_lead_byte(unsigned char ch);

/* Decode UTF-8 byte sequence to Unicode scalar value */
unsigned int terse_decode_utf8_bytes(const unsigned char *bytes, size_t length);

/* Decode UTF-8 stream from file descriptor, starting with first byte */
int terse_decode_utf8_stream(int fd, unsigned char first, unsigned int *out_scalar);

/* Decode Shift_JIS stream from file descriptor, starting with first byte */
int terse_decode_shift_jis_stream(terse_codec_t *codec, int fd, unsigned char first, unsigned int *out_scalar);

/* Convert Shift_JIS byte pair to Unicode scalar using the codec */
unsigned int terse_convert_shift_jis_pair(terse_codec_t *codec, unsigned char lead, unsigned char trail);

/* Replacement characters for invalid sequences */
#define TERSE_UTF8_REPLACEMENT 0xfffdU
#define TERSE_SHIFT_JIS_REPLACEMENT '?'

#endif /* TERSE_CODEC_H_INCLUDED */
