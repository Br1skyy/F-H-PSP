# Pipeline and build

All commands run from the repo root. You need an owned copy of the game
at `Fear & Hunger_WIN/www` (gitignored, never committed).

## 0. Toolchain and Python

- PSP toolchain: install pspdev (`~/pspdev` below) following
  https://github.com/pspdev/pspdev, then put it on PATH:
  `export PATH=$HOME/pspdev/bin:$PATH` (gives you `psp-gcc`,
  `psp-config`, `bin2o`).
- Python 3 with Pillow: `pip install Pillow`.

## 1. Convert the game (first time only)

```bash
python3 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24
python3 tools/pad_pow2.py
python3 tools/bake.py "Fear & Hunger_WIN/www" --out converted/baked
python3 tools/compile_snippets.py "Fear & Hunger_WIN/www" --out converted/code
python3 tools/bake_higher.py --map Map030
```

What this does per image: decrypt, downscale (tiles 24 px, everything
else half scale), palettise to 255 colors with index 0 as transparent,
swizzle to GU T8. Output is `.t8` + `.clut` + `.meta.json`.

Rules that have bitten us:

- The GE only accepts power-of-2 textures. The converter aligns to
  16x8 blocks, which is not enough, so `pad_pow2.py` pads every staged
  category (tilesets, characters, enemies) in place. Skipping it bands
  sprites across the screen.
- Converted `.t8` files are swizzled. To pad one, deswizzle it first,
  pad, then re-swizzle. Padding swizzled bytes scrambles rows.
- CLUTs must be 16 byte aligned.
- Character sheets end up padded to 512x512 at stage time, never in
  `converted/`. Re-running the converter silently un-pads the cache.

## 2. Stage data and build

One script stages all 106 files the Makefile embeds (copies, renames,
pads, and the quick bakers that write into `data/` directly), then
build as usual:

```bash
python3 tools/stage_data.py "Fear & Hunger_WIN/www"
cd psp/gu_demo && make dist && cd ../.. && rm -f FHDEMO.zip && \
  (cd Build && zip -q ../FHDEMO.zip -r .)
```

`make dist` builds the EBOOT and copies it plus any streamed
`data/` it needs into `Build/` at the repo root. `Build/` mirrors
exactly what goes on the stick: copy the whole folder to
`PSP/GAME/<NAME>/`. Battle data streams from the stick at fight
time: `data/troops.blob` (all 220 troops), `data/skillce.blob`
(shared skill CEs), and `data/enemies/` (per-foe art).

`stage_data.py` reads its file list from the Makefile itself, so it
stays correct when the build gains files. It fails loudly naming
whatever is still missing and which step produces it.

Link rules (learned on real hardware, a PSP-2000 hard-freezes otherwise):

- Link graphics/ctrl/display/ge/rtc/debug only. Filter out the SDK
  defaults `-lpspnet -lpspnet_apctl`, and never link `-lpspkernel`.
- The ELF must show zero `ForKernel` imports: check with
  `psp-strings -a fh_demo.elf | grep ForKernel` (expect empty).
- Keep the heap small (1 MB). Everything is static, nothing mallocs.

## 3. Install

Copy everything in `Build/` to `PSP/GAME/<NAME>/` on the stick.

## 4. Tests

Plain gcc, from the repo root. Run them before changing engine code.

```bash
gcc -Wall -O2 -I runtime -o /tmp/test_battle tests/test_battle.c runtime/battle.c -lm && /tmp/test_battle
gcc -Wall -O2 -I runtime -I . -o /tmp/test_interp tests/test_interp.c runtime/interp.c runtime/text.c && /tmp/test_interp
gcc -Wall -O2 -I runtime -o /tmp/test_map tests/test_map.c runtime/map.c && /tmp/test_map
gcc -Wall -O2 -I runtime -I psp/gu_demo -o /tmp/test_troopflow tests/test_troopflow.c runtime/battle.c runtime/interp.c runtime/battle_blob.c -lm && /tmp/test_troopflow
```

The troop test loads `psp/gu_demo/data/*.blob`, so stage data
first; it byte-compares the blob against the emitted headers, then
replays the demo fights on the blob path.

The battle and troop tests read `converted/code/formulas.bin`, so run
the converters first.

## Budget

2 MB VRAM is the real limit. Framebuffers take about 0.55 MB, leaving
roughly 1.4 MB for textures. A 20x12 view is around 1,000 batched
quads, which is trivial. Audio is hardware MP3/AT3 when we get there.
