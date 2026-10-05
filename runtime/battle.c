
#include "battle.h"
#include <math.h>
#include <stddef.h>

void bt_srand(Bt *bt, unsigned seed) { bt->rng = seed ? seed : 1u; }

unsigned bt_randn(Bt *bt, unsigned n) {

    bt->rng = bt->rng * 1664525u + 1013904223u;
    if (n == 0) return 0;
    return (unsigned)(((bt->rng >> 16) * (uint64_t)n) >> 16) % n;
}

int bt_stat(const BtF *f, int param_id) {
    int base;
    switch (param_id) {
        case 2: base = f->atk; break;
        case 3: base = f->def; break;
        case 4: base = f->mat; break;
        case 5: base = f->mdf; break;
        case 6: base = f->agi; break;
        case 7: base = f->luk; break;
        default: return 0;
    }
    int v = bt_param(base, 0, 1.0, f->buff[param_id]);
    return v < 1 ? 1 : v;
}

static double stat_of(const BtF *f, int arg) {
    switch (arg) {
        case 0: case 1: case 2: case 3: case 4: case 5:
            return (double)bt_stat(f, arg + 2);
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
    /* A malformed program returns 0 instead of touching memory outside st[]. */
#define NEED(k) do { if (sp < (k)) return 0.0; } while (0)
#define ROOM()  do { if (sp >= 32) return 0.0; } while (0)
    for (int i = 0; i < n; i++) {
        uint8_t op = p[i].op, arg = p[i].arg;
        double imm = p[i].imm, t;
        switch (op) {
            case 0: ROOM(); st[sp++] = imm; break;
            case 1: NEED(2); sp--; st[sp - 1] += st[sp]; break;
            case 2: NEED(2); sp--; st[sp - 1] -= st[sp]; break;
            case 3: NEED(2); sp--; st[sp - 1] *= st[sp]; break;
            case 4: NEED(2); sp--; t = st[sp]; st[sp - 1] = t != 0.0 ? st[sp - 1] / t : 0.0; break;
            case 5: NEED(2); sp--; t = st[sp]; st[sp - 1] = t != 0.0 ? fmod(st[sp - 1], t) : 0.0; break;
            case 6: NEED(2); sp--; st[sp - 1] = st[sp - 1] < st[sp] ? 1.0 : 0.0; break;
            case 7: NEED(2); sp--; st[sp - 1] = st[sp - 1] <= st[sp] ? 1.0 : 0.0; break;
            case 8: NEED(2); sp--; st[sp - 1] = st[sp - 1] > st[sp] ? 1.0 : 0.0; break;
            case 9: NEED(2); sp--; st[sp - 1] = st[sp - 1] >= st[sp] ? 1.0 : 0.0; break;
            case 10: NEED(2); sp--; st[sp - 1] = st[sp - 1] == st[sp] ? 1.0 : 0.0; break;
            case 11: NEED(2); sp--; st[sp - 1] = st[sp - 1] != st[sp] ? 1.0 : 0.0; break;
            case 12: NEED(2); sp--; st[sp - 1] = (st[sp - 1] != 0.0 && st[sp] != 0.0) ? 1.0 : 0.0; break;
            case 13: NEED(2); sp--; st[sp - 1] = (st[sp - 1] != 0.0 || st[sp] != 0.0) ? 1.0 : 0.0; break;
            case 14: ROOM(); st[sp++] = stat_of(a, arg); break;
            case 15: ROOM(); st[sp++] = stat_of(b, arg); break;
            case 16: {
                NEED(1);
                double ix = st[sp - 1];
                st[sp - 1] = (vars && ix >= 0.0 && ix < (double)BT_VM_VARS)
                                 ? (double)vars[(int)ix] : 0.0;
                break;
            }
            case 17: {
                NEED(1);
                double ix = st[sp - 1];
                st[sp - 1] = (sw && ix >= 0.0 && ix < (double)BT_VM_SWITCHES)
                                 ? (sw[(int)ix] ? 1.0 : 0.0) : 0.0;
                break;
            }
            case 18: NEED(2); sp--; t = st[sp]; if (t > st[sp - 1]) st[sp - 1] = t; break;
            case 19: NEED(2); sp--; t = st[sp]; if (t < st[sp - 1]) st[sp - 1] = t; break;
            case 20: NEED(1); st[sp - 1] = floor(st[sp - 1]); break;
            case 21: return sp > 0 ? st[sp - 1] : 0.0;
            default: return 0.0;
        }
    }
#undef NEED
#undef ROOM
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
    return v < 0.0 ? 0 : (int)floor(v + 0.5);   /* MV: Math.round */
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
            if (id == 1) {              /* MV revive(): 0 HP comes back as 1 */
                if (f->hp <= 0) f->hp = 1;
                f->alive = 1;
            }
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
        BtF *f = &bt->f[i];
        f->guard = 0;
        bt_remove_state(f, 2);
        /* MV updateBuffTurns + removeBuffsAuto */
        for (int k = 0; k < BT_STATS; k++) {
            if (f->buff_turns[k] > 0) f->buff_turns[k]--;
            if (f->buff[k] != 0 && f->buff_turns[k] == 0) f->buff[k] = 0;
        }
    }
}


static double eval_formula(const BtSkill *sk, const BtF *a, const BtF *b,
                           const int32_t *vars, const unsigned char *sw) {
    double v = bt_vm(sk->prog, sk->nins, a, b, vars, sw);
    if (!(v >= 0.0) && !(v <= 0.0)) return 0.0;
    if (v < 0.0) v = 0.0;
    if (sk->dmg_type == 3 || sk->dmg_type == 4) v = -v;
    return v;
}

static double element_rate(const BtSkill *sk, const BtF *sub, const BtF *tgt) {

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


    double succ = (double)(sk->success < 0 ? 0 : sk->success > 100 ? 100 : sk->success) / 100.0;
    double hit = succ, eva = 0.0;
    if (sk->hit_type == 1) {
        hit = succ * sub->hit;
        eva = tgt->eva;
    } else if (sk->hit_type == 2) {
        hit = succ;
        eva = 0.0;
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
        if (*crit) value *= 3.0;

        {
            double amp = value >= 0.0 ? value : -value;
            amp = amp * (double)sk->variance / 100.0;
            long a = (long)amp;
            long v = (long)bt_randn(bt, (unsigned)(a + 1)) +
                     (long)bt_randn(bt, (unsigned)(a + 1)) - a;
            value = value >= 0.0 ? value + (double)v : value - (double)v;
        }
        if (value > 0.0 && tgt->guard) value /= 2.0 * tgt->grd;

        dmg = (int)floor(value + 0.5);


        switch (sk->dmg_type) {
            case 1:
                tgt->hp -= dmg;
                bt_check_dead(tgt);
                break;
            case 2:
                if (dmg > tgt->mp) dmg = tgt->mp;      /* MV: min(target.mp) */
                tgt->mp -= dmg;
                if (tgt->mp < 0) tgt->mp = 0;
                break;
            case 3:
                tgt->hp -= dmg;
                if (tgt->hp > tgt->maxhp) tgt->hp = tgt->maxhp;
                break;
            case 4:
                tgt->mp -= dmg;
                if (tgt->mp > tgt->maxmp) tgt->mp = tgt->maxmp;
                break;
            case 5:
                if (dmg > tgt->hp) dmg = tgt->hp;      /* MV: min(target.hp) */
                tgt->hp -= dmg;
                bt_check_dead(tgt);
                sub->hp += dmg;
                if (sub->hp > sub->maxhp) sub->hp = sub->maxhp;
                break;
            case 6:
                if (dmg > tgt->mp) dmg = tgt->mp;
                tgt->mp -= dmg;
                if (tgt->mp < 0) tgt->mp = 0;
                sub->mp += dmg;
                if (sub->mp > sub->maxmp) sub->mp = sub->maxmp;
                break;
            default:
                break;
        }

        if ((sk->dmg_type == 3 || sk->dmg_type == 4) && dmg < 0) dmg = -dmg;
    }

    return dmg;
}


static int roll_chance(Bt *bt, double chance) {
    if (chance <= 0.0) return 0;
    if (chance >= 1.0) return 1;
    return bt_randn(bt, 1000000) < (unsigned)(chance * 1000000.0);
}

static void fx_hp_changed(BtF *f) {
    if (f->hp > f->maxhp) f->hp = f->maxhp;
    bt_check_dead(f);               /* MV refresh(): death follows HP */
}

static void fx_buff(BtF *t, int param, int turns, int dir) {
    if (!t->alive || param < 0 || param >= BT_STATS) return;
    int lv = t->buff[param] + dir;
    if (lv > 2) lv = 2;
    if (lv < -2) lv = -2;
    t->buff[param] = lv;
    if (lv != 0 && t->buff_turns[param] < turns) t->buff_turns[param] = turns;
}

void bt_apply_fx(Bt *bt, BtF *sub, BtF *tgt, const BtFx *fx, int nfx,
                 int certain, int is_item, double pha,
                 int *reserved, int *nres) {
    (void)reserved;  /* kept for API compatibility; not used yet */
    (void)nres;

    double luk = 1.0 + ((double)bt_stat(sub, 7) - (double)bt_stat(tgt, 7)) * 0.001;
    if (luk < 0.0) luk = 0.0;
    for (int k = 0; k < nfx; k++) {
        switch (fx[k].code) {
            case 11: {
                double x = (double)tgt->maxhp * fx[k].v1 + (double)fx[k].v2;
                if (is_item) x *= pha;
                int v = (int)floor(x);          /* one floor, as MV */
                if (v != 0) {
                    tgt->hp += v;
                    fx_hp_changed(tgt);
                }
                break;
            }
            case 12: {
                double x = (double)tgt->maxmp * fx[k].v1 + (double)fx[k].v2;
                if (is_item) x *= pha;
                int v = (int)floor(x);
                if (v != 0) {
                    tgt->mp += v;
                    if (tgt->mp > tgt->maxmp) tgt->mp = tgt->maxmp;
                    if (tgt->mp < 0) tgt->mp = 0;
                }
                break;
            }
            case 13:
                break;
            case 21: {
                if (fx[k].data == 0) {
                    for (int s = 0; s < 4; s++) {
                        int sid = sub->atkst[s];
                        if (sid <= 0) continue;
                        if (roll_chance(bt, fx[k].v1 * luk))
                            bt_add_state(tgt, sid);
                    }
                } else {
                    double ch = fx[k].v1 * (certain ? 1.0 : luk);
                    if (roll_chance(bt, ch)) bt_add_state(tgt, fx[k].data);
                }
                break;
            }
            case 22:
                if (roll_chance(bt, fx[k].v1)) bt_remove_state(tgt, fx[k].data);
                break;
            case 31:
                fx_buff(tgt, fx[k].data, (int)fx[k].v1, +1);
                break;
            case 32:
                if (roll_chance(bt, luk))
                    fx_buff(tgt, fx[k].data, (int)fx[k].v1, -1);
                break;
            case 33:            /* remove buff: only if currently buffed */
                if (fx[k].data >= 0 && fx[k].data < BT_STATS &&
                    tgt->buff[fx[k].data] > 0) {
                    tgt->buff[fx[k].data] = 0;
                    tgt->buff_turns[fx[k].data] = 0;
                }
                break;
            case 34:            /* remove debuff: only if currently debuffed */
                if (fx[k].data >= 0 && fx[k].data < BT_STATS &&
                    tgt->buff[fx[k].data] < 0) {
                    tgt->buff[fx[k].data] = 0;
                    tgt->buff_turns[fx[k].data] = 0;
                }
                break;
            default:
                break;
        }
    }
}

int bt_ai_pick_for(Bt *bt, const BtF *self, const BtAiAct *acts, int n_acts,
                   int party_level, int turn, const unsigned char *sw) {
    int valid[BT_AI_MAX], nv = 0, rmax = 0;
    for (int i = 0; i < n_acts && nv < BT_AI_MAX; i++) {
        int ok = 1;
        int p1 = acts[i].cp1, p2 = acts[i].cp2;
        switch (acts[i].ctype) {
            case 1:
                if (p2 == 0)
                    ok = turn == p1;
                else
                    ok = turn > 0 && turn >= p1 && turn % p2 == p1 % p2;
                break;
            case 2:             /* HP percent window */
                if (self) {
                    double r = self->maxhp > 0 ? 100.0 * self->hp / self->maxhp : 0.0;
                    ok = r >= p1 && r <= p2;
                }
                break;
            case 3:             /* MP percent window */
                if (self) {
                    double r = self->maxmp > 0 ? 100.0 * self->mp / self->maxmp : 0.0;
                    ok = r >= p1 && r <= p2;
                }
                break;
            case 4:
                if (self) ok = bt_has_state(self, p1);
                break;
            case 5:
                ok = party_level >= p1;
                break;
            case 6:
                ok = sw && p1 >= 0 && p1 < BT_VM_SWITCHES && sw[p1];
                break;
            default: ok = 1; break;
        }
        if (ok) {
            valid[nv++] = i;
            if (acts[i].rating > rmax) rmax = acts[i].rating;
        }
    }
    if (nv == 0) return -1;
    /* MV keeps only actions rated above (max - 3) before weighting. */
    int zero = rmax - 3, sum = 0, nk = 0;
    for (int i = 0; i < nv; i++) {
        if (acts[valid[i]].rating > zero) {
            valid[nk++] = valid[i];
            sum += acts[valid[i]].rating - zero;
        }
    }
    if (nk == 0 || sum <= 0) return valid[0];
    unsigned v = bt_randn(bt, (unsigned)sum);
    for (int i = 0; i < nk; i++) {
        v -= (unsigned)(acts[valid[i]].rating - zero);
        if ((int)v < 0) return valid[i];
    }
    return valid[nk - 1];
}

int bt_ai_pick(Bt *bt, const BtAiAct *acts, int n_acts, int party_level,
               int turn, const unsigned char *sw) {
    return bt_ai_pick_for(bt, NULL, acts, n_acts, party_level, turn, sw);
}

int bt_order(Bt *bt, int *out, int max) {
    int n = bt->n_party + bt->n_foes;
    int idx[BT_MAX_PARTY + BT_MAX_FOES];
    int c = 0;
    for (int i = 0; i < n && c < max; i++) {
        if (bt->f[i].alive) idx[c++] = i;
    }

    for (int i = 1; i < c; i++) {
        int t = idx[i], j = i - 1;
        while (j >= 0) {
            int a = idx[j];
            int swap = 0;
            if (bt_stat(&bt->f[t], 6) != bt_stat(&bt->f[a], 6))
                swap = bt_stat(&bt->f[t], 6) > bt_stat(&bt->f[a], 6);
            else
                swap = !bt->f[t].is_foe && bt->f[a].is_foe;
            if (!swap) break;
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = t;
    }
    int m = c;

    {
        double pa = 0, fa = 0;
        int pn = 0, fn = 0;
        for (int i = 0; i < c; i++) {
            if (bt->f[idx[i]].is_foe) {
                fa += bt_stat(&bt->f[idx[i]], 6);
                fn++;
            } else {
                pa += bt_stat(&bt->f[idx[i]], 6);
                pn++;
            }
        }
        if (pn > 0 && fn > 0) {
            pa /= pn;
            fa /= fn;
            for (int i = 0; i < c && m < max; i++) {
                double opp = bt->f[idx[i]].is_foe ? pa : fa;
                if ((double)bt_stat(&bt->f[idx[i]], 6) > opp * 1.5) out[m++] = idx[i];
            }
        }
    }
    for (int i = 0; i < c; i++) out[i] = idx[i];
    return m;
}

int bt_order_act(Bt *bt, const int *spd, int *out, int max) {


    int n = bt->n_party + bt->n_foes;
    int idx[BT_MAX_PARTY + BT_MAX_FOES];
    int key[BT_MAX_PARTY + BT_MAX_FOES];
    int c = 0;
    for (int i = 0; i < n && c < max; i++) {
        if (!bt->f[i].alive) continue;
        idx[c] = i;
        int agi = bt_stat(&bt->f[i], 6);
        int j = 5 + agi / 4;
        if (j < 1) j = 1;
        key[c] = agi + (int)bt_randn(bt, (unsigned)j) +
                 (spd ? spd[i] : 0);
        c++;
    }

    for (int i = 1; i < c; i++) {
        int t = idx[i], tk = key[i], j = i - 1;
        while (j >= 0) {
            int a = idx[j];
            int swap = 0;
            if (tk != key[j])
                swap = tk > key[j];
            else
                swap = !bt->f[t].is_foe && bt->f[a].is_foe;
            if (!swap) break;
            idx[j + 1] = idx[j];
            key[j + 1] = key[j];
            j--;
        }
        idx[j + 1] = t;
        key[j + 1] = tk;
    }
    int m = c;

    {
        double pa = 0, fa = 0;
        int pn = 0, fn = 0;
        for (int i = 0; i < c; i++) {
            if (bt->f[idx[i]].is_foe) {
                fa += bt_stat(&bt->f[idx[i]], 6);
                fn++;
            } else {
                pa += bt_stat(&bt->f[idx[i]], 6);
                pn++;
            }
        }
        if (pn > 0 && fn > 0) {
            pa /= pn;
            fa /= fn;
            for (int i = 0; i < c && m < max; i++) {
                double opp = bt->f[idx[i]].is_foe ? pa : fa;
                if ((double)bt_stat(&bt->f[idx[i]], 6) > opp * 1.5) out[m++] = idx[i];
            }
        }
    }
    for (int i = 0; i < c; i++) out[i] = idx[i];
    return m;
}

int bt_escape(Bt *bt, int party_agi, int troop_agi) {
    double r = bt->escape_ratio;
    if (r <= 0.0)               /* not seeded yet: 0.5 * party / troop */
        r = troop_agi > 0 ? 0.5 * (double)party_agi / (double)troop_agi : 1.0;
    unsigned roll = bt_randn(bt, 1000000);
    if ((double)roll < r * 1000000.0) {
        bt->over = 3;
        return 1;
    }
    bt->escape_ratio = r + 0.1;
    return 0;
}

int bt_make_targets(Bt *bt, int user, int scope, int sel, int *out, int max) {
    int n = bt->n_party + bt->n_foes, c = 0;
    if (user < 0 || user >= n || max <= 0) return 0;
    int side = bt->f[user].is_foe;
    int opp[BT_MAX_PARTY + BT_MAX_FOES], no = 0;
    int fri[BT_MAX_PARTY + BT_MAX_FOES], nf = 0;
    for (int i = 0; i < n; i++) {
        if (bt->f[i].is_foe != side) opp[no++] = i; else fri[nf++] = i;
    }
    int *pool = scope >= 7 ? fri : opp;
    int np = scope >= 7 ? nf : no;
    int want_dead = (scope == 9 || scope == 10);
    int cand[BT_MAX_PARTY + BT_MAX_FOES], nc = 0;
    for (int i = 0; i < np; i++)
        if ((bt->f[pool[i]].alive != 0) != want_dead) cand[nc++] = pool[i];

    switch (scope) {
        case 0:
            return 0;
        case 11:
            out[c++] = user;
            return c;
        case 2: case 8: case 10:
            for (int i = 0; i < nc && c < max; i++) out[c++] = cand[i];
            return c;
        case 3: case 4: case 5: case 6: {
            int cnt = scope - 2;
            for (int t = 0; t < cnt && nc > 0 && c < max; t++)
                out[c++] = cand[bt_randn(bt, (unsigned)nc)];
            return c;
        }
        case 1: case 7: case 9: {
            if (nc == 0) return 0;
            for (int i = 0; i < nc; i++)
                if (cand[i] == sel) { out[c++] = sel; return c; }
            if (sel < 0) { out[c++] = cand[bt_randn(bt, (unsigned)nc)]; return c; }
            out[c++] = cand[0];             /* MV smoothTarget */
            return c;
        }
        default:
            out[c++] = user;
            return c;
    }
}

long bt_exp_for(int n, int basis, int extra, int acc, int drop) {

    if (n < 1) n = 1;
    double e = (double)basis * pow((double)(n - 1), 0.9 + (double)acc / 250.0) *
               (double)n * (double)(n + 1) / (double)(6 + drop) +
               (double)(n - 1) * (double)extra;
    return (long)(e + 0.5);
}

int bt_page_fire(const BtPageCond *c, int turn, int isTurnEnd,
                 const BtF *foes, int nfoes,
                 const BtActorHp *actors, int nactors,
                 const unsigned char *sw) {

    if (!c->turnEnding && !c->turnValid && !c->enemyValid &&
            !c->actorValid && !c->switchValid)
        return 0;
    if (c->turnEnding && !isTurnEnd)
        return 0;
    if (c->turnValid) {
        int n = turn, a = c->turnA, b = c->turnB;
        if (b == 0 && n != a)
            return 0;
        if (b > 0 && (n < 1 || n < a || n % b != a % b))
            return 0;
    }
    if (c->enemyValid) {
        if (c->enemyIdx < 0 || c->enemyIdx >= nfoes)
            return 0;
        const BtF *e = &foes[c->enemyIdx];
        double rate = e->maxhp > 0 ? 100.0 * (double)e->hp / (double)e->maxhp : 0.0;
        if (rate > (double)c->enemyHp)
            return 0;
    }
    if (c->actorValid) {
        const BtActorHp *a = NULL;
        for (int i = 0; i < nactors; i++)
            if (actors[i].id == c->actorId) { a = &actors[i]; break; }
        if (!a)
            return 0;
        double rate = a->maxhp > 0 ? 100.0 * (double)a->hp / (double)a->maxhp : 0.0;
        if (rate > (double)c->actorHp)
            return 0;
    }
    if (c->switchValid) {
        if (!sw || !sw[c->switchId])
            return 0;
    }
    return 1;
}
