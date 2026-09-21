/* Rendering implementation */
#include "render.h"
#include "map_runtime.h"
#include <pspge.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <string.h>

unsigned char *font_px = 0;
unsigned int *font_cl = 0;

/* Text colors sampled from the game's own Window.png palette grid
 * (rpg_windows.js Window_Base.textColor: px = 96+(n%8)*12+6, and py row).
 * Only 0 (white) and 2 (item orange) appear in baked dialogue so far. */
static const unsigned int MSG_PAL[32] = {
    0xffffffff, 0xffd6a020, 0xff4c78ff, 0xff40cc66,
    0xffffcc99, 0xffffc0cc, 0xffa0ffff, 0xff808080,
    0xffc0c0c0, 0xffcc8020, 0xff1038ff, 0xff10a000,
    0xffde9a3e, 0xffff98a0, 0xff39d6fd, 0xff000000,
    0xff9b7e64, 0xff7bdcdb, 0xff2020ff, 0xff402020,
    0xff2a3023, 0xff112511, 0xff2a3023, 0xff112511,
    0xff80ff80, 0xff8080c0, 0xffff8080, 0xfff0c041,
    0xff40100a, 0xff60e060, 0xffe060a0, 0xffff80c0,
};
#include <math.h>

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

/* Lighting (Terrax replacement): the OG mask math, no render target.
 * Terrax fills the light mask black, then punches a radial gradient
 * (white core r<20, LINEAR ramp white→black out to r=300, full-res px)
 * applied as multiply: center shows the scene at full brightness, edges
 * fall to pure black. A vertex-interpolated mesh faceted visibly along
 * triangle diagonals, so instead the mask is ONE fullscreen sprite sampling
 * a baked radial texture (bilinear = pixel-smooth): texel alpha holds
 * 1 - mask, blended normally over black RGB, i.e. out = dst × mask.
 * Half-scale: core r<10, ramp to R=150. UV window recenters on the player
 * every frame (CLAMP gives black outside the radius). */
#define LIGHT_R 150     /* player radius, screen px (Terrax 300 @ full scale) */
#define LIGHT_CORE 10   /* white core radius (Terrax 20 @ full scale) */
#define LIGHT_TEX 256   /* baked gradient texture size (radius = half) */

static unsigned char light_px[LIGHT_TEX * LIGHT_TEX];
static unsigned int light_cl[256] __attribute__((aligned(16)));

static void light_bake(void) {
    /* Texel alpha = 1 - mask sampled on the exact Terrax curve. */
    static unsigned char lin[LIGHT_TEX * LIGHT_TEX];
    for (int y = 0; y < LIGHT_TEX; y++) {
        for (int x = 0; x < LIGHT_TEX; x++) {
            float dx = (float)x - (LIGHT_TEX / 2 - 0.5f);
            float dy = (float)y - (LIGHT_TEX / 2 - 0.5f);
            float d = sqrtf(dx * dx + dy * dy);
            float core = (float)LIGHT_TEX / 2.0f * (float)LIGHT_CORE / (float)LIGHT_R;
            float m = 1.0f;
            if (d > core)
                m = 1.0f - (d - core) / ((float)LIGHT_TEX / 2.0f - core);
            if (m < 0.0f) m = 0.0f;
            lin[y * LIGHT_TEX + x] = (unsigned char)((1.0f - m) * 255.0f);
        }
    }
    for (int i = 0; i < 256; i++)
        light_cl[i] = ((unsigned int)i << 24);  /* black, alpha = 1 - mask */
    /* 16×8 swizzle (same layout as convert_assets.py). */
    int dst = 0;
    for (int by = 0; by < LIGHT_TEX; by += 8) {
        for (int bx = 0; bx < LIGHT_TEX; bx += 16) {
            for (int r = 0; r < 8; r++) {
                int src = (by + r) * LIGHT_TEX + bx;
                for (int k = 0; k < 16; k++)
                    light_px[dst + k] = lin[src + k];
                dst += 16;
            }
        }
    }
}

/* Vertex type for textured quads */
typedef struct { float u, v; unsigned int color; float x, y, z; } TVert;
#define TVERT_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int *gu_list_ptr;

void render_init(void *fbp0, void *fbp1, void *zbp, unsigned int *gu_list) {
    gu_list_ptr = gu_list;
    light_bake();
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

/* OG Sprite_Character addressing (rpg_sprites.js): $ sheets hold one
 * character (3 cols × 4 rows); others hold 8 (12 cols × 8 rows, block from
 * characterIndex). pattern = sheet column (NPC pages state it directly),
 * dir_mv = RPG Maker direction (2,4,6,8) -> row 0-3. */
static void char_cell(int img_w, int img_h, int is_big, int index, int pattern,
                      int dir_mv, int *fx, int *fy, int *cw, int *ch) {
    int row = (dir_mv - 2) / 2;
    if (row < 0) row = 0;
    if (row > 3) row = 3;
    if (pattern < 0) pattern = 0;
    if (pattern > 2) pattern = 2;
    if (is_big) {
        *cw = img_w / 3; *ch = img_h / 4;
        *fx = pattern * (*cw); *fy = row * (*ch);
    } else {
        *cw = img_w / 12; *ch = img_h / 8;
        *fx = ((index % 4) * 3 + pattern) * (*cw);
        *fy = ((index / 4) * 4 + row) * (*ch);
    }
}

/* Draw one character cell. Same texture state as the map path (proven):
 * CLUT first, then TexMode/TexImage, alpha-test + blend. */
static void render_character_cell(unsigned char *sprite_data,
                                  unsigned int *clut_data,
                                  int tex_w, int tex_h, int stride,
                                  int frame_x, int frame_y,
                                  int frame_w, int frame_h,
                                  int screen_x, int screen_y) {
    if (!sprite_data || !clut_data) return;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, clut_data);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);  /* T8, swizzled=1 */
    sceGuTexImage(0, tex_w, tex_h, stride, sprite_data);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    /* Draw single frame using GU_SPRITES */
    TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    v[0].u = (float)frame_x; v[0].v = (float)frame_y;
    v[0].color = 0xffffffff;
    v[0].x = (float)screen_x; v[0].y = (float)screen_y; v[0].z = 0.0f;
    v[1].u = (float)(frame_x + frame_w); v[1].v = (float)(frame_y + frame_h);
    v[1].color = 0xffffffff;
    v[1].x = (float)(screen_x + frame_w);
    v[1].y = (float)(screen_y + frame_h); v[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data) {
    if (!sprite_data || !clut_data) return;
    
    /* Player sheets: non-$ 480×440, walking = characterIndex 0 block.
     * player->step_frame 0=center(idle),1=left,2=right -> sheet col 1,0,2;
     * player->dir 0-3 -> row 0-3. */
    int pat_col = (player->step_frame == 0) ? 1
                : (player->step_frame == 1) ? 0 : 2;
    int dir = player->dir;
    if (dir < 0) dir = 0;
    if (dir > 3) dir = 3;
    int block = player->sprite_pattern;
    if (block < 0) block = 0;
    if (block > 7) block = 7;

    int fx, fy, fw, fh;
    char_cell(480, 440, 0, block, pat_col, dir * 2 + 2,
              &fx, &fy, &fw, &fh);

    /* Bottom-align the cell on the 24px tile */
    int screen_x = player->x - cam_x - (fw - TILE) / 2;
    int screen_y = player->y - cam_y - (fh - TILE);

    render_character_cell(sprite_data, clut_data, 512, 512, 512,
                          fx, fy, fw, fh, screen_x, screen_y);
}

/* Lighting composite: ONE fullscreen sprite sampling the baked radial
 * mask, recentered on the player every frame. out = dst × mask.
 * torch_on restores the Fire flicker (radius ±7, the Terrax default);
 * the plain player globe stays steady (playerflicker = false). */
static void render_light_pass(const Player *player, int cam_x, int cam_y,
                              int frames, int torch_on) {
    int px = player->x - cam_x + TILE / 2;
    int py = player->y - cam_y - 8;
    float r = (float)LIGHT_R;
    if (torch_on)
        r += (float)(((frames * 13) % 15) - 7);
    /* Texture px per screen px: texture radius (128) covers world r. */
    float k = ((float)LIGHT_TEX / 2.0f) / r;
    float u0 = (float)LIGHT_TEX / 2.0f - (float)px * k;
    float u1 = u0 + (float)SCR_W * k;
    float v0 = (float)LIGHT_TEX / 2.0f - (float)py * k;
    float v1 = v0 + (float)SCR_H * k;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, light_cl);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, LIGHT_TEX, LIGHT_TEX, LIGHT_TEX, light_px);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    v[0].u = u0; v[0].v = v0; v[0].color = 0xff000000;
    v[0].x = 0; v[0].y = 0; v[0].z = 0.0f;
    v[1].u = u1; v[1].v = v1; v[1].color = 0xff000000;
    v[1].x = (float)SCR_W; v[1].y = (float)SCR_H; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

/* Draw one NPC's paged cell (pattern/dir taken straight from the page). */
static void render_npc_sprite(const NpcSprite *npc, int cam_x, int cam_y) {
    int fx, fy, fw, fh;
    char_cell(npc->img_w, npc->img_h, npc->is_big, npc->char_index,
              npc->pattern, npc->dir_mv, &fx, &fy, &fw, &fh);
    int sx = npc->tile_x * TILE - cam_x - (fw - TILE) / 2;
    int sy = npc->tile_y * TILE - cam_y - (fh - TILE);
    render_character_cell(npc->t8, npc->clut, npc->tex_w, npc->tex_h,
                          npc->stride, fx, fy, fw, fh, sx, sy);
}

/* Same-priority characters Y-sort by feet (screen bottom of sprite).
 * Feet sit one tile below the entity origin in screen space. */
static int char_feet_y(int ent_y_px, int cam_y) {
    return ent_y_px - cam_y + TILE;
}

void render_frame(int cam_x, int cam_y,
                  const uint16_t *map_layers, int map_w, int map_h,
                  const Player *player, int current_char,
                  unsigned char *char_sprites[4],
                  unsigned int *char_cluts[4],
                  const uint8_t *higher, int higher_len, int frames,
                  const NpcSprite *npcs, int n_npcs, int torch_on) {
    sceGuStart(GU_DIRECT, gu_list_ptr);
    sceGuClearColor(0xff000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    
    /* OG order: z=0 tiles, below-chars (z=1, priority 0 like corpses),
     * same-priority characters Y-sorted (z=3), z=4 higher ★ tiles
     * (rpg_core.js Tilemap z=0/4, screenZ = priorityType*2+1). */
    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 0);

    /* Below-level NPCs first (all under every same-level character). */
    if (npcs) {
        for (int i = 0; i < n_npcs; i++) {
            if (npcs[i].prio != 0) continue;
            int sx = npcs[i].tile_x * TILE - cam_x;
            int sy = npcs[i].tile_y * TILE - cam_y;
            if (sx < -64 || sx > SCR_W + 64 || sy < -96 || sy > SCR_H + 64)
                continue;
            render_npc_sprite(&npcs[i], cam_x, cam_y);
        }
    }

    /* Collect visible characters (player + on-screen NPCs), sort by feet
     * Y so lower on screen draws later (in front). Upper tiles (z=4)
     * still cover every same-priority character. */
    static int order[1 + 32];
    static int feet[1 + 32];
    int n = 0;
    order[n] = -1;  /* -1 = player */
    feet[n] = char_feet_y(player->y, cam_y);
    n++;
    if (npcs) {
        for (int i = 0; i < n_npcs && n < 33; i++) {
            if (npcs[i].prio != 1) continue;  /* prio 0 drawn earlier */
            int sx = npcs[i].tile_x * TILE - cam_x;
            int sy = npcs[i].tile_y * TILE - cam_y;
            if (sx < -64 || sx > SCR_W + 64 || sy < -96 || sy > SCR_H + 64)
                continue;  /* culled */
            order[n] = i;
            feet[n] = char_feet_y(npcs[i].tile_y * TILE, cam_y);
            n++;
        }
    }
    /* Insertion sort by feet Y (n is tiny). */
    for (int i = 1; i < n; i++) {
        int o = order[i], f = feet[i], j = i - 1;
        while (j >= 0 && feet[j] > f) {
            order[j + 1] = order[j];
            feet[j + 1] = feet[j];
            j--;
        }
        order[j + 1] = o;
        feet[j + 1] = f;
    }
    for (int i = 0; i < n; i++) {
        if (order[i] < 0)
            render_player_sprite(player, cam_x, cam_y,
                                char_sprites[current_char],
                                char_cluts[current_char]);
        else
            render_npc_sprite(&npcs[order[i]], cam_x, cam_y);
    }

    /* Higher tiles draw over the player (canopies, rafters, tall walls) */
    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 1);

    /* Darkness + player glow. */
    render_light_pass(player, cam_x, cam_y, frames, torch_on);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

/* ---- Message window ---- */
#define FONT_ADV 7
#define FONT_ADV 7
#define FONT_LINE 18
#define MSG_COLS 52  /* ~50 chars/row like the OG at this scale */
#define MSG_ROWS 7

static char msg_rows[MSG_ROWS][MSG_COLS + 1];
static unsigned char msg_cols[MSG_ROWS][MSG_COLS];
static int msg_nrows, msg_cx, msg_ccol;

static void msg_newrow(void) {
    if (msg_nrows < MSG_ROWS) {
        msg_cx = 0;
        msg_ccol = 0;
        msg_rows[msg_nrows][0] = 0;
        msg_nrows++;
    }
}

/* UTF-8 -> Latin-1 codepoint (atlas is Latin-1 1:1); others become '?'. */
static void msg_put(unsigned int cp) {
    if (cp >= 256) cp = '?';
    if (cp < 32) cp = '?';
    if (msg_nrows == 0) msg_newrow();
    if (msg_cx >= MSG_COLS) msg_newrow();
    if (msg_nrows > MSG_ROWS) return;
    int r = msg_nrows - 1;
    if (msg_cx < MSG_COLS) {
        msg_rows[r][msg_cx] = (char)cp;
        msg_cols[r][msg_cx] = (unsigned char)msg_ccol;
        msg_cx++;
        msg_rows[r][msg_cx] = 0;
    }
}

static void msg_text_cb(const char *ptr, int len, void *ud) {
    (void)ud;
    int i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)ptr[i];
        if (c == '\n') {
            msg_newrow();
            i++;
        } else if ((c & 0x80) == 0) {
            msg_put(c);
            i++;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < len) {
            /* 2-byte UTF-8 -> Latin-1 when it fits (é in François). */
            unsigned int cp = ((unsigned int)(c & 0x1f) << 6) |
                              ((unsigned int)ptr[i + 1] & 0x3f);
            msg_put(cp);
            i += 2;
        } else {
            msg_put('?');
            i++;
        }
    }
}

static void msg_code_cb(const char *code, int param, void *ud) {
    (void)ud;
    if (code[0] == 'C' && code[1] == 0 && param >= 0 && param < 32)
        msg_ccol = param;
}

void render_message_window(const FhInterp *mit, int msg_ended, int cursor) {
    /* 1. Layout the decoded text into colored rows. */
    msg_nrows = 0;
    msg_cx = 0;
    msg_ccol = 0;
    msg_newrow();
    {
        FhEscCtx ctx = {NULL, 0, NULL, 0, NULL, 0, NULL};
        FhEscCb cb;
        cb.on_text = &msg_text_cb;
        cb.on_code = &msg_code_cb;
        cb.ud = NULL;
        fh_decode_escapes(mit->text, &ctx, &cb);
    }
    /* 2. Append the choice list. Each option is decoded like body text
     * (options can carry raw escapes, e.g. \c[2]Torch); the selected row
     * prints bright, others white, with an orange marker. */
    if (mit->await_choice && mit->choice_text) {
        FhEscCtx cctx = {NULL, 0, NULL, 0, NULL, 0, NULL};
        FhEscCb ccb;
        ccb.on_text = &msg_text_cb;
        ccb.on_code = &msg_code_cb;
        ccb.ud = NULL;
        const char *p = mit->choice_text;
        int idx = 0;
        while (*p && msg_nrows < MSG_ROWS && idx < 8) {
            const char *nl = strchr(p, '\n');
            int n = nl ? (int)(nl - p) : (int)strlen(p);
            /* Decode escapes into a temp run first (color spans), then
             * re-emit as one row with marker. Simpler: marker row first. */
            msg_newrow();
            if (msg_nrows > MSG_ROWS) break;
            int r = msg_nrows - 1;
            msg_rows[r][0] = '>';
            msg_cols[r][0] = 2;
            msg_rows[r][1] = ' ';
            msg_cols[r][1] = 0;
            msg_cx = 2;
            msg_ccol = (idx == cursor) ? 6 : 0;
            char opt[128];
            if (n > 127) n = 127;
            memcpy(opt, p, (size_t)n);
            opt[n] = 0;
            fh_decode_escapes(opt, &cctx, &ccb);
            idx++;
            if (!nl) break;
            p = nl + 1;
        }
    }
    if (msg_ended && msg_nrows < MSG_ROWS) {
        msg_newrow();
        if (msg_nrows <= MSG_ROWS) {
            int r = msg_nrows - 1;
            const char *e = "-- END (O: continue) --";
            int k = 0;
            while (*e && k < MSG_COLS) {
                msg_rows[r][k] = *e;
                msg_cols[r][k] = 7;
                k++;
                e++;
            }
            msg_rows[r][k] = 0;
        }
    }

    /* 3. Window box (fill + 2px border), sized to content. */
    int rows = msg_nrows;
    if (rows < 1) rows = 1;
    if (rows > MSG_ROWS) rows = MSG_ROWS;
    int box_h = rows * FONT_LINE + 20;
    int x0 = 8, x1 = SCR_W - 8, y1 = SCR_H - 8, y0 = y1 - box_h;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    TVert *b = (TVert *)sceGuGetMemory(4 * sizeof(TVert));
    b[0].u = 0; b[0].v = 0; b[0].color = 0xff8c7d76;  /* warm gray border */
    b[0].x = (float)x0; b[0].y = (float)y0; b[0].z = 0.0f;
    b[1].u = 0; b[1].v = 0; b[1].color = 0xff8c7d76;
    b[1].x = (float)x1; b[1].y = (float)y1; b[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, b);
    b = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    b[0].u = 0; b[0].v = 0; b[0].color = 0xdc2a1c1a;  /* F&H skin fill */
    b[0].x = (float)(x0 + 2); b[0].y = (float)(y0 + 2); b[0].z = 0.0f;
    b[1].u = 0; b[1].v = 0; b[1].color = 0xdc2a1c1a;
    b[1].x = (float)(x1 - 2); b[1].y = (float)(y1 - 2); b[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, b);

    /* 4. Glyphs, one batched draw, vertex colors carry \C spans. */
    if (!font_px || !font_cl) {
        sceGuDisable(GU_BLEND);
        return;
    }
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, font_cl);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, 256, 256, 256, font_px);
    sceGuTexFlush();
    sceGuTexSync();
    int total = 0;
    for (int r = 0; r < rows; r++)
        total += (int)strlen(msg_rows[r]);
    TVert *v = (TVert *)sceGuGetMemory(total * 2 * sizeof(TVert));
    TVert *vp = v;
    for (int r = 0; r < rows; r++) {
        int gy = y0 + 10 + r * FONT_LINE;
        for (int k = 0; msg_rows[r][k]; k++) {
            unsigned int cp = (unsigned char)msg_rows[r][k];
            float u0 = (float)((cp % 16) * 16);
            float v0 = (float)((cp / 16) * 16);
            unsigned int col = MSG_PAL[msg_cols[r][k] & 31];
            float gx = (float)(x0 + 12 + k * FONT_ADV);
            vp[0].u = u0; vp[0].v = v0; vp[0].color = col;
            vp[0].x = gx; vp[0].y = (float)gy; vp[0].z = 0.0f;
            vp[1].u = u0 + 16; vp[1].v = v0 + 16; vp[1].color = col;
            vp[1].x = gx + 16; vp[1].y = (float)(gy + 16); vp[1].z = 0.0f;
            vp += 2;
        }
    }
    if (total > 0)
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, total * 2, 0, v);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_debug_text(const char *text) {    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("%s", text);
}
