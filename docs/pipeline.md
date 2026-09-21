# Asset pipeline & build

All paths relative to repo root. Requires an owned copy at
`Fear & Hunger_WIN/www` (git-ignored, never committed).

## 1. Convert (first time only)

```bash
python3 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24
python3 tools/pad_sheets_pow2.py            # tilesets: deswizzle, pad, reswizzle
python3 tools/pad_chars_pow2.py             # character sheets (same, 512x512)
python3 tools/bake_higher.py --map Map030   # star-flag mask for z-order
# + extract_layers / bake steps for layers.bin, passability, jumps, anims
```

Rules that have bitten us:
- Converted `.t8` files are **already swizzled**. Never pad them as linear
  bytes — always deswizzle → pad → re-swizzle (the deleted
  `pad_char_*.py` scripts did it wrong and scrambled every row).
- PSP GE needs power-of-2 strides (tilesets 192→256, 384→512; chars →512).
- CLUT entry 0 = transparent; keep palettes 16-byte aligned for `ClutLoad`.

## 2. Stage demo data

```bash
# tilesets + map
cp converted/tilesets/{Mines_A1,Mines_B,Mines_E,Inside_B,Mines_D}.{t8,clut} \
   psp/gu_demo/data/map030/
# characters (plain defaults; _torch variants are situational, see engine-notes.md)
cp converted/characters/{mercenary,outlander,dark_priest,knight}.{t8,clut} \
   psp/gu_demo/data/
```

## 3. Build & package

```bash
export PSPDEV=~/pspdev PSPDEV_BIN=$PSPDEV/bin PATH=$PSPDEV/bin:$PATH
cd psp/gu_demo && make        # zero warnings expected
cd ../.. && rm -f FHDEMO.zip && (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
```

Link rules (learned on real hardware — a PSP-2000 hard-freezes otherwise):
- `LIBS` = graphics/ctrl/display/ge/rtc/debug only; filter the SDK's
  default `-lpspnet -lpspnet_apctl`; do **not** link `-lpspkernel`
  (its kernel stubs shadow the user-mode ones).
- Result must show **zero `ForKernel` imports** and **zero fixup warnings**.
  Check with `psp-strings -a fh_demo.elf | grep ForKernel` (expect empty).
- Keep `PSP_HEAP_SIZE_KB` small (1 MB; everything is static).

## 4. Install

Copy `EBOOT.PBP` to `PSP/GAME/<NAME>/EBOOT.PBP`. Folder name is free.

## Performance budget (§7 refresher)

VRAM (2 MB) is the real limit: framebuffers ~0.55 MB, ~1.4 MB left for
textures (stream 1–2 sheets). A 20×12 view ≈ 1,000 batched quads —
trivial. Fill rate (fog layers + light multiply) and chase pathfinding
are the flagged risks; interpreter/battle math is negligible. Audio:
hardware MP3/AT3, SE as short PCM.
