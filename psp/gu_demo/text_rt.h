

#ifndef FH_TEXT_RT_H
#define FH_TEXT_RT_H

#include <stdint.h>

typedef struct {
    void (*on_text)(const char *ptr, int len, void *ud);
    void (*on_code)(const char *code, int param, void *ud);
    void *ud;
} FhEscCb;

typedef struct {
    const int32_t *vars; int nvars;
    const char **actor_names; int nactors;
    const int *party; int party_n;
    const char *currency;
} FhEscCtx;

void fh_decode_escapes(const char *text, const FhEscCtx *ctx, const FhEscCb *cb);

#endif
