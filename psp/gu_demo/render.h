/* Rendering module - handles all GU drawing operations */
#ifndef RENDER_H
#define RENDER_H

#include <pspgu.h>
#include <stdint.h>
#include "../../runtime/player.h"

#define TILE 24
#define SCR_W 480
#define SCR_H 272
#define BUF_W 512

/* Texture/sheet management */
typedef struct { int tw, th, stride; } SheetDef;

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
                  const uint8_t *higher, int higher_len, int frames);

/* Render functions for specific elements.
 * upper_pass=0 draws z=0 tiles (below characters), =1 draws z=4 higher
 * tiles (above same-priority characters). */
void render_map_layers(int cam_x, int cam_y, const uint16_t *map_layers, int map_w, int map_h,
                       const uint8_t *higher, int higher_len, int upper_pass);
void render_player_sprite(const Player *player, int cam_x, int cam_y,
                         unsigned char *sprite_data, unsigned int *clut_data);

/* Debug overlay */
void render_debug_text(const char *text);

#endif /* RENDER_H */
