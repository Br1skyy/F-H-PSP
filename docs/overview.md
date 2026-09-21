# Overview

Native C reimplementation of Fear & Hunger (RPG Maker MV game) for PSP.
The repo holds **code only** — no game assets. You must own the game and run
the converters (see `pipeline.md`).

## Targets

| Item | Target | Status (2026-09-20) |
|---|---|---|
| Screen | 480×272 | done |
| Framerate | floor 15, typical 20–30 | **60 (vsync-locked) on PSP-2000** |
| Hardware | PSP-2000/3000 (64 MB), degrades to 1000-class 24 MB | boots, ~5 MB footprint |
| Distribution | converter + runtime only, no game assets shipped | this repo |

## Architecture

**Converter (PC, Python)** reads the owned game folder and writes packed data:
decrypt → downscale (24px tiles, 0.5×) → palettise (T8, index 0 =
transparent) → swizzle (16×8 blocks) → `.t8` + `.clut` + `.meta.json`,
plus baked tables (jumps, passability, autotiles, anims) and packed maps.

**Runtime (C, PSPSDK, GU)** reimplements the MV systems this game uses
(interpreter 89/89 event codes, tile renderer, player, text). It does not
run the original JS engine. The original `www/js/*.js` is the spec —
port from source, never from memory.

## Method per system

Read the JS → write the C → run the same scenario in both → diff → fix.
Golden-master tests in `tests/` (`test_interp`, `test_map`, …) must stay
green. Trial evidence and dead ends go to `progress.md` (append-only).

## Legal

Reimplement behavior; do not copy source or assets into redistributables.
MV runtime and each plugin have their own terms. Game assets stay with the
owner's copy. Keep pushes private.
