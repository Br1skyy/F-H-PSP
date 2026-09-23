

#ifndef FH_MAP_RUNTIME_H
#define FH_MAP_RUNTIME_H

#include <stdint.h>

#define FH_TILE_FULL 48
#define FH_TILE_HALF 24
#define FH_TILE_A1 2048
#define FH_TILE_A2 2816
#define FH_TILE_A3 4352
#define FH_TILE_A4 5888
#define FH_TILE_A5 1536
#define FH_TILE_MAX 8192

typedef struct { int slot, su, sv, sw, sh; } FhQuad;

int fh_is_autotile(int tid);
int fh_is_a5(int tid);

int fh_normal_quad(int tid, FhQuad *q);

int fh_auto_cell(int tid, int *set, int *bx, int *by, int *table);
int fh_auto_shape(int tid);
int fh_auto_kind(int tid);


typedef struct { FhQuad q; int dx, dy; int tid; } FhDraw;


int fh_collect(int x0, int y0, int nx, int ny, int tile_px, int ox, int oy,
               int z, int slot, const uint16_t *layers, int W, int H,
               FhDraw *out, int max);

#endif
