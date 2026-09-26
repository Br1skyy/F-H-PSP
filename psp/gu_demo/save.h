#ifndef FH_SAVE_H
#define FH_SAVE_H

#include "../../runtime/interp.h"

#define FH_SAVE_MAGIC 0x48465356u
#define FH_SAVE_VERSION 1u

typedef struct {
    int map, px, py, dir, character, gold;
    unsigned char sw[FH_MAX_SWITCHES];
    int var[FH_MAX_VARS];
    unsigned char inv_item[FH_MAX_ITEMS];
    unsigned char inv_weap[FH_MAX_WEAPONS];
    unsigned char inv_arm[FH_MAX_ARMORS];
    unsigned char askill[FH_MAX_ACTORS][FH_MAX_SKILLS];
    int audio_vol[5];
} SaveState;

int save_exists(void);
int save_store_state(const SaveState *st);
int save_load_state(SaveState *st);

#endif
