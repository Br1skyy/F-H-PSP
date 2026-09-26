# Todo

## Done

- Asset converters (decrypt, downscale, palettise, swizzle) and bakers
- Event interpreter (all 89 codes, plus the extra battle codes and
  condition types the troops needed) plus text decoder
- Map renderer (tiles, z-order, collision), player movement and sprites
- NPCs, talk triggers, message window with choices
- Exact Terrax lighting through a CPU mask plus generated light tables
- Real hardware boot at locked 60 fps on PSP-2000
- Battle data: all 220 troops stream from one indexed blob, shared
  skill CEs stream beside it, enemy art and per-area battlebacks
  stream per fight
- Battle UI as the original has it: target window, selection blink,
  gauges and palette colors, encounter flash, foe depth by screen feet
- Audio engine: mixed SE voices plus streaming music, ambient, and
  jingles with volume, pan, pitch, and fades
- Intro movie player (hardware video decode) plus transcode script
- Select-button debug menu that starts any troop
- `tools/build.py`, a portable builder that makes a byte-identical
  EBOOT with no make or sh, plus `make dist` filling `Build/`
- Title screen (card art, New Game, Continue, Options with volumes),
  main menu (Item, Skill, Equip, Status with faces), working
  equipment changes with stat preview, and Memory Stick saves

## In progress

- Battle polish from playtesting (timing and volumes need ears on
  real hardware)
- Fog (lighting is exact, fog is not verified yet)

## Next

- Menu item and skill use, shops, formation (off in the original)
- Map events beyond talk (doors, footsteps, and 200 other Map030
  triggers), then more maps
- Defeat routing: 301 win/lose branches, map transfers, game over
  screen (the guard torment scene falls out of this)
- Hunger and darkness systems
- Bug reports from playtesting (attach the battle log lines and
  `ms0:/fh_battle.txt` when something looks wrong)
