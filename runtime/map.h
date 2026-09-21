/* Tile resolver — ports Tilemap._drawNormalTile/_drawAutotile (rpg_core.js).
 *
 * Units are FULL MV pixels (48px tiles); the caller scales to its tile size.
 * Autotile tables (FLOOR/WALL/WATERFALL) are baked data (converted/baked/
 * autotiles.json); this game ships zero A1-A4 autotiles (max tid 1663), so
 * that path is insurance, unit-tested with synthetic ids.
 *
 * Normal tiles: set = A5->4 else 5+tid/256; sx=(tid/128%2*8+tid%8)*48;
 *               sy=(tid%256/8%16)*48. (rpg_core.js:5010)
 * Autotiles: kind=(tid-2048)/48, shape=(tid-2048)%48; (set,bx,by) per family
 *               at animation frame 0; 4 quadrant rects from the tables.
 */
#ifndef FH_MAP_H
#define FH_MAP_H

#include <stdint.h>

#define FH_TILE_FULL 48
#define FH_TILE_HALF 24
#define FH_TILE_A1 2048
#define FH_TILE_A2 2816
#define FH_TILE_A3 4352
#define FH_TILE_A4 5888
#define FH_TILE_A5 1536
#define FH_TILE_MAX 8192

typedef struct { int slot, su, sv, sw, sh; } FhQuad;  /* source rect, full-px */

int fh_is_autotile(int tid);
int fh_is_a5(int tid);
/* Normal tile -> 1 quad. Returns 0 when tid is empty/out of range. */
int fh_normal_quad(int tid, FhQuad *q);
/* Autotile tile -> set/bx/by/table. table: 0 FLOOR, 1 WALL, 2 WATERFALL. */
int fh_auto_cell(int tid, int *set, int *bx, int *by, int *table);
int fh_auto_shape(int tid);
int fh_auto_kind(int tid);

/* Ordered draw item for one (z, sheet) group: source rect (full-px) + dest
 * origin in screen pixels + source tile id (for the higher-tile ★ flag,
 * flags[tid] & 0x10, which draws above same-priority characters).
 * The EBOOT and the PC harness share this. */
typedef struct { FhQuad q; int dx, dy; int tid; } FhDraw;

/* Collect visible quads for one layer+sheet group, row-major (ty,tx).
 * layers points at 4*H*W row-major u16 tileIds. dx/dy are final screen
 * pixels (scroll offset applied). Returns count (<= max). */
int fh_collect(int x0, int y0, int nx, int ny, int tile_px, int ox, int oy,
               int z, int slot, const uint16_t *layers, int W, int H,
               FhDraw *out, int max);

#endif
