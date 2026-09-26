#include "save.h"
#include <stdio.h>
#include <string.h>

#define SAVE_PATH "fh_save0.bin"

int save_exists(void) {
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

int save_store_state(const SaveState *st) {
    FILE *f;
    unsigned magic = FH_SAVE_MAGIC, ver = FH_SAVE_VERSION;
    if (!st)
        return -1;
    f = fopen(SAVE_PATH, "wb");
    if (!f)
        return -1;
    fwrite(&magic, 4, 1, f);
    fwrite(&ver, 4, 1, f);
    fwrite(&st->map, 4, 1, f);
    fwrite(&st->px, 4, 1, f);
    fwrite(&st->py, 4, 1, f);
    fwrite(&st->dir, 4, 1, f);
    fwrite(&st->character, 4, 1, f);
    fwrite(&st->gold, 4, 1, f);
    fwrite(&st->party_n, 4, 1, f);
    fwrite(st->party, 4, 16, f);
    fwrite(st->sw, 1, sizeof(st->sw), f);
    fwrite(st->var, 4, FH_MAX_VARS, f);
    fwrite(st->inv_item, 1, sizeof(st->inv_item), f);
    fwrite(st->inv_weap, 1, sizeof(st->inv_weap), f);
    fwrite(st->inv_arm, 1, sizeof(st->inv_arm), f);
    fwrite(st->askill, 1, sizeof(st->askill), f);
    fwrite(st->audio_vol, 4, 5, f);
    fclose(f);
    return 0;
}

int save_load_state(SaveState *st) {
    FILE *f;
    unsigned magic = 0, ver = 0;
    if (!st)
        return -1;
    f = fopen(SAVE_PATH, "rb");
    if (!f)
        return -1;
    if (fread(&magic, 4, 1, f) != 1 || magic != FH_SAVE_MAGIC ||
        fread(&ver, 4, 1, f) != 1 || ver != FH_SAVE_VERSION) {
        fclose(f);
        return -1;
    }
    if (fread(&st->map, 4, 1, f) != 1 ||
        fread(&st->px, 4, 1, f) != 1 ||
        fread(&st->py, 4, 1, f) != 1 ||
        fread(&st->dir, 4, 1, f) != 1 ||
        fread(&st->character, 4, 1, f) != 1 ||
        fread(&st->gold, 4, 1, f) != 1 ||
        fread(&st->party_n, 4, 1, f) != 1 ||
        fread(st->party, 4, 16, f) != 16 ||
        fread(st->sw, 1, sizeof(st->sw), f) != sizeof(st->sw) ||
        fread(st->var, 4, FH_MAX_VARS, f) != FH_MAX_VARS ||
        fread(st->inv_item, 1, sizeof(st->inv_item), f) !=
            sizeof(st->inv_item) ||
        fread(st->inv_weap, 1, sizeof(st->inv_weap), f) !=
            sizeof(st->inv_weap) ||
        fread(st->inv_arm, 1, sizeof(st->inv_arm), f) !=
            sizeof(st->inv_arm) ||
        fread(st->askill, 1, sizeof(st->askill), f) !=
            sizeof(st->askill) ||
        fread(st->audio_vol, 4, 5, f) != 5) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}
