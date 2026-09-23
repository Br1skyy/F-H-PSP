
#include "player.h"
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>

#define TILE_SIZE 24

void player_init(Player *p, int tile_x, int tile_y) {
    memset(p, 0, sizeof(*p));
    p->x = tile_x * TILE_SIZE;
    p->y = tile_y * TILE_SIZE;
    p->dir = 0;
    p->step_frame = 0;
    p->step_count = 0;
    p->moving = 0;
    p->move_speed = 4;
    p->anim_pattern = 1;
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

    int up    = buttons & PSP_CTRL_UP;
    int down  = buttons & PSP_CTRL_DOWN;
    int left  = buttons & PSP_CTRL_LEFT;
    int right = buttons & PSP_CTRL_RIGHT;


    if (up && down) { up = down = 0; }
    if (left && right) { left = right = 0; }


    if (down) return 0;
    if (left) return 1;
    if (right) return 2;
    if (up) return 3;

    return -1;
}

int player_can_pass(const Player *p, int tile_x, int tile_y,
                    const uint16_t *passability, int map_w, int map_h,
                    const uint8_t *solid) {
    if (tile_x < 0 || tile_y < 0 || tile_x >= map_w || tile_y >= map_h)
        return 0;

    if (!passability)
        return 1;


    const uint8_t *pass8 = (const uint8_t*)passability;
    uint8_t flags = pass8[tile_y * map_w + tile_x];


    if (flags == 0)
        return 0;
    if (solid && solid[tile_y * map_w + tile_x])
        return 0;

    return 1;
}

int player_update(Player *p, unsigned int buttons, const uint16_t *passability,
                  int map_w, int map_h, const uint8_t *solid) {


    if (p->moving) {

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


        p->anim_count += 1.5f;
        if (p->anim_count >= 15.0f) {
            p->anim_count = 0.0f;
            p->anim_pattern = (p->anim_pattern + 1) % 4;
            int dp = p->anim_pattern < 3 ? p->anim_pattern : 1;
            p->step_frame = (dp == 1) ? 0 : (dp == 0) ? 1 : 2;
        }

        if (p->step_count >= TILE_SIZE) {


            p->step_count = 0;
            p->moving = 0;
            p->move_acc = 0.0f;
        }
        return 1;
    }


    int dir = player_input_dir(buttons);
    if (dir < 0) {

        p->anim_pattern = 1;
        p->anim_count = 0.0f;
        p->step_frame = 0;
        return 0;
    }


    p->dir = dir;


    int target_x = p->x / TILE_SIZE;
    int target_y = p->y / TILE_SIZE;

    switch (dir) {
        case 0: target_y++; break;
        case 1: target_x--; break;
        case 2: target_x++; break;
        case 3: target_y--; break;
    }


    if (!player_can_pass(p, target_x, target_y, passability, map_w, map_h,
                         solid)) {

        p->anim_pattern = 1;
        p->anim_count = 0.0f;
        p->step_frame = 0;
        return 0;
    }


    p->moving = 1;
    p->step_count = 0;
    p->move_acc = 0.0f;

    return 1;
}

void player_render(Player *p, int cam_x, int cam_y) {
    if (!p->sprite_data || !p->sprite_clut)
        return;


    int frame_w = p->sprite_w / 3;
    int frame_h = p->sprite_h / 4;


    int src_x = p->step_frame * frame_w;
    int src_y = p->dir * frame_h;


    int screen_x = p->x - cam_x;
    int screen_y = p->y - cam_y;


    int offset_x = (frame_w - TILE_SIZE) / 2;
    int offset_y = frame_h - TILE_SIZE;

    screen_x -= offset_x;
    screen_y -= offset_y;


    (void)src_x; (void)src_y;
    (void)screen_x; (void)screen_y;
}
