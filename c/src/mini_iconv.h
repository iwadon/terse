#ifndef MINI_ICONV_H
#define MINI_ICONV_H

#include <stddef.h>

typedef struct mini_iconv_handle *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
int iconv_close(iconv_t cd);

#endif /* MINI_ICONV_H */
