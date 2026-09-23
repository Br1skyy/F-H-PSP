# Overview

Fear & Hunger rebuilt in C for the PSP. The repo holds code only. The
original `www/js/*.js` is the spec: read the JS, write the C, run the same
scenario in both, diff, fix. Never port from memory.

## What runs today

One slice: Map030 (Mines). Walk around, switch between 4 characters, talk
to NPCs, and fight the prison guard (limbs, dismemberment, Talk, Run).
Message windows and choices match the original data. It holds 60 fps on
a PSP-2000 in about 5 MB.

## Layout

- `runtime/` is plain C with no platform headers. It builds on PC so the
  tests can drive it directly.
- `psp/gu_demo/` is the PSP program: main loop, GU renderer, input,
  battle flow, and staged data under `data/` (gitignored, built by tools).
- `tools/` converts your game copy to PSP friendly art and tables.
- `tests/` holds test harnesses for the engine pieces.

## Method

Every system is ported against the shipped scripts, then checked with
the harnesses in `tests/` (`test_interp.c`, `test_battle.c`,
`test_map.c`, `test_troopflow.c`).
