
#ifndef RENDER_H
#define RENDER_H

#include <pspgu.h>
#include <stdint.h>
#include "../../runtime/player.h"
#include "interp_rt.h"
#include "text_rt.h"

#define TILE 24
#define SCR_W 480
#define SCR_H 272
#define BUF_W 512


typedef struct { int tw, th, stride; } SheetDef;


typedef struct {
    unsigned char *t8;
    unsigned int *clut;
    int img_w, img_h, tex_w, tex_h, stride, is_big;
    int tile_x, tile_y;
    int char_index, pattern, dir_mv;
    int prio;
} NpcSprite;

typedef struct {
    int tx, ty, r;
    unsigned int color;
    float brightness;
    int flicker, id;
} MapLight;

typedef struct {
    float x, y, r, r1;
    unsigned int color;
    float brightness;
    int flicker;
} FhLight;

extern const SheetDef SHEETS[9];
extern unsigned char *sheet_px[9];
extern unsigned int sheet_cl[9][256];


void render_init(void *fbp0, void *fbp1, void *zbp, unsigned int *gu_list);


void render_frame(int cam_x, int cam_y,
                  const uint16_t *map_layers, int map_w, int map_h,
                  const Player *player, int current_char,
                  unsigned char *char_sprites[4],
                  unsigned int *char_cluts[4],
                   const uint8_t *higher, int higher_len, int frames,
                   const NpcSprite *npcs, int n_npcs,
                   const FhLight *lights, int nlights);


void render_map_layers(int cam_x, int cam_y, const uint16_t *map_layers, int map_w, int map_h,
                       const uint8_t *higher, int higher_len, int upper_pass);
void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data);


void render_message_window(const FhInterp *mit, int msg_ended, int cursor,
                           int page_wait);


int msg_text_revealed(void);
void msg_reveal_all(void);
void msg_content_changed(void);


extern unsigned char *font_px;
extern unsigned int font_cl[256];
extern unsigned char *font_adv;


extern unsigned char *window_px;
extern unsigned int window_cl[256];


extern unsigned char *floor_px;
extern int battle_live_bg;
extern unsigned int floor_cl[256];


extern unsigned char *anim_t8[8];
extern unsigned int anim_cl[8][256];

extern unsigned char *icon_t8;
extern unsigned int icon_cl[256];


typedef struct {
    unsigned char *t8;
    unsigned int *clut;
    int tw, th, stride, w, h;
    float x, y;

    int alive;
} BtFoeDraw;

typedef struct {
    int x, y, value, kind, ttl, max;
} BtPopup;


typedef struct {
    char text[48];
    unsigned int color;
    int icon;
} BtListRow;

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
                   int show_list, const int *st_icons, int nst_icons);


void battle_anim_start(int anim_id, int foe_idx, int mirror);
void battle_anim_tick(void);
void battle_anim_reset(void);
void render_battle_anims(const BtFoeDraw *foes, int nfoes);


void render_debug_text(const char *text);

#endif
