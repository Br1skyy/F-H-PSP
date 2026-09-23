

#ifndef FH_BATTLE_H
#define FH_BATTLE_H

#include <stdint.h>

#define BT_STATS 8
#define BT_MAX_FOES 8
#define BT_MAX_PARTY 4
#define BT_MAX_STATES 8
#define BT_ERATE_N 8


typedef struct { uint8_t op, arg; double imm; } BtIns;


typedef struct {
    int id, hit_type;
    int scope;
    int speed, variance, element;
    int dmg_type;
    int crit;
    int success;
    const BtIns *prog;
    int nins;
} BtSkill;


typedef struct { int skill, rating, ctype, cp1, cp2; } BtAiAct;


typedef struct {
    int is_foe, ref;
    int maxhp, hp, maxmp, mp;
    int atk, def, mat, mdf, agi, luk;
    double hit, eva, cri, cev, pdr, mdr, grd;
    double erate[BT_ERATE_N];
    int atk_elem;
    int level;
    long exp_cur;
    int alive, guard;
    int states[BT_MAX_STATES];
    int nstates;
    int buff[BT_STATS];
    int atkst[4];
} BtF;


typedef struct {
    BtF f[BT_MAX_PARTY + BT_MAX_FOES];
    int n_party, n_foes;
    int turn;
    unsigned rng;
    int over;
    int exp_all, gold_all;
    double escape_ratio;
} Bt;


void bt_srand(Bt *bt, unsigned seed);
unsigned bt_randn(Bt *bt, unsigned n);


double bt_vm(const BtIns *p, int n, const BtF *a, const BtF *b,
             const int32_t *vars, const unsigned char *sw);


int bt_prog_find(const unsigned char *blob, int kind, int id,
                 const unsigned char **out);


void bt_ins_get(const unsigned char *raw, BtIns *out);


int bt_strike(Bt *bt, const BtSkill *sk, BtF *sub, BtF *tgt,
              const int32_t *vars, const unsigned char *sw,
              int *crit, int *missed, int *evaded);


int bt_ai_pick(Bt *bt, const BtAiAct *acts, int n_acts, int party_level,
               int turn, const unsigned char *sw);


int bt_order(Bt *bt, int *out, int max);


int bt_order_act(Bt *bt, const int *spd, int *out, int max);


typedef struct { int code, data; double v1; int v2; } BtFx;


void bt_apply_fx(Bt *bt, BtF *sub, BtF *tgt, const BtFx *fx, int nfx,
                 int certain, int is_item, double pha,
                 int *reserved, int *nres);


int bt_escape(Bt *bt, int party_agi, int troop_agi);


long bt_exp_for(int n, int basis, int extra, int acc, int drop);


void bt_add_state(BtF *f, int id);
void bt_remove_state(BtF *f, int id);
int bt_has_state(const BtF *f, int id);
void bt_check_dead(BtF *f);
void bt_round_end(Bt *bt);


int bt_param(int base, int plus, double rate, int stage);


typedef struct {
    int span;
    int turnEnding, turnValid, turnA, turnB;
    int enemyValid, enemyIdx, enemyHp;
    int actorValid, actorId, actorHp;
    int switchValid, switchId;
} BtPageCond;


typedef struct { int id, hp, maxhp; } BtActorHp;


int bt_page_fire(const BtPageCond *c, int turn, int isTurnEnd,
                 const BtF *foes, int nfoes,
                 const BtActorHp *actors, int nactors,
                 const unsigned char *sw);

#endif
