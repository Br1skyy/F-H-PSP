/* Event interpreter core — md §5.2. Platform-independent (no PSP headers):
 * builds for the PC test harness AND the PSP EBOOT.
 *
 * Commands carry baked `jump` targets (tools/bake.py) so the runtime never
 * scans. Semantics mirror Game_Interpreter.skipBranch/command111/411/402/
 * command113/413 (rpg_objects.js): SKIP cmds jump to the next list index
 * with indent <= theirs (or past the end); 413 jumps back to its 112;
 * 113 outside a loop jumps past the end (abort-event idiom, 6676x in data).
 */
#ifndef FH_INTERP_RT_H
#define FH_INTERP_RT_H

#include <stdint.h>

#define FH_MAX_SWITCHES 3601   /* real sizes from System.json (audit) */
#define FH_MAX_VARS      451
#define FH_MAX_PARTY      16
#define FH_MAX_ACTORS     64
#define FH_MAX_STATES    128
#define FH_ROUTE_Q        64
#define FH_CALL_DEPTH      8
#define FH_MAX_ITEMS     256   /* real counts: items 220, weapons 50, armors 60 */
#define FH_MAX_WEAPONS    64
#define FH_MAX_ARMORS     64
#define FH_MAX_CHARS     512   /* char id + 2 indexed (-1 player .. 509) */
#define FH_ANIM_Q         64
#define FH_NAME_CAP       64   /* audio/pic names run long (53+ chars observed) */
#define FH_PLUG_Q        256
#define FH_TEXT_CAP     8192
#define FH_TRACE_CAP   65536

typedef struct {
    uint16_t code;
    uint8_t  indent;
    uint16_t jump;      /* baked target index; list length (=end) allowed */
    uint8_t  op;        /* sub-op: 111 cond type; 122 operation; 223 wait; else 0 */
    int32_t  p[10];     /* numeric params (pictures need 9) */
    const char *s;      /* string param (text lines, SE names, scripts) or NULL */
} FhCmd;

typedef struct { const FhCmd *list; int len, pc; } FhFrame;

typedef struct {
    const FhCmd *list;
    int len, pc, wait;
    unsigned char sw[FH_MAX_SWITCHES];
    int32_t var[FH_MAX_VARS];
    int branch;         /* battle result 0/1/2 for 601/602/603 */
    int branchv[16];      /* last 111 result per indent (for 411) */
    int choice_sel;     /* test/UI-provided selection for 102/402/403 (-1 = cancelled) */
    int await_choice;   /* set by 102: UI must pick (choice_text lines) then clear */
    const char *choice_text; /* \n-joined options from the 102 command */
    int choice_n;       /* option count */
    char text[FH_TEXT_CAP];
    int text_len;
    int page_open;                      /* 1 = page accumulated, 2 = shown */
    int trace_skip;                     /* skip one trace append (PAGE resume) */
    char msg_face[FH_NAME_CAP];             /* 101 face sheet name (may be "") */
    int msg_face_idx;                       /* 101 face index */
    int party[FH_MAX_PARTY]; int party_n;       /* 129 roster */
    int tint[4]; int tint_frames;                /* 223 screen tone */
    char last_se[64]; int se_count;              /* 250 SE log (backend hooks later) */
    struct { int ch, steps, wait; } routes[FH_ROUTE_Q]; int route_n; /* 205 queued (movement pending) */
    unsigned char astate[FH_MAX_ACTORS][FH_MAX_STATES]; /* 313 actor states */
    FhFrame callst[FH_CALL_DEPTH]; int depth;    /* 117 common-event stack */
    const int *ce_ids; const FhCmd **ce_lists; const int *ce_lens; int ce_n;
    int inv_item[FH_MAX_ITEMS], inv_weap[FH_MAX_WEAPONS], inv_arm[FH_MAX_ARMORS]; /* 126/127/128 */
    int hp[FH_MAX_ACTORS], mp[FH_MAX_ACTORS];   /* 311/312 (harness seeds hp) */
    struct { int ch, anim; } anims[FH_ANIM_Q]; int anim_n; /* 212 requests */
    int chx[FH_MAX_CHARS], chy[FH_MAX_CHARS], chd[FH_MAX_CHARS]; /* 203 locate (id+2) */
    int ev_id, on_map;                          /* 214 erase context */
    int transparent, followers_on;              /* 211 player transparency, 216 followers */
    struct { int map, x, y, dir, fade, pending; } transfer; /* 201 reserved transfer */
    struct { int ch, icon; } balloons[FH_ANIM_Q]; int balloon_n; /* 213 */
    int erased[64]; int erased_n;                /* 214 erased event ids */
    struct { int dir, dist, speed; } scrolls[16]; int scroll_n;  /* 204 */
#define FH_MAX_PICS 128
    unsigned char selfsw[512][4];               /* 123 self switches (ev_id, A-D) */
    int timer_on, timer_frames;                 /* 124 timer */
    int gold;                                   /* 125 party gold */
    char bbgm[FH_NAME_CAP];                     /* 132 battle BGM */
    int menu_disabled;                          /* 135 menu access */
    int fade;                                   /* 221 out (-1) / 222 in (+1) */
    int flash[4], flash_frames;                 /* 224 screen flash */
    int shake[3];                               /* 225 power, speed, frames */
    struct { int used, origin, x, y, sx, sy, op, blend, rot, tone[4]; char name[FH_NAME_CAP]; } pic[FH_MAX_PICS]; /* 231-235 */
    int weather[3];                             /* 236 type, power, frames */
#define FH_MAX_SKILLS 384   /* real max skill id 340 */
    int exp_[FH_MAX_ACTORS], level[FH_MAX_ACTORS], apram[FH_MAX_ACTORS][8]; /* 315/316/317 */
    unsigned char askill[FH_MAX_ACTORS][FH_MAX_SKILLS]; /* 318 learn/forget */
    int equip[FH_MAX_ACTORS][8];                /* 319 slot -> item id */
    char aname[FH_MAX_ACTORS][FH_NAME_CAP];     /* 320 custom name */
    int aclass[FH_MAX_ACTORS];                  /* 321 class id */
    char anick[FH_MAX_ACTORS][FH_NAME_CAP];     /* 324 nickname */
    char aprof[FH_MAX_ACTORS][256];             /* 325 profile */
    int tp[FH_MAX_ACTORS];                      /* 326 TP */
#define FH_MAX_ENEMIES 16
    int ehp[FH_MAX_ENEMIES], emp[FH_MAX_ENEMIES], etp[FH_MAX_ENEMIES]; /* 331/332/342 */
    unsigned char estate[FH_MAX_ENEMIES][FH_MAX_STATES]; /* 333 */
    int eappear[FH_MAX_ENEMIES], etransform[FH_MAX_ENEMIES]; /* 335/336 */
    int troop_n;                                /* harness-set troop size */
    struct { int side, idx, skill, target, pending; } force; /* 339 force action */    struct { int troop, esc, lose, pending; } battle; /* 301 reserved battle */
    struct { int type, id, price; } shop[16]; int shop_n, shop_only; /* 302 (+605 goods) */
    int nameinput[2];                           /* 303 actor, max chars */
    int scene_req;                              /* 351 menu 352 save 353 over 354 title */
    int numinput[3], itemchoice[2];             /* 103/104 (dead, recorded) */
    char last_bgm[FH_NAME_CAP], last_bgs[FH_NAME_CAP], last_me[FH_NAME_CAP]; /* 241/245/249 */
    int bgm_fade, bgs_fade, se_stop, bgm_saved, bgm_replayed; /* 242/246/251/243/244 */
    char victory_me[FH_NAME_CAP], defeat_me[FH_NAME_CAP]; /* 133/139 */
    int save_on, encounter_on, formation_on, namedisp; /* 134/136/137/281 */
    int wtone[4];                               /* 138 window tone */
    int tileset;                                /* 282 tileset id */
    char battleback1[FH_NAME_CAP], battleback2[FH_NAME_CAP]; /* 283 */
    struct { char name[FH_NAME_CAP]; int loopx, loopy, sx, sy; } parallax; /* 284 */
    struct { int var, type, x, y; } locinfo;    /* 285 recorded (map ctx pending) */
    int vehicle_in, gather;                     /* 206/217 */
    struct { int veh, map, x, y; } vehloc;      /* 202 */
    struct { int veh; char name[FH_NAME_CAP]; } vehbgm; /* 140 */
    char movie[FH_NAME_CAP];                    /* 261 movie name */
    int refresh_n;                                  /* 355 $gamePlayer.refresh() */
    char chname[FH_MAX_ACTORS][FH_NAME_CAP]; int chidx[FH_MAX_ACTORS];   /* setCharacterImage */
    char btname[FH_MAX_ACTORS][FH_NAME_CAP];                            /* setBattlerImage */
    char fcname[FH_MAX_ACTORS][FH_NAME_CAP]; int fcidx[FH_MAX_ACTORS];   /* setFaceImage */
    struct { char name[32], args[96]; } plug[FH_PLUG_Q]; int plug_n;     /* 356 dispatch log */
    int unknown;        /* count of logged-unknown commands (never crashes) */
    int trace[FH_TRACE_CAP];  /* executed pc sequence */
    int trace_len;
    int waits;          /* total wait frames consumed */
} FhInterp;

enum { FH_RUN_END = 0, FH_RUN_WAIT = 1, FH_RUN_STEP = 2, FH_RUN_CHOICE = 3,
         FH_RUN_PAGE = 4 };

void fh_interp_init(FhInterp *it, const FhCmd *list, int len);
int fh_interp_step(FhInterp *it);   /* runs until WAIT or END; STEP if bounded */
int fh_interp_run(FhInterp *it, int max_steps);  /* FH_RUN_END / FH_RUN_WAIT */

#endif
