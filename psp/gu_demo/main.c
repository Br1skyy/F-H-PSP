

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "render.h"
#include "input.h"
#include "map_runtime.h"
#include "interp_rt.h"
#include "text_rt.h"
#include "../../runtime/player.h"
#include "../../runtime/battle.h"
#include "event_demo.h"
#include "battle_db.h"
#include "troop1.h"
#include "troop44.h"
#include "itemce.h"

PSP_MODULE_INFO("F&H port", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(1024);


static FhInterp mit;
static int msg_mode = 0, msg_ended = 0, msg_cursor = 0;
static const FhCmd *msg_list = NULL;
static int msg_len = 0;
static char msg_title[64] = "";
static int page_wait = 0;


static unsigned char keep_sw[FH_MAX_SWITCHES];
static int keep_item[FH_MAX_ITEMS];
static int keep_weap[FH_MAX_WEAPONS];
static int keep_arm[FH_MAX_ARMORS];


static const char *btl_actor_names[40];

static void btl_actor_names_init(void) {
    for (int i = 0; i < 40; i++) btl_actor_names[i] = "";
    for (int r = 0; ACTOR_NAMES[r].id; r++) {
        int id = ACTOR_NAMES[r].id;
        if (id >= 1 && id <= 40) btl_actor_names[id - 1] = ACTOR_NAMES[r].nm;
    }
}

static void msg_open(const FhCmd *list, int len, const char *title) {
    memcpy(keep_sw, mit.sw, sizeof(keep_sw));
    memcpy(keep_item, mit.inv_item, sizeof(keep_item));
    memcpy(keep_weap, mit.inv_weap, sizeof(keep_weap));
    memcpy(keep_arm, mit.inv_arm, sizeof(keep_arm));
    msg_list = list;
    msg_len = len;
    strncpy(msg_title, title, sizeof(msg_title) - 1);
    msg_title[sizeof(msg_title) - 1] = 0;
    fh_interp_init(&mit, msg_list, msg_len);
    memcpy(mit.sw, keep_sw, sizeof(keep_sw));
    memcpy(mit.inv_item, keep_item, sizeof(keep_item));
    memcpy(mit.inv_weap, keep_weap, sizeof(keep_weap));
    memcpy(mit.inv_arm, keep_arm, sizeof(keep_arm));
    mit.actor_names = btl_actor_names;
    mit.nactors = 40;
    msg_mode = 1;
    msg_ended = 0;
    msg_cursor = 0;
    page_wait = 0;
}


static void msg_advance(void) {
    int guard = 10000;
    msg_content_changed();
    while (guard-- > 0) {
        int r = fh_interp_step(&mit);
        if (r == FH_RUN_PAGE) {
            page_wait = 1;
            break;
        }
        if (r == FH_RUN_CHOICE) break;
        if (r == FH_RUN_END) {
            msg_ended = 1;
            break;
        }
        if (r == FH_RUN_WAIT) break;
    }
}


static int battle_mode = 0;
static Bt btl;
static int btl_phase;
static int btl_cmd, btl_tgt;
static int btl_order[12], btl_norder, btl_oi, btl_wait;


static int btl_act_kind, btl_act_id, btl_act_target;

static int btl_foe_skill[8];
static int btl_resCE[8], btl_nresCE;
static int btl_ce_run;
static int btl_midturn;


static int btl_run_pre, btl_run_failed;


static void btl_trace(const char *line) {
    SceUID fd = sceIoOpen("ms0:/fh_battle.txt",
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
}
static BtPopup btl_pops[8];


static char btl_log[2][96];
static int btl_log_t[2];

static char btl_actor_name[32];

static int btl_flash[8], btl_collapse[8];

static void btl_log_push(const char *s) {
    if (!s || !s[0]) return;
    snprintf(btl_log[0], sizeof(btl_log[0]), "%s", btl_log[1]);
    snprintf(btl_log[1], sizeof(btl_log[1]), "%s", s);
    btl_log_t[0] = btl_log_t[1];
    btl_log_t[1] = 420;
}
static int btl_actor_row;


static int btl_mcol, btl_mrow, btl_loop, btl_pat, btl_pat_t, btl_pose_t;

static void btl_motion(int mc, int mr, int loop) {

    if (btl_mcol != mc || btl_mrow != mr || btl_loop != loop) {
        btl_mcol = mc;
        btl_mrow = mr;
        btl_loop = loop;
        btl_pat = 0;
        btl_pat_t = 0;
    }
}

static void btl_motion_tick(void) {
    if (++btl_pat_t >= 12) {
        btl_pat_t = 0;
        if (btl_loop) btl_pat = (btl_pat + 1) % 4;
        else if (btl_pat < 2) btl_pat++;
    }
}


static BtSkill btl_skills[4];
static BtIns btl_progbuf[4][24];

static const BtSkill *btl_skill(int id) {
    for (int i = 0; i < 4; i++)
        if (btl_skills[i].id == id) return &btl_skills[i];
    return &btl_skills[0];
}

static void btl_resolve_skills(void) {
    extern unsigned char d_form_start[];
    static const int ids[4] = {1, 3, 4, 5};
    for (int i = 0; i < 4; i++) {
        const unsigned char *raw = NULL;
        int n = bt_prog_find(d_form_start, 0, ids[i], &raw);
        btl_skills[i].id = ids[i];
        btl_skills[i].prog = btl_progbuf[i];
        btl_skills[i].nins = 0;
        if (raw && n > 0 && n <= 24) {
            for (int k = 0; k < n; k++)
                bt_ins_get(raw + (unsigned)k * 10u, &btl_progbuf[i][k]);
            btl_skills[i].nins = n;
        }
        for (int k = 0; SKILL_DB[k].id; k++) {
            if (SKILL_DB[k].id == ids[i]) {
                btl_skills[i].hit_type = SKILL_DB[k].hit;
                btl_skills[i].scope = SKILL_DB[k].scope;
                btl_skills[i].speed = SKILL_DB[k].speed;
                btl_skills[i].variance = SKILL_DB[k].var;
                btl_skills[i].element = SKILL_DB[k].elem;
                btl_skills[i].dmg_type = SKILL_DB[k].type;
                btl_skills[i].crit = SKILL_DB[k].crit;
                btl_skills[i].success = SKILL_DB[k].succ;
                break;
            }
        }
    }
}

static void btl_foe_xy(int i, float *x, float *y);
static void btl_foe_art(int enemy_id, unsigned char **t8, unsigned int **cl);


static FhInterp tit;
static int btl_ev_active;
static int btl_ev_flags[16];
static int btl_turn_count;
static int btl_ev_cursor, btl_ev_ended, btl_ev_pagewait;
static int btl_plug_i;
static int btl_anim_i;
static int btl_forced[8];
static int btl_force_tgt[8];
static unsigned btl_rng;

static int btl_troop_id;

static const BtPageCond *btl_cond;
static const FhCmd **btl_lists;
static const int *btl_lens;
static int btl_npages;
static const int *btl_ce_ids;
static const FhCmd **btl_ce_lists;
static const int *btl_ce_lens;
static int btl_ce_n;
typedef struct { int id, cls, eq[8]; } BtlActorSeed;

static const BtlActorSeed *btl_seeds;


static void btl_check_end(void) {
    int foes_alive = 0, party_alive = 0;
    for (int i = 0; i < btl.n_foes; i++)
        if (btl.f[1 + i].alive) foes_alive++;
    if (btl.f[0].alive) party_alive++;
    if (!foes_alive) {
        btl.over = 1;
        btl.exp_all = 0;
        btl.gold_all = 0;
        for (int k = 0; TROOP_RW[k].id; k++) {
            if (TROOP_RW[k].id == btl_troop_id) {
                btl.exp_all = TROOP_RW[k].exp;
                btl.gold_all = TROOP_RW[k].gold;
                break;
            }
        }
        {
            char vbuf[96];
            snprintf(vbuf, sizeof(vbuf), "%s was victorious!",
                     btl_actor_name);
            btl_log_push(vbuf);
        }
        btl_phase = 3;
    } else if (!party_alive) {
        btl.over = 2;
        {
            char vbuf[96];
            snprintf(vbuf, sizeof(vbuf), "%s was defeated.",
                     btl_actor_name);
            btl_log_push(vbuf);
        }
        btl_phase = 3;
    }
}


static void btl_ev_seed(void) {
    tit.actor_names = btl_actor_names;
    tit.nactors = 40;
    memcpy(tit.sw, mit.sw, sizeof(tit.sw));
    memcpy(tit.var, mit.var, sizeof(tit.var));
    memcpy(tit.inv_item, mit.inv_item, sizeof(tit.inv_item));
    memcpy(tit.inv_weap, mit.inv_weap, sizeof(tit.inv_weap));
    memcpy(tit.inv_arm, mit.inv_arm, sizeof(tit.inv_arm));
    tit.party[0] = btl.f[0].ref;
    tit.party_n = 1;
    int aid = btl.f[0].ref;
    if (aid >= 0 && aid < FH_MAX_ACTORS) {
        tit.hp[aid] = btl.f[0].hp;
        tit.mp[aid] = btl.f[0].mp;
        for (int s = 0; s < btl.f[0].nstates; s++) {
            int sid = btl.f[0].states[s];
            if (sid >= 0 && sid < FH_MAX_STATES) tit.astate[aid][sid] = 1;
        }
        for (int r = 0; btl_seeds[r].id; r++) {
            if (btl_seeds[r].id == aid) {
                tit.aclass[aid] = btl_seeds[r].cls;
                for (int e = 0; e < 8; e++)
                    tit.equip[aid][e] = btl_seeds[r].eq[e];
                break;
            }
        }
    }
    tit.troop_n = btl.n_foes;
    for (int i = 0; i < btl.n_foes && i < FH_MAX_ENEMIES; i++) {
        tit.ehp[i] = btl.f[1 + i].hp;
        tit.emp[i] = btl.f[1 + i].mp;
        tit.emaxhp[i] = btl.f[1 + i].maxhp;
        tit.emaxmp[i] = btl.f[1 + i].maxmp;
        for (int s = 0; s < btl.f[1 + i].nstates; s++) {
            int sid = btl.f[1 + i].states[s];
            if (sid >= 0 && sid < FH_MAX_STATES) tit.estate[i][sid] = 1;
        }
    }
    tit.ce_ids = btl_ce_ids;
    tit.ce_lists = btl_ce_lists;
    tit.ce_lens = btl_ce_lens;
    tit.ce_n = btl_ce_n;
}


struct GabCap {
    char *dst;
    size_t cap, len;
};

static void gab_text_cb(const char *ptr, int len, void *ud) {
    struct GabCap *g = (struct GabCap *)ud;
    while (len-- > 0 && g->len + 1 < g->cap)
        g->dst[g->len++] = *ptr++;
    g->dst[g->len] = 0;
}

static void gab_code_cb(const char *code, int param, void *ud) {
    (void)code;
    (void)param;
    (void)ud;
}

static void btl_ev_sync(void) {
    if (tit.rng) btl_rng = tit.rng;
    memcpy(mit.sw, tit.sw, sizeof(mit.sw));
    memcpy(mit.var, tit.var, sizeof(mit.var));
    memcpy(mit.inv_item, tit.inv_item, sizeof(mit.inv_item));
    memcpy(mit.inv_weap, tit.inv_weap, sizeof(mit.inv_weap));
    memcpy(mit.inv_arm, tit.inv_arm, sizeof(mit.inv_arm));
    int aid = btl.f[0].ref;
    if (aid >= 0 && aid < FH_MAX_ACTORS) {

        if (tit.hp[aid] != btl.f[0].hp) {
            btl.f[0].hp = tit.hp[aid];
            if (btl.f[0].hp > btl.f[0].maxhp) btl.f[0].hp = btl.f[0].maxhp;
            if (btl.f[0].hp <= 0) { btl.f[0].hp = 0; btl.f[0].alive = 0; }
        }
        btl.f[0].mp = tit.mp[aid];
        if (btl.f[0].mp < 0) btl.f[0].mp = 0;
        if (btl.f[0].mp > btl.f[0].maxmp) btl.f[0].mp = btl.f[0].maxmp;
        btl.f[0].nstates = 0;
        for (int st = 0; st < FH_MAX_STATES; st++)
            if (tit.astate[aid][st]) bt_add_state(&btl.f[0], st);
    }
    for (int i = 0; i < btl.n_foes; i++) {
        BtF *f = &btl.f[1 + i];


        if (f->alive) {
            f->hp = tit.ehp[i];
            if (f->hp > f->maxhp) f->hp = f->maxhp;


            if (tit.erecover[i] && f->hp < f->maxhp &&
                tit.ehp[i] >= tit.emaxhp[i])
                f->hp = f->maxhp;
            if (f->hp <= 0) {
                f->hp = 0;
                f->alive = 0;
                if (i < 8 && btl_collapse[i] == 0) btl_collapse[i] = 32;
            }
            f->mp = tit.emp[i] < 0 ? 0 : tit.emp[i];
        }
        f->nstates = 0;
        for (int st = 0; st < FH_MAX_STATES; st++)
            if (tit.estate[i][st]) bt_add_state(f, st);


        if (tit.etransform[i] > 0) {
            int nid = tit.etransform[i], r = 0;
            while (FOE_DB[r].id && FOE_DB[r].id != nid) r++;
            if (FOE_DB[r].id) {
                f->ref = nid;
                f->maxhp = FOE_DB[r].mhp;
                if (f->hp > f->maxhp) f->hp = f->maxhp;
                if (tit.erecover[i] && f->alive) f->hp = f->maxhp;
                f->atk = FOE_DB[r].atk;
                f->def = FOE_DB[r].def;
                f->mat = FOE_DB[r].mat;
                f->mdf = FOE_DB[r].mdf;
                f->agi = FOE_DB[r].agi;
                f->luk = FOE_DB[r].luk;
                f->hit = FOE_DB[r].hit;
                f->eva = FOE_DB[r].eva;
                for (int e = 0; e < BT_ERATE_N; e++)
                    f->erate[e] = FOE_DB[r].er[e];
                f->atk_elem = FOE_DB[r].elem;
            }
            tit.etransform[i] = 0;
        }
    }

    if (tit.force.pending) {
        if (tit.force.side == 0 && tit.force.idx >= 0 &&
            tit.force.idx < BT_MAX_FOES) {
            btl_forced[tit.force.idx] = tit.force.skill;
            btl_force_tgt[tit.force.idx] = tit.force.target;
        }
        tit.force.pending = 0;
    }


    while (btl_plug_i < tit.plug_n) {
        if (!strcmp(tit.plug[btl_plug_i].name, "GabText")) {
            char tb[80];
            snprintf(tb, sizeof(tb), "gab: %.60s\n",
                     tit.plug[btl_plug_i].args);
            btl_trace(tb);
        }
        if (!strcmp(tit.plug[btl_plug_i].name, "GabText")) {


            char gbuf[160];
            struct GabCap gcap;
            FhEscCtx gctx;
            FhEscCb gcb;
            gcap.dst = gbuf;
            gcap.cap = sizeof(gbuf);
            gcap.len = 0;
            gbuf[0] = 0;
            gctx.vars = tit.var;
            gctx.nvars = FH_MAX_VARS;
            gctx.actor_names = btl_actor_names;
            gctx.nactors = 40;
            gctx.party = tit.party;
            gctx.party_n = tit.party_n;
            gctx.currency = NULL;
            gcb.on_text = &gab_text_cb;
            gcb.on_code = &gab_code_cb;
            gcb.ud = &gcap;
            fh_decode_escapes(tit.plug[btl_plug_i].args, &gctx, &gcb);
            btl_log_push(gbuf);
        } else if (!strcmp(tit.plug[btl_plug_i].name, "ShowGab")) {

        } else if (!strcmp(tit.plug[btl_plug_i].name, "ClearGab")) {
            btl_log[0][0] = 0;
            btl_log[1][0] = 0;
            btl_log_t[0] = btl_log_t[1] = 0;
        }
        btl_plug_i++;
    }


    while (btl_anim_i < tit.anim_n) {
        int ch = tit.anims[btl_anim_i].ch;
        if (ch <= -100) {
            int idx = -100 - ch;
            if (idx >= 0 && idx < BT_MAX_FOES)
                battle_anim_start(tit.anims[btl_anim_i].anim, idx,
                                  tit.anims[btl_anim_i].mirror);
        }
        btl_anim_i++;
    }


    if (tit.battle.pending == 2) {
        tit.battle.pending = 0;
        btl.over = 3;
        btl_log[0][0] = 0;
        btl_log[1][0] = 0;
        btl_log_t[0] = btl_log_t[1] = 0;
        btl_phase = 3;
    }
    btl_check_end();
}


static int btl_ev_scan(int isTurnEnd) {
    BtActorHp ac;
    ac.id = btl.f[0].ref;
    ac.hp = btl.f[0].hp;
    ac.maxhp = btl.f[0].maxhp;
    for (int i = 0; i < btl_npages; i++) {
        if (btl_ev_flags[i]) continue;
        if (!bt_page_fire(&btl_cond[i], btl_turn_count, isTurnEnd,
                          &btl.f[1], btl.n_foes, &ac, 1, mit.sw))
            continue;
        return i;
    }
    return -1;
}


static void btl_ce_begin(int ceid) {
    int k = -1;
    for (int i = 0; i < CEB_N; i++) {
        if (CEB_IDS[i] == ceid) {
            k = i;
            break;
        }
    }
    if (k < 0) return;
    {
        char tb[48];
        snprintf(tb, sizeof(tb), "ce%d turn=%d phase=%d\n", ceid,
                 btl_turn_count, btl_phase);
        btl_trace(tb);
    }
    fh_interp_init(&tit, CEB_LIST[k], CEB_LEN[k]);
    tit.rng = btl_rng ? btl_rng : 1u;
    btl_ev_seed();
    tit.ce_ids = CEB_IDS;
    tit.ce_lists = CEB_LIST;
    tit.ce_lens = CEB_LEN;
    tit.ce_n = CEB_N;
    msg_content_changed();
    btl_ev_active = 1;
    btl_ev_ended = 0;
    btl_ev_cursor = 0;
    btl_ev_pagewait = 0;
    btl_plug_i = 0;
    btl_anim_i = 0;
}

static void btl_ev_begin(int i) {
    char tb[48];
    snprintf(tb, sizeof(tb), "pg%d turn=%d phase=%d\n", i, btl_turn_count,
             btl_phase);
    btl_trace(tb);
    fh_interp_init(&tit, btl_lists[i], btl_lens[i]);
    tit.rng = btl_rng ? btl_rng : 1u;
    btl_ev_seed();
    if (btl_cond[i].span <= 1) btl_ev_flags[i] = 1;
    msg_content_changed();
    btl_ev_active = 1;
    btl_ev_ended = 0;
    btl_ev_cursor = 0;
    btl_ev_pagewait = 0;
    btl_plug_i = 0;
    btl_anim_i = 0;
}


static void btl_ev_advance(void) {
    int guard = 10000;
    while (guard-- > 0) {
        int r = fh_interp_step(&tit);
        btl_ev_sync();
        if (r == FH_RUN_PAGE) {
            btl_ev_pagewait = 1;
            break;
        }
        if (r == FH_RUN_CHOICE) break;
        if (r == FH_RUN_END) {
            btl_ev_ended = 1;
            break;
        }
        if (r == FH_RUN_WAIT) break;
    }
}


static void btl_next_turn(void) {
    bt_round_end(&btl);
    for (int i = 0; i < btl_npages; i++)
        if (btl_cond[i].span == 1) btl_ev_flags[i] = 0;
    btl_turn_count++;
    btl.turn++;
    btl_motion(0, 1, 1);
}


static BtSkill btl_force_skill;
static BtIns btl_force_prog[24];

static const BtSkill *btl_skill_any(int id) {
    for (int i = 0; i < 4; i++)
        if (btl_skills[i].id == id) return &btl_skills[i];
    extern unsigned char d_form_start[];
    const unsigned char *raw = NULL;
    int n = bt_prog_find(d_form_start, 0, id, &raw);
    btl_force_skill.id = id;
    btl_force_skill.prog = btl_force_prog;
    btl_force_skill.nins = 0;
    if (raw && n > 0 && n <= 24) {
        for (int k = 0; k < n; k++)
            bt_ins_get(raw + (unsigned)k * 10u, &btl_force_prog[k]);
        btl_force_skill.nins = n;
    }
    for (int k = 0; SKILL_DB[k].id; k++) {
        if (SKILL_DB[k].id == id) {
            btl_force_skill.hit_type = SKILL_DB[k].hit;
            btl_force_skill.scope = SKILL_DB[k].scope;
            btl_force_skill.speed = SKILL_DB[k].speed;
            btl_force_skill.variance = SKILL_DB[k].var;
            btl_force_skill.element = SKILL_DB[k].elem;
            btl_force_skill.dmg_type = SKILL_DB[k].type;
            btl_force_skill.crit = SKILL_DB[k].crit;
            btl_force_skill.success = SKILL_DB[k].succ;
            return &btl_force_skill;
        }
    }
    return btl_skill(1);
}


static BtSkill btl_itemsk;
static BtIns btl_itemprog[24];

static const BtSkill *btl_item_skill(int id) {
    int r = -1;
    for (int k = 0; ITEM_DB[k].id; k++) {
        if (ITEM_DB[k].id == id) {
            r = k;
            break;
        }
    }
    if (r < 0) return NULL;
    extern unsigned char d_form_start[];
    const unsigned char *raw = NULL;
    int n = bt_prog_find(d_form_start, 1, id, &raw);
    btl_itemsk.id = id;
    btl_itemsk.hit_type = ITEM_DB[r].hit;
    btl_itemsk.scope = ITEM_DB[r].scope;
    btl_itemsk.speed = ITEM_DB[r].speed;
    btl_itemsk.variance = ITEM_DB[r].var;
    btl_itemsk.element = ITEM_DB[r].elem;
    btl_itemsk.dmg_type = ITEM_DB[r].type;
    btl_itemsk.crit = 0;
    btl_itemsk.success = ITEM_DB[r].succ;
    btl_itemsk.prog = btl_itemprog;
    btl_itemsk.nins = 0;
    if (raw && n > 0 && n <= 24) {
        for (int k = 0; k < n; k++)
            bt_ins_get(raw + (unsigned)k * 10u, &btl_itemprog[k]);
        btl_itemsk.nins = n;
    }
    return &btl_itemsk;
}


static int btl_fx(int is_item, int id, BtFx *out, int cap) {
    int n = 0;
    if (!is_item) {
        for (int k = 0; SKILL_FX[k].id && n < cap; k++) {
            if (SKILL_FX[k].id != id) continue;
            out[n].code = SKILL_FX[k].code;
            out[n].data = SKILL_FX[k].data;
            out[n].v1 = SKILL_FX[k].v1;
            out[n].v2 = SKILL_FX[k].v2;
            n++;
        }
    } else {
        for (int k = 0; ITEM_FX[k].id && n < cap; k++) {
            if (ITEM_FX[k].id != id) continue;
            out[n].code = ITEM_FX[k].code;
            out[n].data = ITEM_FX[k].data;
            out[n].v1 = ITEM_FX[k].v1;
            out[n].v2 = ITEM_FX[k].v2;
            n++;
        }
    }
    return n;
}


static int btl_skillrow(int id) {
    for (int k = 0; SKILL_DB[k].id; k++)
        if (SKILL_DB[k].id == id) return k;
    return -1;
}


static int btl_known[24], btl_nknown;

static void btl_known_add(int sid) {
    int dup = 0;
    for (int i = 0; i < btl_nknown; i++)
        if (btl_known[i] == sid) dup = 1;
    if (dup || btl_nknown >= 24) return;
    int sr = btl_skillrow(sid);
    if (sr >= 0 && (SKILL_DB[sr].occ == 0 || SKILL_DB[sr].occ == 1))
        btl_known[btl_nknown++] = sid;
}


static int btl_item_ids[64], btl_nitems;
static int btl_list_cur, btl_list_top;

static void btl_item_rebuild(void) {
    btl_nitems = 0;
    for (int k = 0; ITEM_DB[k].id && btl_nitems < 64; k++) {
        int id = ITEM_DB[k].id;
        if (ITEM_DB[k].occ != 0 && ITEM_DB[k].occ != 1) continue;
        if (id < 0 || id >= FH_MAX_ITEMS) continue;
        if (mit.inv_item[id] > 0) btl_item_ids[btl_nitems++] = id;
    }
}

static void btl_known_rebuild(void) {
    btl_nknown = 0;
    int aid = btl.f[0].ref, cls = 0, lv = 1;


    btl_known_add(40);
    btl_known_add(11);
    for (int r = 0; btl_seeds[r].id; r++) {
        if (btl_seeds[r].id == aid) {
            cls = btl_seeds[r].cls;
            break;
        }
    }
    for (int r = 0; ACTOR_DB[r].id; r++) {
        if (ACTOR_DB[r].id == aid) {
            lv = ACTOR_DB[r].level;
            break;
        }
    }
    for (int k = 0; CLASS_LEARN[k].cls; k++) {
        if (CLASS_LEARN[k].cls != cls || CLASS_LEARN[k].lv > lv) continue;
        btl_known_add(CLASS_LEARN[k].sk);
    }
    if (aid < 0 || aid >= FH_MAX_ACTORS) return;
    for (int s = 1; s < FH_MAX_SKILLS; s++) {
        if (!mit.askill[aid][s] && !tit.askill[aid][s]) continue;
        btl_known_add(s);
    }
}

static void btl_start(u64 tick, int char_idx, int troop_id) {

    static const int char_actor[4] = {1, 5, 4, 3};
    int want = char_actor[char_idx];
    int row = 0;
    for (; ACTOR_DB[row].id; row++)
        if (ACTOR_DB[row].id == want) break;
    btl_actor_row = row;


    btl_troop_id = troop_id;
    if (troop_id == 44) {
        btl_cond = TROOP44_COND;
        btl_lists = TROOP44_LIST;
        btl_lens = TROOP44_LEN;
        btl_npages = TROOP44_NPAGES;
        btl_ce_ids = TROOP44_CE_IDS;
        btl_ce_lists = TROOP44_CE_LIST;
        btl_ce_lens = TROOP44_CE_LEN;
        btl_ce_n = TROOP44_CE_N;
        btl_seeds = (const BtlActorSeed *)TROOP44_ACTORS;
    } else {
        btl_troop_id = 1;
        btl_cond = TROOP1_COND;
        btl_lists = TROOP1_LIST;
        btl_lens = TROOP1_LEN;
        btl_npages = TROOP1_NPAGES;
        btl_ce_ids = TROOP1_CE_IDS;
        btl_ce_lists = TROOP1_CE_LIST;
        btl_ce_lens = TROOP1_CE_LEN;
        btl_ce_n = TROOP1_CE_N;
        btl_seeds = (const BtlActorSeed *)TROOP1_ACTORS;
    }
    memset(&btl, 0, sizeof(btl));
    BtF *a = &btl.f[0];
    a->is_foe = 0;
    a->ref = want;
    a->maxhp = a->hp = ACTOR_DB[row].mhp;
    a->maxmp = a->mp = ACTOR_DB[row].mmp;
    a->atk = ACTOR_DB[row].atk;
    a->def = ACTOR_DB[row].def;
    a->mat = ACTOR_DB[row].mat;
    a->mdf = ACTOR_DB[row].mdf;
    a->agi = ACTOR_DB[row].agi;
    a->luk = ACTOR_DB[row].luk;
    a->hit = ACTOR_DB[row].hit;
    a->eva = ACTOR_DB[row].eva;
    a->cri = ACTOR_DB[row].cri;
    a->cev = 0.0;
    a->pdr = a->mdr = a->grd = 1.0;
    for (int i = 0; i < BT_ERATE_N; i++) a->erate[i] = ACTOR_DB[row].er[i];
    a->atk_elem = ACTOR_DB[row].elem;
    a->level = ACTOR_DB[row].level;
    a->exp_cur = ACTOR_DB[row].exp;
    a->alive = 1;
    btl.n_party = 1;

    btl.n_foes = 0;
    int tagi = 0;
    for (int k = 0; TROOP_MB[k].troop; k++) {
        if (TROOP_MB[k].troop != btl_troop_id) continue;
        if (btl.n_foes >= BT_MAX_FOES) break;
        int id = TROOP_MB[k].foe, r = 0;
        for (; FOE_DB[r].id; r++)
            if (FOE_DB[r].id == id) break;
        if (!FOE_DB[r].id) continue;
        BtF *f = &btl.f[1 + btl.n_foes];
        f->is_foe = 1;
        f->ref = id;
        f->maxhp = f->hp = FOE_DB[r].mhp;
        f->atk = FOE_DB[r].atk;
        f->def = FOE_DB[r].def;
        f->mat = FOE_DB[r].mat;
        f->mdf = FOE_DB[r].mdf;
        f->agi = FOE_DB[r].agi;
        f->luk = FOE_DB[r].luk;
        f->hit = FOE_DB[r].hit;
        f->eva = FOE_DB[r].eva;
        f->cri = 0.0;
        f->cev = 0.0;
        f->pdr = f->mdr = f->grd = 1.0;
        for (int e = 0; e < BT_ERATE_N; e++) f->erate[e] = FOE_DB[r].er[e];
        f->atk_elem = FOE_DB[r].elem;
        f->alive = 1;
        tagi += f->agi;
        btl.n_foes++;
    }
    btl.turn = 1;
    bt_srand(&btl, (unsigned)(tick & 0xffffffffu));
    btl_rng = (unsigned)(tick & 0xffffffffu);
    if (!btl_rng) btl_rng = 1u;


    mit.var[4] = 1 + (int)bt_randn(&btl, 4);
    mit.sw[3155] = 1;
    {
        int tavg = btl.n_foes > 0 ? tagi / btl.n_foes : 0;
        if (tavg <= 0) tavg = 1;
        btl.escape_ratio = 0.5 * (double)a->agi / (double)tavg;
    }
    btl_phase = 0;
    btl_cmd = 0;
    btl_tgt = 0;
    btl_log[0][0] = btl_log[1][0] = 0;
    btl_log_t[0] = btl_log_t[1] = 0;
    for (int i = 0; i < 8; i++) {
        btl_flash[i] = 0;
        btl_collapse[i] = 0;
    }
    btl_motion(0, 1, 1);
    btl_pose_t = 0;
    for (int i = 0; i < 8; i++) btl_pops[i].ttl = 0;
    battle_anim_reset();


    btl_ev_active = 0;
    for (int i = 0; i < 16; i++) btl_ev_flags[i] = 0;
    btl_turn_count = 0;
    for (int i = 0; i < 8; i++) btl_forced[i] = -1;
    for (int i = 0; i < 8; i++) btl_foe_skill[i] = -1;
    btl_nresCE = 0;
    btl_ce_run = 0;
    btl_run_pre = 0;
    btl_run_failed = 0;
    btl_act_kind = 0;
    btl_act_id = 0;
    btl_act_target = 0;
    btl_known_rebuild();
    {
        SceUID fd = sceIoOpen("ms0:/fh_battle.txt",
                              PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd >= 0) {
            char tb[48];
            snprintf(tb, sizeof(tb), "battle troop=%d\n", btl_troop_id);
            sceIoWrite(fd, tb, strlen(tb));
            sceIoClose(fd);
        }
    }
    battle_mode = 1;
    {
        int pg = btl_ev_scan(0);
        if (pg >= 0) btl_ev_begin(pg);
    }
}


static void btl_foe_xy(int i, float *x, float *y) {
    int seen = -1;
    for (int k = 0; TROOP_MB[k].troop; k++) {
        if (TROOP_MB[k].troop != btl_troop_id) continue;
        if (++seen == i) {
            *x = 240.0f + ((float)TROOP_MB[k].x - 408.0f) * 0.5f;
            *y = (float)TROOP_MB[k].y * 0.5f - 24.0f;
            return;
        }
    }
    *x = SCR_W / 2;
    *y = SCR_H / 2;
}


static void btl_clamp_tgt(void) {
    if (btl_tgt < 0 || btl_tgt >= btl.n_foes ||
        !btl.f[1 + btl_tgt].alive) {
        btl_tgt = 0;
        while (btl_tgt < btl.n_foes && !btl.f[1 + btl_tgt].alive)
            btl_tgt++;
        if (btl_tgt >= btl.n_foes) btl_tgt = 0;
    }
    if (btl_act_target < 0 || btl_act_target >= btl.n_foes ||
        !btl.f[1 + btl_act_target].alive)
        btl_act_target = btl_tgt;
}


static void btl_begin_exec(void) {
    btl_resolve_skills();
    int spd[BT_MAX_PARTY + BT_MAX_FOES] = {0};

    if (btl_act_kind == 1) {
        btl.f[0].guard = 1;
        btl_motion(0, 3, 1);
        spd[0] = 2000;
    } else if (btl_act_kind == 3) {
        int r = btl_skillrow(btl_act_id);
        if (r >= 0) spd[0] = SKILL_DB[r].speed;
    } else if (btl_act_kind == 4) {
        for (int k = 0; ITEM_DB[k].id; k++) {
            if (ITEM_DB[k].id == btl_act_id) {
                spd[0] = ITEM_DB[k].speed;
                break;
            }
        }
    }


    for (int i = 0; i < btl.n_foes; i++) {
        btl_foe_skill[i] = -1;
        spd[1 + i] = 0;
        if (!btl.f[1 + i].alive) continue;
        if (btl_forced[i] >= 0) {
            btl_foe_skill[i] = btl_forced[i];
            btl_forced[i] = -1;
        } else {
            BtF *sub = &btl.f[1 + i];
            static BtAiAct tab[8];
            int ntab = 0;
            for (int k = 0; FOE_AI[k].foe; k++) {
                if (FOE_AI[k].foe == sub->ref && ntab < 8) {
                    tab[ntab].skill = FOE_AI[k].skill;
                    tab[ntab].rating = FOE_AI[k].rating;
                    tab[ntab].ctype = FOE_AI[k].ctype;
                    tab[ntab].cp1 = FOE_AI[k].cp1;
                    tab[ntab].cp2 = FOE_AI[k].cp2;
                    ntab++;
                }
            }
            if (ntab > 0) {
                int pick = bt_ai_pick(&btl, tab, ntab, 2, btl.turn, mit.sw);
                if (pick >= 0) btl_foe_skill[i] = tab[pick].skill;
            }
        }
        if (btl_foe_skill[i] >= 0) {
            const BtSkill *sk = btl_skill_any(btl_foe_skill[i]);
            if (sk) spd[1 + i] = sk->speed;
        }
    }
    btl_norder = bt_order_act(&btl, spd, btl_order, 12);
    btl_oi = 0;
    btl_wait = 20;
    btl_phase = 2;
    {
        char ab[96];
        snprintf(ab, sizeof(ab), "order n=%d turn=%d kind=%d id=%d\n",
                 btl_norder, btl.turn, btl_act_kind, btl_act_id);
        btl_trace(ab);
    }
}


static void btl_pop_at(int x, int y, int value, int kind) {
    for (int i = 0; i < 8; i++) {
        if (btl_pops[i].ttl <= 0) {
            btl_pops[i].x = x;
            btl_pops[i].y = y;
            btl_pops[i].value = value;
            btl_pops[i].kind = kind;
            btl_pops[i].ttl = btl_pops[i].max = 45;
            return;
        }
    }
}


static void btl_log_name(char *dst, size_t n, BtF *f) {
    if (f == &btl.f[0]) {
        snprintf(dst, n, "%s", btl_actor_name);
        return;
    }
    int r = 0;
    while (FOE_DB[r].id && FOE_DB[r].id != f->ref) r++;
    snprintf(dst, n, "%s", FOE_DB[r].id ? FOE_DB[r].name : "?");
}


static void btl_do_use(BtF *sub, const BtSkill *sk, const BtFx *fx, int nfx,
                       int is_item, int repeats, BtF **tgts, int ntgt,
                       int foe_side) {

    for (int k = 0; k < nfx; k++) {
        if (fx[k].code == 44 && btl_nresCE < 8)
            btl_resCE[btl_nresCE++] = fx[k].data;
    }
    int use_logged = 0;
    int anim_played = 0;
    for (int r = 0; r < repeats; r++) {
        for (int t = 0; t < ntgt; t++) {
            BtF *tgt = tgts[t];
            if (!tgt->alive) continue;

            if (!use_logged) {
                use_logged = 1;
                char ubuf[96];
                if (!is_item) {
                    int sr = btl_skillrow(sk->id);
                    snprintf(ubuf, sizeof(ubuf), "%s",
                             (sr >= 0) ? SKILL_DB[sr].nm : "?");
                } else {
                    int ir = -1;
                    for (int k = 0; ITEM_DB[k].id; k++) {
                        if (ITEM_DB[k].id == sk->id) {
                            ir = k;
                            break;
                        }
                    }
                    snprintf(ubuf, sizeof(ubuf), "%s",
                             ir >= 0 ? ITEM_DB[ir].nm : "?");
                }
                btl_log_push(ubuf);
            }
            int hp0 = tgt->hp, mp0 = tgt->mp;
            int crit, missed, evaded;
            int dmg = bt_strike(&btl, sk, sub, tgt, NULL, NULL, &crit,
                                &missed, &evaded);
            int hit = !missed && !evaded;
            if (hit)
                bt_apply_fx(&btl, sub, tgt, fx, nfx, sk->hit_type == 0,
                            is_item, 1.0, btl_resCE, &btl_nresCE);

            if (foe_side && tgt == &btl.f[0] && hit && dmg > 0 &&
                (sk->dmg_type == 1 || sk->dmg_type == 2 ||
                 sk->dmg_type == 5 || sk->dmg_type == 6)) {
                btl_motion(0, 4, 0);
                btl_pose_t = 24;
            }

            {
                char tname[40], rbuf[96];
                btl_log_name(tname, sizeof(tname), tgt);
                int is_actor_tgt = (tgt == &btl.f[0]);
                if (missed || evaded) {
                    snprintf(rbuf, sizeof(rbuf), "Miss! %s took no damage!",
                             tname);
                    btl_log_push(rbuf);
                } else if (sk->dmg_type == 1 || sk->dmg_type == 2 ||
                           sk->dmg_type == 5 || sk->dmg_type == 6) {
                    if (crit)
                        btl_log_push(is_actor_tgt ? "A painful blow!!"
                                                  : "An excellent hit!!");
                } else if (sk->dmg_type == 3 || sk->dmg_type == 4) {
                    if (dmg > 0) {
                        snprintf(rbuf, sizeof(rbuf), "%s recovered %d %s!",
                                 tname, dmg,
                                 sk->dmg_type == 3 ? "Body" : "Mind");
                        btl_log_push(rbuf);
                    }
                } else {

                    int rec = (tgt->hp - hp0) + (tgt->mp - mp0);
                    if (rec > 0) {
                        char tname[40], rbuf[96];
                        btl_log_name(tname, sizeof(tname), tgt);
                        snprintf(rbuf, sizeof(rbuf), "%s recovered %d %s!",
                                 tname, rec,
                                 (tgt->hp - hp0) >= (tgt->mp - mp0) ? "Body"
                                                                    : "Mind");
                        btl_log_push(rbuf);
                    }
                }
            }


            if (!foe_side && tgt != sub) {
                int mi = (int)(tgt - &btl.f[1]);
                if (mi >= 0 && mi < 8) {

                    if (hit && sk->dmg_type > 0 && sk->dmg_type != 3 &&
                        sk->dmg_type != 4)
                        btl_flash[mi] = 10;
                    if (!tgt->alive && btl_collapse[mi] == 0)
                        btl_collapse[mi] = 32;
                }
            }


            int px = 240, py = 200;
            if (tgt == &btl.f[0]) {
                px = 156;
                py = 70;
            } else if (!foe_side) {
                float fx_, fy_;
                btl_foe_xy((int)(tgt - &btl.f[1]), &fx_, &fy_);
                px = (int)fx_;
                py = (int)fy_ - 60;
            }
            if (missed || evaded) {
                btl_pop_at(px, py, 0, 1);
            } else if (sk->dmg_type == 3 || sk->dmg_type == 4) {
                if (dmg > 0) btl_pop_at(px, py, dmg, 3);
            } else {
                int rec = (tgt->hp - hp0) + (tgt->mp - mp0);
                if (rec > 0) btl_pop_at(px, py, rec, 3);
            }

            {
                char ab[128], tnm[32];
                if (!foe_side && tgt != sub) {
                    int r2 = 0;
                    while (FOE_DB[r2].id && FOE_DB[r2].id != tgt->ref) r2++;
                    snprintf(tnm, sizeof(tnm), "%s",
                             FOE_DB[r2].id ? FOE_DB[r2].name : "?");
                } else if (foe_side && tgt == &btl.f[0]) {
                    snprintf(tnm, sizeof(tnm), "actor");
                } else {
                    snprintf(tnm, sizeof(tnm), "self");
                }
                snprintf(ab, sizeof(ab),
                         "strike sub=%s sk=%d tgt=%s dmg=%d %s%s hp=%d/%d\n",
                         foe_side ? "foe" : "actor", sk->id, tnm, dmg,
                         missed ? "MISS " : "", evaded ? "EVA " : "",
                         tgt->hp, tgt->maxhp);
                btl_trace(ab);
            }


            if (!anim_played && !is_item) {
                anim_played = 1;
                int sr = btl_skillrow(sk->id);
                if (sr >= 0 && SKILL_DB[sr].anim > 0) {
                    int fi = -1;
                    if (!foe_side && tgt != sub)
                        fi = (int)(tgt - &btl.f[1]);
                    else if (foe_side)
                        fi = -2;
                    battle_anim_start(SKILL_DB[sr].anim, fi, 0);
                }
            }
        }
    }
}


static int btl_exec_step(void) {
    if (btl_oi >= btl_norder) return 1;
    int fi = btl_order[btl_oi++];
    BtF *sub = &btl.f[fi];
    if (!sub->alive) return 0;
    if (!sub->is_foe) {


        if (btl_act_kind == 5) return 0;


        const BtSkill *sk = NULL;
        BtFx fx[8];
        int nfx = 0, repeats = 1, is_item = 0, scope = 1;
        if (btl_act_kind == 0) {
            sk = btl_skill(1);
            nfx = btl_fx(0, 1, fx, 8);
            repeats = 3;


            if (btl.f[0].ref == 5)
                btl_motion(3, 2, 0);
            else
                btl_motion(3, 1, 0);
            btl_pose_t = 30;
        } else if (btl_act_kind == 1) {
            sk = btl_skill_any(2);
            nfx = btl_fx(0, 2, fx, 8);
        } else if (btl_act_kind == 3) {
            sk = btl_skill_any(btl_act_id);
            nfx = btl_fx(0, btl_act_id, fx, 8);
            int r = btl_skillrow(btl_act_id);
            if (r >= 0) {
                repeats = SKILL_DB[r].reps;
                if (repeats < 1) repeats = 1;
                if (btl.f[0].mp >= SKILL_DB[r].mp) btl.f[0].mp -= SKILL_DB[r].mp;
            }
            btl_motion(3, 3, 0);
            btl_pose_t = 30;
        } else if (btl_act_kind == 4) {
            sk = btl_item_skill(btl_act_id);
            if (!sk) return 0;
            is_item = 1;
            nfx = btl_fx(1, btl_act_id, fx, 8);
            for (int k = 0; ITEM_DB[k].id; k++) {
                if (ITEM_DB[k].id == btl_act_id) {
                    repeats = ITEM_DB[k].reps;
                    if (repeats < 1) repeats = 1;
                    if (ITEM_DB[k].cons && mit.inv_item[btl_act_id] > 0)
                        mit.inv_item[btl_act_id]--;
                    break;
                }
            }
            btl_motion(3, 5, 0);
            btl_pose_t = 30;
        }
        if (!sk) return 0;
        scope = sk->scope;


        btl_clamp_tgt();
        BtF *tgts[8];
        int ntgt = 0;
        if (scope == 1) {
            BtF *t = &btl.f[1 + btl_act_target];
            if (t->alive) tgts[ntgt++] = t;
        } else if (scope == 2) {
            for (int i = 0; i < btl.n_foes && ntgt < 8; i++)
                if (btl.f[1 + i].alive) tgts[ntgt++] = &btl.f[1 + i];
        } else if (scope == 3 || scope == 4) {
            for (int t = 0; t < scope - 2 && ntgt < 8; t++) {
                int alive[8], na = 0;
                for (int i = 0; i < btl.n_foes; i++)
                    if (btl.f[1 + i].alive) alive[na++] = i;
                if (!na) break;
                tgts[ntgt++] = &btl.f[1 + alive[bt_randn(&btl, (unsigned)na)]];
            }
        } else if (scope == 0) {
            ntgt = 0;
        } else {
            tgts[ntgt++] = sub;
        }
        btl_do_use(sub, sk, fx, nfx, is_item, repeats, tgts, ntgt, 0);
    } else {

        int foe_idx = fi - 1;
        int skill = (foe_idx >= 0 && foe_idx < 8) ? btl_foe_skill[foe_idx] : -1;
        if (skill < 0) return 0;
        const BtSkill *sk = btl_skill_any(skill);
        if (!sk) return 0;
        BtFx fx[8];
        int nfx = btl_fx(0, skill, fx, 8);
        int repeats = 1;
        {
            int r = btl_skillrow(skill);
            if (r >= 0) {
                repeats = SKILL_DB[r].reps;
                if (repeats < 1) repeats = 1;
            }
        }


        BtF *tgts[8];
        int ntgt = 0;
        if (sk->scope == 7 || sk->scope == 8 || sk->scope == 11 ||
            sk->scope == 0)
            tgts[ntgt++] = sub;
        else
            tgts[ntgt++] = &btl.f[0];
        btl_do_use(sub, sk, fx, nfx, 0, repeats, tgts, ntgt, 1);
    }

    btl_check_end();
    return 0;
}


static int exit_request = 0;

static int exitCallback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    exit_request = 1;
    return 0;
}

static int callbackThread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupExitCallback(void) {
    int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}


#define MAP_W 145
#define MAP_H 105

static unsigned int gu_list[262144] __attribute__((aligned(16)));
static uint16_t map_layers[4][MAP_H][MAP_W];
static uint8_t map_passability[MAP_H][MAP_W];
static uint8_t map_higher[1664];
static uint8_t npc_solid[MAP_W * MAP_H];
static Player player;
static int current_character = 0;

typedef struct {
    const char *name;
    unsigned char *sprite_data;
    unsigned char *clut_data;
    unsigned char *torch_data;
    unsigned char *torch_clut;
} CharacterDef;

static CharacterDef characters[4];


extern unsigned char d_a1_start[], d_b_start[], d_e_start[], d_ib_start[], d_d_start[];
extern unsigned char d_a1c_start[], d_bc_start[], d_ec_start[], d_ibc_start[], d_dc_start[];
extern unsigned char d_layers_start[], d_pass_start[], d_high_start[];
extern unsigned char d_merc_start[], d_mercc_start[];
extern unsigned char d_outl_start[], d_outlc_start[];
extern unsigned char d_priest_start[], d_priestc_start[];
extern unsigned char d_knight_start[], d_knightc_start[];
extern unsigned char d_flame_start[], d_flamec_start[];
extern unsigned char d_creat_start[], d_creatc_start[];
extern unsigned char d_mobj_start[], d_mobjc_start[];
extern unsigned char d_ghost_start[], d_ghostc_start[];
extern unsigned char d_guard1_start[], d_guard1c_start[];
extern unsigned char d_font_start[], d_fontc_start[], d_fontadv_start[];
extern unsigned char d_win_start[], d_winc_start[];
extern unsigned char d_floor_start[], d_floorc_start[];
extern unsigned char d_tmerc_start[], d_tmercc_start[];
extern unsigned char d_toutl_start[], d_toutlc_start[];
extern unsigned char d_tpriest_start[], d_tpriestc_start[];
extern unsigned char d_tknight_start[], d_tknightc_start[];
extern unsigned char d_e0_start[], d_e0c_start[];
extern unsigned char d_e1_start[], d_e1c_start[];
extern unsigned char d_e2_start[], d_e2c_start[];
extern unsigned char d_e3_start[], d_e3c_start[];
extern unsigned char d_e4_start[], d_e4c_start[];
extern unsigned char d_e5_start[], d_e5c_start[];
extern unsigned char d_e6_start[], d_e6c_start[];
extern unsigned char d_b0_start[], d_b0c_start[];
extern unsigned char d_b1_start[], d_b1c_start[];
extern unsigned char d_b2_start[], d_b2c_start[];
extern unsigned char d_b3_start[], d_b3c_start[];
extern unsigned char d_b4_start[], d_b4c_start[];
extern unsigned char d_b5_start[], d_b5c_start[];
extern unsigned char d_an0_start[], d_an0c_start[];
extern unsigned char d_an1_start[], d_an1c_start[];
extern unsigned char d_an2_start[], d_an2c_start[];
extern unsigned char d_an3_start[], d_an3c_start[];
extern unsigned char d_an4_start[], d_an4c_start[];
extern unsigned char d_an5_start[], d_an5c_start[];
extern unsigned char d_an6_start[], d_an6c_start[];
extern unsigned char d_an7_start[], d_an7c_start[];
extern unsigned char d_ico_start[], d_icoc_start[];
extern unsigned char d_form_start[];
extern unsigned char d_bv0a_start[], d_bv0ac_start[];
extern unsigned char d_bv0b_start[], d_bv0bc_start[];
extern unsigned char d_bv1a_start[], d_bv1ac_start[];
extern unsigned char d_bv1b_start[], d_bv1bc_start[];
extern unsigned char d_bv2a_start[], d_bv2ac_start[];
extern unsigned char d_bv2b_start[], d_bv2bc_start[];
extern unsigned char d_bv3a_start[], d_bv3ac_start[];
extern unsigned char d_bv3b_start[], d_bv3bc_start[];


static unsigned char *foe_t8[7];
static unsigned int *foe_cl[7];


static unsigned char *bal_t8[6];
static unsigned int *bal_cl[6];


unsigned char *anim_t8[8];
unsigned int anim_cl[8][256] __attribute__((aligned(16)));
unsigned char *icon_t8;
unsigned int icon_cl[256] __attribute__((aligned(16)));


static void btl_foe_art(int enemy_id, unsigned char **t8, unsigned int **cl) {
    if (btl_troop_id == 44) {
        static const int ids[6] = {112, 113, 114, 115, 116, 117};
        for (int s = 0; s < 6; s++) {
            if (enemy_id == ids[s]) {
                *t8 = bal_t8[s];
                *cl = bal_cl[s];
                return;
            }
        }
        if (enemy_id == 502) { *t8 = bal_t8[0]; *cl = bal_cl[0]; return; }
        if (enemy_id == 503) { *t8 = bal_t8[2]; *cl = bal_cl[2]; return; }
    } else {
        static const int ids[7] = {1, 2, 3, 4, 5, 6, 7};
        static const int alt[7] = {486, 0, 484, 485, 0, 0, 483};
        for (int s = 0; s < 7; s++) {
            if (enemy_id == ids[s] || (alt[s] && enemy_id == alt[s])) {
                *t8 = foe_t8[s];
                *cl = foe_cl[s];
                return;
            }
        }
    }
    *t8 = NULL;
    *cl = NULL;
}


static unsigned char *bv_t8a[4], *bv_t8b[4];
static unsigned int *bv_cla[4], *bv_clb[4];


static struct { unsigned char *t8, *clut; int iw, ih, tw, th, stride, big; } npc_sheets[5];


typedef struct {
    int ev_id, sheet, tx, ty, index, pattern, dir_mv, trig, prio, dirfix;
    const FhCmd *talk;
    int talk_len;

    int alt_sw[2];
    const FhCmd *alt_talk[2];
    int alt_len[2];


    int battle_troop;
} NpcDef;

static NpcDef npcs[15];
static NpcSprite npc_draw[15];

static void load_map030(void) {

    sheet_px[4] = d_a1_start; sheet_px[5] = d_b_start; sheet_px[6] = d_e_start;
    sheet_px[7] = d_ib_start; sheet_px[8] = d_d_start;
    memcpy(sheet_cl[4], d_a1c_start, sizeof(sheet_cl[4]));
    memcpy(sheet_cl[5], d_bc_start, sizeof(sheet_cl[5]));
    memcpy(sheet_cl[6], d_ec_start, sizeof(sheet_cl[6]));
    memcpy(sheet_cl[7], d_ibc_start, sizeof(sheet_cl[7]));
    memcpy(sheet_cl[8], d_dc_start, sizeof(sheet_cl[8]));


    memcpy(map_layers, d_layers_start, sizeof(map_layers));
    memcpy(map_passability, d_pass_start, sizeof(map_passability));
    memcpy(map_higher, d_high_start, sizeof(map_higher));


    characters[0].name = "Mercenary";
    characters[0].sprite_data = d_merc_start;
    characters[0].clut_data = d_mercc_start;

    characters[1].name = "Outlander";
    characters[1].sprite_data = d_outl_start;
    characters[1].clut_data = d_outlc_start;

    characters[2].name = "Dark Priest";
    characters[2].sprite_data = d_priest_start;
    characters[2].clut_data = d_priestc_start;

    characters[3].name = "Knight";
    characters[3].sprite_data = d_knight_start;
    characters[3].clut_data = d_knightc_start;


    characters[0].torch_data = d_tmerc_start;
    characters[0].torch_clut = d_tmercc_start;
    characters[1].torch_data = d_toutl_start;
    characters[1].torch_clut = d_toutlc_start;
    characters[2].torch_data = d_tpriest_start;
    characters[2].torch_clut = d_tpriestc_start;
    characters[3].torch_data = d_tknight_start;
    characters[3].torch_clut = d_tknightc_start;


    foe_t8[0] = d_e0_start; foe_cl[0] = (unsigned int *)d_e0c_start;
    foe_t8[1] = d_e1_start; foe_cl[1] = (unsigned int *)d_e1c_start;
    foe_t8[2] = d_e2_start; foe_cl[2] = (unsigned int *)d_e2c_start;
    foe_t8[3] = d_e3_start; foe_cl[3] = (unsigned int *)d_e3c_start;
    foe_t8[4] = d_e4_start; foe_cl[4] = (unsigned int *)d_e4c_start;
    foe_t8[5] = d_e5_start; foe_cl[5] = (unsigned int *)d_e5c_start;
    foe_t8[6] = d_e6_start; foe_cl[6] = (unsigned int *)d_e6c_start;

    bal_t8[0] = d_b0_start; bal_cl[0] = (unsigned int *)d_b0c_start;
    bal_t8[1] = d_b1_start; bal_cl[1] = (unsigned int *)d_b1c_start;
    bal_t8[2] = d_b2_start; bal_cl[2] = (unsigned int *)d_b2c_start;
    bal_t8[3] = d_b3_start; bal_cl[3] = (unsigned int *)d_b3c_start;
    bal_t8[4] = d_b4_start; bal_cl[4] = (unsigned int *)d_b4c_start;
    bal_t8[5] = d_b5_start; bal_cl[5] = (unsigned int *)d_b5c_start;


    anim_t8[0] = d_an0_start;
    anim_t8[1] = d_an1_start;
    anim_t8[2] = d_an2_start;
    anim_t8[3] = d_an3_start;
    anim_t8[4] = d_an4_start;
    anim_t8[5] = d_an5_start;
    memcpy(anim_cl[0], d_an0c_start, sizeof(anim_cl[0]));
    memcpy(anim_cl[1], d_an1c_start, sizeof(anim_cl[1]));
    memcpy(anim_cl[2], d_an2c_start, sizeof(anim_cl[2]));
    memcpy(anim_cl[3], d_an3c_start, sizeof(anim_cl[3]));
    memcpy(anim_cl[4], d_an4c_start, sizeof(anim_cl[4]));
    memcpy(anim_cl[5], d_an5c_start, sizeof(anim_cl[5]));
    anim_t8[6] = d_an6_start;
    anim_t8[7] = d_an7_start;
    memcpy(anim_cl[6], d_an6c_start, sizeof(anim_cl[6]));
    memcpy(anim_cl[7], d_an7c_start, sizeof(anim_cl[7]));
    icon_t8 = d_ico_start;
    memcpy(icon_cl, d_icoc_start, sizeof(icon_cl));
    bv_t8a[0] = d_bv0a_start; bv_cla[0] = (unsigned int *)d_bv0ac_start;
    bv_t8b[0] = d_bv0b_start; bv_clb[0] = (unsigned int *)d_bv0bc_start;
    bv_t8a[1] = d_bv1a_start; bv_cla[1] = (unsigned int *)d_bv1ac_start;
    bv_t8b[1] = d_bv1b_start; bv_clb[1] = (unsigned int *)d_bv1bc_start;
    bv_t8a[2] = d_bv2a_start; bv_cla[2] = (unsigned int *)d_bv2ac_start;
    bv_t8b[2] = d_bv2b_start; bv_clb[2] = (unsigned int *)d_bv2bc_start;
    bv_t8a[3] = d_bv3a_start; bv_cla[3] = (unsigned int *)d_bv3ac_start;
    bv_t8b[3] = d_bv3b_start; bv_clb[3] = (unsigned int *)d_bv3bc_start;


    npc_sheets[0].t8 = d_creat_start; npc_sheets[0].clut = d_creatc_start;
    npc_sheets[0].iw = 288; npc_sheets[0].ih = 192;
    npc_sheets[0].tw = 512; npc_sheets[0].th = 256;
    npc_sheets[0].stride = 512; npc_sheets[0].big = 0;
    npc_sheets[1].t8 = d_ghost_start; npc_sheets[1].clut = d_ghostc_start;
    npc_sheets[1].iw = 120; npc_sheets[1].ih = 220;
    npc_sheets[1].tw = 128; npc_sheets[1].th = 256;
    npc_sheets[1].stride = 128; npc_sheets[1].big = 1;
    npc_sheets[2].t8 = d_mobj_start; npc_sheets[2].clut = d_mobjc_start;
    npc_sheets[2].iw = 288; npc_sheets[2].ih = 192;
    npc_sheets[2].tw = 512; npc_sheets[2].th = 256;
    npc_sheets[2].stride = 512; npc_sheets[2].big = 0;
    npc_sheets[3].t8 = d_flame_start; npc_sheets[3].clut = d_flamec_start;
    npc_sheets[3].iw = 288; npc_sheets[3].ih = 192;
    npc_sheets[3].tw = 512; npc_sheets[3].th = 256;
    npc_sheets[3].stride = 512; npc_sheets[3].big = 0;
    npc_sheets[4].t8 = d_guard1_start; npc_sheets[4].clut = d_guard1c_start;
    npc_sheets[4].iw = 480; npc_sheets[4].ih = 440;
    npc_sheets[4].tw = 512; npc_sheets[4].th = 512;
    npc_sheets[4].stride = 512; npc_sheets[4].big = 0;


    static const NpcDef baked[] = {

        {208, 0, 56,11, 1,2,8, 0,1,1, NULL, 0},
        {209, 0, 57,11, 2,0,8, 0,1,1, NULL, 0},
        {213, 0, 58,11, 2,1,8, 0,0,1, M30_EV213, M30_EV213_LEN},
        {210, 0, 56,12, 5,2,2, 0,0,1, NULL, 0},
        {211, 0, 57,12, 6,0,2, 0,0,1, NULL, 0},
        {212, 0, 58,12, 6,1,2, 0,0,1, NULL, 0},
        {185, 1, 82,11, 0,2,8, 4,1,0, NULL, 0},
        {187, 1, 71,21, 0,2,8, 4,1,0, NULL, 0},
        {266, 2,104, 9, 4,0,8, 0,1,1, NULL, 0},
        {267, 2,105, 9, 4,1,8, 0,1,1, NULL, 0},
        {268, 2,105, 8, 4,1,6, 0,1,1, NULL, 0},
        {269, 2,104, 8, 4,0,6, 0,1,1, NULL, 0},
        { 20, 3, 84,20, 5,1,2, 0,1,1, M30_EV020, M30_EV020_LEN},
        { 21, 3, 84,19, 1,1,8, 0,1,1, NULL, 0},


        { 94, 4, 58,23, 0,1,2, 2,1,0, NULL, 0},
    };
    for (int i = 0; i < 15; i++) {
        npcs[i] = baked[i];
        npc_draw[i].t8 = npc_sheets[baked[i].sheet].t8;
        npc_draw[i].clut = (unsigned int *)npc_sheets[baked[i].sheet].clut;
        npc_draw[i].img_w = npc_sheets[baked[i].sheet].iw;
        npc_draw[i].img_h = npc_sheets[baked[i].sheet].ih;
        npc_draw[i].tex_w = npc_sheets[baked[i].sheet].tw;
        npc_draw[i].tex_h = npc_sheets[baked[i].sheet].th;
        npc_draw[i].stride = npc_sheets[baked[i].sheet].stride;
        npc_draw[i].is_big = npc_sheets[baked[i].sheet].big;
        npc_draw[i].tile_x = baked[i].tx;
        npc_draw[i].tile_y = baked[i].ty;
        npc_draw[i].char_index = baked[i].index;
        npc_draw[i].pattern = baked[i].pattern;
        npc_draw[i].dir_mv = baked[i].dir_mv;
        npc_draw[i].prio = baked[i].prio;
    }


    for (int i = 0; i < 15; i++) {
        if (npcs[i].ev_id == 213) {
            npcs[i].alt_sw[0] = 2640;
            npcs[i].alt_talk[0] = M30_EV213_P1;
            npcs[i].alt_len[0] = M30_EV213_P1_LEN;
        }
        if (npcs[i].ev_id == 20) {
            npcs[i].alt_sw[0] = 501;
            npcs[i].alt_sw[1] = 3581;
        }


        if (npcs[i].ev_id == 94)
            npcs[i].battle_troop = 44;
    }


    player_init(&player, 58, 19);
    player.dir = 0;
    player_set_sprite(&player, characters[0].sprite_data,
                     (unsigned int*)characters[0].clut_data, 480, 440, 0);


    memset(npc_solid, 0, sizeof(npc_solid));
    for (int i = 0; i < 15; i++) {
        if (npcs[i].prio == 1)
            npc_solid[npcs[i].ty * MAP_W + npcs[i].tx] = 1;
    }


    font_px = d_font_start;
    memcpy(font_cl, d_fontc_start, sizeof(font_cl));
    font_adv = d_fontadv_start;
    btl_actor_names_init();


    window_px = d_win_start;
    memcpy(window_cl, d_winc_start, sizeof(window_cl));


    floor_px = d_floor_start;
    memcpy(floor_cl, d_floorc_start, sizeof(floor_cl));

    sceKernelDcacheWritebackAll();
}


#ifdef DIAG_STAGES


#include <pspiofilemgr.h>
static SceUID diag_fd = -1;
static void diag_mark(const char *s) {
    if (diag_fd >= 0) sceIoWrite(diag_fd, s, strlen(s));
}
static void diag_color(unsigned int c, const char *label, void *fbp0, void *fbp1) {
    (void)fbp1;
    diag_mark(label);
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(c);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    sceKernelDelayThread(400000);
}
#endif

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

#ifdef DIAG_STAGES
    diag_fd = sceIoOpen("ms0:/fh_stage.txt",
                        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    diag_mark("entry\n");
#endif

    pspDebugScreenInit();
    setupExitCallback();


    input_init();


    void *fbp0 = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_8888);
    void *fbp1 = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_8888);
    void *zbp = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_4444);
    int total_frames = 0;

    render_init(fbp0, fbp1, zbp, gu_list);
#ifdef DIAG_STAGES
    diag_color(0xffff0000, "gu-init\n", fbp0, fbp1);
#endif
    load_map030();
#ifdef DIAG_STAGES
    diag_color(0xff00ff00, "data-loaded\n", fbp0, fbp1);
#endif


    InputState input = {0};
    int cam_x = player.x - SCR_W / 2;
    int cam_y = player.y - SCR_H / 2;


    u64 tick_last;
    u32 tick_freq = sceRtcGetTickResolution();
    sceRtcGetCurrentTick(&tick_last);
    int frames = 0, fps = 0;


    static char debug_text[128] = "";
    int last_debug_update = 0;
    int talk_cool = 0;
    int talk_lock = 0;


    int torch_lit = 0;


    while (!exit_request) {
        input_update(&input);


        if ((mit.sw[501] ? 1 : 0) != torch_lit) {
            torch_lit = mit.sw[501] ? 1 : 0;
            CharacterDef *cd = &characters[current_character];
            if (torch_lit)
                player_set_sprite(&player, cd->torch_data,
                                  (unsigned int *)cd->torch_clut, 480, 440, 0);
            else
                player_set_sprite(&player, cd->sprite_data,
                                  (unsigned int *)cd->clut_data, 480, 440, 0);
        }


        if (input_pressed(&input, PSP_CTRL_LTRIGGER) && !battle_mode) {
            current_character = (current_character + 3) % 4;
            CharacterDef *cd = &characters[current_character];
            if (torch_lit)
                player_set_sprite(&player, cd->torch_data,
                                  (unsigned int *)cd->torch_clut, 480, 440, 0);
            else
                player_set_sprite(&player, cd->sprite_data,
                                  (unsigned int *)cd->clut_data, 480, 440, 0);
        }
        if (input_pressed(&input, PSP_CTRL_RTRIGGER) && !battle_mode) {
            current_character = (current_character + 1) % 4;
            CharacterDef *cd = &characters[current_character];
            if (torch_lit)
                player_set_sprite(&player, cd->torch_data,
                                  (unsigned int *)cd->torch_clut, 480, 440, 0);
            else
                player_set_sprite(&player, cd->sprite_data,
                                  (unsigned int *)cd->clut_data, 480, 440, 0);
        }


        if (input_held(&input, PSP_CTRL_START)) break;


        if (!(input.buttons & PSP_CTRL_CIRCLE)) talk_lock = 0;
        if (talk_cool > 0) talk_cool--;
        if (input_pressed(&input, PSP_CTRL_CIRCLE) && !player.moving &&
            talk_cool == 0 && !talk_lock && !battle_mode) {
            int ptx = player.x / TILE, pty = player.y / TILE;
            int dx = 0, dy = 0;
            switch (player.dir) {
                case 0: dy = 1; break;
                case 1: dx = -1; break;
                case 2: dx = 1; break;
                case 3: dy = -1; break;
            }
            int found = -1;
            for (int pass = 0; pass < 2 && found < 0; pass++) {
                int tx = pass == 0 ? ptx : ptx + dx;
                int ty = pass == 0 ? pty : pty + dy;
                for (int i = 0; i < 15; i++) {
                    if (npcs[i].tx != tx || npcs[i].ty != ty) continue;
                    if (pass == 0) {
                        if (npcs[i].trig == 0 && npcs[i].prio == 0) found = i;
                    } else {
                        if (npcs[i].trig >= 0 && npcs[i].trig <= 2 &&
                            npcs[i].prio == 1) found = i;
                    }
                    if (found >= 0) break;
                }
            }
            if (found >= 0) {

                if (!npcs[found].dirfix) {
                    int ax = ptx - npcs[found].tx, ay = pty - npcs[found].ty;
                    int mv = (ay > 0) ? 2 : (ay < 0) ? 8 :
                             (ax > 0) ? 6 : 4;
                    npc_draw[found].dir_mv = mv;
                }

                if (npcs[found].battle_troop > 0) {
                    u64 btick;
                    sceRtcGetCurrentTick(&btick);
                    snprintf(btl_actor_name, sizeof(btl_actor_name), "%s",
                             characters[current_character].name);
                    btl_start(btick, current_character,
                              npcs[found].battle_troop);
                    continue;
                }
                if (npcs[found].talk && npcs[found].talk_len > 1) {


                    const FhCmd *list = npcs[found].talk;
                    int len = npcs[found].talk_len;
                    for (int k = 1; k >= 0; k--) {
                        int sw = npcs[found].alt_sw[k];
                        if (sw > 0 && sw < FH_MAX_SWITCHES && mit.sw[sw]) {
                            list = npcs[found].alt_talk[k];
                            len = npcs[found].alt_len[k];
                            break;
                        }
                    }
                    if (list && len > 1) {
                        char title[64];
                        snprintf(title, sizeof(title), "Map030 ev%d (%s)",
                                 npcs[found].ev_id,
                                 npcs[found].ev_id == 20 ? "torch" : "saw corpse");
                        msg_open(list, len, title);
                    }
                }
            }
        }


        unsigned char *char_sprites[4] = {
            characters[0].sprite_data, characters[1].sprite_data,
            characters[2].sprite_data, characters[3].sprite_data
        };
        unsigned int *char_cluts[4] = {
            (unsigned int*)characters[0].clut_data, (unsigned int*)characters[1].clut_data,
            (unsigned int*)characters[2].clut_data, (unsigned int*)characters[3].clut_data
        };


        if (msg_mode) {
            if (!msg_ended) {
                if (mit.await_choice) {

                    if (!msg_text_revealed()) msg_reveal_all();
                    if (input_pressed(&input, PSP_CTRL_UP) && msg_cursor > 0) msg_cursor--;
                    if (input_pressed(&input, PSP_CTRL_DOWN) && msg_cursor < mit.choice_n - 1) msg_cursor++;
                    if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        mit.choice_sel = msg_cursor;
                        mit.await_choice = 0;
                        page_wait = 0;
                    }
                    if (input_pressed(&input, PSP_CTRL_CROSS)) {
                        mit.choice_sel = -1;
                        mit.await_choice = 0;
                        page_wait = 0;
                    }
                } else if (!msg_text_revealed()) {
                    if (input_pressed(&input, PSP_CTRL_CIRCLE))
                        msg_reveal_all();
                } else if (page_wait) {
                    if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        page_wait = 0;
                        msg_advance();
                    }
                } else {
                    msg_advance();
                }
            } else if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                msg_mode = 0;
                talk_cool = 45;

                talk_lock = 1;
                continue;
            }


            render_frame(cam_x, cam_y, &map_layers[0][0][0], MAP_W, MAP_H,
                        &player, current_character, char_sprites, char_cluts,
                        map_higher, sizeof(map_higher), total_frames,
                        npc_draw, 15, torch_lit);
            sceGuStart(GU_DIRECT, gu_list);
            render_message_window(&mit, msg_ended, msg_cursor, page_wait);
            sceGuFinish();
            sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
            sceDisplayWaitVblankStart();
            fbp0 = sceGuSwapBuffers();
            frames++;


            u64 now;
            sceRtcGetCurrentTick(&now);
            if (now - tick_last >= tick_freq) {
                float span = (float)(now - tick_last) / (float)tick_freq;
                fps = (int)(frames / span);
                frames = 0;
                tick_last = now;
            }
            continue;
        }


        if (input_pressed(&input, PSP_CTRL_SQUARE) && !player.moving &&
            !battle_mode) {
            u64 btick;
            sceRtcGetCurrentTick(&btick);
            snprintf(btl_actor_name, sizeof(btl_actor_name), "%s",
                     characters[current_character].name);
            btl_start(btick, current_character, 1);
        }


        if (battle_mode) {


            static const char *acmds[] = {"Attack", "Skills", "Guard",
                                          "Item"};
            {
                static int last_phase = -99, last_active = -99;
                if (btl_phase != last_phase || btl_ev_active != last_active) {
                    char ab[64];
                    snprintf(ab, sizeof(ab), "phase %d->%d ev=%d->%d\n",
                             last_phase, btl_phase, last_active,
                             btl_ev_active);
                    btl_trace(ab);
                    last_phase = btl_phase;
                    last_active = btl_ev_active;
                }
            }


            for (int i = 0; i < 8; i++)
                if (btl_pops[i].ttl > 0) btl_pops[i].ttl--;
            battle_anim_tick();


            btl_motion_tick();
            if (btl_pose_t > 0 && --btl_pose_t == 0 &&
                !btl.f[0].guard) {
                btl_motion(0, 1, 1);
            }
            for (int i = 0; i < 2; i++)
                if (btl_log_t[i] > 0 && --btl_log_t[i] == 0)
                    btl_log[i][0] = 0;
            for (int i = 0; i < 8; i++) {
                if (btl_flash[i] > 0) btl_flash[i]--;
                if (btl_collapse[i] > 0) btl_collapse[i]--;
            }


            if (btl_ev_active) {
                if (tit.await_choice) {
                    if (!msg_text_revealed()) msg_reveal_all();
                    if (input_pressed(&input, PSP_CTRL_UP) && btl_ev_cursor > 0)
                        btl_ev_cursor--;
                    if (input_pressed(&input, PSP_CTRL_DOWN) &&
                        btl_ev_cursor < tit.choice_n - 1)
                        btl_ev_cursor++;
                    if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        tit.choice_sel = btl_ev_cursor;
                        tit.await_choice = 0;
                        btl_ev_pagewait = 0;
                    }
                    if (input_pressed(&input, PSP_CTRL_CROSS)) {
                        tit.choice_sel = -1;
                        tit.await_choice = 0;
                        btl_ev_pagewait = 0;
                    }
                } else if (!msg_text_revealed()) {
                    if (input_pressed(&input, PSP_CTRL_CIRCLE))
                        msg_reveal_all();
                } else if (btl_ev_pagewait) {
                    if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        btl_ev_pagewait = 0;
                        btl_ev_advance();
                    }
                } else {
                    btl_ev_advance();
                }
                if (btl_ev_ended) {
                    if (tit.text_len == 0 && !tit.await_choice) {
                        btl_ev_active = 0;
                        btl_trace("close silent\n");
                    } else if (msg_text_revealed() &&
                               input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        btl_ev_active = 0;
                        btl_trace("close text\n");
                    }
                }
                if (!btl_ev_active && btl_phase == 4) {


                    if (btl_nresCE > 0) {
                        btl_ce_begin(btl_resCE[0]);
                        for (int i = 1; i < btl_nresCE; i++)
                            btl_resCE[i - 1] = btl_resCE[i];
                        btl_nresCE--;
                        btl_ce_run = 1;
                    } else if (btl_run_pre) {
                        btl_run_pre = 0;
                        btl_ce_run = 0;
                        int pg = btl_ev_scan(0);
                        if (pg >= 0) {
                            btl_run_failed = 1;
                            btl_ev_begin(pg);
                        } else {


                            btl_run_failed = 0;
                            btl_act_kind = 5;
                            btl_begin_exec();
                        }
                    } else if (btl_run_failed) {
                        btl_run_failed = 0;
                        btl_ce_run = 0;
                        btl_act_kind = 5;
                        btl_begin_exec();
                    } else if (btl_midturn) {
                        btl_midturn = 0;
                        btl_ce_run = 0;
                        int pg = btl_ev_scan(0);
                        if (pg >= 0) btl_ev_begin(pg);
                        else {
                            btl_phase = 2;
                            btl_wait = 15;
                        }
                    } else {
                        btl_ce_run = 0;
                        int pg = btl_ev_scan(1);
                        if (pg >= 0) btl_ev_begin(pg);
                        else {
                            btl_next_turn();
                            {
                                char ab[160];
                                snprintf(ab, sizeof(ab),
                                         "round turn=%d actor=%d foes=%d/%d/%d/%d/%d/%d/%d alive=%d%d%d%d%d%d%d col=%d%d%d%d%d%d%d phase=%d\n",
                                         btl.turn, btl.f[0].hp,
                                         btl.f[1].hp, btl.f[2].hp,
                                         btl.f[3].hp, btl.f[4].hp,
                                         btl.f[5].hp, btl.f[6].hp,
                                         btl.f[7].hp,
                                         !!btl.f[1].alive, !!btl.f[2].alive,
                                         !!btl.f[3].alive, !!btl.f[4].alive,
                                         !!btl.f[5].alive, !!btl.f[6].alive,
                                         !!btl.f[7].alive,
                                         btl_collapse[0], btl_collapse[1],
                                         btl_collapse[2], btl_collapse[3],
                                         btl_collapse[4], btl_collapse[5],
                                         btl_collapse[6], btl_phase);
                                btl_trace(ab);
                            }
                            pg = btl_ev_scan(0);
                            if (pg >= 0) btl_ev_begin(pg);
                            else btl_phase = 0;
                        }
                    }
                }
            } else if (btl_phase == 0) {


                if (input_pressed(&input, PSP_CTRL_UP) && btl_cmd > 0)
                    btl_cmd--;
                if (input_pressed(&input, PSP_CTRL_DOWN) && btl_cmd < 3)
                    btl_cmd++;
                if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                    if (btl_cmd == 0) {
                        btl_act_kind = 0;
                        btl_phase = 1;

                        btl_tgt = 0;
                        while (btl_tgt < btl.n_foes &&
                               !btl.f[1 + btl_tgt].alive)
                            btl_tgt++;
                    } else if (btl_cmd == 1) {

                        btl_known_rebuild();
                        if (btl_nknown > 0) {
                            btl_list_cur = 0;
                            btl_list_top = 0;
                            btl_phase = 5;
                        }
                    } else if (btl_cmd == 2) {
                        btl_act_kind = 1;
                        btl_begin_exec();
                    } else if (btl_cmd == 3) {
                        btl_item_rebuild();
                        if (btl_nitems > 0) {
                            btl_list_cur = 0;
                            btl_list_top = 0;
                            btl_phase = 6;
                        }
                    }
                }
            } else if (btl_phase == 5) {

                if (input_pressed(&input, PSP_CTRL_UP) && btl_list_cur > 0) {
                    btl_list_cur--;
                    if (btl_list_cur < btl_list_top) btl_list_top = btl_list_cur;
                }
                if (input_pressed(&input, PSP_CTRL_DOWN) &&
                    btl_list_cur < btl_nknown - 1) {
                    btl_list_cur++;
                    if (btl_list_cur > btl_list_top + 4)
                        btl_list_top = btl_list_cur - 4;
                }
                if (input_pressed(&input, PSP_CTRL_CIRCLE) &&
                    btl_list_cur < btl_nknown) {
                    int sid = btl_known[btl_list_cur];
                    int r = btl_skillrow(sid);
                    if (r >= 0 && btl.f[0].mp >= SKILL_DB[r].mp) {
                        if (sid == 40 || sid == 11) {


                            btl_act_kind = 5;
                            btl_act_id = sid;


                            btl_nresCE = 0;
                            btl_run_pre = 1;
                            btl_run_failed = 0;
                            if (sid == 11) btl_log_push("Talk");
                            btl_ce_begin(sid == 40 ? 46 : 14);
                            btl_phase = 4;
                        } else {
                        btl_act_kind = 3;
                        btl_act_id = sid;
                        if (SKILL_DB[r].scope == 1) {
                            btl_phase = 7;
                            btl_tgt = 0;
                            while (btl_tgt < btl.n_foes &&
                                   !btl.f[1 + btl_tgt].alive)
                                btl_tgt++;
                        } else {
                            btl_act_target = -1;
                            btl_begin_exec();
                        }
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_CROSS)) {
                    btl_cmd = 1;
                    btl_phase = 0;
                }
            } else if (btl_phase == 6) {

                if (input_pressed(&input, PSP_CTRL_UP) && btl_list_cur > 0) {
                    btl_list_cur--;
                    if (btl_list_cur < btl_list_top) btl_list_top = btl_list_cur;
                }
                if (input_pressed(&input, PSP_CTRL_DOWN) &&
                    btl_list_cur < btl_nitems - 1) {
                    btl_list_cur++;
                    if (btl_list_cur > btl_list_top + 4)
                        btl_list_top = btl_list_cur - 4;
                }
                if (input_pressed(&input, PSP_CTRL_CIRCLE) &&
                    btl_list_cur < btl_nitems) {
                    int iid = btl_item_ids[btl_list_cur];
                    const BtSkill *sk = btl_item_skill(iid);
                    if (sk && mit.inv_item[iid] > 0) {
                        btl_act_kind = 4;
                        btl_act_id = iid;
                        if (sk->scope == 1) {
                            btl_phase = 7;
                            btl_tgt = 0;
                            while (btl_tgt < btl.n_foes &&
                                   !btl.f[1 + btl_tgt].alive)
                                btl_tgt++;
                        } else {
                            btl_act_target = -1;
                            btl_begin_exec();
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_CROSS)) {
                    btl_cmd = 3;
                    btl_phase = 0;
                }
            } else if (btl_phase == 7) {

                if (input_pressed(&input, PSP_CTRL_UP)) {
                    int t = btl_tgt;
                    for (int k = 0; k < btl.n_foes; k++) {
                        t = (t + btl.n_foes - 1) % btl.n_foes;
                        if (btl.f[1 + t].alive) {
                            btl_tgt = t;
                            break;
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_DOWN)) {
                    int t = btl_tgt;
                    for (int k = 0; k < btl.n_foes; k++) {
                        t = (t + 1) % btl.n_foes;
                        if (btl.f[1 + t].alive) {
                            btl_tgt = t;
                            break;
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                    btl_act_target = btl_tgt;
                    btl_begin_exec();
                }
                if (input_pressed(&input, PSP_CTRL_CROSS))
                    btl_phase = (btl_act_kind == 3) ? 5 : 6;
            } else if (btl_phase == 1) {
                if (input_pressed(&input, PSP_CTRL_UP)) {
                    int t = btl_tgt;
                    for (int k = 0; k < btl.n_foes; k++) {
                        t = (t + btl.n_foes - 1) % btl.n_foes;
                        if (btl.f[1 + t].alive) {
                            btl_tgt = t;
                            break;
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_DOWN)) {
                    int t = btl_tgt;
                    for (int k = 0; k < btl.n_foes; k++) {
                        t = (t + 1) % btl.n_foes;
                        if (btl.f[1 + t].alive) {
                            btl_tgt = t;
                            break;
                        }
                    }
                }
                if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                    btl_act_kind = 0;
                    btl_act_target = btl_tgt;
                    btl_begin_exec();
                }
                if (input_pressed(&input, PSP_CTRL_CROSS)) {
                    btl_cmd = 0;
                    btl_phase = 0;
                }
            } else if (btl_phase == 2) {
                if (btl_wait > 0) {
                    btl_wait--;
                } else if (btl_exec_step()) {


                    if (btl_phase == 2) {
                        if (btl_nresCE > 0) {
                            btl_ce_begin(btl_resCE[0]);
                            for (int i = 1; i < btl_nresCE; i++)
                                btl_resCE[i - 1] = btl_resCE[i];
                            btl_nresCE--;
                            btl_ce_run = 1;
                            btl_phase = 4;
                        } else {
                            int pg = btl_ev_scan(1);
                            if (pg >= 0) {
                                btl_ev_begin(pg);
                                btl_phase = 4;
                            } else {
                                btl_next_turn();
                                pg = btl_ev_scan(0);
                                if (pg >= 0) btl_ev_begin(pg);
                                else btl_phase = 0;
                            }
                        }
                    }
                } else {
                    btl_wait = 25;
                }
            } else if (btl_phase == 4) {


            } else {


                if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                    battle_mode = 0;
                    talk_cool = 45;
                    talk_lock = 1;
                }
            }


            if (!btl_ev_active &&
                (btl_phase == 0 || btl_phase == 1 || btl_phase == 5 ||
                 btl_phase == 6 || btl_phase == 7)) {
                int mpg = btl_ev_scan(0);
                if (mpg >= 0) btl_ev_begin(mpg);
            }


            BtFoeDraw draws[BT_MAX_FOES];
            for (int i = 0; i < btl.n_foes; i++) {
                float x, y;
                btl_foe_xy(i, &x, &y);
                int r = 0;
                while (FOE_DB[r].id && FOE_DB[r].id != btl.f[1 + i].ref) r++;
                btl_foe_art(btl.f[1 + i].ref, &draws[i].t8, &draws[i].clut);
                draws[i].tw = FOE_DB[r].tw;
                draws[i].th = FOE_DB[r].th;
                draws[i].stride = FOE_DB[r].stride;
                draws[i].w = FOE_DB[r].w;
                draws[i].h = FOE_DB[r].h;
                draws[i].x = x;
                draws[i].y = y;
                draws[i].alive =
                    btl.f[1 + i].alive || btl_collapse[i] > 0;
            }
            const char *targets[BT_MAX_FOES];
            int ntgt = 0;
            for (int i = 0; i < btl.n_foes; i++) {
                if (!btl.f[1 + i].alive) continue;
                int r = 0;
                while (FOE_DB[r].id && FOE_DB[r].id != btl.f[1 + i].ref) r++;
                targets[ntgt++] = FOE_DB[r].id ? FOE_DB[r].name : "?";
            }
            char st_name[32];
            snprintf(st_name, sizeof(st_name), "%s",
                     characters[current_character].name);

            int tcursor = 0, seen = 0;
            for (int i = 0; i < btl.n_foes; i++) {
                if (!btl.f[1 + i].alive) continue;
                if (i == btl_tgt) tcursor = seen;
                seen++;
            }
            sceGuStart(GU_DIRECT, gu_list);
            sceGuClearColor(0xff000000);
            sceGuClear(GU_COLOR_BUFFER_BIT);

            BtListRow lrows[4];
            int nlrows = 0, lcur = 0, show_list = 0;
            if ((btl_phase == 5 || btl_phase == 6) && !btl_ev_active) {
                show_list = 1;
                lcur = btl_list_cur - btl_list_top;
                if (btl_phase == 5) {
                    for (int i = btl_list_top;
                         i < btl_nknown && nlrows < 5; i++) {
                        int sid = btl_known[i], r = btl_skillrow(sid);
                        int cost = r >= 0 ? SKILL_DB[r].mp : 0;
                        int ok = r >= 0 && btl.f[0].mp >= cost;
                        snprintf(lrows[nlrows].text, sizeof(lrows[0].text),
                                 "%s %d", r >= 0 ? SKILL_DB[r].nm : "?",
                                 cost);
                        lrows[nlrows].color =
                            ok ? 0xffffffff : 0xff808080;
                        lrows[nlrows].icon =
                            r >= 0 ? SKILL_DB[r].icon : -1;
                        nlrows++;
                    }
                } else {
                    for (int i = btl_list_top;
                         i < btl_nitems && nlrows < 5; i++) {
                        int iid = btl_item_ids[i], r = -1;
                        for (int k = 0; ITEM_DB[k].id; k++) {
                            if (ITEM_DB[k].id == iid) {
                                r = k;
                                break;
                            }
                        }
                        snprintf(lrows[nlrows].text, sizeof(lrows[0].text),
                                 "%s x%d", r >= 0 ? ITEM_DB[r].nm : "?",
                                 mit.inv_item[iid]);
                        lrows[nlrows].color = 0xffffffff;
                        lrows[nlrows].icon =
                            r >= 0 ? ITEM_DB[r].icon : -1;
                        nlrows++;
                    }
                }
            }

            int st_icons[3], nst_icons = 0;
            for (int i = 0; i < btl.f[0].nstates && nst_icons < 3; i++) {
                int sid = btl.f[0].states[i], icon = -1;
                for (int k = 0; STATE_ICON[k].st; k++) {
                    if (STATE_ICON[k].st == sid) {
                        icon = STATE_ICON[k].icon;
                        break;
                    }
                }
                st_icons[nst_icons++] = icon;
            }


            int sel_foe = -1;
            if ((btl_phase == 1 || btl_phase == 7) && !btl_ev_active)
                sel_foe = btl_tgt;
            render_battle(draws, btl.n_foes, btl_pops, 8, st_name,
                         btl.f[0].hp, btl.f[0].maxhp, btl.f[0].mp,
                         btl.f[0].maxmp,
                         acmds, 4, btl_cmd,
                         btl_phase == 0 && !btl_ev_active, targets, ntgt,
                         tcursor,
                         (btl_phase == 1 || btl_phase == 7) &&
                             !btl_ev_active,
                         btl_log_t[0] > 0 ? btl_log[0] : NULL,
                         btl_log_t[1] > 0 ? btl_log[1] : NULL,
                         bv_t8a[current_character], bv_cla[current_character],
                         bv_t8b[current_character], bv_clb[current_character],


                         btl_mcol + (btl_pat < 3 ? btl_pat : 1),
                         btl_mrow,
                         100 +
                             ((btl_pose_t > 0 && !btl_loop) ? 16 : 0),
                         214 - 112,
                         sel_foe, btl_flash, btl_collapse,
                         lrows, nlrows, lcur, show_list,
                         st_icons, nst_icons);

            render_battle_anims(draws, btl.n_foes);


            sceGuFinish();
            sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
            if (btl_ev_active && (tit.text_len > 0 || tit.await_choice)) {
                sceGuStart(GU_DIRECT, gu_list);
                render_message_window(&tit, btl_ev_ended, btl_ev_cursor,
                                      btl_ev_pagewait);
                sceGuFinish();
                sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
            }
            sceDisplayWaitVblankStart();
            fbp0 = sceGuSwapBuffers();
            frames++;
            u64 now;
            sceRtcGetCurrentTick(&now);
            if (now - tick_last >= tick_freq) {
                float span = (float)(now - tick_last) / (float)tick_freq;
                fps = (int)(frames / span);
                frames = 0;
                tick_last = now;
            }
            continue;
        }


        int pre_px = player.x, pre_py = player.y;
        player_update(&player, input.buttons, (uint16_t*)map_passability,
                      MAP_W, MAP_H, npc_solid);


        if (!player.moving && player.x == pre_px && player.y == pre_py) {
            unsigned int held = input.buttons &
                (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT);
            if (held) {
                int dx = 0, dy = 0;
                switch (player.dir) {
                    case 0: dy = 1; break;
                    case 1: dx = -1; break;
                    case 2: dx = 1; break;
                    case 3: dy = -1; break;
                }
                int tx = player.x / TILE + dx, ty = player.y / TILE + dy;
                for (int i = 0; i < 15; i++) {
                    if (npcs[i].tx == tx && npcs[i].ty == ty &&
                        npcs[i].trig >= 1 && npcs[i].trig <= 2 &&
                        npcs[i].prio == 1 && npcs[i].battle_troop > 0) {
                        u64 btick;
                        sceRtcGetCurrentTick(&btick);
                        snprintf(btl_actor_name, sizeof(btl_actor_name), "%s",
                                 characters[current_character].name);
                        btl_start(btick, current_character,
                                  npcs[i].battle_troop);
                        break;
                    }
                }
                if (battle_mode) continue;
            }
        }


        int target_x = player.x - SCR_W / 2;
        int target_y = player.y - SCR_H / 2;


        if (target_x < 0) target_x = 0;
        if (target_y < 0) target_y = 0;
        if (target_x > MAP_W * TILE - SCR_W) target_x = MAP_W * TILE - SCR_W;
        if (target_y > MAP_H * TILE - SCR_H) target_y = MAP_H * TILE - SCR_H;


        cam_x = cam_x + (target_x - cam_x) / 2;
        cam_y = cam_y + (target_y - cam_y) / 2;


        render_frame(cam_x, cam_y, &map_layers[0][0][0], MAP_W, MAP_H,
                    &player, current_character, char_sprites, char_cluts,
                    map_higher, sizeof(map_higher), total_frames,
                    npc_draw, 15, torch_lit);


        if (frames == 0 || frames - last_debug_update >= 30) {
            snprintf(debug_text, sizeof(debug_text), "F&H: %s | L/R:switch | fps:%d",
                    characters[current_character].name, fps);
            last_debug_update = frames;
        }
        render_debug_text(debug_text);

        sceDisplayWaitVblankStart();
        fbp0 = sceGuSwapBuffers();
        frames++;
        total_frames++;
#ifdef DIAG_STAGES
        if (frames == 1) diag_mark("frame1\n");
        else if (frames == 60) diag_mark("frame60\n");
        else if (frames == 600) { diag_mark("frame600\n"); sceIoClose(diag_fd); diag_fd = -1; }
#endif


        u64 now;
        sceRtcGetCurrentTick(&now);
        if (now - tick_last >= tick_freq) {
            float span = (float)(now - tick_last) / (float)tick_freq;
            fps = (int)(frames / span);
            frames = 0;
            tick_last = now;
        }
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
