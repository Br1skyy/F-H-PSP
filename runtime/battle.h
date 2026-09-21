/* Battle engine: damage pipeline, turn order, AI picks, EXP (D3 + D4).
 * Ports Game_Action/Game_Battler/BattleManager (rpg_objects.js,
 * rpg_managers.js) plus the audit's effective rules (AGI sort, Galv ExAgi
 * +50% rule). Data (DB rows, baked formulas) lives outside; this file is
 * data-free and unit-tested (tests/test_battle.c).
 *
 * Deviations (logged): counters/magic-reflection skipped (no applier in
 * slice); surprise/preemptive skipped; troop pages not executed here;
 * drops recorded only; RNG is an explicit LCG (seedable for goldens).
 */
#ifndef FH_BATTLE_H
#define FH_BATTLE_H

#include <stdint.h>

#define BT_STATS 8
#define BT_MAX_FOES 8
#define BT_MAX_PARTY 4
#define BT_MAX_STATES 8
#define BT_ERATE_N 8

/* VM ins: u8 op, u8 arg, f64 imm (matches converted/code/formulas.bin).
 * Ops: 0 PUSH 1 ADD 2 SUB 3 MUL 4 DIV 5 MOD 6 LT 7 LE 8 GT 9 GE 10 EQ
 * 11 NE 12 AND 13 OR 14 A_STAT 15 B_STAT 16 VAR 17 SWITCH 18 MAX 19 MIN
 * 20 FLOOR 21 RET. Stat args: 0 atk .. 9 mmp 10 level. */
typedef struct { uint8_t op, arg; double imm; } BtIns;

/* Baked skill (Skills.json). */
typedef struct {
    int id, hit_type;   /* 0 certain, 1 physical, 2 magical */
    int scope;          /* 1 one enemy, ... (slice: 1 only) */
    int speed, variance, element;  /* element -1 = attacker's */
    int dmg_type;       /* 0 none, 1 hp, ... */
    int crit;           /* damage.critical */
    const BtIns *prog;
    int nins;
} BtSkill;

/* Baked enemy AI action (Enemies.json actions[]). */
typedef struct { int skill, rating, ctype, cp1, cp2; } BtAiAct;

/* Live battler (resolved stats; actors and limbs share this). */
typedef struct {
    int is_foe, ref;    /* ref = enemy idx or actor id */
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
    int buff[BT_STATS]; /* -2..+2 stages, rate = 1 + 0.25n */
} BtF;

/* Battle context (single battle at a time). */
typedef struct {
    BtF f[BT_MAX_PARTY + BT_MAX_FOES];
    int n_party, n_foes;
    int turn;
    unsigned rng;
    int over;           /* 0 ongoing, 1 victory, 2 defeat, 3 escaped */
    int exp_all, gold_all;
    double escape_ratio;
} Bt;

/* RNG: LCG; bt_randn(n) mirrors Math.randomInt(n) in [0, n). */
void bt_srand(Bt *bt, unsigned seed);
unsigned bt_randn(Bt *bt, unsigned n);

/* Formula VM (double math per plan §5.3). vars/switches nullable (0). */
double bt_vm(const BtIns *p, int n, const BtF *a, const BtF *b,
             const int32_t *vars, const unsigned char *sw);

/* Locate a compiled program in a formulas.bin blob (u32 count, then
 * u8 kind, u16 id, u16 nins + nins x 10B raw ins). Returns ins count with
 * *out set to the raw bytes (use bt_ins_get; never cast: on-disk ins are
 * 10 bytes, BtIns is 16). Kinds: 0 skill, 1 item, ... */
int bt_prog_find(const unsigned char *blob, int kind, int id,
                 const unsigned char **out);

/* Decode one raw ins into a BtIns (memcpy: bin doubles are unaligned). */
void bt_ins_get(const unsigned char *raw, BtIns *out);

/* Full strike pipeline (apply()): hit/eva/crit rolls, makeDamageValue
 * (element, pdr/mdr, critical x3, variance, guard, round), HP applied,
 * death recorded. Sets missed, evaded, crit outputs. No damage for type 0. */
int bt_strike(Bt *bt, const BtSkill *sk, BtF *sub, BtF *tgt,
              const int32_t *vars, const unsigned char *sw,
              int *crit, int *missed, int *evaded);

/* Enemy AI: filter valid (turn/hp/mp/state/party-level/switch), then
 * rating-weighted pick (ratingZero = max-3). Returns action index or -1
 * (no valid action: battler idles). needs party_level + troop turn. */
int bt_ai_pick(Bt *bt, const BtAiAct *acts, int n_acts, int party_level,
               int turn, const unsigned char *sw);

/* Turn order: agi desc (actors first on ties), then Galv extra turns
 * (agi > opposing avg * 1.5 act once more at the end). out gets indices
 * into f[]; returns count. */
int bt_order(Bt *bt, int *out, int max);

/* Escape roll (processEscape): success = rand < ratio; fail adds 0.1. */
int bt_escape(Bt *bt, int party_agi, int troop_agi);

/* EXP curve (Game_Actor.expForLevel): basis*(n-1)^(0.9+acc/250) etc. */
long bt_exp_for(int n, int basis, int extra, int acc, int drop);

/* States: 1 dead (hp<=0), 2 guard (cleared at round end). */
void bt_add_state(BtF *f, int id);
void bt_remove_state(BtF *f, int id);
int bt_has_state(const BtF *f, int id);
void bt_check_dead(BtF *f);
void bt_round_end(Bt *bt);  /* clears guard */

/* Buffed param: (base + plus) * rate * (1 + 0.25 * stage). */
int bt_param(int base, int plus, double rate, int stage);

#endif /* FH_BATTLE_H */
