/* See battle.h. Line-by-line ports noted per function. */
#include "battle.h"
#include <math.h>
#include <stddef.h>

void bt_srand(Bt *bt, unsigned seed) { bt->rng = seed ? seed : 1u; }

unsigned bt_randn(Bt *bt, unsigned n) {
    /* LCG standing in for Math.randomInt (deterministic goldens). */
    bt->rng = bt->rng * 1664525u + 1013904223u;
    if (n == 0) return 0;
    return (unsigned)(((bt->rng >> 16) * (uint64_t)n) >> 16) % n;
}

static double stat_of(const BtF *f, int arg) {
    switch (arg) {
        case 0: return (double)f->atk;
        case 1: return (double)f->def;
        case 2: return (double)f->mat;
        case 3: return (double)f->mdf;
        case 4: return (double)f->agi;
        case 5: return (double)f->luk;
        case 6: return (double)f->hp;
        case 7: return (double)f->mp;
        case 8: return (double)f->maxhp;
        case 9: return (double)f->maxmp;
        case 10: return (double)f->level;
        default: return 0.0;
    }
}

double bt_vm(const BtIns *p, int n, const BtF *a, const BtF *b,
             const int32_t *vars, const unsigned char *sw) {
    double st[32];
    int sp = 0;
    for (int i = 0; i < n; i++) {
        uint8_t op = p[i].op, arg = p[i].arg;
        double imm = p[i].imm, t;
        switch (op) {
            case 0: st[sp++] = imm; break;                        /* PUSH */
            case 1: sp--; st[sp - 1] += st[sp]; break;            /* ADD */
            case 2: sp--; st[sp - 1] -= st[sp]; break;            /* SUB */
            case 3: sp--; st[sp - 1] *= st[sp]; break;            /* MUL */
            case 4: sp--; t = st[sp]; st[sp - 1] = t != 0.0 ? st[sp - 1] / t : 0.0; break;
            case 5: sp--; t = st[sp]; st[sp - 1] = t != 0.0 ? fmod(st[sp - 1], t) : 0.0; break;
            case 6: sp--; st[sp - 1] = st[sp - 1] < st[sp] ? 1.0 : 0.0; break;
            case 7: sp--; st[sp - 1] = st[sp - 1] <= st[sp] ? 1.0 : 0.0; break;
            case 8: sp--; st[sp - 1] = st[sp - 1] > st[sp] ? 1.0 : 0.0; break;
            case 9: sp--; st[sp - 1] = st[sp - 1] >= st[sp] ? 1.0 : 0.0; break;
            case 10: sp--; st[sp - 1] = st[sp - 1] == st[sp] ? 1.0 : 0.0; break;
            case 11: sp--; st[sp - 1] = st[sp - 1] != st[sp] ? 1.0 : 0.0; break;
            case 12: sp--; st[sp - 1] = (st[sp - 1] != 0.0 && st[sp] != 0.0) ? 1.0 : 0.0; break;
            case 13: sp--; st[sp - 1] = (st[sp - 1] != 0.0 || st[sp] != 0.0) ? 1.0 : 0.0; break;
            case 14: st[sp++] = stat_of(a, arg); break;           /* A_STAT */
            case 15: st[sp++] = stat_of(b, arg); break;           /* B_STAT */
            case 16: st[sp - 1] = (vars && (int)st[sp - 1] >= 0) ? (double)vars[(int)st[sp - 1]] : 0.0; break;
            case 17: st[sp - 1] = (sw && (int)st[sp - 1] >= 0) ? (sw[(int)st[sp - 1]] ? 1.0 : 0.0) : 0.0; break;
            case 18: sp--; t = st[sp]; if (t > st[sp - 1]) st[sp - 1] = t; break;
            case 19: sp--; t = st[sp]; if (t < st[sp - 1]) st[sp - 1] = t; break;
            case 20: st[sp - 1] = floor(st[sp - 1]); break;
            case 21: return sp > 0 ? st[sp - 1] : 0.0;            /* RET */
            default: return 0.0;
        }
        if (sp < 0) sp = 0;
        if (sp > 32) sp = 32;
    }
    return sp > 0 ? st[sp - 1] : 0.0;
}

int bt_prog_find(const unsigned char *blob, int kind, int id,
                 const unsigned char **out) {
    *out = NULL;
    if (!blob) return 0;
    unsigned n = (unsigned)blob[0] | ((unsigned)blob[1] << 8) |
                 ((unsigned)blob[2] << 16) | ((unsigned)blob[3] << 24);
    const unsigned char *p = blob + 4;
    for (unsigned i = 0; i < n; i++) {
        int k = p[0];
        int eid = (int)p[1] | ((int)p[2] << 8);
        int nins = (int)p[3] | ((int)p[4] << 8);
        p += 5;
        if (k == kind && eid == id) {
            *out = p;
            return nins;
        }
        p += (unsigned)nins * 10u;
    }
    return 0;
}

void bt_ins_get(const unsigned char *raw, BtIns *out) {
    out->op = raw[0];
    out->arg = raw[1];
    {
        double v = 0.0;
        unsigned char *d = (unsigned char *)&v;
        for (int i = 0; i < 8; i++) d[i] = raw[2 + i];
        out->imm = v;
    }
}

int bt_param(int base, int plus, double rate, int stage) {
    if (stage > 2) stage = 2;
    if (stage < -2) stage = -2;
    double v = ((double)base + (double)plus) * rate * (1.0 + 0.25 * (double)stage);
    return v < 0.0 ? 0 : (int)v;
}

void bt_add_state(BtF *f, int id) {
    if (f->nstates >= BT_MAX_STATES) return;
    for (int i = 0; i < f->nstates; i++)
        if (f->states[i] == id) return;
    f->states[f->nstates++] = id;
    if (id == 1) f->alive = 0;
}

void bt_remove_state(BtF *f, int id) {
    for (int i = 0; i < f->nstates; i++) {
        if (f->states[i] == id) {
            f->states[i] = f->states[--f->nstates];
            return;
        }
    }
}

int bt_has_state(const BtF *f, int id) {
    for (int i = 0; i < f->nstates; i++)
        if (f->states[i] == id) return 1;
    return 0;
}

void bt_check_dead(BtF *f) {
    if (f->hp <= 0) {
        f->hp = 0;
        bt_add_state(f, 1);
    } else {
        f->alive = 1;
        bt_remove_state(f, 1);
    }
}

void bt_round_end(Bt *bt) {
    for (int i = 0; i < bt->n_party + bt->n_foes; i++) {
        bt->f[i].guard = 0;
        bt_remove_state(&bt->f[i], 2);
    }
}

/* evalDamageFormula: max(eval,0)*sign, NaN->0 (dmg type 3,4 drain sign). */
static double eval_formula(const BtSkill *sk, const BtF *a, const BtF *b,
                           const int32_t *vars, const unsigned char *sw) {
    double v = bt_vm(sk->prog, sk->nins, a, b, vars, sw);
    if (!(v >= 0.0) && !(v <= 0.0)) return 0.0;  /* NaN */
    if (v < 0.0) v = 0.0;
    if (sk->dmg_type == 3 || sk->dmg_type == 4) v = -v;
    return v;
}

static double element_rate(const BtSkill *sk, const BtF *sub, const BtF *tgt) {
    /* calcElementRate: elementId<0 -> max over attack elements. */
    if (sk->element < 0) {
        int e = sub->atk_elem;
        if (e >= 0 && e < BT_ERATE_N) return tgt->erate[e];
        return 1.0;
    }
    if (sk->element >= 0 && sk->element < BT_ERATE_N)
        return tgt->erate[sk->element];
    return 1.0;
}

int bt_strike(Bt *bt, const BtSkill *sk, BtF *sub, BtF *tgt,
              const int32_t *vars, const unsigned char *sw,
              int *crit, int *missed, int *evaded) {
    *crit = 0;
    *missed = 0;
    *evaded = 0;
    if (!sub->alive || !tgt->alive) return 0;

    /* Hit / evade (apply): physical uses hit/evas, magical mrf/mev,
     * certain (hitType 0) always connects. */
    double hit = 1.0, eva = 0.0;
    if (sk->hit_type == 1) {
        hit = 1.0 * sub->hit;
        eva = tgt->eva;
    } else if (sk->hit_type == 2) {
        hit = 1.0;
        eva = 0.0;  /* mrf/mev: no magical traits in slice */
    }
    if (bt_randn(bt, 1000000) >= (unsigned)(hit * 1000000.0)) {
        *missed = 1;
        return 0;
    }
    if (eva > 0.0 && bt_randn(bt, 1000000) < (unsigned)(eva * 1000000.0)) {
        *evaded = 1;
        return 0;
    }

    int dmg = 0;
    if (sk->dmg_type > 0) {
        if (sk->crit && bt_randn(bt, 1000000) <
            (unsigned)((sub->cri * (1.0 - tgt->cev)) * 1000000.0))
            *crit = 1;
        double base = eval_formula(sk, sub, tgt, vars, sw);
        double value = base * element_rate(sk, sub, tgt);
        if (sk->hit_type == 1) value *= tgt->pdr;
        if (sk->hit_type == 2) value *= tgt->mdr;
        if (*crit) value *= 3.0;  /* applyCritical */
        /* applyVariance: ±(v+v-amp), v in [0,amp] */
        {
            double amp = value >= 0.0 ? value : -value;
            amp = amp * (double)sk->variance / 100.0;
            long a = (long)amp;
            long v = (long)bt_randn(bt, (unsigned)(a + 1)) +
                     (long)bt_randn(bt, (unsigned)(a + 1)) - a;
            value = value >= 0.0 ? value + (double)v : value - (double)v;
        }
        if (value > 0.0 && tgt->guard) value /= 2.0 * tgt->grd;  /* applyGuard */
        /* JS Math.round (half up): floor(x + 0.5), NOT C round(). */
        dmg = (int)floor(value + 0.5);
        tgt->hp -= dmg;
        bt_check_dead(tgt);
    }
    /* Item effects (states etc.) land in the states pass (follow-up). */
    return dmg;
}

int bt_ai_pick(Bt *bt, const BtAiAct *acts, int n_acts, int party_level,
               int turn, const unsigned char *sw) {
    int valid[8], nv = 0, rmax = 0;
    for (int i = 0; i < n_acts && nv < 8; i++) {
        int ok = 1;
        switch (acts[i].ctype) {
            case 1: { /* meetsTurnCondition(param1, param2) */
                int p1 = acts[i].cp1, p2 = acts[i].cp2;
                if (p2 == 0)
                    ok = turn == p1;
                else
                    ok = turn > 0 && turn >= p1 && turn % p2 == p1 % p2;
                break;
            }
            case 6: ok = sw && sw[acts[i].cp1] ? 1 : 0; break;  /* switch */
            default: ok = 1; break;  /* 0 always; 2/3/4/5 need live
                                      * HP/MP/state/level context (note) */
        }
        (void)party_level;
        if (ok) {
            valid[nv++] = i;
            if (acts[i].rating > rmax) rmax = acts[i].rating;
        }
    }
    if (nv == 0) return -1;
    int zero = rmax - 3, sum = 0;
    for (int i = 0; i < nv; i++) sum += acts[i].rating - zero;
    if (sum <= 0) return valid[0];
    unsigned v = bt_randn(bt, (unsigned)sum);
    for (int i = 0; i < nv; i++) {
        v -= (unsigned)(acts[valid[i]].rating - zero);
        if ((int)v < 0) return valid[i];
    }
    return valid[nv - 1];
}

int bt_order(Bt *bt, int *out, int max) {
    int n = bt->n_party + bt->n_foes;
    int idx[BT_MAX_PARTY + BT_MAX_FOES];
    int c = 0;
    for (int i = 0; i < n && c < max; i++) {
        if (bt->f[i].alive) idx[c++] = i;
    }
    /* AGI desc, party (actors) first on ties. */
    for (int i = 1; i < c; i++) {
        int t = idx[i], j = i - 1;
        while (j >= 0) {
            int a = idx[j];
            int swap = 0;
            if (bt->f[t].agi != bt->f[a].agi)
                swap = bt->f[t].agi > bt->f[a].agi;
            else
                swap = !bt->f[t].is_foe && bt->f[a].is_foe;
            if (!swap) break;
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = t;
    }
    int m = c;
    /* Galv extra turns: agi > opposing-team avg * 1.5 acts once more. */
    {
        double pa = 0, fa = 0;
        int pn = 0, fn = 0;
        for (int i = 0; i < c; i++) {
            if (bt->f[idx[i]].is_foe) {
                fa += bt->f[idx[i]].agi;
                fn++;
            } else {
                pa += bt->f[idx[i]].agi;
                pn++;
            }
        }
        if (pn > 0 && fn > 0) {
            pa /= pn;
            fa /= fn;
            for (int i = 0; i < c && m < max; i++) {
                double opp = bt->f[idx[i]].is_foe ? pa : fa;
                if ((double)bt->f[idx[i]].agi > opp * 1.5) out[m++] = idx[i];
            }
        }
    }
    for (int i = 0; i < c; i++) out[i] = idx[i];
    return m;
}

int bt_escape(Bt *bt, int party_agi, int troop_agi) {
    double r = troop_agi > 0 ? 0.5 * (double)party_agi / (double)troop_agi : 1.0;
    unsigned roll = bt_randn(bt, 1000000);
    if ((double)roll < r * 1000000.0) {
        bt->over = 3;
        return 1;
    }
    bt->escape_ratio += 0.1;
    return 0;
}

long bt_exp_for(int n, int basis, int extra, int acc, int drop) {
    /* Game_Actor.expForLevel (rpg_objects.js). n >= 1. */
    if (n < 1) n = 1;
    double e = (double)basis * pow((double)(n - 1), 0.9 + (double)acc / 250.0) *
               (double)n * (double)(n + 1) / (double)(6 + drop) +
               (double)(n - 1) * (double)extra;
    return (long)(e + 0.5);
}
