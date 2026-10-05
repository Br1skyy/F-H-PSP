/* Battle rule regressions.
 *
 * Each block pins one behavior of RPG Maker MV's Game_Action / BattleManager
 * that the port used to get wrong. Expected values are worked out by hand from
 * the MV rules (noted per block), not copied from the C code's own output.
 *
 * Compile with -DOLD_API_ONLY to run only the blocks that use API that
 * already existed before the fixes (useful to prove a bug is real).
 */
#include <stdio.h>
#include <string.h>
#include "../runtime/battle.h"

static int fails = 0;
#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { printf("FAIL: " fmt "\n", ##__VA_ARGS__); fails++; } \
} while (0)

static BtF mk(int atk, int def, int agi) {
    BtF f;
    memset(&f, 0, sizeof(f));
    f.maxhp = f.hp = 100;
    f.maxmp = f.mp = 100;
    f.atk = atk;
    f.def = def;
    f.agi = agi;
    f.hit = 1.0;
    f.pdr = f.mdr = f.grd = 1.0;
    for (int i = 0; i < BT_ERATE_N; i++) f.erate[i] = 1.0;
    f.level = 2;
    f.alive = 1;
    return f;
}

static const BtIns PROG_45[] = { {0, 0, 45.0}, {21, 0, 0.0} };

static BtSkill skill45(int dmg_type) {
    /* id, hit_type(0 certain), scope, speed, variance 0, element, dmg_type,
       crit 0, success 100 */
    BtSkill s = {9, 0, 1, 0, 0, 0, dmg_type, 0, 100, PROG_45, 2};
    return s;
}

static void t_ai_filter(void) {
    /* MV Game_Enemy.makeActions: only actions rated above (max-3) are kept
       before the weighted pick. Ratings 10,10,5,5 -> zero=7 -> 5s are dropped
       and the two 10s must split 50/50. (The old code let the dropped 5s add
       negative weight, so index 1 was never chosen.) */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 12345);
    BtAiAct a[] = {{1, 10, 0, 0, 0}, {2, 10, 0, 0, 0}, {3, 5, 0, 0, 0}, {4, 5, 0, 0, 0}};
    int cnt[4] = {0, 0, 0, 0};
    for (int i = 0; i < 20000; i++) {
        int p = bt_ai_pick(&bt, a, 4, 1, 1, NULL);
        if (p >= 0 && p < 4) cnt[p]++;
    }
    CHECK(cnt[2] == 0 && cnt[3] == 0, "low-rated actions picked: %d %d", cnt[2], cnt[3]);
    CHECK(cnt[0] > 9000 && cnt[0] < 11000, "10-rated split %d/%d", cnt[0], cnt[1]);

    /* ratings 5,5,1 -> zero=2 -> weights 3,3 (the 1 is out): 50/50 */
    BtAiAct b[] = {{1, 5, 0, 0, 0}, {2, 5, 0, 0, 0}, {3, 1, 0, 0, 0}};
    int c2[3] = {0, 0, 0};
    for (int i = 0; i < 20000; i++) {
        int p = bt_ai_pick(&bt, b, 3, 1, 1, NULL);
        if (p >= 0 && p < 3) c2[p]++;
    }
    CHECK(c2[2] == 0, "1-rated action picked %d times", c2[2]);
    CHECK(c2[0] > 9000 && c2[0] < 11000, "5/5 split %d/%d", c2[0], c2[1]);

    /* weights 3,2,1 for ratings 5,4,3 (zero=2): 50% / 33% / 17% */
    BtAiAct c[] = {{1, 5, 0, 0, 0}, {2, 4, 0, 0, 0}, {3, 3, 0, 0, 0}};
    int c3[3] = {0, 0, 0};
    for (int i = 0; i < 30000; i++) c3[bt_ai_pick(&bt, c, 3, 1, 1, NULL)]++;
    CHECK(c3[0] > 14000 && c3[0] < 16000 && c3[2] > 4000 && c3[2] < 6000,
          "5/4/3 split %d/%d/%d", c3[0], c3[1], c3[2]);
}

static void t_fx_buff_removal(void) {
    /* MV itemEffectRemoveBuff / RemoveDebuff only clear the matching kind. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 1);
    BtF a = mk(10, 10, 10), b = mk(10, 10, 10);
    int res[8], nres = 0;
    BtFx rm_buff[] = {{33, 2, 0.0, 0}}, rm_debuff[] = {{34, 2, 0.0, 0}};

    b.buff[2] = -1;
    bt_apply_fx(&bt, &a, &b, rm_buff, 1, 1, 0, 1.0, res, &nres);
    CHECK(b.buff[2] == -1, "remove-BUFF wiped a debuff (%d)", b.buff[2]);
    bt_apply_fx(&bt, &a, &b, rm_debuff, 1, 1, 0, 1.0, res, &nres);
    CHECK(b.buff[2] == 0, "remove-DEBUFF failed (%d)", b.buff[2]);

    b.buff[2] = 1;
    bt_apply_fx(&bt, &a, &b, rm_debuff, 1, 1, 0, 1.0, res, &nres);
    CHECK(b.buff[2] == 1, "remove-DEBUFF wiped a buff (%d)", b.buff[2]);
    bt_apply_fx(&bt, &a, &b, rm_buff, 1, 1, 0, 1.0, res, &nres);
    CHECK(b.buff[2] == 0, "remove-BUFF failed (%d)", b.buff[2]);
}

static void t_fx_recover(void) {
    /* MV: value = floor((mhp*v1 + v2) * rec * (item ? pha : 1)) -- ONE floor.
       maxhp 159: 15.9+5=20.9; x1.5 = 31.35 -> 31. Two floors gave 30. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 1);
    BtF a = mk(10, 10, 10), b = mk(10, 10, 10);
    int res[8], nres = 0;
    b.maxhp = 159; b.hp = 10;
    BtFx heal[] = {{11, 0, 0.1, 5}};
    bt_apply_fx(&bt, &a, &b, heal, 1, 1, 1, 1.5, res, &nres);
    CHECK(b.hp == 41, "item heal with pha: hp=%d (want 41)", b.hp);

    b.maxmp = 159; b.mp = 10;
    BtFx mheal[] = {{12, 0, 0.1, 5}};
    bt_apply_fx(&bt, &a, &b, mheal, 1, 1, 1, 1.5, res, &nres);
    CHECK(b.mp == 41, "item mp heal with pha: mp=%d (want 41)", b.mp);

    /* a negative "recover" effect (damaging potion) must clamp and kill,
       like MV's gainHp -> refresh() does. */
    BtF v = mk(10, 10, 10);
    v.hp = 50;
    BtFx kill[] = {{11, 0, -1.0, 0}};
    bt_apply_fx(&bt, &a, &v, kill, 1, 1, 0, 1.0, res, &nres);
    CHECK(v.hp == 0 && !v.alive && bt_has_state(&v, 1),
          "negative recover: hp=%d alive=%d", v.hp, v.alive);

    BtF m = mk(10, 10, 10);
    m.mp = 30;
    BtFx drain[] = {{12, 0, -1.0, 0}};
    bt_apply_fx(&bt, &a, &m, drain, 1, 1, 0, 1.0, res, &nres);
    CHECK(m.mp == 0, "negative mp recover: mp=%d", m.mp);
}

static void t_revive(void) {
    /* MV removeState(death) -> revive(): hp 0 becomes 1. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 1);
    BtF a = mk(10, 10, 10), b = mk(10, 10, 10);
    int res[8], nres = 0;
    b.hp = 0; bt_check_dead(&b);
    CHECK(!b.alive && bt_has_state(&b, 1), "setup: should be dead");
    BtFx rev[] = {{22, 1, 1.0, 0}};
    bt_apply_fx(&bt, &a, &b, rev, 1, 1, 0, 1.0, res, &nres);
    CHECK(b.alive && b.hp == 1 && !bt_has_state(&b, 1),
          "revive: alive=%d hp=%d", b.alive, b.hp);

    /* heal-then-revive ordering used by Phoenix-style items */
    BtF c = mk(10, 10, 10);
    c.hp = 0; bt_check_dead(&c);
    BtFx both[] = {{11, 0, 0.5, 0}, {22, 1, 1.0, 0}};
    bt_apply_fx(&bt, &a, &c, both, 2, 1, 1, 1.0, res, &nres);
    CHECK(c.alive && c.hp == 50, "heal+revive: alive=%d hp=%d", c.alive, c.hp);
}

static void t_drain_caps(void) {
    /* MV executeHpDamage: drain value = min(target.hp, value);
       executeMpDamage: value = min(target.mp, value) unless MP recover. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 7);
    int c, m, e;
    BtSkill hpdrain = skill45(5), mpdrain = skill45(6), mpdmg = skill45(2);

    BtF a = mk(10, 10, 10), b = mk(10, 10, 10);
    a.hp = 10; b.hp = 20;
    int d = bt_strike(&bt, &hpdrain, &a, &b, NULL, NULL, &c, &m, &e);
    CHECK(d == 20 && b.hp == 0 && !b.alive && a.hp == 30,
          "hp drain overkill: dmg=%d b.hp=%d a.hp=%d (want 20/0/30)", d, b.hp, a.hp);

    BtF a2 = mk(10, 10, 10), b2 = mk(10, 10, 10);
    a2.mp = 5; b2.mp = 10;
    d = bt_strike(&bt, &mpdrain, &a2, &b2, NULL, NULL, &c, &m, &e);
    CHECK(d == 10 && b2.mp == 0 && a2.mp == 15,
          "mp drain: dmg=%d b.mp=%d a.mp=%d (want 10/0/15)", d, b2.mp, a2.mp);

    BtF a3 = mk(10, 10, 10), b3 = mk(10, 10, 10);
    b3.mp = 10;
    d = bt_strike(&bt, &mpdmg, &a3, &b3, NULL, NULL, &c, &m, &e);
    CHECK(d == 10 && b3.mp == 0, "mp damage cap: dmg=%d mp=%d", d, b3.mp);
}

static void t_escape_ratio(void) {
    /* MV: ratio is set once (0.5*party/troop) and grows +0.1 per failure, so
       a 5% start must reach certainty within 10 failures. */
    int worst = 0;
    for (unsigned seed = 1; seed <= 300; seed++) {
        Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, seed);
        int tries = 0;
        while (!bt_escape(&bt, 10, 100) && tries < 100) tries++;
        if (tries > worst) worst = tries;
    }
    CHECK(worst <= 10, "escape never ramps up: worst case %d failures", worst);

    /* a pre-seeded ratio is honoured */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 3);
    bt.escape_ratio = 2.0;
    CHECK(bt_escape(&bt, 1, 1000) == 1 && bt.over == 3, "seeded ratio ignored");
}

static void t_order_uses_buffed_agi(void) {
    Bt bt; memset(&bt, 0, sizeof(bt));
    bt.n_party = 1; bt.n_foes = 1;
    bt.f[0] = mk(10, 10, 10);
    bt.f[1] = mk(10, 10, 12); bt.f[1].is_foe = 1;
    int out[4];
    bt_order(&bt, out, 4);
    CHECK(out[0] == 1, "unbuffed: foe (agi 12) should act first");
    bt.f[0].buff[6] = 2;                       /* agi +2 stages: 10 -> 15 */
    bt_order(&bt, out, 4);
    CHECK(out[0] == 0, "agi buff ignored in turn order");
}

static void t_vm_safety(void) {
    /* Malformed programs must not read/write outside the stack, and variable
       or switch indices outside the arrays must read as 0. */
    BtF a = mk(10, 10, 10), b = mk(10, 10, 10);
    BtIns under[] = { {1, 0, 0.0}, {21, 0, 0.0} };          /* ADD on empty stack */
    CHECK(bt_vm(under, 2, &a, &b, NULL, NULL, NULL) == 0.0, "stack underflow not safe");

    BtIns over[40];
    for (int i = 0; i < 40; i++) { over[i].op = 0; over[i].arg = 0; over[i].imm = 1.0; }
    CHECK(bt_vm(over, 40, &a, &b, NULL, NULL, NULL) == 0.0, "stack overflow not safe");

#ifndef OLD_API_ONLY
    static int32_t vars[BT_VM_VARS];
    static unsigned char sw[BT_VM_SWITCHES];
    vars[3] = 42; sw[7] = 1;
    BtIns v3[] = { {0, 0, 3.0}, {16, 0, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(v3, 3, &a, &b, vars, sw, NULL) == 42.0, "variable read");
    BtIns s7[] = { {0, 0, 7.0}, {17, 0, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(s7, 3, &a, &b, vars, sw, NULL) == 1.0, "switch read");
    BtIns vbig[] = { {0, 0, (double)BT_VM_VARS}, {16, 0, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(vbig, 3, &a, &b, vars, sw, NULL) == 0.0, "variable index past end");
    BtIns sbig[] = { {0, 0, 1e9}, {17, 0, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(sbig, 3, &a, &b, vars, sw, NULL) == 0.0, "switch index past end");
    BtIns vneg[] = { {0, 0, -5.0}, {16, 0, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(vneg, 3, &a, &b, vars, sw, NULL) == 0.0, "negative variable index");
#endif
}

#ifndef OLD_API_ONLY

static void t_param_rounding(void) {
    /* MV Game_BattlerBase.param: Math.round(value), not truncation. */
    CHECK(bt_param(33, 0, 1.5, 0) == 50, "49.5 should round to 50, got %d",
          bt_param(33, 0, 1.5, 0));
    CHECK(bt_param(30, 27, 1.0, 1) == 71, "71.25 -> 71");
    CHECK(bt_param(10, 0, 1.0, -2) == 5, "debuff x2 -> 5");
    CHECK(bt_param(10, 0, 1.0, 9) == 15, "stage clamps at +2 -> 15");
}

static void t_buffs_affect_stats(void) {
    /* Buff stages must change the stats formulas read. (Before, effect codes
       31/32 changed buff[] but nothing ever consumed it.) a.atk = program op
       14 arg 0; a.agi = op 14 arg 4. */
    BtF a = mk(50, 10, 10), b = mk(10, 10, 10);
    BtIns atk[] = { {14, 0, 0.0}, {21, 0, 0.0} };
    BtIns defb[] = { {15, 1, 0.0}, {21, 0, 0.0} };
    CHECK(bt_vm(atk, 2, &a, &b, NULL, NULL, NULL) == 50.0, "unbuffed atk");
    a.buff[2] = 1;
    CHECK(bt_vm(atk, 2, &a, &b, NULL, NULL, NULL) == 63.0, "atk +1 (62.5 rounds up)");
    a.buff[2] = -2;
    CHECK(bt_vm(atk, 2, &a, &b, NULL, NULL, NULL) == 25.0, "atk -2");
    b.buff[3] = 2; b.def = 20;
    CHECK(bt_vm(defb, 2, &a, &b, NULL, NULL, NULL) == 30.0, "target def +2");

    /* MV clamps every non-MMP parameter to at least 1 */
    BtF z = mk(0, 0, 0);
    CHECK(bt_vm(atk, 2, &z, &b, NULL, NULL, NULL) == 1.0, "zero atk reads as 1");
    CHECK(bt_stat(&z, 3) == 1, "bt_stat floor");
    CHECK(bt_stat(&a, 2) == 25, "bt_stat atk -2");
}

static void t_buff_turns(void) {
    /* MV addBuff: +1 stage (cap 2), turns only ever lengthen; updateBuffTurns
       counts down at turn end and removeBuffsAuto drops expired buffs. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 1);
    bt.n_party = 1; bt.n_foes = 1;
    bt.f[0] = mk(10, 10, 10);
    bt.f[1] = mk(10, 10, 10); bt.f[1].is_foe = 1;
    BtF *a = &bt.f[0], *b = &bt.f[1];
    int res[8], nres = 0;

    BtFx up3[] = {{31, 2, 3.0, 0}}, up1[] = {{31, 2, 1.0, 0}};
    bt_apply_fx(&bt, a, b, up3, 1, 1, 0, 1.0, res, &nres);
    CHECK(b->buff[2] == 1 && b->buff_turns[2] == 3, "buff +1/3t: %d/%d", b->buff[2], b->buff_turns[2]);
    bt_apply_fx(&bt, a, b, up1, 1, 1, 0, 1.0, res, &nres);
    CHECK(b->buff[2] == 2 && b->buff_turns[2] == 3, "re-buff keeps longer timer: %d/%d", b->buff[2], b->buff_turns[2]);
    bt_apply_fx(&bt, a, b, up1, 1, 1, 0, 1.0, res, &nres);
    CHECK(b->buff[2] == 2, "buff caps at +2 (%d)", b->buff[2]);

    bt_round_end(&bt);
    bt_round_end(&bt);
    CHECK(b->buff[2] == 2 && b->buff_turns[2] == 1, "after 2 rounds: %d/%d", b->buff[2], b->buff_turns[2]);
    bt_round_end(&bt);
    CHECK(b->buff[2] == 0 && b->buff_turns[2] == 0, "expired: %d/%d", b->buff[2], b->buff_turns[2]);

    /* debuff, certain-hit (luck rate 1.0 when luk is equal) */
    BtFx dn2[] = {{32, 3, 2.0, 0}};
    bt_apply_fx(&bt, a, b, dn2, 1, 1, 0, 1.0, res, &nres);
    CHECK(b->buff[3] == -1 && b->buff_turns[3] == 2, "debuff: %d/%d", b->buff[3], b->buff_turns[3]);
    bt_round_end(&bt);
    bt_round_end(&bt);
    CHECK(b->buff[3] == 0, "debuff expired (%d)", b->buff[3]);

    /* a buff cannot land on a dead battler */
    BtF dead = mk(10, 10, 10);
    dead.hp = 0; bt_check_dead(&dead);
    bt_apply_fx(&bt, a, &dead, up3, 1, 1, 0, 1.0, res, &nres);
    CHECK(dead.buff[2] == 0, "buffed a corpse");
}

static void t_ai_conditions(void) {
    /* MV Game_Enemy.meetsCondition types 2-5. HP/MP params are percent. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 99);
    BtF self = mk(10, 10, 10);
    BtAiAct hp_low[] = {{1, 5, 2, 0, 50}, {2, 5, 0, 0, 0}};  /* HP 0..50% or anything */

    int only_b = 1, saw_a = 0;
    self.hp = 100;                                  /* 100% -> A not allowed */
    for (int i = 0; i < 2000; i++)
        if (bt_ai_pick_for(&bt, &self, hp_low, 2, 1, 1, NULL) != 1) only_b = 0;
    CHECK(only_b, "HP-gated action used at full health");

    self.hp = 50;                                   /* boundary is inclusive */
    for (int i = 0; i < 2000; i++)
        if (bt_ai_pick_for(&bt, &self, hp_low, 2, 1, 1, NULL) == 0) saw_a = 1;
    CHECK(saw_a, "HP-gated action unavailable at exactly 50%%");

    self.hp = 51; saw_a = 0;
    for (int i = 0; i < 2000; i++)
        if (bt_ai_pick_for(&bt, &self, hp_low, 2, 1, 1, NULL) == 0) saw_a = 1;
    CHECK(!saw_a, "HP-gated action available at 51%%");

    BtAiAct mp_hi[] = {{1, 5, 3, 25, 100}};          /* MP 25..100% */
    self.mp = 10;
    CHECK(bt_ai_pick_for(&bt, &self, mp_hi, 1, 1, 1, NULL) == -1, "MP condition low");
    self.mp = 25;
    CHECK(bt_ai_pick_for(&bt, &self, mp_hi, 1, 1, 1, NULL) == 0, "MP condition at 25%%");

    BtAiAct st[] = {{1, 5, 4, 13, 0}};               /* has state 13 */
    CHECK(bt_ai_pick_for(&bt, &self, st, 1, 1, 1, NULL) == -1, "state cond w/o state");
    bt_add_state(&self, 13);
    CHECK(bt_ai_pick_for(&bt, &self, st, 1, 1, 1, NULL) == 0, "state cond with state");

    BtAiAct lv[] = {{1, 5, 5, 4, 0}};                /* party level >= 4 */
    CHECK(bt_ai_pick_for(&bt, &self, lv, 1, 3, 1, NULL) == -1, "party level 3 vs >=4");
    CHECK(bt_ai_pick_for(&bt, &self, lv, 1, 4, 1, NULL) == 0, "party level 4 vs >=4");

    /* switch index past the array must not be read */
    unsigned char *nosw = NULL;
    BtAiAct swbad[] = {{1, 5, 6, 99999, 0}};
    CHECK(bt_ai_pick_for(&bt, &self, swbad, 1, 1, 1, nosw) == -1, "switch cond, no switches");

    /* the old entry point still works; without the enemy it cannot test HP */
    self.hp = 100;
    CHECK(bt_ai_pick(&bt, hp_low, 2, 1, 1, NULL) >= 0, "legacy bt_ai_pick");
}

static void t_ai_many_actions(void) {
    /* Enemies may have more than 8 actions; none may be silently dropped. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 5);
    BtAiAct many[20];
    for (int i = 0; i < 20; i++) { BtAiAct x = {i + 1, 5, 0, 0, 0}; many[i] = x; }
    int seen[20] = {0};
    for (int i = 0; i < 20000; i++) {
        int p = bt_ai_pick(&bt, many, 20, 1, 1, NULL);
        if (p >= 0 && p < 20) seen[p]++;
    }
    for (int i = 0; i < 20; i++)
        CHECK(seen[i] > 500, "action %d (of 20) picked %d times", i, seen[i]);
}

static void t_targets(void) {
    /* MV Game_Action.makeTargets. Indices: 0 = actor, 1..4 = foes. */
    Bt bt; memset(&bt, 0, sizeof(bt)); bt_srand(&bt, 11);
    bt.n_party = 1; bt.n_foes = 4;
    bt.f[0] = mk(10, 10, 10);
    for (int i = 1; i <= 4; i++) { bt.f[i] = mk(10, 10, 10); bt.f[i].is_foe = 1; }
    int out[16], n;

    n = bt_make_targets(&bt, 0, 1, 3, out, 16);
    CHECK(n == 1 && out[0] == 3, "single target: n=%d t=%d", n, out[0]);

    bt.f[3].hp = 0; bt_check_dead(&bt.f[3]);
    n = bt_make_targets(&bt, 0, 1, 3, out, 16);
    CHECK(n == 1 && out[0] == 1, "dead selection should fall to first alive: n=%d t=%d", n, out[0]);

    n = bt_make_targets(&bt, 0, 2, 1, out, 16);
    CHECK(n == 3 && out[0] == 1 && out[1] == 2 && out[2] == 4, "all enemies n=%d", n);

    for (int scope = 3; scope <= 6; scope++) {
        n = bt_make_targets(&bt, 0, scope, 1, out, 16);
        int ok = (n == scope - 2);
        for (int i = 0; i < n; i++)
            if (out[i] < 1 || out[i] > 4 || out[i] == 3) ok = 0;
        CHECK(ok, "random scope %d: n=%d (want %d, alive foes only)", scope, n, scope - 2);
    }

    n = bt_make_targets(&bt, 0, 0, 1, out, 16);
    CHECK(n == 0, "scope 0 should target nobody (n=%d)", n);
    n = bt_make_targets(&bt, 0, 11, 1, out, 16);
    CHECK(n == 1 && out[0] == 0, "scope 11 = user");
    n = bt_make_targets(&bt, 0, 7, 0, out, 16);
    CHECK(n == 1 && out[0] == 0, "lone actor, one ally = self");
    n = bt_make_targets(&bt, 0, 8, 0, out, 16);
    CHECK(n == 1 && out[0] == 0, "lone actor, all allies = self");

    /* foe user: opponents are the party, allies are the other foes */
    n = bt_make_targets(&bt, 2, 1, 0, out, 16);
    CHECK(n == 1 && out[0] == 0, "foe single-target hits the actor");
    n = bt_make_targets(&bt, 2, 4, 0, out, 16);
    CHECK(n == 2 && out[0] == 0 && out[1] == 0, "foe 2-random hits lone actor twice (n=%d)", n);
    n = bt_make_targets(&bt, 2, 8, 2, out, 16);
    CHECK(n == 3 && out[0] == 1 && out[1] == 2 && out[2] == 4, "foe all-allies skips the dead (n=%d)", n);
    n = bt_make_targets(&bt, 2, 7, 2, out, 16);
    CHECK(n == 1 && out[0] == 2, "foe one-ally with self selected");
    n = bt_make_targets(&bt, 2, 11, 2, out, 16);
    CHECK(n == 1 && out[0] == 2, "foe user scope");

    /* dead-friend scopes (revive skills) */
    n = bt_make_targets(&bt, 2, 9, 3, out, 16);
    CHECK(n == 1 && out[0] == 3, "one dead ally (n=%d)", n);
    n = bt_make_targets(&bt, 2, 10, 3, out, 16);
    CHECK(n == 1 && out[0] == 3, "all dead allies (n=%d)", n);

    /* nobody left to hit */
    for (int i = 1; i <= 4; i++) { bt.f[i].hp = 0; bt_check_dead(&bt.f[i]); }
    n = bt_make_targets(&bt, 0, 2, 1, out, 16);
    CHECK(n == 0, "all foes dead -> no targets (n=%d)", n);
    n = bt_make_targets(&bt, 0, 4, 1, out, 16);
    CHECK(n == 0, "random with none alive (n=%d)", n);
    n = bt_make_targets(&bt, 0, 2, 1, out, 0);
    CHECK(n == 0, "max=0");
}

static void t_hit_evasion(void) {
    /* MV itemEva: physical rolls vs target EVA, magical vs target MEV,
       certain-hit vs nothing. The port used 0 for magical, so every
       magical skill (e.g. Hurting) always connected. */
    Bt bt; memset(&bt, 0, sizeof(bt));
    BtSkill phys = {1, 1, 1, 0, 0, 0, 1, 0, 100, PROG_45, 2};
    BtSkill mag = {12, 2, 1, 0, 0, 6, 1, 0, 100, PROG_45, 2};
    BtSkill cert = {4, 0, 1, 0, 0, 3, 1, 0, 100, PROG_45, 2};
    BtF sub = mk(10, 10, 10);
    sub.hit = 1.0;
    BtF t = mk(10, 10, 10);
    int c, m, e;

    /* Edges are deterministic (no RNG dependence). */
    t.mev = 1.0; t.eva = 0.0;
    bt_srand(&bt, 1);
    bt_strike(&bt, &mag, &sub, &t, NULL, NULL, &c, &m, &e);
    CHECK(!m && e, "magical vs mev=1 must evade (m=%d e=%d)", m, e);

    t.mev = 0.0; t.eva = 1.0;    /* physical EVA must not catch magic */
    bt_srand(&bt, 1);
    bt_strike(&bt, &mag, &sub, &t, NULL, NULL, &c, &m, &e);
    CHECK(!m && !e, "magical must ignore EVA (m=%d e=%d)", m, e);

    t.eva = 1.0; t.mev = 0.0;    /* MEV must not catch physical */
    bt_srand(&bt, 1);
    bt_strike(&bt, &phys, &sub, &t, NULL, NULL, &c, &m, &e);
    CHECK(!m && e, "physical vs eva=1 must evade (m=%d e=%d)", m, e);

    t.eva = 0.0; t.mev = 1.0;
    bt_srand(&bt, 1);
    bt_strike(&bt, &phys, &sub, &t, NULL, NULL, &c, &m, &e);
    CHECK(!m && !e, "physical must ignore MEV (m=%d e=%d)", m, e);

    t.eva = 1.0; t.mev = 1.0;
    bt_srand(&bt, 1);
    bt_strike(&bt, &cert, &sub, &t, NULL, NULL, &c, &m, &e);
    CHECK(!m && !e, "certain-hit ignores EVA/MEV (m=%d e=%d)", m, e);

    /* Mid rate: ~40% over a seeded run. */
    t.eva = 0.0; t.mev = 0.4;
    int n = 0;
    bt_srand(&bt, 9);
    for (int i = 0; i < 20000; i++) {
        t.hp = 100; t.alive = 1;
        bt_strike(&bt, &mag, &sub, &t, NULL, NULL, &c, &m, &e);
        n += e;
    }
    CHECK(n > 7000 && n < 9000, "mev=0.4 evaded %d/20000", n);
}

static void t_states_hit(void) {
    /* Dismemberment feeds accuracy through states: cutting the guard's
       legs puts Weakness (62: EVA/MEV -0.95) on the head, Blindness
       (93: HIT -0.75) blinds attackers. Base stats alone must not decide. */
    static const BtStateXp XP[] = {
        {62, 0, -0.95, -0.95, 0, 0, 1.5, 1, {1, 1, 1, 1, 1}},
        {93, -0.75, 0, 0, 0, 0, 1, 1, {1, 1, 1, 1, 1}},
    };
    Bt bt; memset(&bt, 0, sizeof(bt));
    bt.state_xp = XP; bt.n_state_xp = 2;
    BtSkill phys = {1, 1, 1, 0, 0, 0, 1, 0, 100, PROG_45, 2};
    BtF sub = mk(10, 10, 10);
    sub.hit = 0.97;
    BtF head = mk(10, 10, 10);
    head.eva = 0.55;    /* guard head, before the legs are cut */
    int c, m, e;

    /* Hard to hit: 0.97 x (1-0.55) ~= 44%/strike. */
    bt_srand(&bt, 21);
    int n = 0;
    for (int i = 0; i < 20000; i++) {
        head.hp = 100; head.alive = 1;
        bt_strike(&bt, &phys, &sub, &head, NULL, NULL, &c, &m, &e);
        n += (!m && !e);
    }
    CHECK(n > 7000 && n < 10500, "head w/o Weakness hit %d/20000", n);

    /* Legs cut -> Weakness on the head: 0.55-0.95 < 0, it barely evades. */
    bt_add_state(&head, 62);
    n = 0;
    bt_srand(&bt, 21);
    for (int i = 0; i < 20000; i++) {
        head.hp = 100; head.alive = 1;
        bt_strike(&bt, &phys, &sub, &head, NULL, NULL, &c, &m, &e);
        n += (!m && !e);
    }
    CHECK(n > 19000, "head with Weakness hit %d/20000", n);

    /* Blinded attacker (HIT 0.97-0.75=0.22) can barely land a thing. */
    BtF foe = mk(10, 10, 10);
    foe.eva = 0.05;
    bt_add_state(&sub, 93);
    n = 0;
    bt_srand(&bt, 21);
    for (int i = 0; i < 20000; i++) {
        foe.hp = 100; foe.alive = 1;
        bt_strike(&bt, &phys, &sub, &foe, NULL, NULL, &c, &m, &e);
        n += (!m && !e);
    }
    CHECK(n > 3000 && n < 5500, "blinded attacker hit %d/20000", n);
    bt_remove_state(&sub, 93);

    /* No table -> legacy behaviour, NULL-safe. */
    bt.state_xp = NULL; bt.n_state_xp = 0;
    n = 0;
    bt_srand(&bt, 21);
    for (int i = 0; i < 20000; i++) {
        head.hp = 100; head.alive = 1;
        bt_strike(&bt, &phys, &sub, &head, NULL, NULL, &c, &m, &e);
        n += (!m && !e);
    }
    CHECK(n > 7000 && n < 10500, "NULL table head hit %d/20000", n);
}

static void t_states_offense(void) {
    /* States also scale offense, MV paramRate/sparam style: Weakness takes
       1.5x physical (PDR), ATTACK UP deals from 1.5x ATK, No-criticals
       wipes crits. No battle data has MDR states today, so MDR rides on a
       synthetic row to prove the path. */
    static const BtStateXp XO[] = {
        {62, 0, -0.95, -0.95, 0, 0, 1.5, 1, {1, 1, 1, 1, 1}},
        {58, 0, 0, 0, 0, 0, 1, 1, {1.5, 1, 1, 1, 1}},
        {102, 0, 0, 0, -5, 0, 1, 1, {1, 1, 1, 1, 1}},
        {200, 0, 0, 0, 0, 0, 1, 0.5, {1, 1, 1, 1, 1}},
    };
    Bt bt; memset(&bt, 0, sizeof(bt));
    bt.state_xp = XO; bt.n_state_xp = 4;
    BtSkill phys = {1, 1, 1, 0, 0, 0, 1, 0, 100, PROG_45, 2};
    BtSkill mag = {12, 2, 1, 0, 0, 6, 1, 0, 100, PROG_45, 2};
    BtSkill physc = {1, 1, 1, 0, 0, 0, 1, 1, 100, PROG_45, 2};
    BtF sub = mk(10, 10, 10);
    sub.hit = 1.0;
    BtF tgt = mk(10, 10, 10);
    int c, m, e;

    /* Variance is 0 and hit is certain, so damage is exact. */
    bt_srand(&bt, 3);
    int d = bt_strike(&bt, &phys, &sub, &tgt, NULL, NULL, &c, &m, &e);
    CHECK(d == 45 && !m && !e, "plain physical %d", d);
    bt_add_state(&tgt, 62);
    bt_srand(&bt, 3);
    d = bt_strike(&bt, &phys, &sub, &tgt, NULL, NULL, &c, &m, &e);
    CHECK(d == 68 && !m && !e, "Weakness PDR x1.5: %d (want 68)", d);
    bt_remove_state(&tgt, 62);
    bt_add_state(&tgt, 200);
    tgt.hp = 100; tgt.alive = 1; bt_remove_state(&tgt, 1);
    bt_srand(&bt, 3);
    d = bt_strike(&bt, &mag, &sub, &tgt, NULL, NULL, &c, &m, &e);
    CHECK(d == 23 && !m && !e, "MDR x0.5: %d (want 23)", d);
    bt_remove_state(&tgt, 200);

    /* Param rates run through stats, formulas, order and luk. */
    BtF strong = mk(10, 10, 10);
    CHECK(bt_stat(&strong, 2) == 10, "legacy atk %d", bt_stat(&strong, 2));
    CHECK(bt_statx(&bt, &strong, 2) == 10, "unbuffed atk");
    bt_add_state(&strong, 58);
    CHECK(bt_statx(&bt, &strong, 2) == 15, "ATTACK UP atk");
    CHECK(bt_stat(&strong, 2) == 10, "legacy entry ignores states");
    bt_remove_state(&strong, 58);

    /* CRI 1.0 always crits; No-criticals (-5) must give zero, not wrap.
       Yanfly CriticalControl: x1.5 plus flat 1.5 x LUK (luk 10 here, so
       45 x 1.5 + 15 = 82.5 -> 83), not vanilla x3. */
    BtF killer = mk(10, 10, 10);
    killer.hit = 1.0; killer.cri = 1.0; killer.luk = 10;
    int ncrit = 0, dcrit = 0;
    bt_srand(&bt, 5);
    for (int i = 0; i < 50; i++) {
        tgt.hp = 100; tgt.alive = 1;
        int dd = bt_strike(&bt, &physc, &killer, &tgt, NULL, NULL, &c,
                           &m, &e);
        ncrit += c;
        if (i == 0) dcrit = dd;
    }
    CHECK(ncrit == 50, "cri=1 crits %d/50", ncrit);
    CHECK(dcrit == 83, "yanfly crit 45 -> %d (want 83)", dcrit);
    bt_add_state(&killer, 102);
    ncrit = 0;
    bt_srand(&bt, 5);
    for (int i = 0; i < 50; i++) {
        tgt.hp = 100; tgt.alive = 1;
        bt_strike(&bt, &physc, &killer, &tgt, NULL, NULL, &c, &m, &e);
        ncrit += c;
    }
    CHECK(ncrit == 0, "No-criticals still crit %d/50", ncrit);
    bt_remove_state(&killer, 102);
}

#endif

int main(void) {
    t_ai_filter();
    t_fx_buff_removal();
    t_fx_recover();
    t_revive();
    t_drain_caps();
    t_escape_ratio();
    t_order_uses_buffed_agi();
    t_vm_safety();
#ifndef OLD_API_ONLY
    t_param_rounding();
    t_buffs_affect_stats();
    t_buff_turns();
    t_ai_conditions();
    t_ai_many_actions();
    t_targets();
    t_hit_evasion();
    t_states_hit();
    t_states_offense();
#endif
    if (fails) printf("%d FAILURES\n", fails);
    else printf("ALL PASS\n");
    return fails != 0;
}
