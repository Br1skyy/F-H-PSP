# Battle

In progress. Front-view limb fights (Guard1 troop 1, Ballista troop 44).
One actor, up to 8 foe members. Commands are Attack, Skills, Guard, Item.
There is no party window and no separate escape button.

## Options

Run (skill 40) and Talk (skill 11) are base kit for everyone, plus class
learnings. Both resolve before the round starts: Run arms its common
event and rolls the escape page, Talk plays its dialogue first and then
the foes act.

## Damage

Skill formulas run on a small double precision bytecode VM
(`runtime/battle.c`, baked by `tools/bake_battle_db.py`). The log names
the target that was hit.

## Rules reference

The reference is vanilla RPG Maker MV (`rpg_objects.js` Game_Action,
`rpg_managers.js` BattleManager). If your shipped scripts differ, the
scripts win. Covered by `tests/test_battle_rules.c`.

- Damage: formula, element rate, PDR/MDR, crit (Yanfly CriticalControl:
  x1.5 plus flat 1.5 x attacker's LUK, not vanilla x3), variance, guard,
  round. Damage caps at 9999 (Yanfly DamageCore, never binding here).
  Drain and MP damage are capped at the target's remaining HP/MP.
- Hit: physical rolls success x attacker's HIT then target EVA; magical
  rolls success then target MEV (trait 22/4, baked like EVA); certain-hit
  rolls success only. Crit chance is clamped at zero (no wraparound).
- States (`STATE_XP`, baked from States.json traits): HIT/EVA/MEV/CRI/CEV
  adjust additively, PDR/MDR and ATK/DEF/MAT/MDF/AGI rates multiply, at
  strike/stat time -- so they work the moment a skill or troop page
  applies them. Weakness: -0.95 EVA/MEV, x1.5 physical taken. Blindness:
  -0.75 HIT. No-criticals zeroes crits. Enemy ATTACK/DEFENCE UP and Hunger
  ATK penalties use the same path.
- Limbs: each body part is its own troop member with its own HP and
  accuracy -- the guard's head rides at 55% EVA / 40% MEV while the arms
  sit at 5%. Cutting both legs flips switches 18+19, and the
  dismemberment page answers with Weakness on the head ("loses its
  balance"), dropping it to ~0 evasion and raising physical taken 1.5x.
  Cutting an armed limb removes its attacks outright (dead members never
  enter the turn order); the torso's Tackle is additionally switch-gated
  (sw17: off once the head is weak, on once both arms are gone).
  Destroying head or torso wipes the troop.
- Enemy AI: conditions (turn, HP%, MP%, state, party level, switch) are
  checked, actions rated at or below max-3 are dropped, the rest are
  weighted. HP/MP windows are baked as percent.
- Buffs/debuffs: +-1 stage per hit, capped at 2, 25% per stage, timed.
  Stats floor at 1. Formulas can read game variables and switches.
- Targets: `bt_make_targets` covers all 12 scopes and retargets when the
  chosen target is dead.

Known gaps (need data the bake does not carry yet): state durations and
regen, state resistance, state MHP/MMP/LUK shifts, REC/MRF/CNT/TGR/PHA/MCR
traits, class/equip param-rate traits (4 soul armors), GRD.

## Debugging a fight

The battle trace is buffered in RAM and written to `ms0:/fh_battle.txt` when
the battle ends, so combat never waits on the Memory Stick. Build with
`-DFH_BATTLE_TRACE_LIVE` to write every line immediately when hunting a
crash.

## Grab minigames and long events

Some fights pause for scripted sequences: salmonsnake SNATCH (coin flip,
then a DODGE loop), guard/Darce SNAP NECK (coin flip, then text). These
are verbatim troop data -- SNATCH with the tongue out runs 12-20 seconds
of waits, choices and text, silently when audio is off, so answer the
HEADS/TAILS prompts and keep pressing CIRCLE through the text.

Mashing SQUARE (the MV 'shift' button) during the tongue struggle takes
the escape branch when you hold the right item; without it the loop ends
in the devour path. Button checks (`111/11`) in 20+ troops were dead
until the held-button snapshot was wired -- if an old blob is staged,
restage so the baked button names (`s=shift`) are present.

## Limbs

Limbs are troop members with low HP, each drawn as its own sprite. The
torso art is a full body image (that is how the game ships it), so the
body keeps its shape when pieces detach. Switch 3155 turns on the
dismember path: the setup page swaps in the high HP rows and heals them
full, and the destroy pages can wipe the troop.

## Troop events

Pages come straight from Troops.json (conditions, spans, switches).
Moment pages run while choosing, turn end pages run after the round,
reserved common events run in the event phase. Switches and inventory
carry over to the map.

## Tests

`tests/test_troopflow.c` replays fights on PC (leg kills, destroy
pages, escape, Talk). Run the suites in `docs/pipeline.md` before
changing battle code.

## Debug menu

On the map, press Select to open the battle debug list (plain text
over the map, ugly on purpose). Up/Down moves, Circle starts that
troop, Cross closes. Player movement and talk are frozen while it
is open. It needs `data/troops.blob` staged; without it a notice
is shown instead of opening.
