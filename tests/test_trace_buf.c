#include <stdio.h>
#include <string.h>
#include "../runtime/trace_buf.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static int calls, truncs;
static unsigned total;
static char got[65536];
static void sink(const char *d, unsigned n, int trunc, void *u) {
    (void)u;
    calls++; truncs += trunc;
    if (trunc) total = 0;
    memcpy(got + total, d, n);
    total += n;
    got[total] = 0;
}

static FhTrace T;

int main(void) {
    fh_trace_init(&T, sink, NULL, 0);
    fh_trace_begin(&T);
    for (int i = 0; i < 50; i++) fh_trace_write(&T, "strike line\n");
    CHECK(calls == 0, "buffered mode must not write per line (calls=%d)", calls);
    fh_trace_flush(&T);
    CHECK(calls == 1 && truncs == 1, "one truncating write at end (calls=%d truncs=%d)", calls, truncs);
    CHECK(total == 50 * 12, "all bytes delivered (%u)", total);

    fh_trace_flush(&T);
    CHECK(calls == 1, "empty flush is a no-op");

    /* appends after the first flush do not truncate */
    fh_trace_write(&T, "more\n");
    fh_trace_flush(&T);
    CHECK(calls == 2 && truncs == 1, "second flush appends");
    CHECK(strstr(got, "more\n") && strncmp(got, "strike", 6) == 0, "content preserved");

    /* overflow flushes once, loses nothing, keeps order */
    calls = truncs = 0; total = 0;
    fh_trace_begin(&T);
    int lines = 3000;
    for (int i = 0; i < lines; i++) fh_trace_write(&T, "0123456789\n");
    fh_trace_flush(&T);
    CHECK(total == (unsigned)lines * 11, "no bytes lost on overflow (%u)", total);
    CHECK(calls == 3 && truncs == 1, "33000 bytes = 2 fills + final (calls=%d)", calls);

    /* begin() discards unwritten lines from a previous battle */
    fh_trace_write(&T, "stale\n");
    fh_trace_begin(&T);
    calls = truncs = 0; total = 0;
    fh_trace_write(&T, "fresh\n");
    fh_trace_flush(&T);
    CHECK(strcmp(got, "fresh\n") == 0 && truncs == 1, "stale line leaked: '%s'", got);

    /* a single oversized line is clipped, never overruns */
    static char big[FH_TRACE_CAP * 2];
    memset(big, 'x', sizeof(big) - 1);
    fh_trace_begin(&T);
    fh_trace_write(&T, big);
    fh_trace_flush(&T);
    CHECK(total == FH_TRACE_CAP, "oversized line clipped to cap (%u)", total);

    /* live mode writes through every line */
    FhTrace L;
    fh_trace_init(&L, sink, NULL, 1);
    calls = 0;
    fh_trace_write(&L, "a\n"); fh_trace_write(&L, "b\n");
    CHECK(calls == 2, "live mode flushes each line (%d)", calls);

    printf(fails ? "%d FAILURES\n" : "ALL PASS\n", fails);
    return fails != 0;
}
