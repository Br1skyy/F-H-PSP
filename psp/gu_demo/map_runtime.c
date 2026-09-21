/* COPY of runtime/map.c — see runtime/. Refresh with cp. */
/* See map.h. Tables themselves live in baked data; quadrant lookup takes
 * the table as 48/16/4 x 4 x 2 ints supplied by the caller (keeps this
 * file data-free and unit-testable). */
#include "map_runtime.h"

int fh_is_autotile(int tid) { return tid >= FH_TILE_A1 && tid < FH_TILE_MAX; }
int fh_is_a5(int tid) { return tid >= FH_TILE_A5 && tid < FH_TILE_A1; }

int fh_normal_quad(int tid, FhQuad *q) {
    if (tid <= 0 || tid >= FH_TILE_A1) return 0;
    int set = fh_is_a5(tid) ? 4 : 5 + tid / 256;
    q->slot = set;
    q->su = (tid / 128 % 2 * 8 + tid % 8) * FH_TILE_FULL;
    q->sv = (tid % 256 / 8 % 16) * FH_TILE_FULL;
    q->sw = FH_TILE_FULL;
    q->sh = FH_TILE_FULL;
    return 1;
}

int fh_auto_kind(int tid) { return (tid - FH_TILE_A1) / 48; }
int fh_auto_shape(int tid) { return (tid - FH_TILE_A1) % 48; }

static int is_a1(int t) { return t >= FH_TILE_A1 && t < FH_TILE_A2; }
static int is_a2(int t) { return t >= FH_TILE_A2 && t < FH_TILE_A3; }
static int is_a3(int t) { return t >= FH_TILE_A3 && t < FH_TILE_A4; }
static int is_a4(int t) { return t >= FH_TILE_A4 && t < FH_TILE_MAX; }

int fh_auto_cell(int tid, int *set, int *bx, int *by, int *table) {
    if (!fh_is_autotile(tid)) return 0;
    int kind = fh_auto_kind(tid);
    int tx = kind % 8, ty = kind / 8;
    if (is_a1(tid)) {
        *set = 0;
        if (kind == 0) { *bx = 0; *by = 0; *table = 0; return 1; }
        if (kind == 1) { *bx = 0; *by = 3; *table = 0; return 1; }
        if (kind == 2) { *bx = 6; *by = 0; *table = 0; return 1; }
        if (kind == 3) { *bx = 6; *by = 3; *table = 0; return 1; }
        *bx = tx / 4 * 8;
        *by = ty * 6 + (tx / 2) % 2 * 3;
        if (kind % 2 == 0) { *table = 0; return 1; }
        *bx += 6;
        *table = 2;  /* waterfall (frame-0 row) */
        return 1;
    }
    if (is_a2(tid)) {
        *set = 1;
        *bx = tx * 2;
        *by = (ty - 2) * 3;
        *table = 0;
        return 1;
    }
    if (is_a3(tid)) {
        *set = 2;
        *bx = tx * 2;
        *by = (ty - 6) * 2;
        *table = 1;
        return 1;
    }
    if (!is_a4(tid)) return 0;
    /* A4 */
    *set = 3;
    *bx = tx * 2;
    *by = (int)((ty - 10) * 2.5 + (ty % 2 == 1 ? 0.5 : 0));
    *table = (ty % 2 == 1) ? 1 : 0;
    return 1;
}

int fh_collect(int x0, int y0, int nx, int ny, int tile_px, int ox, int oy,
               int z, int slot, const uint16_t *layers, int W, int H,
               FhDraw *out, int max) {
    int n = 0;
#ifdef REVERSE_ORDER
    /* Diagnostic: collect bottom-up. If failing cells change, the fault is
     * buffer-order-dependent, not data-dependent. */
    for (int ty = ny - 1; ty >= 0; ty--) {
        for (int tx = nx - 1; tx >= 0; tx--) {
#else
    for (int ty = 0; ty < ny; ty++) {
        for (int tx = 0; tx < nx; tx++) {
#endif
            int mx = x0 + tx, my = y0 + ty;
            if (mx < 0 || my < 0 || mx >= W || my >= H) continue;
            int tid = layers[(z * H + my) * W + mx];
            FhQuad q;
            if (!fh_normal_quad(tid, &q) || q.slot != slot) continue;
            if (n >= max) return n;
            out[n].q = q;
            out[n].dx = tx * tile_px - ox;
            out[n].dy = ty * tile_px - oy;
            out[n].tid = tid;
            n++;
        }
    }
    return n;
}
