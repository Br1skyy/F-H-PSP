

#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>


typedef struct {
    int x, y;
    int dir;
    int step_frame;
    int step_count;
    int moving;
    int move_speed;


    int anim_pattern;
    float anim_count;
    float move_acc;


    const unsigned char *sprite_data;
    const unsigned int *sprite_clut;
    int sprite_w, sprite_h;
    int sprite_pattern;
} Player;


void player_init(Player *p, int tile_x, int tile_y);


void player_set_sprite(Player *p, const unsigned char *t8_data,
                      const unsigned int *clut, int w, int h, int pattern);


int player_update(Player *p, unsigned int buttons, const uint16_t *passability,
                  int map_w, int map_h, const uint8_t *solid);


void player_render(Player *p, int cam_x, int cam_y);


int player_can_pass(const Player *p, int tile_x, int tile_y,
                    const uint16_t *passability, int map_w, int map_h,
                    const uint8_t *solid);


int player_input_dir(unsigned int buttons);

#endif
