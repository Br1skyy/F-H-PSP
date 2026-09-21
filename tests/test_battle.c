/* Golden-master: battle engine (runtime/battle.c) vs hand-derived OG values.
 * Damage pipeline, AI picks, turn order, escape, EXP curve, states.
 * Build: gcc -Wall -O2 -I runtime -o test_battle test_battle.c ../runtime/battle.c -lm && ./test_battle
 */
#include <stdio.h>
#include "../runtime/battle.h"

static int fails = 0;
#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { printf("FAIL: " fmt "\n", ##__VA_ARGS__); fails++; } \
} while (0)

static BtF mk(int atk, int def, int agi, double hit, double eva) {
    BtF f;
    __builtin_memset(&f, 0, sizeof(f));
    f.maxhp = f.hp = 100;
    f.atk = atk;
    f.def = def;
    f.agi = agi;
    f.hit = hit;
    f.eva = eva;
    f.pdr = f.mdr = f.grd = 1.0;
    for (int i = 0; i < BT_ERATE_N; i++) f.erate[i] = 1.0;
    f.atk_elem = 1;
    f.level = 2;
    f.alive = 1;
    return f;
}

static const BtIns PROG_ATK[] = {
    {14, 0, 0.0}, {0, 0, 4.0}, {3, 0, 0.0}, {15, 1, 0.0},
    {0, 0, 2.0}, {3, 0, 0.0}, {2, 0, 0.0}, {21, 0, 0.0},
};
static const BtSkill SK_ATK = {1, 1, 1, 0, 20, -1, 1, 0, PROG_ATK, 8};
static const BtIns PROG_45[] = { {0, 0, 45.0}, {21, 0, 0.0} };
static const BtSkill SK_THRUST = {4, 0, 1, 0, 20, 3, 1, 0, PROG_45, 2};

int main(void) {
    Bt bt;
    int crit, missed, evaded;

    /* 1. VM: a.atk*4-b.def*2 with atk 57, def 10 -> 208. */
    {
        BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.0);
        double v = bt_vm(PROG_ATK, 8, &a, &b, NULL, NULL);
        CHECK(v == 208.0, "vm base %f != 208", v);
    }

    /* 2. Full strike, fixed seed: base 208, variance +/-20%%, no crit. */
    {
        BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.0);
        b.maxhp = b.hp = 1300;
        bt_srand(&bt, 1234);
        int d = bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed,
                          &evaded);
        CHECK(!missed && !evaded && !crit, "flags m%d e%d c%d", missed,
              evaded, crit);
        CHECK(d >= 166 && d <= 250, "dmg %d out of variance range", d);
        CHECK(b.hp == 1300 - d, "hp %d", b.hp);
        printf("info: seeded strike dmg=%d\n", d);
    }

    /* 3. Zero hit always misses (guard left arm vs mercenary). */
    {
        BtF a = mk(10, 0, 10, 0.0, 0.0), b = mk(57, 36, 10, 0.97, 0.05);
        bt_srand(&bt, 7);
        int d = bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed,
                          &evaded);
        CHECK(missed && d == 0, "zero-hit should miss (m=%d d=%d)", missed,
              d);
    }

    /* 4. Certain hit ignores hit/eva (stinger thrust, element 3). */
    {
        BtF a = mk(10, 0, 10, 0.0, 0.0), b = mk(57, 36, 10, 0.97, 0.99);
        b.erate[3] = 1.0;
        bt_srand(&bt, 42);
        int d = bt_strike(&bt, &SK_THRUST, &a, &b, NULL, NULL, &crit,
                          &missed, &evaded);
        CHECK(!missed && !evaded, "certain hit must connect");
        CHECK(d >= 36 && d <= 54, "thrust %d out of 45+-20%%", d);
    }

    /* 5. Guard halves (same seed both runs; guarded == round(ung/2)). */
    {
        BtF a = mk(10, 0, 10, 0.0, 0.0), b = mk(57, 36, 10, 0.97, 0.0);
        b.guard = 1;
        bt_srand(&bt, 42);
        int d1, d2, c2, m2, e2;
        BtF b2 = b;
        b2.guard = 0;
        d1 = bt_strike(&bt, &SK_THRUST, &a, &b2, NULL, NULL, &crit, &missed,
                       &evaded);
        bt_srand(&bt, 42);
        d2 = bt_strike(&bt, &SK_THRUST, &a, &b, NULL, NULL, &c2, &m2, &e2);
        /* guarded path divides the identical pre-round value by 2 */
        CHECK(d2 * 2 >= d1 - 1 && d2 * 2 <= d1 + 1,
              "guard %d vs unguarded %d", d2, d1);
    }

    /* 6. Death records state 1 and clears alive. */
    {
        BtF b = mk(0, 0, 10, 1.0, 0.0);
        b.hp = 5;
        BtF a = mk(57, 0, 10, 1.0, 0.0);
        bt_srand(&bt, 9);
        bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed, &evaded);
        CHECK(!b.alive && b.hp == 0 && bt_has_state(&b, 1), "death not set");
    }

    /* 7. Turn order: agi desc; actor wins ties vs limbs. */
    {
        bt.n_party = 1;
        bt.n_foes = 2;
        bt.f[0] = mk(57, 36, 10, 0.97, 0.05);
        bt.f[0].is_foe = 0;
        bt.f[1] = mk(10, 10, 10, 0.0, 0.0);
        bt.f[1].is_foe = 1;
        bt.f[2] = mk(10, 10, 12, 0.0, 0.0);
        bt.f[2].is_foe = 1;
        int out[4], n = bt_order(&bt, out, 4);
        CHECK(n == 3 && out[0] == 2 && out[1] == 0 && out[2] == 1,
              "order wrong (%d,%d,%d)", out[0], out[1], out[2]);
    }

    /* 8. AI: torso idle with switches off; arm hacks; stinger valid pick. */
    {
        unsigned char sw[64] = {0};
        BtAiAct torso[] = {{6, 5, 6, 17, 0}};
        BtAiAct arm[] = {{3, 5, 0, 0, 0}};
        BtAiAct sting[] = {{4, 3, 1, 1, 1}, {5, 5, 0, 0, 0}, {5, 5, 0, 0, 0}};
        bt_srand(&bt, 3);
        CHECK(bt_ai_pick(&bt, torso, 1, 2, 1, sw) == -1, "torso should idle");
        CHECK(bt_ai_pick(&bt, arm, 1, 2, 1, sw) == 0, "arm should hack");
        int p = bt_ai_pick(&bt, sting, 3, 2, 1, sw);
        CHECK(p >= 0 && p < 3, "stinger pick %d", p);
    }

    /* 9. EXP curve (class [19,38,50,10]): 45/107/194/316 at lv 2..5. */
    {
        long e2 = bt_exp_for(2, 19, 38, 50, 10);
        long e3 = bt_exp_for(3, 19, 38, 50, 10);
        long e4 = bt_exp_for(4, 19, 38, 50, 10);
        long e5 = bt_exp_for(5, 19, 38, 50, 10);
        CHECK(e2 == 45 && e3 == 107 && e4 == 194 && e5 == 316,
              "exp %ld %ld %ld %ld", e2, e3, e4, e5);
    }

    /* 10. Buffed param: (30 + 27) * 1 * (1 + 0.25) at +1 stage. */
    CHECK(bt_param(30, 27, 1.0, 1) == 71, "buffed atk");

    if (fails) printf("%d FAILURES\n", fails);
    else printf("ALL PASS\n");
    return fails != 0;
}
