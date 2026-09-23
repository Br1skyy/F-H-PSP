# Pipeline and build

All paths relative to repo root. You need an owned copy at
`Fear & Hunger_WIN/www` (gitignored, never committed).

## 1. Convert art (first time only)

```bash
python3 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24
python3 tools/pad_sheets_pow2.py
python3 tools/pad_chars_pow2.py
python3 tools/bake_higher.py --map Map030
python3 tools/bake_battlers.py --out psp/gu_demo/data
python3 tools/bake_anims.py --out psp/gu_demo/data
python3 tools/bake_font.py --out psp/gu_demo/data
python3 tools/bake_window.py --out psp/gu_demo/data
```

What the converter does per image: decrypt, downscale (tiles 24 px,
everything else half scale), palettise to 255 colors with index 0 as
transparent, swizzle to GU T8. Output is `.t8` + `.clut` + `.meta.json`.

Rules that have bitten us:

- Converted `.t8` files are already swizzled. To pad one, deswizzle it
  first, pad, then re-swizzle. Padding swizzled bytes scrambles rows.
- The GE needs power-of-2 strides. CLUTs must be 16 byte aligned.
- Side-view battlers bake at 112 px cells in two sheets per fighter
  (cols 0-2 and 3-5, rows 1-4). A full 9x6 grid at 112 px would break
  the 512 px texture limit. See `tools/bake_battlers.py`.

## 2. Stage data

Copy the baked outputs into `psp/gu_demo/data/` (tile sheets, map
layers, passability, characters, enemies, fonts, window skin, battlers,
anims, formulas). Staged data is gitignored. The `data/` rules in
`psp/gu_demo/Makefile` list every file the build expects.

## 3. Build and package

```bash
export PATH=$HOME/pspdev/bin:$PATH
cd psp/gu_demo && make
cd ../.. && rm -f FHDEMO.zip && (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
```

Link rules (learned on real hardware, a PSP-2000 hard-freezes otherwise):

- Link graphics/ctrl/display/ge/rtc/debug only. Filter out the SDK
  defaults `-lpspnet -lpspnet_apctl`, and never link `-lpspkernel`.
- The ELF must show zero `ForKernel` imports: check with
  `psp-strings -a fh_demo.elf | grep ForKernel` (expect empty).
- Keep the heap small (1 MB). Everything is static, nothing mallocs.

## 4. Install

Copy `EBOOT.PBP` to `PSP/GAME/<NAME>/EBOOT.PBP`.

## Budget

2 MB VRAM is the real limit. Framebuffers take about 0.55 MB, leaving
roughly 1.4 MB for textures. A 20x12 view is around 1,000 batched
quads, which is trivial. Audio is hardware MP3/AT3 when we get there.
