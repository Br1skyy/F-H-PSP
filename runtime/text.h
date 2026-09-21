/* Message escape decoder — ports Window_Base.convertEscapeCharacters +
 * obtainEscapeCode/obtainEscapeParam (rpg_windows.js, stock MV).
 *
 * Phase 1 (at decode): '\' -> ESC; '\\' -> literal; \V[n]/\N[n]/\P[n]/\G
 * substituted (case-insensitive, V applied twice like the source quirk).
 * Phase 2 (tokenize): after ESC, one of [$ . | ^ ! > < { } \] or [A-Za-z]+,
 * uppercased; optional [digits] param (only digits, like /^\[\d+\]/).
 *
 * Calls cb->on_text(ptr,len) for runs and cb->on_code(code,param) per escape.
 * code is the uppercased letter(s) or single symbol; param -1 when absent.
 */
#ifndef FH_TEXT_H
#define FH_TEXT_H

#include <stdint.h>

typedef struct {
    void (*on_text)(const char *ptr, int len, void *ud);
    void (*on_code)(const char *code, int param, void *ud);
    void *ud;
} FhEscCb;

typedef struct {
    const int32_t *vars; int nvars;
    const char **actor_names; int nactors;   /* 1-based ids */
    const int *party; int party_n;
    const char *currency;
} FhEscCtx;

void fh_decode_escapes(const char *text, const FhEscCtx *ctx, const FhEscCb *cb);

#endif
