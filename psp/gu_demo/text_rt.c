/* COPY of runtime/text.c — see runtime/. Refresh with cp. */
#include "text_rt.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define FH_DEC_CAP 16384

static int is_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Substitute one family (\V \N \P \G) over src into dst. Returns dst length. */
static int sub_family(const char *src, int srclen, char *dst, char fam,
                      const FhEscCtx *ctx) {
    int si = 0, di = 0;
    while (si < srclen) {
        if (src[si] == '\x1b' && si + 1 < srclen &&
            (src[si + 1] == fam || src[si + 1] == fam + 32)) {
            int p = si + 2, id = 0, has = 0;
            if (p < srclen && src[p] == '[') {
                p++;
                while (p < srclen && src[p] >= '0' && src[p] <= '9') {
                    id = id * 10 + (src[p] - '0');
                    p++;
                    has = 1;
                }
                if (p < srclen && src[p] == ']') p++;
            }
            char tmp[64];
            int tn = 0;
            if (fam == 'V' && has && id >= 0 && id < ctx->nvars) {
                tn = snprintf(tmp, sizeof(tmp), "%d", (int)ctx->vars[id]);
            } else if (fam == 'N' && has && id >= 1 && id <= ctx->nactors) {
                const char *nm = ctx->actor_names[id - 1];
                tn = snprintf(tmp, sizeof(tmp), "%s", nm ? nm : "");
            } else if (fam == 'P' && has && id >= 1 && id <= ctx->party_n) {
                int aid = ctx->party[id - 1];
                const char *nm = (aid >= 1 && aid <= ctx->nactors) ? ctx->actor_names[aid - 1] : "";
                tn = snprintf(tmp, sizeof(tmp), "%s", nm ? nm : "");
            } else if (fam == 'G') {
                tn = snprintf(tmp, sizeof(tmp), "%s", ctx->currency ? ctx->currency : "");
            } else {
                /* no match: copy ESC + letter verbatim, let tokenizer decide */
                if (di < FH_DEC_CAP - 2) { dst[di++] = src[si++]; dst[di++] = src[si++]; }
                else break;
                continue;
            }
            if (di + tn >= FH_DEC_CAP) break;
            memcpy(dst + di, tmp, (size_t)tn);
            di += tn;
            si = p;
        } else {
            if (di >= FH_DEC_CAP - 1) break;
            dst[di++] = src[si++];
        }
    }
    return di;
}

void fh_decode_escapes(const char *text, const FhEscCtx *ctx, const FhEscCb *cb) {
    static char b0[FH_DEC_CAP], b1[FH_DEC_CAP], b2[FH_DEC_CAP], b3[FH_DEC_CAP];
    int n = (int)strlen(text), i;
    /* '\' -> ESC */
    if (n >= FH_DEC_CAP - 1) n = FH_DEC_CAP - 2;
    for (i = 0; i < n; i++) b0[i] = (text[i] == '\\') ? '\x1b' : text[i];
    /* ESC ESC -> '\' */
    int w = 0;
    for (i = 0; i < n; i++) {
        if (b0[i] == '\x1b' && i + 1 < n && b0[i + 1] == '\x1b') {
            b1[w++] = '\\';
            i++;
        } else b1[w++] = b0[i];
    }
    /* V twice (source quirk), then N, P, G */
    int n1 = sub_family(b1, w, b2, 'V', ctx);
    int n2 = sub_family(b2, n1, b3, 'V', ctx);
    int n3 = sub_family(b3, n2, b2, 'N', ctx);
    int n4 = sub_family(b2, n3, b3, 'P', ctx);
    int n5 = sub_family(b3, n4, b2, 'G', ctx);
    const char *s = b2;
    int len = n5, pos = 0, run = 0;
    char code[16];
    while (pos < len) {
        if (s[pos] == '\x1b') {
            if (run > 0) { cb->on_text(s + pos - run, run, cb->ud); run = 0; }
            pos++;
            int cn = 0;
            if (pos < len && (s[pos] == '$' || s[pos] == '.' || s[pos] == '|' ||
                              s[pos] == '^' || s[pos] == '!' || s[pos] == '>' ||
                              s[pos] == '<' || s[pos] == '{' || s[pos] == '}' ||
                              s[pos] == '\\')) {
                code[cn++] = s[pos++];
            } else {
                while (pos < len && cn < 15 && is_letter(s[pos]))
                    code[cn++] = (char)toupper((unsigned char)s[pos++]);
            }
            code[cn] = '\0';
            int param = -1;
            if (pos < len && s[pos] == '[') {
                int q = pos + 1, v = 0, hd = 0;
                while (q < len && s[q] >= '0' && s[q] <= '9') {
                    v = v * 10 + (s[q] - '0');
                    q++;
                    hd = 1;
                }
                if (hd && q < len && s[q] == ']') { param = v; pos = q + 1; }
            }
            cb->on_code(code, param, cb->ud);
        } else {
            run++;
            pos++;
        }
    }
    if (run > 0) cb->on_text(s + pos - run, run, cb->ud);
}
