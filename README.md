# F&H PSP — Fear & Hunger PSP Port

Native C reimplementation of Fear & Hunger for PSP-2000/3000.
**Code only — no game assets.** You must own the game; converters build
everything from your copy.

## Status

Boots on real hardware, **60 fps locked** (goals were 15–30).
Map030 (Mines): tilemap + 4 playable characters (L/R to switch) with
OG-exact movement/animation, z-ordered sprites, message demo on SELECT.
Next: lighting/fog. See `docs/todo.md`.

Controls: D-pad move · L/R switch character · SELECT message demo ·
START exit.

## Build

```bash
python3 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24
python3 tools/pad_sheets_pow2.py && python3 tools/pad_chars_pow2.py
python3 tools/bake_higher.py --map Map030
# stage data (see docs/pipeline.md), then:
cd psp/gu_demo && make && cd ../.. && rm -f FHDEMO.zip && \
  (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
```

Copy `EBOOT.PBP` to `PSP/GAME/<NAME>/`. Toolchain: `~/pspdev`.

## Layout

- `runtime/` — portable engine (interp, text, map, player)
- `psp/gu_demo/` — PSP shell (main/render/input + data staging)
- `psp/hw_hello/` — hardware sanity EBOOT (black-screen triage)
- `tools/` — converters and bakers
- `tests/` — golden-master harnesses (must stay green)
- `docs/` — overview, pipeline, engine-notes, todo, progress log

## Legal

Personal use. Game assets © Miro Haverinen — never committed, never
redistributed. Keep forks private. Reimplement behavior; respect plugin
licenses.
