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
