/* Player character: movement, input, rendering, collision.
 * Designed for PSP (24px tiles, 480×272 viewport). */
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

/* Player state */
typedef struct {
    int x, y;              /* position in pixels (not tiles) */
    int dir;               /* 0=down, 1=left, 2=right, 3=up */
    int step_frame;        /* displayed frame: 0=center(idle), 1=left, 2=right */
    int step_count;        /* pixels walked in current step (0-24) */
    int moving;            /* 1 if currently moving, 0 if stopped */
    int move_speed;        /* OG moveSpeed (default 4, kept for API compat) */

    /* OG-faithful animation state (rpg_objects.js Game_CharacterBase):
     * animationWait = (9 - speed) * 3 = 15; count += 1.5/frame while
     * moving; pattern = (_pattern+1) % 4, displayed as p<3 ? p : 1. */
    int anim_pattern;      /* internal _pattern 0-3 (idle = 1) */
    float anim_count;      /* accumulates 1.5/frame while moving */
    float move_acc;        /* fractional-pixel accumulator (1.5px/frame) */
    
    /* Character sprite sheet info */
    const unsigned char *sprite_data;  /* T8 texture data (swizzled) */
    const unsigned int *sprite_clut;   /* 256-entry RGBA8888 palette */
    int sprite_w, sprite_h;            /* sheet dimensions (480×440 typical) */
    int sprite_pattern;                /* which row in sheet: 0-3 for 4-dir sprites */
} Player;

/* Initialize player at map position (tile coordinates) */
void player_init(Player *p, int tile_x, int tile_y);

/* Set player sprite (call after loading character sheet) */
void player_set_sprite(Player *p, const unsigned char *t8_data, 
                      const unsigned int *clut, int w, int h, int pattern);

/* Update player state (call once per frame):
 * - Reads PSP controller input
 * - Updates position with collision
 * - Advances animation frames
 * - Returns 1 if moved, 0 if stopped */
int player_update(Player *p, unsigned int buttons, const uint16_t *passability,
                  int map_w, int map_h);

/* Render player sprite at current position
 * Uses PSP GU to draw the current animation frame
 * cam_x, cam_y: camera offset in pixels */
void player_render(Player *p, int cam_x, int cam_y);

/* Check if player can move to target tile
 * Returns 1 if passable, 0 if blocked */
int player_can_pass(const Player *p, int tile_x, int tile_y,
                    const uint16_t *passability, int map_w, int map_h);

/* Convert input buttons to direction (0-3, or -1 if no direction pressed) */
int player_input_dir(unsigned int buttons);

#endif /* PLAYER_H */
