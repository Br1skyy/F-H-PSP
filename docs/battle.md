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

- Damage: formula, element rate, PDR/MDR, crit x3, variance, guard, round.
  Drain and MP damage are capped at the target's remaining HP/MP.
- Enemy AI: conditions (turn, HP%, MP%, state, party level, switch) are
  checked, actions rated at or below max-3 are dropped, the rest are
  weighted. HP/MP windows are baked as percent.
- Buffs/debuffs: +-1 stage per hit, capped at 2, 25% per stage, timed.
  Stats floor at 1. Formulas can read game variables and switches.
- Targets: `bt_make_targets` covers all 12 scopes and retargets when the
  chosen target is dead.

Known gaps (need data the bake does not carry yet): state durations and
regen, state resistance, REC/MEV/TGR traits, class trait PDR/MDR/GRD.

## Debugging a fight

The battle trace is buffered in RAM and written to `ms0:/fh_battle.txt` when
the battle ends, so combat never waits on the Memory Stick. Build with
`-DFH_BATTLE_TRACE_LIVE` to write every line immediately when hunting a
crash.

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
