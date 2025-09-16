#ifndef TERSE_H_INCLUDED
#define TERSE_H_INCLUDED

typedef struct terse_ctx *terse_ctx_t;

terse_ctx_t terse_open();
void terse_close(terse_ctx_t ctx);

#endif // TERSE_H_INCLUDED
