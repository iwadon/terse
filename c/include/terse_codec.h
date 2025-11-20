#ifndef TERSE_CODEC_H_INCLUDED
#define TERSE_CODEC_H_INCLUDED

#include "terse.h"

#ifndef TERSE_USE_SYSTEM_ICONV
#define TERSE_USE_SYSTEM_ICONV 1
#endif

#if TERSE_USE_SYSTEM_ICONV
#include <iconv.h>
#else
#include "../src/mini_iconv.h"
#endif

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
int terse_decode_shift_jis_stream(terse_handle_t handle, int fd, unsigned char first, unsigned int *out_scalar);

/* Convert Shift_JIS byte pair to Unicode scalar using iconv handle */
unsigned int terse_convert_shift_jis_pair(terse_handle_t handle, unsigned char lead, unsigned char trail);

/* Reset iconv conversion state */
void terse_codec_reset_iconv_state(iconv_t cd);

/* Replacement characters for invalid sequences */
#define TERSE_UTF8_REPLACEMENT 0xfffdU
#define TERSE_SHIFT_JIS_REPLACEMENT '?'

#endif /* TERSE_CODEC_H_INCLUDED */
