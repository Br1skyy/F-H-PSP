/* Rendering implementation */
#include "render.h"
#include "map_runtime.h"
#include <pspge.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <string.h>

#define NX (SCR_W / TILE + 2)
#define NY (SCR_H / TILE + 2)

/* Texture definitions */
const SheetDef SHEETS[9] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {256, 512, 256},  /* Mines_A1: padded from 192x384 */
    {512, 512, 512},  /* Mines_B: padded from 384x384 */
    {512, 512, 512},  /* Mines_E */
    {512, 512, 512},  /* Inside_B */
    {512, 512, 512},  /* Mines_D */
};

unsigned char *sheet_px[9] = {0};
unsigned int sheet_cl[9][256] __attribute__((aligned(16)));

/* Lighting (Terrax replacement): NO render target, NO multiply blend —
 * only ops already proven on this path (T8 swizzled + CLUT + alpha blend).
 * 1. Fullscreen black quad at 92% alpha knocks the scene to ~8%.
 * 2. Warm radial glow sprite(s) add light back (ADD, src×alpha).
 * Looks like torchlight, costs 2 quads. */
#define LIGHT_R 150     /* player radius, screen px (Terrax 300 @ full scale) */
#define LIGHT_DARK_A 235  /* darken quad alpha: scene kept = 1 - 235/255 ≈ 8% */
#define LIGHT_SIZE 128

static unsigned char light_px[LIGHT_SIZE * LIGHT_SIZE];
static unsigned int light_cl[256] __attribute__((aligned(16)));

static void light_swizzle(void) {
    static unsigned char lin[LIGHT_SIZE * LIGHT_SIZE];
    for (int y = 0; y < LIGHT_SIZE; y++) {
        for (int x = 0; x < LIGHT_SIZE; x++) {
            /* Steep cubic falloff: tight pool, no bleached middle.
             * (Additive blending can't reproduce multiply's "never brighter
             * than the scene", so the pool stays dim and narrow.) */
            float dx = ((float)x - 63.5f) / 64.0f;
            float dy = ((float)y - 63.5f) / 64.0f;
            float d = dx * dx + dy * dy;
            float a = 1.0f - d;
            if (a < 0.0f) a = 0.0f;
            a = a * a * a;
            lin[y * LIGHT_SIZE + x] = (unsigned char)(a * 255.0f);
        }
    }
    /* Neutral white peak like the original (playercolor defaults to
     * #FFFFFF); dim enough that additive light never blows out. */
    for (int i = 0; i < 256; i++)
        light_cl[i] = 0x00000000 | (120u << 16) | (120u << 8) | 120u | ((unsigned int)i << 24);
    /* 16×8 swizzle (same layout as convert_assets.py). */
    static unsigned char sw[LIGHT_SIZE * LIGHT_SIZE];
    int dst = 0;
    for (int by = 0; by < LIGHT_SIZE; by += 8) {
        for (int bx = 0; bx < LIGHT_SIZE; bx += 16) {
            for (int r = 0; r < 8; r++) {
                int src = (by + r) * LIGHT_SIZE + bx;
                for (int k = 0; k < 16; k++) sw[dst + k] = lin[src + k];
                dst += 16;
            }
        }
    }
    for (int i = 0; i < LIGHT_SIZE * LIGHT_SIZE; i++) light_px[i] = sw[i];
}

/* Vertex type for textured quads */
typedef struct { float u, v; unsigned int color; float x, y, z; } TVert;
#define TVERT_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int *gu_list_ptr;

void render_init(void *fbp0, void *fbp1, void *zbp, unsigned int *gu_list) {
    gu_list_ptr = gu_list;
    light_swizzle();
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, fbp1, BUF_W);
    sceGuDepthBuffer(zbp, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(1);
}

static void tquad(TVert **vp, float u0, float v0, float u1, float v1,
                  int x0, int y0, int x1, int y1) {
    TVert *v = *vp;
    v[0].u = u0; v[0].v = v0; v[0].color = 0xffffffff;
    v[0].x = (float)x0; v[0].y = (float)y0; v[0].z = 0.0f;
    v[1].u = u1; v[1].v = v1; v[1].color = 0xffffffff;
    v[1].x = (float)x1; v[1].y = (float)y1; v[1].z = 0.0f;
    *vp += 2;
}

void render_map_layers(int cam_x, int cam_y, const uint16_t *map_layers, int map_w, int map_h,
                       const uint8_t *higher, int higher_len, int upper_pass) {
    int x0 = cam_x / TILE, y0 = cam_y / TILE;
    int ox = cam_x % TILE, oy = cam_y % TILE;
    
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuAmbientColor(0xffffffff);
    
    /* Render layers 0-3 bottom to top */
    for (int layer = 0; layer < 4; layer++) {
        for (int s = 4; s < 9; s++) {
            /* Bind texture for this sheet */
            sceGuClutLoad(32, sheet_cl[s]);
            sceGuTexMode(GU_PSM_T8, 0, 0, 1);  /* swizzled */
            sceGuTexImage(0, SHEETS[s].tw, SHEETS[s].th, SHEETS[s].stride, sheet_px[s]);
            sceGuTexFlush();
            sceGuTexSync();
            
            /* Collect tiles for this layer+sheet */
            static FhDraw collected[NX * NY];
            int n = fh_collect(x0, y0, NX, NY, TILE, ox, oy, layer, s,
                             map_layers, map_w, map_h, collected, NX * NY);
            
            if (n == 0) continue;
            
            /* Enable alpha test and blending */
            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GREATER, 0, 0xff);
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
            
            /* Draw matching tiles in one batch (OG z-split: higher ★ tiles
             * go to the upper bitmap, everything else to the lower). */
            TVert *v = (TVert *)sceGuGetMemory(n * 2 * sizeof(TVert));
            TVert *vp = v;
            int m = 0;
            for (int i = 0; i < n; i++) {
                int tid = collected[i].tid;
                int is_high = (higher && tid >= 0 && tid < higher_len)
                              ? higher[tid] : 0;
                if (!!is_high != !!upper_pass) continue;
                FhQuad *q = &collected[i].q;
                tquad(&vp, (float)(q->su / 2), (float)(q->sv / 2),
                          (float)((q->su + q->sw) / 2), (float)((q->sv + q->sh) / 2),
                          collected[i].dx, collected[i].dy,
                          collected[i].dx + TILE, collected[i].dy + TILE);
                m++;
            }
            if (m > 0) sceGuDrawArray(GU_SPRITES, TVERT_FMT, m * 2, 0, v);
        }
    }
    
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data) {
    if (!sprite_data || !clut_data) return;
    
    /* Character sprites are SWIZZLED 512×512 (padded from 480×440).
     * The sheet holds ONE character's states on a 12-col × 8-row grid
     * (RPG Maker non-$ layout, cells 80×110 full-res → 40×55 at our 0.5
     * scale). The walking block is characterIndex 0: cols 0-2, rows 0-3.
     * Rows: 0=down, 1=left, 2=right, 3=up (matches Player.dir).
     * Cols: pattern 1=center(idle), 0=left foot, 2=right foot. */
    int cell_w = 40;  /* 480/12 */
    int cell_h = 55;  /* 440/8 */

    /* sprite_pattern selects the state block (0 = walking). */
    int pat_block = player->sprite_pattern;
    if (pat_block < 0) pat_block = 0;
    if (pat_block > 7) pat_block = 7;
    int bx = (pat_block % 4) * 3;  /* block origin in cells */
    int by = (pat_block / 4) * 4;

    /* player->step_frame: 0=center(idle), 1=left, 2=right.
     * Sheet pattern col: 1=center, 0=left, 2=right. */
    int pat_col = (player->step_frame == 0) ? 1
                : (player->step_frame == 1) ? 0 : 2;

    int dir = player->dir;
    if (dir < 0) dir = 0;
    if (dir > 3) dir = 3;

    /* Calculate which single cell to show based on player state */
    int frame_x = (bx + pat_col) * cell_w;
    int frame_y = (by + dir) * cell_h;
    int frame_w = cell_w;
    int frame_h = cell_h;

    /* Same texture state as the map path (proven working): CLUT first,
     * then TexMode/TexImage, explicit scale/offset, alpha-test + blend
     * so palette index 0 (transparent) is discarded. */
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, clut_data);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);  /* T8, swizzled=1 */
    sceGuTexImage(0, 512, 512, 512, sprite_data);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    /* Screen position (bottom-align the 40×55 cell on the 24px tile) */
    int screen_x = player->x - cam_x - (frame_w - TILE) / 2;
    int screen_y = player->y - cam_y - (frame_h - TILE);
    
    /* UV coords in PIXELS (no /2 needed for swizzled textures with float UVs) */
    float u0 = (float)frame_x;
    float v0 = (float)frame_y;
    float u1 = (float)(frame_x + frame_w);
    float v1 = (float)(frame_y + frame_h);
    
    /* Draw single frame using GU_SPRITES */
    TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    v[0].u = u0; v[0].v = v0; v[0].color = 0xffffffff;
    v[0].x = (float)screen_x; v[0].y = (float)screen_y; v[0].z = 0.0f;
    v[1].u = u1; v[1].v = v1; v[1].color = 0xffffffff;
    v[1].x = (float)(screen_x + frame_w); v[1].y = (float)(screen_y + frame_h); v[1].z = 0.0f;
    
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

/* Lighting composite: darken everything, then add the player glow.
 * Uses only the proven sprite path (T8 swizzled + CLUT + alpha blend).
 * frames drives the torch flicker. */
static void render_light_pass(const Player *player, int cam_x, int cam_y,
                              int frames) {
    /* 1. Knock the scene down to ~8% with a fullscreen black quad.
     * out = dst × (1 - alpha), standard sprite blend. */
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    TVert *d = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    d[0].u = 0; d[0].v = 0; d[0].color = ((unsigned int)LIGHT_DARK_A << 24);
    d[0].x = 0; d[0].y = 0; d[0].z = 0.0f;
    d[1].u = 0; d[1].v = 0; d[1].color = ((unsigned int)LIGHT_DARK_A << 24);
    d[1].x = SCR_W; d[1].y = SCR_H; d[1].z = 0.0f;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, d);

    /* 2. Add the warm glow back around the player (ADD, src×alpha). */
    int px = player->x - cam_x + TILE / 2;
    int py = player->y - cam_y - 8;
    int r = LIGHT_R + ((frames % 9) - 4);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, light_cl);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, LIGHT_SIZE, LIGHT_SIZE, LIGHT_SIZE, light_px);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffff);
    TVert *g = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    g[0].u = 0; g[0].v = 0; g[0].color = 0xffffffff;
    g[0].x = (float)(px - r); g[0].y = (float)(py - r); g[0].z = 0.0f;
    g[1].u = LIGHT_SIZE; g[1].v = LIGHT_SIZE; g[1].color = 0xffffffff;
    g[1].x = (float)(px + r); g[1].y = (float)(py + r); g[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, g);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_frame(int cam_x, int cam_y,
                  const uint16_t *map_layers, int map_w, int map_h,
                  const Player *player, int current_char,
                  unsigned char *char_sprites[4],
                  unsigned int *char_cluts[4],
                  const uint8_t *higher, int higher_len, int frames) {
    sceGuStart(GU_DIRECT, gu_list_ptr);
    sceGuClearColor(0xff000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    
    /* OG order: z=0 tiles, same-priority characters (z=3), z=4 higher ★
     * tiles (rpg_core.js Tilemap z=0/4, screenZ = priorityType*2+1). */
    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 0);
    
    /* Render player */
    render_player_sprite(player, cam_x, cam_y,
                        char_sprites[current_char],
                        char_cluts[current_char]);

    /* Higher tiles draw over the player (canopies, rafters, tall walls) */
    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 1);

    /* Darkness + player glow. */
    render_light_pass(player, cam_x, cam_y, frames);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

void render_debug_text(const char *text) {
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("%s", text);
}
