# Engine facts (extracted from the original `www/js/`)

Sources: `rpg_objects.js` (Game_CharacterBase), `rpg_sprites.js`
(Sprite_Character), `rpg_core.js` (Tilemap), `data/Actors.json`,
`data/Map030.json`, `data/Tilesets.json`. Port from these, not memory.

## Character sprites

- Non-`$` sheets hold **one character's states** on a **12-col × 8-row**
  grid. Cells are 80×110 full-res → **40×55** at our 0.5 scale
  (`patternWidth = w/12`, `patternHeight = h/8`).
- Walking block = characterIndex 0 (cols 0–2, rows 0–3). Other blocks hold
  dead/crawl/attack states (e.g. Map030 DeadMercenary = `mercenary` idx 5).
- Rows: `(direction - 2) / 2` → 2=down, 4=left, 6=right, 8=up.
  Our `Player.dir` 0–3 maps to rows 0–3.
- Cols: `pattern()` with `maxPattern = 4`, displayed as `p < 3 ? p : 1`.
  Idle = 1 (center). Our `step_frame` 0→col 1, 1→col 0, 2→col 2.
- `characterBlockX = index % 4 * 3`, `characterBlockY = floor(index/4) * 4`.
- Default sheets are **plain** (`mercenary`, … per Actors.json and Map030
  EV056). `*_torch` variants (torch raised in every frame) are situational
  (tinderbox events) — selectable later via event code 322 data.

## Movement & animation (`Game_CharacterBase`)

- `realMoveSpeed = moveSpeed` (player default **4**), dashing unused.
- `distancePerFrame = 2^speed / 256` tiles → 1/16 tile/frame = **1.5 px**
  at 24px tiles → **16 frames/tile**. We alternate 1/2px (ints stay exact).
- `animationWait = (9 - speed) * 3` = **15**; count `+= 1.5`/frame while
  moving → footstep every 10 frames; `_pattern = (p+1) % 4`; stop →
  reset to 1 (`straighten`).

## Z-order (Tilemap + screenZ)

- Tiles with `flags[tid] & 0x10` (star) → **upper** bitmap (z=4);
  everything else → **lower** (z=0). Baked per tileset (`bake_higher.py`).
- Characters: `screenZ = priorityType * 2 + 1` (player = 1 → z=3).
- Order: lower tiles → chars (Y-sorted among themselves) → upper tiles.
  No row-splitting: upper tiles always cover same-level characters.
- Shadows/table edges go lower; overpass forces layers 2–3 upper.

## Formulas

Damage/params run on a small bytecode VM using **`double`**
(single-precision FPU risks off-by-one damage at rounding boundaries).

## Lighting (Terrax replacement, §5.5) — NEXT

Full path at 1/4 res: clear light buffer to darkness color → additive
radial-gradient sprite per light
(`GU_ADD, GU_FIX, GU_FIX, 0xFFFFFFFF, 0xFFFFFFFF`) → multiply over scene
(`GU_ADD, GU_DST_COLOR, GU_FIX, 0, 0`), bilinear upscale. Fallback if the
multiply misbehaves on hardware: alpha vignette + additive sprites.
At locked 60 fps we attempt the full path first.

## Chase AI

Port MV's built-in pathfinding as-is (bounded `searchLimit`), stagger
updates across frames when many enemies chase.
