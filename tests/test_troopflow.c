

#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "interp.h"
#include "battle_blob.h"
#include "battle_db.h"
#include "troop1.h"
#include "troop44.h"
#include "itemce.h"

static int fails = 0;
#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { printf("FAIL: " fmt "\n", ##__VA_ARGS__); fails++; } \
} while (0)

static Bt bt;
static unsigned char Gsw[FH_MAX_SWITCHES];
static int32_t Gvar[FH_MAX_VARS];
static BtSkill sk_atk;
static BtIns atk_prog[24];

static void resolve_atk(void) {
    FILE *f = fopen("converted/code/formulas.bin", "rb");
    static unsigned char blob[65536];
    if (f) { fread(blob, 1, sizeof(blob), f); fclose(f); }
    const unsigned char *raw = NULL;
    int nins = bt_prog_find(blob, 0, 1, &raw);
    sk_atk.id = 1;
    sk_atk.prog = atk_prog;
    sk_atk.nins = 0;
    if (raw && nins > 0 && nins <= 24) {
        for (int k = 0; k < nins; k++) bt_ins_get(raw + k * 10u, &atk_prog[k]);
        sk_atk.nins = nins;
    }
    for (int k = 0; SKILL_DB[k].id; k++)
        if (SKILL_DB[k].id == 1) {
            sk_atk.hit_type = SKILL_DB[k].hit;
            sk_atk.scope = SKILL_DB[k].scope;
            sk_atk.speed = SKILL_DB[k].speed;
            sk_atk.variance = SKILL_DB[k].var;
            sk_atk.element = SKILL_DB[k].elem;
            sk_atk.dmg_type = SKILL_DB[k].type;
            sk_atk.crit = SKILL_DB[k].crit;
            sk_atk.success = SKILL_DB[k].succ;
            break;
        }
}

static void seed_btl(int troop) {
    memset(&bt, 0, sizeof(bt));
    int row = 0;
    while (ACTOR_DB[row].id && ACTOR_DB[row].id != 1) row++;
    BtF *a = &bt.f[0];
    a->is_foe = 0;
    a->ref = 1;
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
    a->pdr = a->mdr = a->grd = 1.0;
    for (int i = 0; i < BT_ERATE_N; i++) a->erate[i] = ACTOR_DB[row].er[i];
    a->atk_elem = ACTOR_DB[row].elem;
    a->level = ACTOR_DB[row].level;
    a->alive = 1;
    bt.n_party = 1;
    bt.n_foes = 0;
    for (int k = 0; TROOP_MB[k].troop; k++) {
        if (TROOP_MB[k].troop != troop) continue;
        int id = TROOP_MB[k].foe, r = 0;
        while (FOE_DB[r].id && FOE_DB[r].id != id) r++;
        if (!FOE_DB[r].id) continue;
        BtF *ff = &bt.f[1 + bt.n_foes];
        ff->is_foe = 1;
        ff->ref = id;
        ff->maxhp = ff->hp = FOE_DB[r].mhp;
        ff->atk = FOE_DB[r].atk;
        ff->def = FOE_DB[r].def;
        ff->mat = FOE_DB[r].mat;
        ff->mdf = FOE_DB[r].mdf;
        ff->agi = FOE_DB[r].agi;
        ff->luk = FOE_DB[r].luk;
        ff->hit = FOE_DB[r].hit;
        ff->eva = FOE_DB[r].eva;
        ff->pdr = ff->mdr = ff->grd = 1.0;
        for (int e = 0; e < BT_ERATE_N; e++) ff->erate[e] = FOE_DB[r].er[e];
        ff->atk_elem = FOE_DB[r].elem;
        ff->alive = 1;
        bt.n_foes++;
    }
    bt.turn = 1;
    bt_srand(&bt, 1234);
    memset(Gsw, 0, sizeof(Gsw));
    memset(Gvar, 0, sizeof(Gvar));
    Gvar[4] = 2;
}

typedef struct {
    const BtPageCond *cond;
    const FhCmd **lists;
    const int *lens;
    int npages;
    const int *ce_ids;
    const FhCmd **ce_lists;
    const int *ce_lens;
    int ce_n;
} TroopTab;


static int mrg_ids[64];
static const FhCmd *mrg_lists[64];
static int mrg_lens[64];
static int mrg_n;
static TroopTab mrg_tab;


static void load_tab(int troop) {
    int known[24], nknown = 0, i, k;
    int seeds[128], nseeds = 0;
    if (tblob_load(troop) != 0) {
        printf("FAIL: blob load troop %d\n", troop);
        fails++;
        return;
    }
    known[nknown++] = 40;
    known[nknown++] = 11;
    {
        int aid = 1, cls = 0, lv = 1;
        const TblobActor *sa = tblob_actors();
        for (int r = 0; sa[r].id; r++) {
            if (sa[r].id == aid) {
                cls = sa[r].cls;
                break;
            }
        }
        for (int r = 0; ACTOR_DB[r].id; r++) {
            if (ACTOR_DB[r].id == aid) {
                lv = ACTOR_DB[r].level;
                break;
            }
        }
        for (int r = 0; CLASS_LEARN[r].cls; r++) {
            int dup = 0, q;
            if (CLASS_LEARN[r].cls != cls || CLASS_LEARN[r].lv > lv)
                continue;
            for (q = 0; q < nknown; q++)
                if (known[q] == CLASS_LEARN[r].sk) {
                    dup = 1;
                    break;
                }
            if (!dup && nknown < 24) known[nknown++] = CLASS_LEARN[r].sk;
        }
    }
    for (i = 0; i < nknown && nseeds < 128; i++) {
        for (k = 0; SKILL_FX[k].id && nseeds < 128; k++) {
            if (SKILL_FX[k].id == known[i] && SKILL_FX[k].code == 44)
                seeds[nseeds++] = SKILL_FX[k].data;
        }
    }
    skill_union_build(seeds, nseeds);
    mrg_n = 0;
    for (i = 0; i < tblob_nces() && mrg_n < 64; i++) {
        mrg_ids[mrg_n] = tblob_ce_id(i);
        mrg_lists[mrg_n] = tblob_ce_list(i);
        mrg_lens[mrg_n] = tblob_ce_len(i);
        mrg_n++;
    }
    for (i = 0; i < skill_union_count() && mrg_n < 64; i++) {
        int dup = 0, j;
        for (j = 0; j < mrg_n; j++) {
            if (mrg_ids[j] == skill_union_id(i)) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;
        mrg_ids[mrg_n] = skill_union_id(i);
        mrg_lists[mrg_n] = skill_union_list(i);
        mrg_lens[mrg_n] = skill_union_len(i);
        mrg_n++;
    }
    mrg_tab.cond = tblob_conds();
    mrg_tab.lists = tblob_lists();
    mrg_tab.lens = tblob_lens();
    mrg_tab.npages = tblob_npages();
    mrg_tab.ce_ids = mrg_ids;
    mrg_tab.ce_lists = mrg_lists;
    mrg_tab.ce_lens = mrg_lens;
    mrg_tab.ce_n = mrg_n;
}


static const FhCmd *mrg_ce(int id, int *len) {
    for (int i = 0; i < mrg_n; i++) {
        if (mrg_ids[i] == id) {
            *len = mrg_lens[i];
            return mrg_lists[i];
        }
    }
    *len = 0;
    return 0;
}


static int cmds_equal(const FhCmd *a, const FhCmd *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i].code != b[i].code || a[i].indent != b[i].indent ||
            a[i].jump != b[i].jump || a[i].op != b[i].op)
            return 0;
        for (int p = 0; p < 10; p++)
            if (a[i].p[p] != b[i].p[p]) return 0;
        if (!a[i].s && !b[i].s) continue;
        if (!a[i].s || !b[i].s) return 0;
        if (strcmp(a[i].s, b[i].s)) return 0;
    }
    return 1;
}


static void compare_oracle(int troop, const BtPageCond *oc,
                           const FhCmd **ol, const int *olen, int onp,
                           const char *tag) {
    int i;
    (void)troop;
    CHECK(mrg_tab.npages == onp, "%s: npages %d vs %d", tag,
          mrg_tab.npages, onp);
    if (mrg_tab.npages != onp) return;
    CHECK(!memcmp(mrg_tab.cond, oc, (size_t)onp * sizeof(*oc)),
          "%s: page conditions identical", tag);
    for (i = 0; i < onp; i++) {
        if (mrg_tab.lens[i] != olen[i]) {
            CHECK(0, "%s: page %d len %d vs %d", tag, i,
                  mrg_tab.lens[i], olen[i]);
            continue;
        }
        CHECK(cmds_equal(mrg_tab.lists[i], ol[i], olen[i]),
              "%s: page %d commands identical", tag, i);
    }
}


static void compare_ce_oracles(const char *tag) {
    for (int i = 0; i < mrg_n; i++) {
        int k = 0, len = 0;
        const FhCmd *bl;
        while (k < CEB_N && CEB_IDS[k] != mrg_ids[i]) k++;
        if (k >= CEB_N) continue;
        bl = mrg_lists[i];
        len = mrg_lens[i];
        if (len != CEB_LEN[k]) {
            CHECK(0, "%s: CE%d len %d vs %d", tag, mrg_ids[i], len,
                  CEB_LEN[k]);
            continue;
        }
        CHECK(cmds_equal(bl, CEB_LIST[k], len), "%s: CE%d identical",
              tag, mrg_ids[i]);
    }
}

static void run_page(const TroopTab *t, int pgi, int *flags) {
    FhInterp tit;
    fh_interp_init(&tit, t->lists[pgi], t->lens[pgi]);
    tit.rng = 1234;
    if (t->cond[pgi].span <= 1) flags[pgi] = 1;
    memcpy(tit.sw, Gsw, sizeof(tit.sw));
    memcpy(tit.var, Gvar, sizeof(tit.var));
    tit.party[0] = 1;
    tit.party_n = 1;
    tit.hp[1] = bt.f[0].hp;
    tit.mp[1] = bt.f[0].mp;
    tit.troop_n = bt.n_foes;
    for (int i = 0; i < bt.n_foes; i++) {
        tit.ehp[i] = bt.f[1 + i].hp;
        tit.emp[i] = bt.f[1 + i].mp;
        tit.emaxhp[i] = bt.f[1 + i].maxhp;
        tit.emaxmp[i] = bt.f[1 + i].maxmp;
    }
    tit.ce_ids = t->ce_ids;
    tit.ce_lists = t->ce_lists;
    tit.ce_lens = t->ce_lens;
    tit.ce_n = t->ce_n;
    int steps = 0;
    for (;;) {
        int r = fh_interp_step(&tit);
        if (r == FH_RUN_END) break;
        if (r == FH_RUN_CHOICE) tit.await_choice = 0;
        if (++steps > 300000) break;
    }
    CHECK(steps <= 300000, "troop page %d did not terminate", pgi);
    memcpy(Gsw, tit.sw, sizeof(Gsw));
    memcpy(Gvar, tit.var, sizeof(Gvar));
    for (int i = 0; i < bt.n_foes; i++) {
        BtF *ff = &bt.f[1 + i];
        if (ff->alive) {
            ff->hp = tit.ehp[i];
            if (ff->hp > ff->maxhp) ff->hp = ff->maxhp;

            if (tit.erecover[i] && ff->hp < ff->maxhp &&
                tit.ehp[i] >= tit.emaxhp[i])
                ff->hp = ff->maxhp;
            if (ff->hp <= 0) {
                ff->hp = 0;
                ff->alive = 0;
            }
        }
        for (int st = 0; st < FH_MAX_STATES; st++)
            if (tit.estate[i][st]) bt_add_state(ff, st);
        if (tit.etransform[i] > 0) {
            int nid = tit.etransform[i], r = 0;
            while (FOE_DB[r].id && FOE_DB[r].id != nid) r++;
            if (FOE_DB[r].id) {
                ff->ref = nid;
                ff->maxhp = FOE_DB[r].mhp;
                if (ff->hp > ff->maxhp) ff->hp = ff->maxhp;
                if (tit.erecover[i] && ff->alive) ff->hp = ff->maxhp;
            }
        }
    }
    if (tit.battle.pending == 2) bt.over = 3;
}


static void kill_and_cascade(const TroopTab *t, int member, const char *tag) {
    BtF *tgt = &bt.f[1 + member];
    for (int r = 0; r < 3; r++) {
        int c, m, e;
        bt_strike(&bt, &sk_atk, &bt.f[0], tgt, NULL, NULL, &c, &m, &e);
    }
    CHECK(!tgt->alive, "%s: leg should die (hp=%d)", tag, tgt->hp);
    int alive = 0;
    for (int i = 0; i < bt.n_foes; i++) if (bt.f[1 + i].alive) alive++;
    CHECK(alive == bt.n_foes - 1, "%s: only the leg dies (%d/%d alive)",
          tag, alive, bt.n_foes);
    int flags[16] = {0};
    for (int round = 0; round < 10; round++) {
        BtActorHp ac;
        ac.id = 1;
        ac.hp = bt.f[0].hp;
        ac.maxhp = bt.f[0].maxhp;
        int pg = -1;
        for (int i = 0; i < t->npages; i++) {
            if (flags[i]) continue;
            if (bt_page_fire(&t->cond[i], bt.turn, 1, &bt.f[1], bt.n_foes,
                             &ac, 1, Gsw)) {
                pg = i;
                break;
            }
        }
        if (pg < 0) break;
        run_page(t, pg, flags);
        if (bt.over) break;
    }
    int alive2 = 0;
    for (int i = 0; i < bt.n_foes; i++) if (bt.f[1 + i].alive) alive2++;
    CHECK(bt.over == 0, "%s: battle must continue (over=%d)", tag, bt.over);
    CHECK(alive2 == bt.n_foes - 1, "%s: cascade must not wipe (%d/%d)",
          tag, alive2, bt.n_foes);
    CHECK(!bt.f[1 + member].alive && bt.f[1 + member].hp == 0,
          "%s: dead limb stays dead", tag);
}

static int skillrow(int id) {
    for (int k = 0; SKILL_DB[k].id; k++)
        if (SKILL_DB[k].id == id) return k;
    return -1;
}

static void run_ce(const FhCmd *list, int len, FhInterp *out) {
    fh_interp_init(out, list, len);
    for (;;) {
        int r = fh_interp_step(out);
        if (r == FH_RUN_END) break;
        if (r == FH_RUN_CHOICE) out->await_choice = 0;
    }
}

int main(void) {
    resolve_atk();
    CHECK(sk_atk.nins == 8, "skill1 program resolves (nins=%d)", sk_atk.nins);

    if (tblob_open("psp/gu_demo/data/troops.blob") != 0) {
        printf("FAIL: cannot open troops.blob\n");
        return 1;
    }
    if (skillce_open("psp/gu_demo/data/skillce.blob") != 0) {
        printf("FAIL: cannot open skillce.blob\n");
        return 1;
    }
    load_tab(1);
    compare_oracle(1, TROOP1_COND, TROOP1_LIST, TROOP1_LEN,
                   TROOP1_NPAGES, "t1");
    compare_ce_oracles("t1-ce");

    seed_btl(1);
    CHECK(bt.n_foes == 7, "troop1 has 7 members (%d)", bt.n_foes);
    kill_and_cascade(&mrg_tab, 5, "t1 left leg");

    load_tab(44);
    compare_oracle(44, TROOP44_COND, TROOP44_LIST, TROOP44_LEN,
                   TROOP44_NPAGES, "t44");
    compare_ce_oracles("t44-ce");

    seed_btl(44);
    CHECK(bt.n_foes == 6, "troop44 has 6 members (%d)", bt.n_foes);
    kill_and_cascade(&mrg_tab, 4, "t44 left leg");

    load_tab(1);


    {
        seed_btl(1);
        Gsw[3155] = 1;
        int flags[16] = {0};
        BtActorHp ac;
        ac.id = 1;
        ac.hp = 100;
        ac.maxhp = 100;
        int pg = -1;
        for (int i = 0; i < mrg_tab.npages; i++) {
            if (flags[i]) continue;
            if (bt_page_fire(&mrg_tab.cond[i], 0, 0, &bt.f[1], bt.n_foes, &ac,
                             1, Gsw)) {
                pg = i;
                break;
            }
        }
        CHECK(pg == 5, "dismember: battle-start setup page (%d)", pg);
        run_page(&mrg_tab, 5, flags);
        CHECK(bt.f[1].ref == 486, "dismember: torso transformed (%d)",
              bt.f[1].ref);
        CHECK(bt.f[1].hp == 2500 && bt.f[1].maxhp == 2500,
              "dismember: torso full at new max (%d/%d)", bt.f[1].hp,
              bt.f[1].maxhp);
        CHECK(bt.f[4].ref == 485 && bt.f[3].ref == 484,
              "dismember: hands transformed (%d/%d)", bt.f[4].ref,
              bt.f[3].ref);

        BtF *leg = &bt.f[1 + 5];
        for (int r = 0; r < 3; r++) {
            int c, m, e;
            bt_strike(&bt, &sk_atk, &bt.f[0], leg, NULL, NULL, &c, &m,
                      &e);
        }
        CHECK(!leg->alive, "dismember: leg dies");


        int alive = 0;
        for (int i = 0; i < bt.n_foes; i++) if (bt.f[1 + i].alive) alive++;
        CHECK(alive == 6, "dismember: 6 alive after leg kill (%d)", alive);
        for (int round = 0; round < 10; round++) {
            ac.hp = bt.f[0].hp;
            int p2 = -1;
            for (int i = 0; i < mrg_tab.npages; i++) {
                if (flags[i]) continue;
                if (bt_page_fire(&mrg_tab.cond[i], 1, 1, &bt.f[1], bt.n_foes,
                                 &ac, 1, Gsw)) {
                    p2 = i;
                    break;
                }
            }
            if (p2 < 0) break;
            run_page(&mrg_tab, p2, flags);
            if (bt.over) break;
        }
        CHECK(bt.over == 0, "dismember: no round-1 wipe (over=%d)",
              bt.over);


        int guard = 0;
        while (bt.f[1].hp > 1875 && guard++ < 40) {
            int c, m, e;
            bt_strike(&bt, &sk_atk, &bt.f[0], &bt.f[1], NULL, NULL, &c,
                      &m, &e);
        }
        CHECK(bt.f[1].hp <= 1875 && bt.f[1].alive,
              "dismember: torso ground to %d", bt.f[1].hp);
        for (int round = 0; round < 10 && !bt.over; round++) {
            ac.hp = bt.f[0].hp;
            int p2 = -1;
            for (int i = 0; i < mrg_tab.npages; i++) {
                if (flags[i]) continue;
                if (bt_page_fire(&mrg_tab.cond[i], 1, 1, &bt.f[1], bt.n_foes,
                                 &ac, 1, Gsw)) {
                    p2 = i;
                    break;
                }
            }
            if (p2 < 0) break;
            run_page(&mrg_tab, p2, flags);
        }
        {
            int a2 = 0;
            for (int i = 0; i < bt.n_foes; i++)
                if (bt.f[1 + i].alive) a2++;
            CHECK(a2 == 0, "dismember: torso destroy wipes (%d alive)",
                  a2);
            CHECK(flags[7] != 0, "dismember: pg7 (TORSO DESTROYED) ran");
        }
    }


    {
        int r40 = skillrow(40), r11 = skillrow(11);
        CHECK(r40 >= 0 && SKILL_DB[r40].occ == 1, "Run(40) occasion-1");
        CHECK(r11 >= 0 && SKILL_DB[r11].occ == 1, "Talk(11) occasion-1");
        CHECK(SKILL_DB[r40].scope == 8, "Run scope 8 (self, never aimed)");
        CHECK(SKILL_DB[r11].scope == 2, "Talk scope 2 (all foes)");
    }

    {
        FhInterp ce;
        { int cl = 0; const FhCmd *cc = mrg_ce(14, &cl); CHECK(cc && cl > 0, "union carries CE14"); if (cc) run_ce(cc, cl, &ce); }
        CHECK(ce.sw[52] != 0, "CE14 (Talk) sets sw52");
        { int cl = 0; const FhCmd *cc = mrg_ce(46, &cl); CHECK(cc && cl > 0, "union carries CE46"); if (cc) run_ce(cc, cl, &ce); }
        CHECK(ce.sw[200] != 0 && ce.sw[215] != 0,
              "CE46 (Run!) arms sw200/sw215 (%d/%d)", ce.sw[200],
              ce.sw[215]);
    }


    {
        const TroopTab *t = &mrg_tab;
        FhInterp ce;
        { int cl = 0; const FhCmd *cc = mrg_ce(46, &cl); CHECK(cc && cl > 0, "union carries CE46"); if (cc) run_ce(cc, cl, &ce); }
        CHECK(ce.sw[200] != 0, "run chain: CE46 arms sw200");

        int flags[16] = {0};
        flags[5] = 1;
        BtActorHp ac;
        ac.id = 1;
        ac.hp = 100;
        ac.maxhp = 100;
        int pg = -1;
        for (int i = 0; i < t->npages; i++) {
            if (flags[i]) continue;
            if (bt_page_fire(&t->cond[i], 0, 0, &bt.f[1], bt.n_foes, &ac,
                             1, ce.sw)) {
                pg = i;
                break;
            }
        }
        CHECK(pg == 4, "run chain: moment scan finds the escape page (%d)",
              pg);

        int saw_win = 0, saw_fail = 0;
        for (unsigned seed = 1; seed < 5000 && !(saw_win && saw_fail);
             seed++) {
            FhInterp p;
            fh_interp_init(&p, t->lists[4], t->lens[4]);
            p.rng = seed;
            memcpy(p.sw, ce.sw, sizeof(p.sw));
            for (;;) {
                int r = fh_interp_step(&p);
                if (r == FH_RUN_END) break;
                if (r == FH_RUN_CHOICE) p.await_choice = 0;
            }
            if (p.battle.pending == 2) {
                saw_win = 1;
            } else {
                saw_fail = 1;
                CHECK(p.sw[200] == 0 && p.sw[215] == 0,
                      "run chain: fail disarms sw200/sw215");
            }
        }
        CHECK(saw_win, "run chain: escape can succeed (aborts pre-round)");
        CHECK(saw_fail, "run chain: escape can fail (foes then act)");
    }


    {
        const TroopTab *t = &mrg_tab;
        FhInterp ce;
        { int cl = 0; const FhCmd *cc = mrg_ce(14, &cl); CHECK(cc && cl > 0, "union carries CE14"); if (cc) run_ce(cc, cl, &ce); }
        int flags[16] = {0};
        flags[5] = 1;
        BtActorHp ac;
        ac.id = 1;
        ac.hp = 100;
        ac.maxhp = 100;
        int pg = -1;
        for (int i = 0; i < t->npages; i++) {
            if (flags[i]) continue;
            if (bt_page_fire(&t->cond[i], 0, 0, &bt.f[1], bt.n_foes, &ac,
                             1, ce.sw)) {
                pg = i;
                break;
            }
        }
        CHECK(pg == 0, "talk chain: moment scan finds TALK page (%d)",
              pg);
        if (pg == 0) {
            FhInterp p;
            fh_interp_init(&p, t->lists[0], t->lens[0]);
            p.rng = 77;
            memcpy(p.sw, ce.sw, sizeof(p.sw));
            int steps = 0, ended = 0;
            for (;;) {
                int r = fh_interp_step(&p);
                if (r == FH_RUN_END) { ended = 1; break; }
                if (r == FH_RUN_CHOICE) p.await_choice = 0;
                if (++steps > 300000) break;
            }
            CHECK(ended, "talk chain: TALK page plays through");
            CHECK(p.battle.pending == 0, "talk chain: no abort");
            CHECK(p.sw[52] == 0, "talk chain: pg0 clears sw52");
        }
    }

    if (fails) printf("%d FAILURES\n", fails);
    else printf("ALL PASS\n");
    return fails != 0;
}
