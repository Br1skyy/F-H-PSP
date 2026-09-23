
#include "render.h"
#include "map_runtime.h"
#include "anim_data.h"
#include <pspge.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

unsigned char *font_px = 0;
unsigned int font_cl[256] __attribute__((aligned(16)));
unsigned char *font_adv = 0;
unsigned char *window_px = 0;
unsigned int window_cl[256] __attribute__((aligned(16)));
unsigned char *floor_px = 0;
unsigned int floor_cl[256] __attribute__((aligned(16)));


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

#define NX (SCR_W / TILE + 2)
#define NY (SCR_H / TILE + 2)


const SheetDef SHEETS[9] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {256, 512, 256},
    {512, 512, 512},
    {512, 512, 512},
    {512, 512, 512},
    {512, 512, 512},
};

unsigned char *sheet_px[9] = {0};
unsigned int sheet_cl[9][256] __attribute__((aligned(16)));


#define LIGHT_R 150
#define LIGHT_CORE 10
#define LIGHT_TEX 256

static unsigned char light_px[LIGHT_TEX * LIGHT_TEX];
static unsigned int light_cl[256] __attribute__((aligned(16)));

static void light_bake(void) {

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
        light_cl[i] = ((unsigned int)i << 24);

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


typedef struct { float u, v; unsigned int color; float x, y, z; } TVert;
#define TVERT_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int *gu_list_ptr;
static int render_ticks;

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


    for (int layer = 0; layer < 4; layer++) {
        for (int s = 4; s < 9; s++) {

            sceGuClutLoad(32, sheet_cl[s]);
            sceGuTexMode(GU_PSM_T8, 0, 0, 1);
            sceGuTexImage(0, SHEETS[s].tw, SHEETS[s].th, SHEETS[s].stride, sheet_px[s]);
            sceGuTexFlush();
            sceGuTexSync();


            static FhDraw collected[NX * NY];
            int n = fh_collect(x0, y0, NX, NY, TILE, ox, oy, layer, s,
                             map_layers, map_w, map_h, collected, NX * NY);

            if (n == 0) continue;


            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GREATER, 0, 0xff);
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);


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


static void render_character_cell(unsigned char *sprite_data,
                                   unsigned int *clut_data,
                                   int tex_w, int tex_h, int stride,
                                   int frame_x, int frame_y,
                                   int frame_w, int frame_h,
                                   int screen_x, int screen_y,
                                   int dst_w, int dst_h, int linear) {
    if (!sprite_data || !clut_data) return;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    if (linear)
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    else
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, clut_data);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, tex_w, tex_h, stride, sprite_data);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);


    TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    v[0].u = (float)frame_x; v[0].v = (float)frame_y;
    v[0].color = 0xffffffff;
    v[0].x = (float)screen_x; v[0].y = (float)screen_y; v[0].z = 0.0f;
    v[1].u = (float)(frame_x + frame_w); v[1].v = (float)(frame_y + frame_h);
    v[1].color = 0xffffffff;
    v[1].x = (float)(screen_x + dst_w);
    v[1].y = (float)(screen_y + dst_h); v[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);

    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data) {
    if (!sprite_data || !clut_data) return;


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


    int screen_x = player->x - cam_x - (fw - TILE) / 2;
    int screen_y = player->y - cam_y - (fh - TILE);

    render_character_cell(sprite_data, clut_data, 512, 512, 512,
                          fx, fy, fw, fh, screen_x, screen_y, fw, fh, 0);
}


static void light_mask_blt(float px, float py, float r) {

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


static void render_light_pass(const Player *player, int cam_x, int cam_y,
                              int frames, int torch_on) {
    float px = (float)(player->x - cam_x) + (float)TILE / 2.0f;
    float py = (float)(player->y - cam_y) - 8.0f;
    float r = (float)LIGHT_R;
    if (torch_on)
        r += (float)(((frames * 13) % 15) - 7);
    light_mask_blt(px, py, r);
}


static void render_npc_sprite(const NpcSprite *npc, int cam_x, int cam_y) {
    int fx, fy, fw, fh;
    char_cell(npc->img_w, npc->img_h, npc->is_big, npc->char_index,
              npc->pattern, npc->dir_mv, &fx, &fy, &fw, &fh);
    int sx = npc->tile_x * TILE - cam_x - (fw - TILE) / 2;
    int sy = npc->tile_y * TILE - cam_y - (fh - TILE);
    render_character_cell(npc->t8, npc->clut, npc->tex_w, npc->tex_h,
                          npc->stride, fx, fy, fw, fh, sx, sy, fw, fh, 0);
}


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


    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 0);


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


    static int order[1 + 32];
    static int feet[1 + 32];
    int n = 0;
    order[n] = -1;
    feet[n] = char_feet_y(player->y, cam_y);
    n++;
    if (npcs) {
        for (int i = 0; i < n_npcs && n < 33; i++) {
            if (npcs[i].prio != 1) continue;
            int sx = npcs[i].tile_x * TILE - cam_x;
            int sy = npcs[i].tile_y * TILE - cam_y;
            if (sx < -64 || sx > SCR_W + 64 || sy < -96 || sy > SCR_H + 64)
                continue;
            order[n] = i;
            feet[n] = char_feet_y(npcs[i].tile_y * TILE, cam_y);
            n++;
        }
    }

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


    render_map_layers(cam_x, cam_y, map_layers, map_w, map_h,
                      higher, higher_len, 1);


    render_light_pass(player, cam_x, cam_y, frames, torch_on);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}


static void skin_box(int x0, int y0, int x1, int y1, unsigned int fill,
                     int frame) {
    sceGuDisable(GU_TEXTURE_2D);
    if (fill >> 24) {
        TVert *b = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
        b[0].u = 0; b[0].v = 0; b[0].color = fill;
        b[0].x = (float)x0; b[0].y = (float)y0; b[0].z = 0.0f;
        b[1].u = 0; b[1].v = 0; b[1].color = fill;
        b[1].x = (float)x1; b[1].y = (float)y1; b[1].z = 0.0f;
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, b);
    }
    if (!frame || !window_px) return;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, window_cl);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, 256, 256, 256, window_px);
    sceGuTexFlush();
    sceGuTexSync();
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    TVert *f = (TVert *)sceGuGetMemory(8 * 2 * sizeof(TVert));
    TVert *fp = f;
    int m = 12;
    static const int parts[8][8] = {
        {96, 0, 24, 24, 0, 0, 0, 0},
        {168, 0, 24, 24, 0, 0, 0, 0},
        {96, 72, 24, 24, 0, 0, 0, 0},
        {168, 72, 24, 24, 0, 0, 0, 0},
        {120, 0, 48, 24, 0, 0, 0, 0},
        {120, 72, 48, 24, 0, 0, 0, 0},
        {96, 24, 24, 48, 0, 0, 0, 0},
        {168, 24, 24, 48, 0, 0, 0, 0},
    };
    int dx[8], dy[8], dw[8], dh[8];
    dx[0] = x0; dy[0] = y0; dw[0] = m; dh[0] = m;
    dx[1] = x1 - m; dy[1] = y0; dw[1] = m; dh[1] = m;
    dx[2] = x0; dy[2] = y1 - m; dw[2] = m; dh[2] = m;
    dx[3] = x1 - m; dy[3] = y1 - m; dw[3] = m; dh[3] = m;
    dx[4] = x0 + m; dy[4] = y0; dw[4] = (x1 - x0) - 2 * m; dh[4] = m;
    dx[5] = x0 + m; dy[5] = y1 - m; dw[5] = (x1 - x0) - 2 * m; dh[5] = m;
    dx[6] = x0; dy[6] = y0 + m; dw[6] = m; dh[6] = (y1 - y0) - 2 * m;
    dx[7] = x1 - m; dy[7] = y0 + m; dw[7] = m; dh[7] = (y1 - y0) - 2 * m;
    for (int q = 0; q < 8; q++) {
        fp[0].u = (float)parts[q][0]; fp[0].v = (float)parts[q][1];
        fp[0].color = 0xffffffff;
        fp[0].x = (float)dx[q]; fp[0].y = (float)dy[q]; fp[0].z = 0.0f;
        fp[1].u = (float)(parts[q][0] + parts[q][2]);
        fp[1].v = (float)(parts[q][1] + parts[q][3]);
        fp[1].color = 0xffffffff;
        fp[1].x = (float)(dx[q] + dw[q]);
        fp[1].y = (float)(dy[q] + dh[q]); fp[1].z = 0.0f;
        fp += 2;
    }
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 8 * 2, 0, f);
    sceGuDisable(GU_TEXTURE_2D);
}


#define FONT_LINE 26
#define MSG_COLS 96
#define MSG_ROWS 7
#define MSG_TEXT_W 440.0f

static char msg_rows[MSG_ROWS][MSG_COLS + 1];
static unsigned char msg_cols[MSG_ROWS][MSG_COLS];
static float msg_row_w[MSG_ROWS];
static int msg_nrows, msg_cx, msg_ccol;
static float msg_px;
static int msg_reveal, msg_body_total;
static char msg_name[32];
static int msg_expect_name = 0;

static void msg_newrow(void) {
    if (msg_nrows < MSG_ROWS) {
        msg_cx = 0;
        msg_ccol = 0;
        msg_px = 0.0f;
        msg_row_w[msg_nrows] = 0.0f;
        msg_rows[msg_nrows][0] = 0;
        msg_nrows++;
    }
}


static int msg_body_rows = 0;

int msg_text_revealed(void) {
    return msg_reveal >= msg_body_total;
}

void msg_reveal_all(void) {
    msg_reveal = msg_body_total;
}


void msg_content_changed(void) {
    msg_reveal = 0;
    msg_body_total = 0;
}

static float msg_adv(unsigned int cp) {
    if (!font_adv || cp >= 256) return 4.0f;
    return (float)font_adv[cp] / 2.0f;
}


static void msg_put(unsigned int cp) {
    if (cp >= 256 || cp < 32 || (cp >= 127 && cp < 160)) cp = '?';
    if (msg_nrows == 0) msg_newrow();
    if (msg_nrows > MSG_ROWS) return;
    int r = msg_nrows - 1;

    if (msg_cx < MSG_COLS) {
        msg_rows[r][msg_cx] = (char)cp;
        msg_cols[r][msg_cx] = (unsigned char)msg_ccol;
        msg_cx++;
        msg_rows[r][msg_cx] = 0;
        msg_px += msg_adv(cp);
        msg_row_w[r] = msg_px;
    }
}


static void msg_emit_word(const unsigned int *w, int n) {
    float ww = 0.0f;
    for (int k = 0; k < n; k++) {
        unsigned int cp = w[k];
        if (cp >= 256 || cp < 32 || (cp >= 127 && cp < 160)) cp = '?';
        ww += msg_adv(cp);
    }
    if (ww > MSG_TEXT_W) {

        msg_newrow();
        for (int k = 0; k < n; k++) {
            unsigned int cp = w[k];
            if (cp >= 256 || cp < 32 || (cp >= 127 && cp < 160)) cp = '?';
            if (msg_px > 0.0f && msg_px + msg_adv(cp) > MSG_TEXT_W)
                msg_newrow();
            msg_put(w[k]);
        }
        return;
    }
    if (msg_px > 0.0f && msg_px + ww > MSG_TEXT_W) msg_newrow();
    for (int k = 0; k < n; k++) msg_put(w[k]);
}

static void msg_text_cb(const char *ptr, int len, void *ud) {
    (void)ud;

    static unsigned int tmp[256];
    int n = 0, i = 0;

    if (msg_expect_name && len > 0 && ptr[0] == '<') {
        int k = 1, o = 0;
        while (k < len && ptr[k] != '>' && o < 31) {
            msg_name[o++] = ptr[k++];
        }
        msg_name[o] = 0;
        msg_expect_name = 0;
        if (k < len && ptr[k] == '>') k++;
        i = k;
        if (i >= len) return;
    }
    msg_expect_name = 0;
    while (i < len && n < 255) {
        unsigned char c = (unsigned char)ptr[i];
        if (c == '\n') {
            msg_newrow();
            i++;
        } else if ((c & 0x80) == 0) {
            tmp[n++] = c;
            i++;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < len) {
            tmp[n++] = ((unsigned int)(c & 0x1f) << 6) |
                       ((unsigned int)ptr[i + 1] & 0x3f);
            i += 2;
        } else {
            tmp[n++] = '?';
            i++;
        }
    }

    int k = 0;
    while (k < n) {
        while (k < n && tmp[k] == ' ') {
            if (msg_px > 0.0f) msg_put(' ');
            k++;
        }
        int w0 = k;
        while (k < n && tmp[k] != ' ') k++;
        if (k > w0) msg_emit_word(tmp + w0, k - w0);
    }
}

static void msg_code_cb(const char *code, int param, void *ud) {
    (void)ud;
    if (code[0] == 'C' && code[1] == 0 && param >= 0 && param < 32) {
        msg_ccol = param;
        return;
    }


    if (code[0] == 'N' && code[1] == 0 && param < 0)
        msg_expect_name = 1;
}

void render_message_window(const FhInterp *mit, int msg_ended, int cursor,
                           int page_wait) {

    msg_nrows = 0;
    msg_cx = 0;
    msg_ccol = 0;
    msg_name[0] = 0;
    msg_newrow();
    {
        FhEscCtx ctx = {NULL, 0, mit->actor_names, mit->nactors, NULL, 0,
                        NULL};
        FhEscCb cb;
        cb.on_text = &msg_text_cb;
        cb.on_code = &msg_code_cb;
        cb.ud = NULL;
        fh_decode_escapes(mit->text, &ctx, &cb);
    }

    msg_body_rows = msg_nrows;
    msg_body_total = 0;
    for (int r = 0; r < msg_body_rows && r < MSG_ROWS; r++)
        msg_body_total += (int)strlen(msg_rows[r]);
    if (msg_reveal > msg_body_total) msg_reveal = msg_body_total;


    if (mit->await_choice && mit->choice_text) {
        FhEscCtx cctx = {NULL, 0, mit->actor_names, mit->nactors, NULL, 0,
                         NULL};
        FhEscCb ccb;
        ccb.on_text = &msg_text_cb;
        ccb.on_code = &msg_code_cb;
        ccb.ud = NULL;
        const char *p = mit->choice_text;
        int idx = 0;
        while (*p && msg_nrows < MSG_ROWS && idx < 8) {
            const char *nl = strchr(p, '\n');
            int n = nl ? (int)(nl - p) : (int)strlen(p);


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


    if (!msg_text_revealed()) msg_reveal += 2;
    int rows = msg_nrows;
    if (rows < 1) rows = 1;
    if (rows > MSG_ROWS) rows = MSG_ROWS;
    int box_h = rows * FONT_LINE + 20;
    int bg = mit->msg_bg;
    if (bg < 0 || bg > 2) bg = 0;
    int pos = mit->msg_pos;
    int x0 = 8, x1 = SCR_W - 8, y0, y1;
    if (pos == 0) {
        y0 = 8;
        y1 = y0 + box_h;
    } else if (pos == 1) {
        y0 = (SCR_H - box_h) / 2;
        y1 = y0 + box_h;
    } else {
        y1 = SCR_H - 8;
        y0 = y1 - box_h;
    }
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    if (bg != 2)
        skin_box(x0, y0, x1, y1, (bg == 1) ? 0xa0000000 : 0xc8343c42,
                 bg == 0);


    if (bg == 0 && msg_name[0]) {
        float nw = 20.0f;
        for (int k = 0; msg_name[k]; k++)
            nw += msg_adv((unsigned char)msg_name[k]);
        int nx0 = x0, nx1 = x0 + (int)nw, ny1 = y0 + 6, ny0 = ny1 - 32;
        skin_box(nx0, ny0, nx1, ny1, 0xc8343c42, 1);
    }


    if (!font_px) {
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
    sceGuTexImage(0, 512, 512, 512, font_px);
    sceGuTexFlush();
    sceGuTexSync();
    int total = 0;
    for (int r = 0; r < rows; r++)
        total += (int)strlen(msg_rows[r]);

    TVert *v = (TVert *)sceGuGetMemory((total + 40) * 2 * sizeof(TVert));
    TVert *vp = v;


    int shown = 0;
    for (int r = 0; r < rows; r++) {
        int gy = y0 + 10 + r * FONT_LINE;
        float gx = (float)(x0 + 12);
        int is_body = (r < msg_body_rows);
        for (int k = 0; msg_rows[r][k]; k++) {
            if (is_body && shown >= msg_reveal) break;
            unsigned int cp = (unsigned char)msg_rows[r][k];
            float u0 = (float)((cp % 32) * 16);
            float v0 = (float)((cp / 32) * 32);
            unsigned int col = MSG_PAL[msg_cols[r][k] & 31];
            vp[0].u = u0; vp[0].v = v0; vp[0].color = col;
            vp[0].x = gx; vp[0].y = (float)gy; vp[0].z = 0.0f;
            vp[1].u = u0 + 16; vp[1].v = v0 + 32; vp[1].color = col;
            vp[1].x = gx + 16; vp[1].y = (float)(gy + 32); vp[1].z = 0.0f;
            vp += 2;
            gx += msg_adv(cp);
            if (is_body) shown++;
        }
    }

    if (msg_name[0] && msg_text_revealed()) {
        float nx = (float)(x0 + 10);
        int ny = y0 + 6 - 32 + 8;
        for (int k = 0; msg_name[k]; k++) {
            unsigned int cp = (unsigned char)msg_name[k];
            float u0 = (float)((cp % 32) * 16);
            float v0 = (float)((cp / 32) * 32);
            vp[0].u = u0; vp[0].v = v0; vp[0].color = MSG_PAL[6];
            vp[0].x = nx; vp[0].y = (float)ny; vp[0].z = 0.0f;
            vp[1].u = u0 + 16; vp[1].v = v0 + 32; vp[1].color = MSG_PAL[6];
            vp[1].x = nx + 16; vp[1].y = (float)(ny + 32); vp[1].z = 0.0f;
            vp += 2;
            nx += msg_adv(cp);
        }
    }
    if (vp > v)
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, (int)(vp - v), 0, v);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
}

void render_debug_text(const char *text) {
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("%s", text);
}


#define BT_ANIM_MAX 4
#define ANIM_CELL 96
#define ANIM_RATE 4

typedef struct {
    int active, row;
    int foe_idx, mirror;
    int duration;
    int frame;
} BtAnimLive;

static BtAnimLive bt_anims[BT_ANIM_MAX];
static int anim_flash_ticks;
static float anim_flash_a;
static int anim_flash_rgb;

static int anim_row_of(int id) {
    for (int r = 0; ANIM_IDX[r].id; r++)
        if (ANIM_IDX[r].id == id) return r;
    return -1;
}

static int anim_sheet_idx(const char *name) {
    static const char *names[8] = {
        "coin_flip", "pinecone_pig", "bloodsplurt",
        "blood_shot", "bugs1", "bugs2",
        "slash1", "needle_worm",
    };
    if (!name) return -1;
    for (int i = 0; i < 8; i++)
        if (!strcmp(name, names[i])) return i;
    return -1;
}


void battle_anim_reset(void) {
    for (int i = 0; i < BT_ANIM_MAX; i++) bt_anims[i].active = 0;
    anim_flash_ticks = 0;
    anim_flash_a = 0.0f;
}

void battle_anim_start(int anim_id, int foe_idx, int mirror) {    int r = anim_row_of(anim_id);
    if (r < 0) return;
    for (int i = 0; i < BT_ANIM_MAX; i++) {
        if (bt_anims[i].active) continue;
        bt_anims[i].active = 1;
        bt_anims[i].row = r;
        bt_anims[i].foe_idx = foe_idx;
        bt_anims[i].mirror = mirror ? 1 : 0;
        bt_anims[i].duration = ANIM_IDX[r].nframes * ANIM_RATE + 1;
        bt_anims[i].frame = -1;
        return;
    }

}

void battle_anim_tick(void) {
    for (int i = 0; i < BT_ANIM_MAX; i++) {
        if (!bt_anims[i].active) continue;
        if (bt_anims[i].duration <= 0) {
            bt_anims[i].active = 0;
            continue;
        }
        bt_anims[i].duration--;
        if (bt_anims[i].duration <= 0) {
            bt_anims[i].active = 0;
            continue;
        }

        if (bt_anims[i].duration % ANIM_RATE == 0) {
            int nf = ANIM_IDX[bt_anims[i].row].nframes;
            int fi = nf - (bt_anims[i].duration + ANIM_RATE - 1) / ANIM_RATE;
            if (fi >= 0 && fi < nf) {
                bt_anims[i].frame = fi;
                const FhAnimTiming *tm = ANIM_IDX[bt_anims[i].row].tim;
                for (int k = 0; k < ANIM_IDX[bt_anims[i].row].ntim; k++) {
                    if (tm[k].frame != fi || tm[k].scope != 2) continue;

                    anim_flash_rgb = (tm[k].col[0] & 255) |
                                     ((tm[k].col[1] & 255) << 8) |
                                     ((tm[k].col[2] & 255) << 16);
                    anim_flash_a = (float)(tm[k].col[3] & 255);
                    anim_flash_ticks = tm[k].dur * ANIM_RATE;
                }
            }
        }
    }

    if (anim_flash_ticks > 0) {
        float d = (float)anim_flash_ticks;
        anim_flash_ticks--;
        if (d >= 1.0f) anim_flash_a *= (d - 1.0f) / d;
        if (anim_flash_ticks <= 0) anim_flash_a = 0.0f;
    }
}


static void anim_cell_quad(float cx, float cy, float hx, float hy, float rot,
                           float u0, float v0, unsigned int color) {
    float c = cosf(rot), s = sinf(rot);
    TVert *v = (TVert *)sceGuGetMemory(4 * sizeof(TVert));
    static const float px[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    static const float py[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    static const float pu[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    static const float pv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    for (int k = 0; k < 4; k++) {
        float dx = px[k] * hx, dy = py[k] * hy;
        v[k].u = u0 + pu[k] * (float)ANIM_CELL;
        v[k].v = v0 + pv[k] * (float)ANIM_CELL;
        v[k].color = color;
        v[k].x = cx + dx * c - dy * s;
        v[k].y = cy + dx * s + dy * c;
        v[k].z = 0.0f;
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, TVERT_FMT, 4, 0, v);
}

static const int anim_tex_tw[8] = {512, 512, 512, 512, 512, 512, 512, 512};
static const int anim_tex_th[8] = {512, 512, 512, 512, 256, 256, 256, 512};

void render_battle_anims(const BtFoeDraw *foes, int nfoes) {
    int any = 0;
    for (int i = 0; i < BT_ANIM_MAX; i++)
        if (bt_anims[i].active) { any = 1; break; }
    if (!any && anim_flash_ticks <= 0) return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_BLEND);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);

    for (int i = 0; i < BT_ANIM_MAX; i++) {
        if (!bt_anims[i].active || bt_anims[i].frame < 0) continue;
        int fi = bt_anims[i].frame;
        int nf = ANIM_IDX[bt_anims[i].row].nframes;
        if (fi < 0 || fi >= nf) continue;


        float ox = (float)SCR_W / 2.0f, oy = (float)SCR_H / 2.0f;
        int pos = ANIM_IDX[bt_anims[i].row].pos;
        if (pos != 3) {
            int f = bt_anims[i].foe_idx;
            if (f == -2) {
                ox = 156.0f;
                oy = 158.0f;
            } else {
                if (f < 0 || f >= nfoes) continue;
                ox = foes[f].x;
                oy = foes[f].y;
                if (pos == 0) oy -= (float)foes[f].h;
                else if (pos == 1) oy -= (float)foes[f].h / 2.0f;
            }
        }
        int mirror = bt_anims[i].mirror;
        int c0 = ANIM_IDX[bt_anims[i].row].foff[fi];
        int c1 = ANIM_IDX[bt_anims[i].row].foff[fi + 1];
        for (int c = c0; c < c1; c++) {
            const FhAnimCell *cell = &ANIM_IDX[bt_anims[i].row].cells[c];
            if (cell->pat < 0) continue;
            int si = (cell->pat < 100)
                         ? anim_sheet_idx(ANIM_IDX[bt_anims[i].row].sheet1)
                         : anim_sheet_idx(ANIM_IDX[bt_anims[i].row].sheet2);
            if (si < 0 || !anim_t8[si]) continue;
            int col = cell->pat % 5, rr = (cell->pat % 100) / 5;
            float u0 = (float)(col * ANIM_CELL), v0 = (float)(rr * ANIM_CELL);
            if (u0 + ANIM_CELL > anim_tex_tw[si] ||
                v0 + ANIM_CELL > anim_tex_th[si])
                continue;
            sceGuClutLoad(32, anim_cl[si]);
            sceGuTexImage(0, anim_tex_tw[si], anim_tex_th[si],
                          anim_tex_tw[si], anim_t8[si]);
            sceGuTexFlush();
            sceGuTexSync();


            float cx = ox + (float)(mirror ? -cell->x : cell->x);
            float cy = oy + (float)cell->y;
            float sc = (float)cell->scale / 100.0f;
            float sx = sc * ((cell->mir || mirror) ? -1.0f : 1.0f);
            float rot = (float)(mirror ? -cell->rot : cell->rot) *
                        3.14159265f / 180.0f;
            unsigned int colr = (((unsigned int)(cell->opa & 255)) << 24) |
                                0x00ffffff;
            if (cell->blend == 1)
                sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00ffffff);
            else
                sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA,
                               GU_ONE_MINUS_SRC_ALPHA, 0, 0);
            anim_cell_quad(cx, cy, 48.0f * sx, 48.0f * sc, rot, u0, v0, colr);
        }
    }

    if (anim_flash_ticks > 0 && anim_flash_a > 0.5f) {
        sceGuDisable(GU_TEXTURE_2D);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
        unsigned int c = (((unsigned int)anim_flash_a) << 24) |
                         (unsigned int)anim_flash_rgb;
        v[0].u = 0; v[0].v = 0; v[0].color = c;
        v[0].x = 0; v[0].y = 0; v[0].z = 0.0f;
        v[1].u = 0; v[1].v = 0; v[1].color = c;
        v[1].x = (float)SCR_W; v[1].y = (float)SCR_H; v[1].z = 0.0f;
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);
        sceGuEnable(GU_TEXTURE_2D);
    }
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
}


static void battle_text(const char *s, float x, float y, unsigned int col,
                        TVert **vpp) {
    TVert *vp = *vpp;
    unsigned int sh = col & 0xff000000;
    float gx = x;
    for (; *s; s++) {
        unsigned int cp = (unsigned char)*s;
        if (cp < 32 || cp >= 256) cp = '?';
        float u0 = (float)((cp % 32) * 16);
        float v0 = (float)((cp / 32) * 32);
        vp[0].u = u0; vp[0].v = v0; vp[0].color = sh;
        vp[0].x = gx + 1.0f; vp[0].y = y + 1.0f; vp[0].z = 0.0f;
        vp[1].u = u0 + 16; vp[1].v = v0 + 32; vp[1].color = sh;
        vp[1].x = gx + 17.0f; vp[1].y = y + 33.0f; vp[1].z = 0.0f;
        vp[2].u = u0; vp[2].v = v0; vp[2].color = col;
        vp[2].x = gx; vp[2].y = y; vp[2].z = 0.0f;
        vp[3].u = u0 + 16; vp[3].v = v0 + 32; vp[3].color = col;
        vp[3].x = gx + 16; vp[3].y = y + 32; vp[3].z = 0.0f;
        vp += 4;
        gx += msg_adv(cp);
    }
    *vpp = vp;
}


static float battle_text_w(const char *s) {
    float w = 0.0f;
    for (; s && *s; s++) {
        unsigned int cp = (unsigned char)*s;
        if (cp < 32 || cp >= 256) cp = '?';
        w += msg_adv(cp);
    }
    return w;
}


static void bar_quad(TVert *q, float x0, float y0, float x1, float y1,
                     unsigned int c) {
    q[0].u = 0; q[0].v = 0; q[0].color = c;
    q[0].x = x0; q[0].y = y0; q[0].z = 0.0f;
    q[1].u = 0; q[1].v = 0; q[1].color = c;
    q[1].x = x1; q[1].y = y1; q[1].z = 0.0f;
}


static TVert *gauge_grad(TVert *bp, float x, float y, float fillw,
                         unsigned int c1, unsigned int c2) {
    int r1 = (int)(c1 & 0xff), g1 = (int)((c1 >> 8) & 0xff),
        b1 = (int)((c1 >> 16) & 0xff);
    int r2 = (int)(c2 & 0xff), g2 = (int)((c2 >> 8) & 0xff),
        b2 = (int)((c2 >> 16) & 0xff);
    float fw = (float)((int)fillw);
    if (fw < 0.0f) fw = 0.0f;
    for (int s = 0; s < 8; s++) {
        float sx0 = x + fw * (float)s / 8.0f;
        float sx1 = x + fw * (float)(s + 1) / 8.0f;
        float t = ((float)s + 0.5f) / 8.0f;
        int r = r1 + (int)((float)(r2 - r1) * t);
        int g = g1 + (int)((float)(g2 - g1) * t);
        int b = b1 + (int)((float)(b2 - b1) * t);
        unsigned int c =
            0xff000000 | ((unsigned int)b << 16) | ((unsigned int)g << 8) |
            (unsigned int)r;
        bar_quad(bp, sx0, y, sx1, y + 6.0f, c);
        bp += 2;
    }
    return bp;
}

void render_battle(const BtFoeDraw *foes, int nfoes,
                   const BtPopup *pops, int npops,
                   const char *actor_name, int hp, int mhp, int mp, int mmp,
                   const char *cmds[], int ncmds, int cursor, int show_cmds,
                   const char *targets[], int ntargets, int tcursor,
                   int show_targets, const char *log0, const char *log1,
                   unsigned char *actor_t8a, unsigned int *actor_cla,
                   unsigned char *actor_t8b, unsigned int *actor_clb,
                   int actor_ccol, int actor_crow, int actor_sx,
                   int actor_sy,
                   int sel_foe, const int *flash, const int *collapse,
                   const BtListRow *lrows, int nlrows, int lcursor,
                   int show_list, const int *st_icons, int nst_icons) {


    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    TVert *bd = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
    bd[0].u = 0; bd[0].v = 0; bd[0].color = 0xff180a0c;
    bd[0].x = 0; bd[0].y = 0; bd[0].z = 0.0f;
    bd[1].u = 0; bd[1].v = 0; bd[1].color = 0xff180a0c;
    bd[1].x = (float)SCR_W; bd[1].y = (float)SCR_H; bd[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, bd);
    if (floor_px) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        sceGuTexScale(1.0f, 1.0f);
        sceGuTexOffset(0.0f, 0.0f);
        sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
        sceGuClutLoad(32, floor_cl);
        sceGuTexMode(GU_PSM_T8, 0, 0, 1);
        sceGuTexImage(0, 512, 512, 512, floor_px);
        sceGuTexFlush();
        sceGuTexSync();
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(GU_GREATER, 0, 0xff);
        TVert *bb = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
        bb[0].u = 0; bb[0].v = 0; bb[0].color = 0xffffffff;
        bb[0].x = (float)((SCR_W - 500) / 2);
        bb[0].y = (float)((SCR_H - 370) / 2);
        bb[0].z = 0.0f;
        bb[1].u = 500; bb[1].v = 370; bb[1].color = 0xffffffff;
        bb[1].x = (float)((SCR_W + 500) / 2);
        bb[1].y = (float)((SCR_H + 370) / 2);
        bb[1].z = 0.0f;
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, bb);
        sceGuDisable(GU_TEXTURE_2D);
    }


    render_ticks++;
    int blink = ((render_ticks / 10) % 2) == 0;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    for (int i = 0; i < nfoes; i++) {
        int col = collapse ? collapse[i] : 0;
        if ((!foes[i].alive && col <= 0) || !foes[i].t8 || !foes[i].clut)
            continue;
        sceGuClutLoad(32, foes[i].clut);
        sceGuTexMode(GU_PSM_T8, 0, 0, 1);
        sceGuTexImage(0, foes[i].tw, foes[i].th, foes[i].stride, foes[i].t8);
        sceGuTexFlush();
        sceGuTexSync();
        unsigned int va = 0xffffffff;
        float sink = 0.0f;
        if (col > 0) {
            if (col > 30) col = 30;
            va = (((unsigned int)(255 * col / 30)) << 24) | 0x00ffffff;
            sink = (float)(30 - col);
        }
        TVert *v = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
        v[0].u = 0; v[0].v = 0; v[0].color = va;
        v[0].x = foes[i].x - (float)foes[i].w / 2.0f;
        v[0].y = foes[i].y - (float)foes[i].h + sink;
        v[0].z = 0.0f;
        v[1].u = (float)foes[i].w; v[1].v = (float)foes[i].h;
        v[1].color = va;
        v[1].x = foes[i].x + (float)foes[i].w / 2.0f;
        v[1].y = foes[i].y + sink;
        v[1].z = 0.0f;
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);

        int hot = (i == sel_foe && blink) ||
                  (flash && flash[i] > 0);
        if (hot) {
            unsigned int wa = (col > 0)
                                  ? (((unsigned int)(160 * col / 30)) << 24) |
                                        0x00ffffff
                                  : 0xa0ffffff;
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00ffffff);
            TVert *w = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
            w[0].u = 0; w[0].v = 0; w[0].color = wa;
            w[0].x = v[0].x; w[0].y = v[0].y; w[0].z = 0.0f;
            w[1].u = (float)foes[i].w; w[1].v = (float)foes[i].h;
            w[1].color = wa;
            w[1].x = v[1].x; w[1].y = v[1].y; w[1].z = 0.0f;
            sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, w);
            sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA,
                           0, 0);
        }
    }
    sceGuDisable(GU_TEXTURE_2D);


    {
        int cc = actor_ccol, cr = actor_crow;
        if (cc < 0) cc = 0;
        if (cc > 5) cc = 5;
        if (cr < 1) cr = 1;
        if (cr > 4) cr = 4;
        unsigned char *at8 = (cc < 3) ? actor_t8a : actor_t8b;
        unsigned int *acl = (cc < 3) ? actor_cla : actor_clb;
        if (at8 && acl) {
            render_character_cell(at8, acl, 512, 512, 512,
                                  (cc % 3) * 112, (cr - 1) * 112, 112, 112,
                                  actor_sx, actor_sy, 112, 112, 0);
        }
    }


    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    skin_box(150, 216, 472, 268, 0xc8343c42, 1);
    if (show_cmds || show_list) {
        int nn = show_cmds ? ncmds : nlrows;
        if (nn < 1) nn = 1;
        if (nn > 5) nn = 5;
        skin_box(8, 156, 142, 162 + (nn - 1) * 18 + 38, 0xc8343c42, 1);
    }
    int en_rows = 0;
    if (show_targets && ntargets > 0) {
        en_rows = ntargets > 7 ? 7 : ntargets;
        skin_box(296, 8, 472, 8 + en_rows * 22 + 28, 0xc8343c42, 1);
    }


    {
        int pulse = (render_ticks / 12) % 2;
        unsigned int hc = pulse ? 0xd0281c26 : 0xb0281c26;
        if (show_cmds && cursor >= 0 && cursor < ncmds)
            skin_box(10, 162 + cursor * 20, 140, 162 + cursor * 20 + 22,
                     hc, 0);
        if (show_list && lrows && lcursor >= 0 && lcursor < nlrows)
            skin_box(10, 162 + lcursor * 20, 140, 162 + lcursor * 20 + 22,
                     hc, 0);
        if (show_targets && tcursor >= 0 && tcursor < en_rows)
            skin_box(298, 20 + tcursor * 22, 470, 20 + tcursor * 22 + 24,
                     hc, 0);
    }


    int cmdtotal = (int)strlen(actor_name) + 24;
    for (int i = 0; i < ncmds; i++) cmdtotal += (int)strlen(cmds[i]) + 2;
    int targtotal = 0;
    for (int i = 0; i < ntargets; i++) targtotal += (int)strlen(targets[i]) + 2;
    int poptotal = npops * 8;
    int loglen = 0;
    if (log0) loglen += (int)strlen(log0);
    if (log1) loglen += (int)strlen(log1);
    int listtotal = 0;
    if (show_list && lrows) {
        for (int i = 0; i < nlrows; i++)
            listtotal += (int)strlen(lrows[i].text) + 2;
    }


    TVert *v = (TVert *)sceGuGetMemory(
        (cmdtotal + targtotal + poptotal + loglen + listtotal + 48) * 2 * 2 *
        sizeof(TVert));
    TVert *vp = v;


    light_mask_blt(200.0f, 160.0f, 220.0f);


    if (font_px) {
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(GU_GREATER, 0, 0xff);
        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        sceGuTexScale(1.0f, 1.0f);
        sceGuTexOffset(0.0f, 0.0f);
        sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
        sceGuClutLoad(32, font_cl);
        sceGuTexMode(GU_PSM_T8, 0, 0, 1);
        sceGuTexImage(0, 512, 512, 512, font_px);
        sceGuTexFlush();
        sceGuTexSync();


        {
            char hpb[32], mpb[32];
            snprintf(hpb, sizeof(hpb), "%d/%d", hp, mhp);
            snprintf(mpb, sizeof(mpb), "%d/%d", mp, mmp);
            unsigned int hcol = 0xffffffff;
            if (hp <= 0) hcol = 0xff808080;
            else if (mhp > 0 && hp < (mhp + 3) / 4) hcol = 0xff2020ff;
            battle_text(actor_name, 158.0f, 222.0f, 0xffffffff, &vp);
            battle_text("Body", 158.0f, 240.0f, MSG_PAL[16], &vp);
            battle_text(hpb, 298.0f - battle_text_w(hpb), 240.0f, hcol,
                        &vp);
            battle_text("Mind", 322.0f, 240.0f, MSG_PAL[16], &vp);
            battle_text(mpb, 462.0f - battle_text_w(mpb), 240.0f,
                        0xffffffff, &vp);
        }
        if (show_cmds) {
            for (int i = 0; i < ncmds; i++) {
                float cy = 162.0f + (float)i * 18.0f;
                battle_text(cmds[i], 30.0f, cy, 0xffffffff, &vp);
            }
        }

        if (show_list && lrows) {
            for (int i = 0; i < nlrows; i++) {
                float cy = 162.0f + (float)i * 18.0f;
                battle_text(lrows[i].text, 50.0f, cy, lrows[i].color, &vp);
            }
        }
        for (int i = 0; i < npops; i++) {

            float k = pops[i].max > 0 ? (float)pops[i].ttl / (float)pops[i].max : 0.0f;
            if (k < 0.0f) k = 0.0f;
            char buf[16];
            unsigned int col;
            if (pops[i].kind == 1) {
                snprintf(buf, sizeof(buf), "MISS");
                col = 0xffffffff;
            } else {
                snprintf(buf, sizeof(buf), "%d", pops[i].value);
                col = pops[i].kind == 2 ? 0xff39f6fd
                                        : pops[i].kind == 3 ? 0xff40cc66
                                                            : 0xffffffff;
            }
            col = (col & 0x00ffffff) |
                  (((unsigned int)(255.0f * k)) << 24);
            float py = (float)pops[i].y - (1.0f - k) * 24.0f;
            battle_text(buf, (float)pops[i].x, py, col, &vp);
        }


        if (show_targets && ntargets > 0) {
            for (int i = 0; i < en_rows && i < ntargets; i++) {
                float cy = 22.0f + (float)i * 22.0f;
                const char *nm = (targets[i] && targets[i][0]) ? targets[i]
                                                               : "?";
                battle_text(nm, 314.0f, cy, 0xffffffff, &vp);
            }
        }


        if (log0 && log0[0])
            battle_text(log0, 12.0f, 10.0f, 0xffffffff, &vp);
        if (log1 && log1[0])
            battle_text(log1, 12.0f, 32.0f, 0xffffffff, &vp);


        if (vp > v)
            sceGuDrawArray(GU_SPRITES, TVERT_FMT, (int)(vp - v), 0, v);
    }

    if (icon_t8) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        sceGuTexScale(1.0f, 1.0f);
        sceGuTexOffset(0.0f, 0.0f);
        sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
        sceGuClutLoad(32, icon_cl);
        sceGuTexMode(GU_PSM_T8, 0, 0, 1);
        sceGuTexImage(0, 256, 512, 256, icon_t8);
        sceGuTexFlush();
        sceGuTexSync();
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(GU_GREATER, 0, 0xff);
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

        {
            float nx = 158.0f;
            for (const char *p = actor_name; *p; p++)
                nx += msg_adv((unsigned char)*p);
            nx += 6.0f;
            for (int i = 0; i < nst_icons && i < 3; i++) {
                int idx = st_icons[i];
                if (idx < 0) continue;
                TVert *q = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
                q[0].u = (float)((idx % 16) * 16);
                q[0].v = (float)((idx / 16) * 16);
                q[0].color = 0xffffffff;
                q[0].x = nx + (float)(i * 18);
                q[0].y = 230.0f;
                q[0].z = 0.0f;
                q[1].u = q[0].u + 16.0f;
                q[1].v = q[0].v + 16.0f;
                q[1].color = 0xffffffff;
                q[1].x = q[0].x + 16.0f;
                q[1].y = q[0].y + 16.0f;
                q[1].z = 0.0f;
                sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, q);
            }
        }

        if (show_list && lrows) {
            for (int i = 0; i < nlrows; i++) {
                if (lrows[i].icon < 0) continue;
                float cy = 162.0f + (float)i * 18.0f + 1.0f;
                TVert *q = (TVert *)sceGuGetMemory(2 * sizeof(TVert));
                q[0].u = (float)((lrows[i].icon % 16) * 16);
                q[0].v = (float)((lrows[i].icon / 16) * 16);
                q[0].color = 0xffffffff;
                q[0].x = 30.0f;
                q[0].y = cy;
                q[0].z = 0.0f;
                q[1].u = q[0].u + 16.0f;
                q[1].v = q[0].v + 16.0f;
                q[1].color = 0xffffffff;
                q[1].x = 46.0f;
                q[1].y = cy + 16.0f;
                q[1].z = 0.0f;
                sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, q);
            }
        }
        sceGuDisable(GU_BLEND);
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDisable(GU_ALPHA_TEST);
    }


    {
        float hpf = mhp > 0 ? (float)hp / (float)mhp : 0.0f;
        float mpf = mmp > 0 ? (float)mp / (float)mmp : 0.0f;
        if (hpf < 0.0f) hpf = 0.0f;
        if (mpf < 0.0f) mpf = 0.0f;
        if (hpf > 1.0f) hpf = 1.0f;
        if (mpf > 1.0f) mpf = 1.0f;
        sceGuDisable(GU_TEXTURE_2D);

        TVert *br = (TVert *)sceGuGetMemory(18 * 2 * sizeof(TVert));
        TVert *bp = br;
        float by = 258.0f;
        bar_quad(bp, 158.0f, by, 298.0f, by + 6.0f, 0xff402020);
        bp += 2;
        bar_quad(bp, 322.0f, by, 462.0f, by + 6.0f, 0xff402020);
        bp += 2;
        bp = gauge_grad(bp, 158.0f, by, 140.0f * hpf, 0xff2a3050,
                        0xff112589);
        bp = gauge_grad(bp, 322.0f, by, 140.0f * mpf, 0xff2a3050,
                        0xff112589);
        sceGuDrawArray(GU_SPRITES, TVERT_FMT, (int)(bp - br), 0, br);
    }
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
}
