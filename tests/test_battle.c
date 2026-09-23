

#include <stdio.h>
#include "../runtime/battle.h"
#include <string.h>

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
static const BtSkill SK_ATK = {1, 1, 1, 0, 20, -1, 1, 0, 100, PROG_ATK, 8};
static const BtIns PROG_45[] = { {0, 0, 45.0}, {21, 0, 0.0} };
static const BtSkill SK_THRUST = {4, 0, 1, 0, 20, 3, 1, 0, 100, PROG_45, 2};

int main(void) {
    Bt bt;
    int crit, missed, evaded;


    {
        BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.0);
        double v = bt_vm(PROG_ATK, 8, &a, &b, NULL, NULL);
        CHECK(v == 208.0, "vm base %f != 208", v);
    }


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


    {
        BtF a = mk(10, 0, 10, 0.95, 0.05), b = mk(57, 36, 10, 0.97, 0.05);
        bt_srand(&bt, 7);
        int d = bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed,
                          &evaded);
        CHECK(!missed, "arm should usually hit (m=%d)", missed);
        (void)d;
    }


    {
        int evades = 0;
        for (int s = 0; s < 20; s++) {
            BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.55);
            bt_srand(&bt, 100 + (unsigned)s);
            int d = bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit,
                              &missed, &evaded);
            (void)d;
            if (evaded) evades++;
        }
        CHECK(evades > 5 && evades < 17, "head evades=%d/20", evades);
    }


    {
        BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.0);
        b.maxhp = b.hp = 5000;
        b.erate[2] = 1.15;
        bt_srand(&bt, 1234);
        int d = bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed,
                          &evaded);

        CHECK(d >= 191 && d <= 287, "slash-boosted %d", d);
    }


    {
        BtF a = mk(10, 0, 10, 0.0, 0.0), b = mk(57, 36, 10, 0.97, 0.99);
        b.erate[3] = 1.0;
        bt_srand(&bt, 42);
        int d = bt_strike(&bt, &SK_THRUST, &a, &b, NULL, NULL, &crit,
                          &missed, &evaded);
        CHECK(!missed && !evaded, "certain hit must connect");
        CHECK(d >= 36 && d <= 54, "thrust %d out of 45+-20%%", d);
    }


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

        CHECK(d2 * 2 >= d1 - 1 && d2 * 2 <= d1 + 1,
              "guard %d vs unguarded %d", d2, d1);
    }


    {
        BtF b = mk(0, 0, 10, 1.0, 0.0);
        b.hp = 5;
        BtF a = mk(57, 0, 10, 1.0, 0.0);
        bt_srand(&bt, 9);
        bt_strike(&bt, &SK_ATK, &a, &b, NULL, NULL, &crit, &missed, &evaded);
        CHECK(!b.alive && b.hp == 0 && bt_has_state(&b, 1), "death not set");
    }


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


    {
        long e2 = bt_exp_for(2, 19, 38, 50, 10);
        long e3 = bt_exp_for(3, 19, 38, 50, 10);
        long e4 = bt_exp_for(4, 19, 38, 50, 10);
        long e5 = bt_exp_for(5, 19, 38, 50, 10);
        CHECK(e2 == 45 && e3 == 107 && e4 == 194 && e5 == 316,
              "exp %ld %ld %ld %ld", e2, e3, e4, e5);
    }


    CHECK(bt_param(30, 27, 1.0, 1) == 71, "buffed atk");


    {
        FILE *f = fopen("converted/code/formulas.bin", "rb");
        if (f) {
            static unsigned char blob[8192];
            size_t n = fread(blob, 1, sizeof(blob), f);
            fclose(f);
            (void)n;
            const unsigned char *raw = NULL;
            int nins = bt_prog_find(blob, 0, 1, &raw);
            CHECK(nins == 8 && raw, "bin skill1 nins=%d", nins);
            if (raw && nins == 8) {
                BtIns prog[8];
                for (int i = 0; i < 8; i++) bt_ins_get(raw + i * 10, &prog[i]);
                BtF a = mk(57, 0, 10, 1.0, 0.0), b = mk(0, 10, 10, 1.0, 0.0);
                double v = bt_vm(prog, 8, &a, &b, NULL, NULL);
                CHECK(v == 208.0, "bin vm %f", v);
            }
        }
    }


    {

        BtSkill mp = {9, 0, 1, 0, 0, 0, 2, 0, 100, PROG_45, 2};
        Bt bt2;
        memset(&bt2, 0, sizeof(bt2));
        BtF a = mk(10, 10, 10, 1.0, 0.0), b = mk(10, 10, 10, 1.0, 0.0);
        a.alive = b.alive = 1;
        b.maxmp = b.mp = 100;
        bt_srand(&bt2, 7);
        int c0, m0, e0;
        int d = bt_strike(&bt2, &mp, &a, &b, NULL, NULL, &c0, &m0, &e0);
        CHECK(d == 45 && b.mp == 55 && b.hp == 100 && b.alive,
              "mpdmg d=%d mp=%d", d, b.mp);

        BtSkill hr = {9, 0, 7, 0, 0, 0, 3, 0, 100, PROG_45, 2};
        b.hp = 60;
        d = bt_strike(&bt2, &hr, &a, &b, NULL, NULL, &c0, &m0, &e0);
        CHECK(d == 45 && b.hp == 100, "hprec d=%d hp=%d", d, b.hp);

        BtSkill dr = {9, 1, 1, 0, 0, 3, 5, 0, 100, PROG_45, 2};
        a.hit = 1.0;
        b.hp = 100;
        a.hp = 50;
        a.maxhp = 100;
        d = bt_strike(&bt2, &dr, &a, &b, NULL, NULL, &c0, &m0, &e0);
        CHECK(d == 45 && b.hp == 55 && a.hp == 95,
              "drain d=%d bhp=%d ahp=%d", d, b.hp, a.hp);
    }


    {
        BtSkill hack = {3, 1, 1, 0, 20, 2, 1, 0, 90, PROG_45, 2};
        Bt bt2;
        memset(&bt2, 0, sizeof(bt2));
        BtF a = mk(50, 10, 10, 1.0, 0.0), b = mk(10, 10, 10, 1.0, 0.0);
        a.alive = b.alive = 1;
        bt_srand(&bt2, 1234);
        int misses = 0;
        for (int s = 0; s < 40; s++) {
            int c0, m0, e0;
            b.hp = 100;
            b.alive = 1;
            bt_strike(&bt2, &hack, &a, &b, NULL, NULL, &c0, &m0, &e0);
            misses += m0;
        }
        CHECK(misses > 0 && misses < 40, "succ90 misses=%d/40", misses);
    }


    {
        Bt bt2;
        memset(&bt2, 0, sizeof(bt2));
        BtF a = mk(10, 10, 10, 1.0, 0.0), b = mk(10, 10, 10, 1.0, 0.0);
        a.alive = b.alive = 1;
        a.luk = b.luk = 10;
        int res[8], nres = 0;

        BtFx fx1[] = {{11, 0, 0.5, 0}};
        b.hp = 20;
        bt_srand(&bt2, 1);
        bt_apply_fx(&bt2, &a, &b, fx1, 1, 1, 0, 0.0, res, &nres);
        CHECK(b.hp == 70, "fx11 hp=%d", b.hp);

        BtFx fx2[] = {{21, 18, 1.0, 0}};
        bt_apply_fx(&bt2, &a, &b, fx2, 1, 1, 0, 0.0, res, &nres);
        CHECK(bt_has_state(&b, 18), "fx21 state18");

        BtFx fx3[] = {{22, 18, 1.0, 0}};
        bt_apply_fx(&bt2, &a, &b, fx3, 1, 1, 0, 0.0, res, &nres);
        CHECK(!bt_has_state(&b, 18), "fx22 state18 gone");

        BtFx fx4[] = {{31, 2, 1.0, 0}};
        bt_apply_fx(&bt2, &a, &b, fx4, 1, 1, 0, 0.0, res, &nres);
        CHECK(b.buff[2] == 1, "fx31 buff=%d", b.buff[2]);
        BtFx fx5[] = {{33, 2, 0.0, 0}};
        bt_apply_fx(&bt2, &a, &b, fx5, 1, 1, 0, 0.0, res, &nres);
        CHECK(b.buff[2] == 0, "fx33 buff=%d", b.buff[2]);

        BtFx fx6[] = {{44, 145, 1.0, 0}};
        bt_apply_fx(&bt2, &a, &b, fx6, 1, 1, 0, 0.0, res, &nres);
        CHECK(nres == 0, "fx44 ignored nres=%d", nres);
    }


    {
        Bt bt2;
        memset(&bt2, 0, sizeof(bt2));
        bt2.n_party = 1;
        bt2.n_foes = 1;
        bt2.f[0] = mk(10, 10, 50, 1.0, 0.0);
        bt2.f[0].is_foe = 0;
        bt2.f[1] = mk(10, 10, 60, 1.0, 0.0);
        bt2.f[1].is_foe = 1;
        int spd[2] = {2000, 0};
        int out[4];
        bt_srand(&bt2, 5);
        int n = bt_order_act(&bt2, spd, out, 4);
        CHECK(n >= 2 && out[0] == 0, "guard-first (%d,%d)", out[0], out[1]);
    }

    if (fails) printf("%d FAILURES\n", fails);
    else printf("ALL PASS\n");
    return fails != 0;
}
