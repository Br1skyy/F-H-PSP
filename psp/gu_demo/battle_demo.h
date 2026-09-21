/* Baked battle slice: Guard1 troop (id 1) + the four starting actors.
 * Source: Fear & Hunger/www/data/{Enemies,Actors,Classes,Weapons,Armors,
 * Skills,Troops}.json + converted/code/formulas.bin (skill bytecode below
 * is decoded verbatim from the bin; verify with the formulas.json dump).
 * Actor stats are resolved (class base + equipment); trades: none.
 * Enemy x/y are troop canvas coords (816x624); scale to screen at draw.
 */
#include "../../runtime/battle.h"

/* --- Skill bytecode (formulas.bin; ops per battle.h) --- */
static const BtIns DEMO_PROG_ATK[] = {
    {14, 0, 0.0}, {0, 0, 4.0}, {3, 0, 0.0}, {15, 1, 0.0},
    {0, 0, 2.0}, {3, 0, 0.0}, {2, 0, 0.0}, {21, 0, 0.0},
};
static const BtIns DEMO_PROG_35[] = { {0, 0, 35.0}, {21, 0, 0.0} };
static const BtIns DEMO_PROG_45[] = { {0, 0, 45.0}, {21, 0, 0.0} };

static const BtSkill DEMO_SKILLS[] = {
    /* id, hit, scope, speed, var, elem, type, crit, prog, nins */
    {1, 1, 1, 0, 20, -1, 1, 0, DEMO_PROG_ATK, 8},   /* Attack */
    {3, 1, 1, 0, 20, 2, 1, 0, DEMO_PROG_35, 2},     /* Hack */
    {4, 0, 1, 0, 20, 3, 1, 0, DEMO_PROG_45, 2},     /* Stinger thrust */
    {5, 0, 1, 0, 20, 0, 0, 0, NULL, 0},             /* Pulsating (no dmg) */
    {0, 0, 0, 0, 0, 0, 0, 0, NULL, 0},
};

/* --- Guard1 limbs (Enemies.json 1-7; erate all 1.0; pdr/mdr/grd 1.0) --- */
/* id, mhp, atk, def, mat, mdf, agi, luk, hit, eva, elem, exp, gold */
static const struct {
    int id, mhp, atk, def, mat, mdf, agi, luk, exp, gold, elem;
    double hit, eva;
} DEMO_FOES[] = {
    {1, 1300, 10, 10, 12, 10, 10, 11, 0, 0, 1, 0.95, 0.05},
    {2, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
    {3, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
    {4, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
    {5, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
    {6, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
    {7, 20, 10, 10, 10, 10, 10, 10, 0, 0, 1, 0.0, 0.0},
};

/* Enemy AI (actions[]; ctype 0 always, 1 turn, 6 switch). */
static const BtAiAct DEMO_AI_TORSO[] = { {6, 5, 6, 17, 0} };
static const BtAiAct DEMO_AI_ARM_L[] = { {3, 5, 0, 0, 0} };
static const BtAiAct DEMO_AI_STING[] = {
    {4, 3, 1, 1, 1}, {5, 5, 0, 0, 0}, {5, 5, 0, 0, 0},
};

/* Troop layout (Troops.json 1, canvas 816x624): enemy idx, x, y. */
static const struct { int foe, x, y; } DEMO_TROOP[] = {
    {0, 411, 421}, {1, 393, 432}, {2, 342, 432}, {3, 534, 423},
    {4, 363, 429}, {5, 462, 441}, {6, 417, 441},
};

/* Enemy art files (converted/enemies, staged in psp data): guard1_torso,
 * guard1_head, guard1_R_hand, guard1_L_hand, guard1_R_leg, guard1_L_leg,
 * guard1_stinger. */

/* Display names + active/testure dims (from *.meta.json). */
static const char *DEMO_FOE_NAMES[] = {
    "Guard torso", "Guard head", "Guard R arm", "Guard L arm",
    "Guard R leg", "Guard L leg", "Guard stinger",
};

static const struct { int tw, th, stride, w, h; } DEMO_FOE_DIMS[] = {
    {128, 256, 128, 111, 156},
    {64, 256, 64, 48, 162},
    {64, 128, 64, 46, 90},
    {128, 128, 128, 76, 117},
    {64, 64, 64, 33, 48},
    {64, 64, 64, 46, 60},
    {32, 64, 32, 31, 59},
};

/* Actor ids for the four playable characters (characters[] order). */
static const int DEMO_CHAR_ACTOR[] = {1, 5, 4, 3};

/* --- Party (Actors 1/3/4/5 at initialLevel 2; class base + equipment) --- */
/* id, level, mhp, mmp, atk, def, mat, mdf, agi, luk, elem, exp-basis/extra/acc/drop */
static const struct {
    int id, level, mhp, mmp, atk, def, mat, mdf, agi, luk, elem;
    int basis, extra, acc, drop;
    double er1, er2, er3, hit, eva, cri;
} DEMO_ACTORS[] = {
    /* Mercenary: Scimitar atk+27/elem2, Leather vest def+20 */
    {1, 2, 100, 100, 57, 36, 16, 16, 10, 32, 2, 19, 38, 50, 10,
     1.0, 1.0, 1.0, 0.97, 0.05, 0.04},
    /* Knight: Long sword 35/elem2, shield def+20, plate def+40, resists */
    {3, 2, 100, 100, 65, 76, 16, 16, 10, 32, 2, 19, 38, 50, 10,
     0.64, 0.52, 0.56, 0.97, 0.05, 0.04},
    /* Dark Priest: Short sword 10/elem2, robe def+5/mat+12, resists */
    {4, 2, 100, 100, 40, 21, 28, 16, 10, 32, 2, 19, 38, 50, 10,
     0.95, 0.95, 0.95, 0.97, 0.05, 0.04},
    /* Outlander: Short bow 10/elem3, fur def+20, resists */
    {5, 2, 100, 100, 40, 36, 16, 16, 10, 32, 3, 19, 38, 50, 10,
     0.92, 0.92, 0.92, 0.97, 0.05, 0.04},
};
