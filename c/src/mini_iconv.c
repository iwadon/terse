#include "mini_iconv.h"

#include "mini_iconv_tables.h"
#include "terse.h"
#include "terse_handle.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum mini_iconv_direction {
	MINI_ICONV_SJIS_TO_UTF8 = 0,
	MINI_ICONV_UTF8_TO_SJIS = 1,
};

struct mini_iconv_handle {
	enum mini_iconv_direction direction;
};

static void
mini_iconv_canonicalize(const char *src, char *dst, size_t dst_size)
{
	size_t pos = 0;
	if (!src || !dst || dst_size == 0) {
		return;
	}
	for (size_t i = 0; src[i] != '\0'; ++i) {
		unsigned char ch = (unsigned char)src[i];
		if (ch == '-' || ch == '_' || ch == '.' || ch == ' ') {
			continue;
		}
		if (pos + 1 >= dst_size) {
			break;
		}
		dst[pos++] = (char)toupper(ch);
	}
	dst[pos] = '\0';
}

static int
mini_iconv_is_utf8(const char *name)
{
	char canonical[TERSE_SMALL_BUFFER_SIZE];
	mini_iconv_canonicalize(name, canonical, sizeof(canonical));
	return strcmp(canonical, "UTF8") == 0;
}

static int
mini_iconv_is_shift_jis(const char *name)
{
	char canonical[TERSE_SMALL_BUFFER_SIZE];
	mini_iconv_canonicalize(name, canonical, sizeof(canonical));
	return strcmp(canonical, "SHIFTJIS") == 0 || strcmp(canonical, "SJIS") == 0 || strcmp(canonical, "CP932") == 0;
}

static size_t
mini_iconv_utf8_length(uint32_t codepoint)
{
	if (codepoint <= 0x7f) {
		return 1;
	}
	if (codepoint <= 0x7ff) {
		return 2;
	}
	if (codepoint <= 0xffff) {
		return 3;
	}
	return 4;
}

static void
mini_iconv_write_utf8(uint32_t codepoint, char *out)
{
	if (codepoint <= 0x7f) {
		out[0] = (char)codepoint;
		return;
	}
	if (codepoint <= 0x7ff) {
		out[0] = (char)(0xc0 | ((codepoint >> 6) & 0x1f));
		out[1] = (char)(0x80 | (codepoint & 0x3f));
		return;
	}
	if (codepoint <= 0xffff) {
		out[0] = (char)(0xe0 | ((codepoint >> 12) & 0x0f));
		out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
		out[2] = (char)(0x80 | (codepoint & 0x3f));
		return;
	}
	out[0] = (char)(0xf0 | ((codepoint >> 18) & 0x07));
	out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
	out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
	out[3] = (char)(0x80 | (codepoint & 0x3f));
}

static uint32_t
mini_iconv_lookup_halfwidth(unsigned char byte)
{
	if (byte < 0xa1 || byte > 0xdf) {
		return 0;
	}
	return mini_iconv_halfwidth_table[byte - 0xa1];
}

static uint32_t
mini_iconv_lookup_double(unsigned char lead, unsigned char trail)
{
	int li = mini_iconv_lead_index(lead);
	if (li < 0) {
		return 0;
	}
	int ti = mini_iconv_trail_index(trail);
	if (ti < 0) {
		return 0;
	}
	uint16_t value = mini_iconv_sjis_double_map[li * MINI_ICONV_SJIS_TRAIL_COUNT + (size_t)ti];
	return value;
}

static uint16_t
mini_iconv_lookup_unicode(uint32_t codepoint)
{
	size_t left = 0;
	size_t right = MINI_ICONV_UNICODE_MAP_COUNT;
	while (left < right) {
		size_t mid = left + (right - left) / 2;
		uint32_t key = mini_iconv_unicode_keys[mid];
		if (codepoint == key) {
			return mini_iconv_unicode_values[mid];
		}
		if (codepoint < key) {
			right = mid;
			continue;
		}
		left = mid + 1;
	}
	return 0;
}

static int
mini_iconv_decode_utf8(const unsigned char *in, size_t inleft, uint32_t *out_codepoint, size_t *consumed)
{
	if (inleft == 0) {
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	unsigned char b0 = in[0];
	if (b0 < 0x80) {
		*out_codepoint = b0;
		*consumed = 1;
		return 0;
	}
	if ((b0 & 0xe0) == 0xc0) {
		if (inleft < 2) {
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		unsigned char b1 = in[1];
		if ((b1 & 0xc0) != 0x80 || (b0 & 0xfe) == 0xc0) {
			return TERSE_ERR_INVALID_ENCODING;
		}
		*out_codepoint = ((uint32_t)(b0 & 0x1f) << 6) | (uint32_t)(b1 & 0x3f);
		*consumed = 2;
		return 0;
	}
	if ((b0 & 0xf0) == 0xe0) {
		if (inleft < 3) {
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		unsigned char b1 = in[1];
		unsigned char b2 = in[2];
		if ((b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) {
			return TERSE_ERR_INVALID_ENCODING;
		}
		uint32_t codepoint = ((uint32_t)(b0 & 0x0f) << 12) | ((uint32_t)(b1 & 0x3f) << 6) | (uint32_t)(b2 & 0x3f);
		if (codepoint < 0x800 || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
			return TERSE_ERR_INVALID_ENCODING;
		}
		*out_codepoint = codepoint;
		*consumed = 3;
		return 0;
	}
	if ((b0 & 0xf8) == 0xf0) {
		if (inleft < 4) {
			return TERSE_ERR_INVALID_ARGUMENT;
		}
		unsigned char b1 = in[1];
		unsigned char b2 = in[2];
		unsigned char b3 = in[3];
		if ((b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) {
			return TERSE_ERR_INVALID_ENCODING;
		}
		uint32_t codepoint = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(b1 & 0x3f) << 12) | ((uint32_t)(b2 & 0x3f) << 6) | (uint32_t)(b3 & 0x3f);
		if (codepoint < 0x10000 || codepoint > 0x10ffff) {
			return TERSE_ERR_INVALID_ENCODING;
		}
		*out_codepoint = codepoint;
		*consumed = 4;
		return 0;
	}
	return TERSE_ERR_INVALID_ENCODING;
}

static size_t
mini_iconv_sjis_to_utf8(iconv_t handle, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	(void)handle;
	unsigned char *in_ptr = (unsigned char *)(*inbuf);
	size_t in_remaining = *inbytesleft;
	char *out_ptr = *outbuf;
	size_t out_remaining = *outbytesleft;

	while (in_remaining > 0) {
		unsigned char lead = in_ptr[0];
		uint32_t codepoint = 0;
		size_t consumed = 1;
		if (lead <= 0x7f) {
			codepoint = lead;
		} else if (lead >= 0xa1 && lead <= 0xdf) {
			codepoint = mini_iconv_lookup_halfwidth(lead);
			if (codepoint == 0) {
				errno = EILSEQ;
				goto error;
			}
		} else {
			if (in_remaining < 2) {
				errno = EINVAL;
				goto error;
			}
			unsigned char trail = in_ptr[1];
			codepoint = mini_iconv_lookup_double(lead, trail);
			if (codepoint == 0) {
				errno = EILSEQ;
				goto error;
			}
			consumed = 2;
		}

		size_t utf8_len = mini_iconv_utf8_length(codepoint);
		if (out_remaining < utf8_len) {
			errno = TERSE_ERR_BUFFER_TOO_SMALL;
			goto error;
		}

		mini_iconv_write_utf8(codepoint, out_ptr);
		in_ptr += consumed;
		in_remaining -= consumed;
		out_ptr += utf8_len;
		out_remaining -= utf8_len;
	}

	*inbuf = (char *)in_ptr;
	*inbytesleft = in_remaining;
	*outbuf = out_ptr;
	*outbytesleft = out_remaining;
	return 0;

error:
	*inbuf = (char *)in_ptr;
	*inbytesleft = in_remaining;
	*outbuf = out_ptr;
	*outbytesleft = out_remaining;
	return (size_t)-1;
}

static size_t
mini_iconv_utf8_to_sjis(iconv_t handle, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	(void)handle;
	unsigned char *in_ptr = (unsigned char *)(*inbuf);
	size_t in_remaining = *inbytesleft;
	char *out_ptr = *outbuf;
	size_t out_remaining = *outbytesleft;

	while (in_remaining > 0) {
		uint32_t codepoint = 0;
		size_t consumed = 0;
		int err = mini_iconv_decode_utf8(in_ptr, in_remaining, &codepoint, &consumed);
		if (err != 0) {
			errno = err;
			goto error;
		}

		uint16_t sjis_value;
		if (codepoint <= 0x7f) {
			sjis_value = (uint16_t)codepoint;
		} else {
			sjis_value = mini_iconv_lookup_unicode(codepoint);
			if (sjis_value == 0) {
				errno = EILSEQ;
				goto error;
			}
		}

		size_t needed = (sjis_value <= 0xff) ? 1 : 2;
		if (out_remaining < needed) {
			errno = TERSE_ERR_BUFFER_TOO_SMALL;
			goto error;
		}

		if (needed == 1) {
			out_ptr[0] = (char)sjis_value;
		} else {
			out_ptr[0] = (char)((sjis_value >> 8) & 0xff);
			out_ptr[1] = (char)(sjis_value & 0xff);
		}

		in_ptr += consumed;
		in_remaining -= consumed;
		out_ptr += needed;
		out_remaining -= needed;
	}

	*inbuf = (char *)in_ptr;
	*inbytesleft = in_remaining;
	*outbuf = out_ptr;
	*outbytesleft = out_remaining;
	return 0;

error:
	*inbuf = (char *)in_ptr;
	*inbytesleft = in_remaining;
	*outbuf = out_ptr;
	*outbytesleft = out_remaining;
	return (size_t)-1;
}

iconv_t
iconv_open(const char *tocode, const char *fromcode)
{
	int direction = -1;
	if (mini_iconv_is_utf8(tocode) && mini_iconv_is_shift_jis(fromcode)) {
		direction = MINI_ICONV_SJIS_TO_UTF8;
	} else if (mini_iconv_is_shift_jis(tocode) && mini_iconv_is_utf8(fromcode)) {
		direction = MINI_ICONV_UTF8_TO_SJIS;
	} else {
		errno = EINVAL;
		return (iconv_t)(intptr_t)(-1);
	}

	struct mini_iconv_handle *handle = (struct mini_iconv_handle *)malloc(sizeof(*handle));
	if (!handle) {
		errno = ENOMEM;
		return (iconv_t)(intptr_t)(-1);
	}
	handle->direction = (enum mini_iconv_direction)direction;
	return (iconv_t)handle;
}

size_t
iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	if (!cd) {
		errno = EBADF;
		return (size_t)-1;
	}
	if (!inbuf || !outbuf || !inbytesleft || !outbytesleft) {
		errno = EINVAL;
		return (size_t)-1;
	}
	if (*inbuf == NULL) {
		return 0;
	}

	struct mini_iconv_handle *handle = (struct mini_iconv_handle *)cd;
	if (handle->direction == MINI_ICONV_SJIS_TO_UTF8) {
		return mini_iconv_sjis_to_utf8(cd, inbuf, inbytesleft, outbuf, outbytesleft);
	}
	return mini_iconv_utf8_to_sjis(cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

int
iconv_close(iconv_t cd)
{
	if (!cd) {
		errno = EBADF;
		return -1;
	}
	free((struct mini_iconv_handle *)cd);
	return 0;
}
