/* Player character implementation for PSP */
#include "player.h"
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>

#define TILE_SIZE 24

void player_init(Player *p, int tile_x, int tile_y) {
    memset(p, 0, sizeof(*p));
    p->x = tile_x * TILE_SIZE;
    p->y = tile_y * TILE_SIZE;
    p->dir = 0;  /* facing down */
    p->step_frame = 0;
    p->step_count = 0;
    p->moving = 0;
    p->move_speed = 4;  /* OG moveSpeed 4 (rpg_objects.js initMembers) */
    p->anim_pattern = 1;  /* OG idle pattern (straighten/resetPattern) */
    p->anim_count = 0.0f;
    p->move_acc = 0.0f;
}

void player_set_sprite(Player *p, const unsigned char *t8_data,
                      const unsigned int *clut, int w, int h, int pattern) {
    p->sprite_data = t8_data;
    p->sprite_clut = clut;
    p->sprite_w = w;
    p->sprite_h = h;
    p->sprite_pattern = pattern;
}

int player_input_dir(unsigned int buttons) {
    /* Priority: check diagonals first (for 4-dir movement, pick dominant) */
    int up    = buttons & PSP_CTRL_UP;
    int down  = buttons & PSP_CTRL_DOWN;
    int left  = buttons & PSP_CTRL_LEFT;
    int right = buttons & PSP_CTRL_RIGHT;
    
    /* Cancel opposite directions */
    if (up && down) { up = down = 0; }
    if (left && right) { left = right = 0; }
    
    /* 4-directional: prefer vertical if both pressed */
    if (down) return 0;
    if (left) return 1;
    if (right) return 2;
    if (up) return 3;
    
    return -1;  /* no direction */
}

int player_can_pass(const Player *p, int tile_x, int tile_y,
                    const uint16_t *passability, int map_w, int map_h,
                    const uint8_t *solid) {
    if (tile_x < 0 || tile_y < 0 || tile_x >= map_w || tile_y >= map_h)
        return 0;  /* out of bounds */
    
    if (!passability)
        return 1;  /* no passability data = always passable */
    
    /* Note: passability might be uint8 or uint16 depending on format
     * For now, treat as uint8 and just check if non-zero */
    const uint8_t *pass8 = (const uint8_t*)passability;
    uint8_t flags = pass8[tile_y * map_w + tile_x];

    /* Full 4-dir passage bits + counter tiles are not ported yet; a tile
     * is either open or shut. Same-priority events block via solid. */
    if (flags == 0)
        return 0;
    if (solid && solid[tile_y * map_w + tile_x])
        return 0;
    
    return 1;
}

int player_update(Player *p, unsigned int buttons, const uint16_t *passability,
                  int map_w, int map_h, const uint8_t *solid) {
    /* If already moving, continue the current step.
     * OG timing at moveSpeed 4: distancePerFrame = 2^4/256 tile/frame =
     * 1/16 tile/frame = 1.5px/frame at our 24px tiles -> 16 frames/tile.
     * Anim: count += 1.5/frame, advance every animationWait=15 counts
     * (every 10 frames), pattern cycles 1,2,1,0 displayed. */
    if (p->moving) {
        /* Fractional movement: alternate 1,2,1,2... px (avg 1.5). */
        p->move_acc += 1.5f;
        int step = (int)p->move_acc;
        p->move_acc -= (float)step;
        p->step_count += step;
        switch (p->dir) {
            case 0: p->y += step; break;
            case 1: p->x -= step; break;
            case 2: p->x += step; break;
            case 3: p->y -= step; break;
        }

        /* OG updateAnimationCount + updatePattern. */
        p->anim_count += 1.5f;
        if (p->anim_count >= 15.0f) {
            p->anim_count = 0.0f;
            p->anim_pattern = (p->anim_pattern + 1) % 4;
            int dp = p->anim_pattern < 3 ? p->anim_pattern : 1;
            p->step_frame = (dp == 1) ? 0 : (dp == 0) ? 1 : 2;
        }

        if (p->step_count >= TILE_SIZE) {
            /* Finished one tile. OG does NOT straighten here when the
             * next step continues (resetPattern only fires while stopped),
             * so the walk cycle runs on across tiles: 1,2,1,0... */
            p->step_count = 0;
            p->moving = 0;
            p->move_acc = 0.0f;
        }
        return 1;
    }
    
    /* Not moving: check for new input */
    int dir = player_input_dir(buttons);
    if (dir < 0) {
        /* Truly stopped: OG straighten() — back to idle pattern. */
        p->anim_pattern = 1;
        p->anim_count = 0.0f;
        p->step_frame = 0;
        return 0;  /* no input */
    }
    
    /* Update facing direction */
    p->dir = dir;
    
    /* Calculate target tile */
    int target_x = p->x / TILE_SIZE;
    int target_y = p->y / TILE_SIZE;
    
    switch (dir) {
        case 0: target_y++; break;  /* down */
        case 1: target_x--; break;  /* left */
        case 2: target_x++; break;  /* right */
        case 3: target_y--; break;  /* up */
    }
    
    /* Check collision */
    if (!player_can_pass(p, target_x, target_y, passability, map_w, map_h,
                         solid)) {
        /* Bumped: stopped, so straighten to idle like the OG. */
        p->anim_pattern = 1;
        p->anim_count = 0.0f;
        p->step_frame = 0;
        return 0;  /* blocked */
    }
    
    /* Start moving (OG keeps idle pattern until the anim count fires). */
    p->moving = 1;
    p->step_count = 0;
    p->move_acc = 0.0f;

    return 1;
}

void player_render(Player *p, int cam_x, int cam_y) {
    if (!p->sprite_data || !p->sprite_clut)
        return;  /* no sprite loaded */
    
    /* Character sprite layout: ONE character's states on a 12-col × 8-row
     * grid of cells (80×110 full-res, 40×55 at our 0.5 scale).
     * The walking block is characterIndex 0 (cols 0-2, rows 0-3);
     * other blocks hold other states (dead, crawl, attack, ...).
     * Rows: 0=down, 1=left, 2=right, 3=up (matches Player.dir).
     * Cols: pattern 1=center(idle), 0=left foot, 2=right foot, so
     * step_frame 0 (idle) -> col 1, 1 -> col 0, 2 -> col 2.
     * (The live renderer is render_player_sprite() in
     * psp/gu_demo/render.c; the demo loads the *_torch sheets.) */
    
    int frame_w = p->sprite_w / 3;   /* 160 pixels per frame at 480px width */
    int frame_h = p->sprite_h / 4;   /* 110 pixels per direction at 440px height */
    
    /* Source rectangle in sprite sheet */
    int src_x = p->step_frame * frame_w;
    int src_y = p->dir * frame_h;
    
    /* Screen position (subtract camera offset) */
    int screen_x = p->x - cam_x;
    int screen_y = p->y - cam_y;
    
    /* Center the sprite on the tile (sprite is taller than 24px) */
    int offset_x = (frame_w - TILE_SIZE) / 2;
    int offset_y = frame_h - TILE_SIZE;  /* align bottom of sprite with tile */
    
    screen_x -= offset_x;
    screen_y -= offset_y;
    
    /* TODO: PSP GU rendering code
     * This needs to:
     * 1. Load sprite texture + CLUT
     * 2. Set up sprite rendering mode (alpha blend)
     * 3. Draw textured quad with UV mapped to current frame
     * 
     * For now, this is a placeholder - actual GU calls will be added
     * in the main rendering loop integration.
     */
    
    (void)src_x; (void)src_y;  /* suppress unused warnings */
    (void)screen_x; (void)screen_y;
}
