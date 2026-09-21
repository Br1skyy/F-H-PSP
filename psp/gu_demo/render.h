/* Rendering module - handles all GU drawing operations */
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

/* Texture/sheet management */
typedef struct { int tw, th, stride; } SheetDef;

/* NPC/event sprite (page image from Map JSON, baked into main.c).
 * Sheet dims are ACTIVE px (w/h from converted meta); tex/stride are the
 * padded power-of-2 upload dims. dir_mv uses RPG Maker codes (2,4,6,8). */
typedef struct {
    unsigned char *t8;
    unsigned int *clut;
    int img_w, img_h, tex_w, tex_h, stride, is_big;
    int tile_x, tile_y;
    int char_index, pattern, dir_mv;
    int prio;  /* 0 = below characters (corpses), 1 = same level */
} NpcSprite;

extern const SheetDef SHEETS[9];
extern unsigned char *sheet_px[9];
extern unsigned int sheet_cl[9][256];

/* Initialize rendering system */
void render_init(void *fbp0, void *fbp1, void *zbp, unsigned int *gu_list);

/* Render one frame of the game world.
 * higher/higher_len: higher-tile mask (flags[tid] & 0x10, baked from
 * Tilesets.json). Higher tiles (z=4) draw above same-priority characters
 * (z=3), everything else (z=0) below — rpg_core.js Tilemap. */
void render_frame(int cam_x, int cam_y, 
                  const uint16_t *map_layers, int map_w, int map_h,
                  const Player *player, int current_char,
                  unsigned char *char_sprites[4],
                  unsigned int *char_cluts[4],
                  const uint8_t *higher, int higher_len, int frames,
                  const NpcSprite *npcs, int n_npcs, int torch_on);

/* Render functions for specific elements.
 * upper_pass=0 draws z=0 tiles (below characters), =1 draws z=4 higher
 * tiles (above same-priority characters). */
void render_map_layers(int cam_x, int cam_y, const uint16_t *map_layers, int map_w, int map_h,
                       const uint8_t *higher, int higher_len, int upper_pass);
void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data);

/* Message window (Window_Message replacement): dim box + game-font text
 * with \C colors, choice list with cursor. Replaces debug-screen text. */
void render_message_window(const FhInterp *mit, int msg_ended, int cursor);

/* Game font atlas (baked by tools/bake_font.py, assigned at load). */
extern unsigned char *font_px;
extern unsigned int *font_cl;

/* Debug overlay */
void render_debug_text(const char *text);

#endif /* RENDER_H */
