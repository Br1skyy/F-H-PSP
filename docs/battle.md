# Battle

Front-view limb fights (Guard1 troop 1, Ballista troop 44). One actor,
up to 8 foe members. Commands are Attack, Skills, Guard, Item. There is
no party window and no separate escape button.

## Options

Run (skill 40) and Talk (skill 11) are base kit for everyone, plus class
learnings. Both resolve before the round starts: Run arms its common
event and rolls the escape page (a clean getaway ends the fight with no
foe actions, a failed one spends the actor's turn), Talk plays its
dialogue first and then the foes act. Nothing attacks mid dialogue.

## Damage

Skill formulas run on a small double precision bytecode VM
(`runtime/battle.c`, baked by `tools/bake_battle_db.py`). Physical rolls
hit and evade, certain hits skip them, crits triple, variance and guard
apply, then HP/MP moves and death is recorded. The log always names the
target that was hit.

## Limbs and dismemberment

Limbs are troop members with 20 HP. Killing one hides it the same frame.
The guard torso art is a full body image with limb overlays, same as the
original, so the body keeps its shape when pieces detach.

Switch 3155 (set by the Map010 new game setup, seeded at battle start
here) turns on the dismember path: the setup page transforms the torso
and limbs into their high HP rows and heals them full. The destroy pages
wipe the troop when the torso drops to 75 percent or the head dies. That
is the real win path, same data as the original.

## Gauges

Body and Mind bars copy the original exactly: 6 px tall, dark back,
brown to red gradient, label plus current/max. Both colors are sampled
from the game's own Window.png, which is why both bars share the same
gradient instead of red versus blue.

## Troop events

Pages come straight from Troops.json (conditions, spans, switches).
Moment pages run while choosing, turn end pages run after the round,
reserved common events (Talk, Run, Guard, blood effects) run in the
event phase. Switches and inventory carry over to the map.

## Tests

`tests/test_troopflow.c` replays full fights on PC: leg kills stay
local, no round one wipe, destroy pages fire on real damage, escape can
succeed or fail, Talk plays clean. Keep it green with the rest.
