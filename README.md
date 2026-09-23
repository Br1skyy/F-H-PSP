# F&H PSP - Fear & Hunger on PSP

Native C port of Fear & Hunger (2018, RPG Maker MV) for PSP-2000/3000.

**Note:** this is a clean-room reimplementation, not a decomp. No game code
or assets live in this repo. You must own the game; the converters build
everything from your copy.

If you came here expecting to download the game, it is not here. You need
a legal copy and a PSP toolchain, then follow the build steps below.

## Documentation

- **Overview** - what this is, what works, method
- **Pipeline** - converters, data staging, build and install
- **Battle** - how the battle system works
- **Todo** - what is done and what is next

All docs live in `docs/`.

## Status

Honest status: it boots on real hardware and holds 60 fps. Map030 (Mines)
is walkable with 4 playable characters, NPC talk, and full guard battles
with limbs, dismemberment, Talk/Run, and troop events. There is no title
screen, menu, save system, or audio yet.

What works:

- Map030 tilemap, scrolling, collision, z-order, NPCs, talk and choices
- Guard battles (troop 1 debug fight, troop 44 ballista guard): Attack,
  Skills, Guard, Item, limb targeting, dismemberment, escape, Talk pages
- OG status gauges sampled from the game's own Window.png
- PC golden tests for interp, battle, maps, and troop flow (all green)

What is missing:

- Title, menus, items/equipment screens, saves, shops
- Audio (music and sound effects)
- Maps other than Map030, more troops, hunger and darkness systems
- Lighting and fog (map is fullbright except the player glow)

## Build

You need the PSP toolchain in `~/pspdev`, Python 3 with Pillow, and your
game files at `Fear & Hunger_WIN/www` (both gitignored, never committed).
Full steps, including data staging and the PC test commands, are in
`docs/pipeline.md`. The short version:

```bash
python3 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24
python3 tools/stage_data.py "Fear & Hunger_WIN/www"
cd psp/gu_demo && make && cd ../.. && rm -f FHDEMO.zip && \
  (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
```

Copy `EBOOT.PBP` to `PSP/GAME/<NAME>/` on your PSP. Folder name is free.

## Project Structure

```
F&H PSP/
  runtime/       # portable engine: interp, battle, map, player, text
  psp/gu_demo/   # PSP shell: main loop, renderer, input + staged data
  psp/hw_hello/  # hardware sanity EBOOT (black-screen triage)
  tools/         # converters and bakers (PC Python)
  tests/         # golden-master harnesses (must stay green)
  docs/          # project documentation
  LICENSE        # MIT for our code
```

`runtime/` has no PSP headers and builds on PC for tests. The PSP shell
in `psp/gu_demo/` owns rendering, input, and battle flow. `tools/` turns
your game copy into `.t8`/`.clut` art and baked tables.

## Controls

D-pad moves. L/R switch character. CIRCLE talks and confirms. CROSS
cancels. SQUARE starts the debug guard fight. START exits.

## Credit

- Miro Haverinen made Fear & Hunger
- Kadokawa made RPG Maker MV; battle plugins by Yanfly and Olivia
- The PS1/PSP decomp community for tools and knowledge (splat, m2c)

## License and legal

MIT for our code. The game belongs to its owners. Bring your own copy,
never commit game files, and use this for personal use only.
