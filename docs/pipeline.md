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
python3 tools/pad_sheets_pow2.py && python3 tools/pad_chars_pow2.py
python3 tools/bake.py "Fear & Hunger_WIN/www" --out converted/baked
python3 tools/compile_snippets.py "Fear & Hunger_WIN/www" --out converted/code
python3 tools/bake_higher.py --map Map030
```

What this does per image: decrypt, downscale (tiles 24 px, everything
else half scale), palettise to 255 colors with index 0 as transparent,
swizzle to GU T8. Output is `.t8` + `.clut` + `.meta.json`.

Rules that have bitten us:

- Converted `.t8` files are already swizzled. To pad one, deswizzle it
  first, pad, then re-swizzle. Padding swizzled bytes scrambles rows.
- The GE needs power-of-2 strides. CLUTs must be 16 byte aligned.
- Character sheets are padded to 512x512 at stage time, never in
  `converted/`. Re-running the converter silently un-pads the cache.

## 2. Stage data and build

One script stages all 106 files the Makefile embeds (copies, renames,
pads, and the quick bakers that write into `data/` directly), then
build as usual:

```bash
python3 tools/stage_data.py "Fear & Hunger_WIN/www"
cd psp/gu_demo && make && cd ../.. && rm -f FHDEMO.zip && \
  (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
```

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

Copy `EBOOT.PBP` to `PSP/GAME/<NAME>/EBOOT.PBP`.

## 4. Tests

Plain gcc, from the repo root. Run them before changing engine code.

```bash
gcc -Wall -O2 -I runtime -o /tmp/test_battle tests/test_battle.c runtime/battle.c -lm && /tmp/test_battle
gcc -Wall -O2 -I runtime -I . -o /tmp/test_interp tests/test_interp.c runtime/interp.c runtime/text.c && /tmp/test_interp
gcc -Wall -O2 -I runtime -o /tmp/test_map tests/test_map.c runtime/map.c && /tmp/test_map
gcc -Wall -O2 -I runtime -I psp/gu_demo -o /tmp/test_troopflow tests/test_troopflow.c runtime/battle.c runtime/interp.c -lm && /tmp/test_troopflow
```

The battle and troop tests read `converted/code/formulas.bin`, so run
the converters first.

## Budget

2 MB VRAM is the real limit. Framebuffers take about 0.55 MB, leaving
roughly 1.4 MB for textures. A 20x12 view is around 1,000 batched
quads, which is trivial. Audio is hardware MP3/AT3 when we get there.
