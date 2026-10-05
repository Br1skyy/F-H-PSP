/* In-RAM battle trace.
 *
 * The old trace opened, appended to and closed a Memory Stick file on every
 * line, a synchronous FAT write in the middle of combat. This collects lines
 * in RAM and hands them to a sink in one write: when the buffer fills, and at
 * the end of a battle. Portable (no PSP headers) so tests can drive it.
 */
#ifndef FH_TRACE_BUF_H
#define FH_TRACE_BUF_H

#include <string.h>

#define FH_TRACE_CAP 16384

typedef void (*FhTraceSink)(const char *data, unsigned len, int truncate,
                            void *user);

typedef struct {
    char buf[FH_TRACE_CAP];
    unsigned len;
    int truncate;           /* next flush replaces the file instead of appending */
    int live;               /* flush after every line (debugging crashes) */
    FhTraceSink sink;
    void *user;
} FhTrace;

static inline void fh_trace_init(FhTrace *t, FhTraceSink sink, void *user,
                                 int live) {
    t->len = 0;
    t->truncate = 1;
    t->live = live;
    t->sink = sink;
    t->user = user;
}

static inline void fh_trace_flush(FhTrace *t) {
    if (t->len == 0) return;
    if (t->sink) t->sink(t->buf, t->len, t->truncate, t->user);
    t->len = 0;
    t->truncate = 0;
}

/* New battle: drop anything unwritten; the next flush starts a fresh file. */
static inline void fh_trace_begin(FhTrace *t) {
    t->len = 0;
    t->truncate = 1;
}

static inline void fh_trace_write(FhTrace *t, const char *s) {
    unsigned n = (unsigned)strlen(s);
    if (n > FH_TRACE_CAP) n = FH_TRACE_CAP;         /* absurd line: clip */
    if (t->len + n > FH_TRACE_CAP) fh_trace_flush(t);
    memcpy(t->buf + t->len, s, n);
    t->len += n;
    if (t->live) fh_trace_flush(t);
}

#endif
