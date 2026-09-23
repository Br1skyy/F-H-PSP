

#ifndef FH_INTERP_RT_H
#define FH_INTERP_RT_H

#include <stdint.h>

#define FH_MAX_SWITCHES 3601
#define FH_MAX_VARS      451
#define FH_MAX_PARTY      16
#define FH_MAX_ACTORS     64
#define FH_MAX_STATES    128
#define FH_ROUTE_Q        64
#define FH_CALL_DEPTH      8
#define FH_MAX_ITEMS     256
#define FH_MAX_WEAPONS    64
#define FH_MAX_ARMORS     64
#define FH_MAX_CHARS     512
#define FH_ANIM_Q         64
#define FH_NAME_CAP       64
#define FH_PLUG_Q        256
#define FH_TEXT_CAP     8192
#define FH_TRACE_CAP   65536

typedef struct {
    uint16_t code;
    uint8_t  indent;
    uint16_t jump;
    uint8_t  op;
    int32_t  p[10];
    const char *s;
} FhCmd;

typedef struct { const FhCmd *list; int len, pc; } FhFrame;

typedef struct {
    const FhCmd *list;
    int len, pc, wait;
    unsigned char sw[FH_MAX_SWITCHES];
    int32_t var[FH_MAX_VARS];
    int branch;
    int branchv[16];
    int choice_sel;
    int await_choice;
    const char *choice_text;
    int choice_n;
    char text[FH_TEXT_CAP];
    int text_len;
    int page_open;
    int trace_skip;
    char msg_face[FH_NAME_CAP];
    int msg_face_idx;
    int msg_bg;
    int msg_pos;
    int party[FH_MAX_PARTY]; int party_n;
    int tint[4]; int tint_frames;
    char last_se[64]; int se_count;
    struct { int ch, steps, wait; } routes[FH_ROUTE_Q]; int route_n;
    unsigned char astate[FH_MAX_ACTORS][FH_MAX_STATES];
    FhFrame callst[FH_CALL_DEPTH]; int depth;
    const int *ce_ids; const FhCmd **ce_lists; const int *ce_lens; int ce_n;
    int inv_item[FH_MAX_ITEMS], inv_weap[FH_MAX_WEAPONS], inv_arm[FH_MAX_ARMORS];
    int hp[FH_MAX_ACTORS], mp[FH_MAX_ACTORS];


    struct { int ch, anim, mirror; } anims[FH_ANIM_Q]; int anim_n;
    int chx[FH_MAX_CHARS], chy[FH_MAX_CHARS], chd[FH_MAX_CHARS];
    int ev_id, on_map;
    int transparent, followers_on;
    struct { int map, x, y, dir, fade, pending; } transfer;
    struct { int ch, icon; } balloons[FH_ANIM_Q]; int balloon_n;
    int erased[64]; int erased_n;
    struct { int dir, dist, speed; } scrolls[16]; int scroll_n;
#define FH_MAX_PICS 128
    unsigned char selfsw[512][4];
    int timer_on, timer_frames;
    int gold;
    char bbgm[FH_NAME_CAP];
    int menu_disabled;
    int fade;
    int flash[4], flash_frames;
    int shake[3];
    struct { int used, origin, x, y, sx, sy, op, blend, rot, tone[4]; char name[FH_NAME_CAP]; } pic[FH_MAX_PICS];
    int weather[3];
#define FH_MAX_SKILLS 384
    int exp_[FH_MAX_ACTORS], level[FH_MAX_ACTORS], apram[FH_MAX_ACTORS][8];
    unsigned char askill[FH_MAX_ACTORS][FH_MAX_SKILLS];
    int equip[FH_MAX_ACTORS][8];
    char aname[FH_MAX_ACTORS][FH_NAME_CAP];
    int aclass[FH_MAX_ACTORS];
    char anick[FH_MAX_ACTORS][FH_NAME_CAP];
    char aprof[FH_MAX_ACTORS][256];
    int tp[FH_MAX_ACTORS];
#define FH_MAX_ENEMIES 16
    int ehp[FH_MAX_ENEMIES], emp[FH_MAX_ENEMIES], etp[FH_MAX_ENEMIES];
    int emaxhp[FH_MAX_ENEMIES], emaxmp[FH_MAX_ENEMIES];


    unsigned char estate[FH_MAX_ENEMIES][FH_MAX_STATES];
    unsigned char erecover[FH_MAX_ENEMIES];

    int eappear[FH_MAX_ENEMIES], etransform[FH_MAX_ENEMIES];
    int troop_n;
    struct { int side, idx, skill, target, pending; } force;     struct { int troop, esc, lose, pending; } battle;
    struct { int type, id, price; } shop[16]; int shop_n, shop_only;
    int nameinput[2];
    int scene_req;
    int numinput[3], itemchoice[2];
    char last_bgm[FH_NAME_CAP], last_bgs[FH_NAME_CAP], last_me[FH_NAME_CAP];
    int bgm_fade, bgs_fade, se_stop, bgm_saved, bgm_replayed;
    char victory_me[FH_NAME_CAP], defeat_me[FH_NAME_CAP];
    int save_on, encounter_on, formation_on, namedisp;
    int wtone[4];
    int tileset;
    char battleback1[FH_NAME_CAP], battleback2[FH_NAME_CAP];
    struct { char name[FH_NAME_CAP]; int loopx, loopy, sx, sy; } parallax;
    struct { int var, type, x, y; } locinfo;
    int vehicle_in, gather;
    struct { int veh, map, x, y; } vehloc;
    struct { int veh; char name[FH_NAME_CAP]; } vehbgm;
    char movie[FH_NAME_CAP];
    int refresh_n;
    char chname[FH_MAX_ACTORS][FH_NAME_CAP]; int chidx[FH_MAX_ACTORS];
    char btname[FH_MAX_ACTORS][FH_NAME_CAP];
    char fcname[FH_MAX_ACTORS][FH_NAME_CAP]; int fcidx[FH_MAX_ACTORS];
    struct { char name[32], args[96]; } plug[FH_PLUG_Q]; int plug_n;
    int unknown;
    const char **actor_names; int nactors;
    unsigned rng;


    int trace[FH_TRACE_CAP];
    int trace_len;
    int waits;
} FhInterp;

enum { FH_RUN_END = 0, FH_RUN_WAIT = 1, FH_RUN_STEP = 2, FH_RUN_CHOICE = 3,
         FH_RUN_PAGE = 4 };

void fh_interp_init(FhInterp *it, const FhCmd *list, int len);
int fh_interp_step(FhInterp *it);
int fh_interp_run(FhInterp *it, int max_steps);


void fh_srand(FhInterp *it, unsigned seed);
unsigned fh_randn(FhInterp *it, unsigned n);

#endif
