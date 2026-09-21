# Progress log (append-only — do not rewrite entries)

Moved verbatim from `fear-and-hunger-psp-port-plan-v2.md` during the 2026-09-20 docs reorganization. New entries append below.

---

## 11. Progress log (append-only — original sections above are untouched)

### 2026-09-19 — Phase 0 audit DONE (tools/audit.py, tools/override_map.py, tools/decrypt.py)
> DONE: §4.2 audit script built as `tools/audit.py` (extended: also dumps map sizes/event counts, switches/vars, encryption, asset sizes, plugin list). Run: `python3.12 tools/audit.py "Fear & Hunger_WIN/www" --out audit_out`. Outputs `audit_out/audit.json` + `audit_out/audit_report.txt`.
> DONE: §3.2 override map built as `tools/override_map.py`. Run: `python3.12 tools/override_map.py "Fear & Hunger_WIN/www" --out audit_out`. Outputs `audit_out/override_map.txt` + `.json`. Result: 1957 touched methods, **92 methods touched by 2+ plugins** — battle pipeline (`Game_Action.*`, `Game_Battler.*`) is the hotspot, Olivia_OctoBattle repeats many defs internally. Read multi-touch list first.
> DONE: §4.3 decrypt helper built as `tools/decrypt.py` (`decrypt_bytes`, `--check`, `--file`). Verified against real copy (see Q6).
> DONE (user): §4.1 tile scale test — user did it, skipped by converter team.

Findings against §9 open questions:
1. **Q1 event codes — ANSWERED.** 57 plugin entries in `js/plugins.js`, 48 enabled (matches plan). Total ~1.07M event-command occurrences. Top codes: 121 switches (178k), 111/412 conditionals (149k each), 401 text body (111k), 101 show text (72k), 411 else (75k), 505 comment next (65k), 230 wait?/picture-ish (54k), 122 vars (48k), 129 actors (47k), 205 move route (44k), 223 tint (42k), 313 transfer?/actor ops (39k). Plugin commands (15,966 total): GabText 7504, ShowGab 6615, ClearGab 981, Light 259, steamworks 205, createFilter/setFilter 123 each, RegionReveal 45, choice_text/disable_choice 24 each, DisableDashing 21. Note tags: Synthesis/Item 28 each, Target 8, Critical 5, Bypass/Armor 4, skill_id 2, CCPS_weapon 1. Full table in `audit_out/audit.json`.
2. **Q2 VM fraction — IN PROGRESS** (see `tools/formula_coverage.py`, next entry).
3. **Q3 hunger — ANSWERED.** NOT actor EXP. Dedicated variables: var 27 `HUNGER` + 106 `TempHUNGER` + per-character 138–150 (`HUNGER_MERC/KNIGHT/DKPRIEST/Outlander/Captain/Moonless/Kid/Marriage/Fusion/Baby/Ghoul1-3`). So converter must preserve vars 27/106/138-150 semantics; EXP-bar "Hunger" label in menus is display-only.
4. **Q4 WeaponSkill / physical_attack_animation — ANSWERED.** `WeaponSkill.js` (Sasuke Kannazuki): weapon note `<skill_id:N>` replaces normal Attack (id 1) with skill N; first-weapon wins for dual-wield. `physical_attack_animation` (Coelocanth, param `CC_physicalattacks`): actors draw weapon for physical-hit-type skills + skill-type list + `<CCPS_weapon:true/false>` per-skill override. Both = small data-driven rules, no heavy runtime cost.
5. **Q5 biggest maps — ANSWERED (lights still open).** Largest by tiles: Map030 145×105 (15,225 tiles, 398 events), Map020/Map174 125×90 (11,250 tiles, 321/222 events), Map182 160×70 (11,200 tiles, 333 ev), then 140×70 maps (9,800 tiles). These 4 are the benchmark-A candidates. Light counts per map NOT yet counted (Terrax `Light` plugin cmd appears 259× globally) — TODO: count Light radius/color params per map.
6. **Q6 encryption/sizes — ANSWERED.** Encrypted: `hasEncryptedImages=True, hasEncryptedAudio=True`, key present. Counts: 2074 `.rpgmvp` (img), 433 `.rpgmvo` (audio), 0 `.rpgmvm`. Raw sizes on disk (encrypted): img ~500.8 MB, audio ~208.4 MB, movies ~2.3 MB, data ~144.7 MB, js ~6.5 MB. Converter MUST implement §4.3 decrypt first (done in `tools/decrypt.py`).
7. **Q7 switch 3520 — ANSWERED (name only).** Switch 3520 = `<Global Meta> Filter Effects`; only `filter`-named switch in System.json. Gates FilterController createFilter/setFilter/eraseFilter (123/123/5 uses). Beyond-filter effects still TODO: grep event pages for `3520` conditional branches.
8. **Q8 obfuscation — ANSWERED (preliminary).** Heuristic scan (long-line/minified/`_0x` check) over all 48 enabled plugins: **none flagged**. All sampled sources (TerraxLighting, WeaponSkill, physical_attack_animation) have readable headers. Treat as non-obfuscated unless a later read finds otherwise.

### 2026-09-19 — §4.4 formula coverage DONE (tools/formula_coverage.py)
> DONE: built `tools/formula_coverage.py` (Python port of the acorn idea — no Node needed). Run: `python3.12 tools/formula_coverage.py "Fear & Hunger_WIN/www" --out audit_out`. Outputs `audit_out/coverage.json` + `coverage_report.txt`.
> Findings (answers Q2): **damage formulas 188 occurrences / 28 distinct — 100% compilable to the §5.3 VM subset.** All are plain constants (`30`,`35`,`20`,`400`,`900`…) plus `a.atk * 4 - b.def * 2`. Float-vs-double risk (§5.3) is moot for constants but keep `double` for the `a.atk*4-b.def*2` case.
> Script lines (355/655): 4227 occurrences / 1076 distinct — **NOT VM exprs, all runtime API ops: 3314× setCharacter/setBattlerImage, 669× `$gamePlayer.refresh()`, ~243× setFaceImage variants, 1× `TouchInput.update=function(){}`.** Script conditions (111/12): 0 occurrences. **Verdict: no ES5 interpreter (mujs/Duktape) needed for the observed set — hand-port 4 ops** (`refresh-player`, `set-character`, `set-battler`, `set-face`) + ignore the single TouchInput stub (mouse-only, per dropped DisableMouse). Fallback list = 1 line (the TouchInput stub).

### 2026-09-19 — Phase 1 feasibility demo DONE on PC (demo/feas_demo.py, HW proof still open)
> DONE: built `demo/feas_demo.py` (pygame, runs with `python3.12`; headless benchmark: `python3.12 demo/feas_demo.py "Fear & Hunger_WIN/www" --seconds 8 --no-window`). Uses REAL assets from the player's copy (decrypt-on-the-fly via `tools/decrypt.py`, Map030 worst case 145×105/398 events, real Ancient_A sheet downscaled 48→24px, 12 lights @ 120×68 quarter-res buffer, 5 fog layers, 398 event sprites + staggered MV-style BFS chase, battle overlay with popups). No PSPSDK on this machine, so this is a PC structural approximation — NOT the §8 hardware proof.
> RESULT (PC headless, 8 s): **62.3 fps, ~858 quads/frame (3 layers), pathfinding 0.42 ms/frame (staggered 1/4 chasers/frame), 398 events culled to on-screen subset.** VRAM-shape matches plan: 480×272 framebuffer + one resident sheet + 120×68 light buffer. Interpretation: logic + draw structure is light enough that a batched GU port (§5.4) has headroom; risk stays where the plan says (fill-rate of 5 fog layers + light multiply on real 2 MB VRAM, §7). NEXT: run tests A–E on real PSP-1000/2000 at 333 MHz per §8 pass criteria (worst ≥15, typical ≥20); try window mode (`--seconds N` without `--no-window`, arrows/WASD + F/L toggles) for visual check.

### 2026-09-19 — PSP-native demo DONE (psp/gu_demo/EBOOT.PBP, toolchain ~/pspdev)
> DONE: ported the PC demo to C + GU. Sources: `psp/gu_demo/main.c` + `Makefile` (build.mak pattern from `sdk/samples/gu/blit`). Toolchain found at `~/pspdev` (psp-gcc 15.2.0 Allegrex; NOT on default PATH — build with `export PATH=$PATH:~/pspdev/bin`). Build: `make` in `psp/gu_demo` → `EBOOT.PBP` (~144 KB, zero warnings). Copy the `gu_demo` folder's EBOOT to `PSP/GAME/FHDEMO/` on hardware or open it in PPSSPP.
> What it stresses (same A/B/C shape as the PC demo, real GU calls): 3 tile layers as batched `GU_SPRITES` (~858 quads/frame), 398 events w/ on-screen culling + bounded-BFS chase staggered 1/4 per frame, 12 additive light glows + dark overlay (the §5.5 *fallback* path — full quarter-res rendertarget multiply is v2), 5 scrolling fog bands, battle plate + popup bars, RTC fps + path-ms HUD. Controls: D-pad scroll, CROSS fog, CIRCLE lights, TRIANGLE battle, START exit.
> HONEST SCOPE NOTE (user asked "isn't it minimal?"): yes — this demo ports ~5% of a game: procedural colored quads (no real T8 sheets yet), no interpreter/battle logic/audio/saves/menus. What it DOES prove on hardware: vertex throughput, fill-rate of fog+lights, and chase-AI cost at real event counts — the three plan-flagged risks (§7). Next steps in order: (1) read fps off real HW/PPSSPP, (2) v2 with one real T8 swizzled sheet (§5.4), (3) then the slice (§8 Phase 2) — interpreter + messages + one limb battle.

### 2026-09-19 — Converter step 1 (images) DONE (tools/convert_assets.py, md §2 items 1–2)
> DONE: built `tools/convert_assets.py` per the md's build order (converter FIRST, before map renderer). Pipeline: decrypt (§4.3) → downscale (world art by TILE/48, default `--tile 24`) → 8-bit palettise (index 0 = transparent, 255-color MEDIACUT + RGBA8888 CLUT) → PSP swizzle (16×8 blocks, same algorithm as the gu/blit sample) → `.t8` + `.clut` + `.meta.json` + `manifest.json` with VRAM estimate. Run: `python3.12 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24 [--only <cat/prefix>]`.
> VERIFIED on real assets: `Ancient_A` 384×768 → 192×384 T8 (72 KB, stride-exact, CLUT 1024 B, deswizzle roundtrip uses all 256 indices); `mercenary_torch` → 480×440 (206 KB). Bug caught by verifier: PIL mode-'1' mask packs bits and truncated the index stream — fixed to mode-'L'. Lesson for runtime: trust stride asserts, not eyeballing.
> VRAM MATH (plan §7 check): one tileset sheet = 72 KB T8 + 1 KB CLUT — ~20 sheets fit the ~1.5 MB texture budget; a character sheet = ~206 KB. Full-set conversion stays on disk, runtime streams 1–2 sheets as planned. NEXT per build order: map renderer (v2 of gu_demo loading one real `.t8` sheet), then interpreter + messages.

### 2026-09-19 — Converter items 3–6 DONE (converters before logic, per build order)
> DONE `tools/convert_audio.py` (§2.3): `.rpgmvo`→decrypt→ BGM/BGS/ME to MP3 96k (HW-decodable, §7) and SE to WAV 22 kHz mono s16 PCM. Verified: `fear_and_hunger` 4.2 MB→1.2 MB MP3; SE → `pcm_s16le 22050Hz mono` (ffprobe). Full game ≈ 411 SE + 7 BGM + BGS/ME; run without `--only` for the whole set (needs ffmpeg, present).
> DONE `tools/pack_data.py` (§2.4 + §7 archive): all 183 `data/*.json` → one `game.pak` (magic FHPK, global pooled string table — 24,253 strings — index + typed blobs). JSON 144.7 MB → PAK 123.6 MB (85%), **roundtrip decode of every file: 0 errors**. Note: 123 MB is disk/streaming size, not RAM — runtime loads per-map via the index (§7 sequential reads).
> DONE `tools/compile_snippets.py` (§2.5 + §4.4/§5.3): all **188/188 damage formulas → `formulas.bin` (4,944 bytes, 10-byte Ins: u8 op/u8 arg/f64 imm) with 0 fallback and 0 host-eval diffs**. Two parser bugs caught by verification (tuple-dispatch, `[ab]` lexing `atk` as `a`+`tk`) — fixed. Script *lines* stay an op table for the interpreter (4 hand-ported API ops, no ES5 engine needed — see §4.4 entry).
> DONE `tools/bake.py` (§2.6, ported from source per §3.1): (1) `jumps.json` — **270,178 skip/loop commands in 20,675 lists** with §5.2 jump targets baked from engine-exact `skipBranch`/command113/413 semantics (rpg_objects.js:10153+); (2) `passability/*.bin` — 169 maps, per-tile 4-bit LEAVE masks from static layers via ported `checkPassage` (rpg_objects.js:5911), data layout `data[(z*h+y)*w+x]`; spot-check: flag[0]=0x10 → all 3,464 empty Map030 tiles mask 0; (3) `autotiles.json` — TILE_IDs + FLOOR(48)/WALL(16)/WATERFALL(4) tables extracted from rpg_core.js, sizes asserted; finding: shapes are pre-baked in map data (`kind*48+shape`), so runtime needs tables + kind→cell resolver only — no neighbor scan; (4) `anims.json` — all 310 animations as frame cell/timing lists. Gameplay-significant find: **113 (Break Loop) is used 6,676× outside any loop as an abort-event idiom** — engine-faithful behavior (jump to list end) baked, not flagged.
> CONVERTER SUITE COMPLETE (§2 items 1–6). NEXT per build order: map renderer — gu_demo v2 loading a real `.t8` + baked passability + autotile tables; then interpreter + messages fed by `jumps.json` + `game.pak`.

### 2026-09-19 — Audio validation + map renderer v2 (real art on the GU path)
> AUDIO VERDICT: GOOD. All 7 BGM + sampled BGS/ME → MP3 (HW-decodable), 8 sampled SE → PCM WAV; every output ffprobe-decode-checked, 0 bad. `Cursor1→806 B` is a genuine 18 ms UI click, not a bug. Fixed manifest merge bug (incremental `--only` runs overwrote instead of merging). Full 411-SE conversion is uniform pipeline + compute time — deferred to final build.
> DONE gu_demo v2 (`psp/gu_demo/main.c`, `EBOOT.PBP` ~150 KB, zero warnings): SQUARE-toggable real-texture mode (default when files load) rendering the converted `Ancient_A` sheet through **`GU_PSM_T8` + 256-entry CLUT + `GU_NEAREST`** (§5.4) as batched `GU_SPRITES` with `GU_TFX_MODULATE` tint from the baked Map030 passability mask (white = walkable, red = blocked). Data ships next to the EBOOT (`psp/gu_demo/data/`: `.t8` 72 KB + `.clut` 1 KB + `Map030.bin` 15 KB → copy to `PSP/GAME/FHDEMO/data/`). Camera clamped to the 145×105 map bounds (negative scroll broke UV math — fixed). HUD shows TEX/PROC mode. Texture is RAM-resident in v2 (VRAM placement TBD). NEXT per build order: interpreter + messages fed by `jumps.json` + `game.pak`; HW visual check of the T8 sheet still open.

### 2026-09-19 — Interpreter core v1 + golden-master tests (runtime/interp.*, build order: interpreter + messages)
> SCOPE (answers "how much needs interpreting"): **1,718,042 command occurrences, 89 distinct codes, 20,675 branch lists.** Top codes: 121 switches (178k), 111/412 (149k each), 401 text (111k), 230 waits (54k), 122 vars (49k), 129 actors (48k), 205 routes (45k), 223 tint (42k), 313 (39k), 250 SE (32k), 356 plugin cmds (16k). Core v1 implements the structural spine (0/101/401/405/102/402/403/404/111/411/412/112/113/413/601-604/121/122/230/108); the rest logs-and-continues (`unknown` counter, never crashes).
> DONE `runtime/interp.h/.c` — platform-independent C (same files build for PC tests and the PSP EBOOT): baked `jump` targets, per-indent branch memory, 3601 switches / 451 vars (real sizes), text buffer, wait frames, executed-pc trace. Three engine-source corrections applied during build: 403 enters iff `branch<0` (command403); 411 needs a baked jump + skips else iff branch true (command411); 113-without-413 = jump past end (command113 scan semantics).
> DONE golden-master harness (§3.5 in miniature): `tools/emit_interp_test.py` converts REAL lists to vectors + Python reference sim → `tests/test_vec.h`; `tests/test_interp.c` compares trace/state/text/waits/unknown. **7/7 ALL PASS** (T1 switches, T2/T2b conditional false+true paths, T3/T3b choice select+cancel, T4/T4b battle win+lose branches — Map001 ev1/ev9/ev87, Map024 ev254). Two test-harness bugs (newline convention, empty-array init) caught and fixed; C core itself passed unchanged on first full run.
> INVESTIGATION STATUS (user asked): yes, every behavior is read from source first per §3.1 — rpg_objects.js `checkPassage`/`tileId`/`isPassable`/all `commandXXX` branch handlers/`skipBranch`, rpg_core.js TILE_IDs + autotile tables, WeaponSkill + physical_attack_animation plugin sources, System.json (hunger vars, switch 3520), real event-list dumps (Map001 ev9 soul-stone chains). No behavior ported from memory.
> NEXT: widen op coverage by frequency (230 waits done; 129/205/223/313/250/117/355/356 next), message escape codes (Window_Message), then link interp into the PSP EBOOT with on-screen text.

### 2026-09-19 — Interpreter widening round 1 (117/129/205/223/250/313, all from source)
> DONE: read `command117/129/205/223/313/250` + `iterateActorEx/Id` (rpg_objects.js) and implemented: 117 common-event child frames (call stack depth 8, id→list table), 129 party roster (16 slots; actor `setup` init deferred), 205 move-route queue (char/steps/wait recorded; movement system pending), 223 screen tint + optional wait, 250 SE log (name + count; backend hooks later), 313 actor add/remove state via ported `iterateActorEx` (direct id or var-indirect, whole-party when 0). `FhCmd.p` widened to 6 ints for 223's tone+duration — all vectors regenerated. Trace entries now carry frame depth in high bits so cross-frame golden tests compare exactly.
> DONE golden test T5 (Map001 ev84/pg16 → small CE): 60 steps across 2 frames, 920 tint-wait frames consumed, party=[11], SE logged, 3 routes queued, 15 `unknown` (unimplemented codes in those lists — logged, run still green). **8/8 ALL PASS.** The `unknown` counter is now the remaining-work meter: implement in frequency order, watch it fall to 0 on real lists.
> NEXT: next-frequency ops (212 anim, 203 event locate, 126 items, 311/312 actor HP/MP, 355/356 script/plugin dispatch), `Window_Message` escape codes, then link interp into the EBOOT.

### 2026-09-19 — Interpreter widening round 2 (126/127/128, 311/312, 212, 203)
> MAPS QUESTION (user asked "how will we create the exact maps from our sprite sheet"): that is the **map renderer** build-order step, after the interpreter. Design (already baked for): map tile ids (`data[(z*h+y)*w+x]`) → autotile kind/shape via `autotiles.json` tables + kind→cell resolver → quads sampling the converted `.t8` sheets (gu_demo v2 proves the texture path); passability tints from `passability/*.bin`; events/characters placed from map data. Not built yet — interpreter first, per order.
> DONE: read `command126/311/312/212/203` + `operateValue`/`changeHp`/`iterateActorEx` (rpg_objects.js) and implemented: 126/127/128 gainItem via ported `operateValue` (const/var operands; arrays sized from data: 256/64/64); 311 `changeHp` (alive-only, allowDeath clamp) + 312 `gainMp` (floor 0; mmp clamp needs actor data) over ported `iterateActorEx` (direct/var-indirect/whole-party); 212 animation requests with waits from baked frame counts (`duration = frames*4+1`, rpg_sprites.js:1235); 203 locate direct/var/swap + direction into a 512-slot char table. Source facts used: `operateValue` = ±(const|var); `changeHp` clamps at `1-hp` without allowDeath; anim rate = 4.
> DONE goldens T6 (Map001 ev323: items + party [17,4,14] + HP, 20 steps) and T7 (Map001 ev222: locate + 688-frame anim wait + SE + routes, 29 steps). **10/10 ALL PASS.** Remaining meter: `unknown` on real lists + next codes (355/356 dispatch, message escapes), then EBOOT link.

### 2026-09-19 — Messages + script/plugin dispatch (escapes, 355, 356)
> ESCAPE SCAN (101,676 text lines): `\C[n]` 65k (colors 0/2/7), `\N[n]` 6.8k (actor names), `\.` 3.8k, `\}` 253, `\V` 25, `\I` 18 — **all stock MV codes, zero YEP-specific escapes in text**. So the stock `convertEscapeCharacters`/`obtainEscapeCode`/`obtainEscapeParam` port suffices for text as-shipped (YEP_MessageCore still matters for layout, later).
> DONE `runtime/text.h/.c`: engine-exact decoder (backslash→ESC, `\\`→literal, double `\V` application quirk preserved, `[digits]`-only params, uppercased codes). Verified token-for-token against the Python mirror on real message buffers.
> DONE 355 script ops (audit: every script is single-line — 655 never follows 355, so per-row handling is exact): `$gamePlayer.refresh()` counter + `setCharacterImage`/`setBattlerImage`/`setFaceImage` parsed into appearance tables. DONE 356: `command356`-style first-word/args split into a 256-entry dispatch log (GabText/ShowGab/Light/Filter… backends later).
> DONE goldens T8 (Map008 ev214: `\C`+`\N` decode, exact token stream), T9 (CommonEvents 262: costume changes), T10 (Map001 ev10: plugin dispatch). **13/13 ALL PASS**, zero warnings (`snprintf` for name copies).
> NEXT: link interp + text into the PSP EBOOT with on-screen message rendering (bitmap font per §4.1), then the map renderer step (exact tilemaps from sheets).

### 2026-09-19 — Interpreter widening round 4 (505/211/216/322/201/213/214/204)
> Coverage now **46/89 codes, 96.7% of occurrences**. Findings: **505 (65k) is move-route body data with no engine handler — silent skip, exact** (route steps come from 205 params, already queued). **214 and 213 have 0 occurrences game-wide** — engine-exact 3-line impls stay, no real-list golden possible. Rule going forward: wait frames only when duration is data-known (tint/anim); movement/transfer/scroll/balloon record-only until their systems exist.
> DONE: 211 transparency flag, 216 followers + refresh, 322 full actor-graphic swap (char+face+battler parsed from one string, refresh), 201 transfer direct/var modes into a pending slot, 213 balloon queue, 214 erase-this-event (needs ev_id + on_map context), 204 scroll queue. 322 reuses the appearance tables, so T12 asserts through the same checks as T9.
> DONE goldens T11 (presence), T12 (graphic), T13 (transfer), T14 (scroll): **17/17 ALL PASS**. Real bug caught: non-ASCII text ("François") took different paths through the emitter (`ord`-based) vs C (byte-wise) — unified both on UTF-8 bytes in `cstr`/`canon_esc`.
> NEXT: 123/135/319/231/234/235/232/318/301/333/337/119… (pictures/weather, equip, labels, battle-start), then EBOOT link.

### 2026-09-19 — Interpreter widening round 5 (flow + screen + pictures: 115/118/119/123/124/125/132/135/221/222/224/225/231-236)
> Coverage now **64/89 codes, 98.75% of occurrences**. Engine facts applied: 119 = whole-list label scan (baked into `jumps.json` — one genuinely missing 'STORE' label falls through, engine-faithful); `fadeSpeed()` = 24 (rpg_objects.js:8919); anim/pic/weather waits only when the wait flag is set; picture ids 1-based into 128 slots; 236 type is a string mapped none/rain/storm/snow→0-3. `FhCmd.p` widened 6→10 for picture params (all vectors regenerated). Dead-code census grows: **213/214/103/104/125 = 0 occurrences game-wide** (impls stay, no goldens possible).
> Loop finding: many 119-lists are intentional infinite wait-loops (first pick spun 100k steps in BOTH sims identically) — the wait/resume model already supports them; goldens use a terminating 119-list. T16c (Map002 ev1 pg2) is a rich 20-step test: fade + 7 plugin dispatches + movie stub + transfer, 2,114 waits consumed exactly.
> DONE goldens T15/T16/T16b/T16c/T17/T17b/T18/T18b. **26/26 ALL PASS**, zero warnings.
> REMAINING (31 codes, 1.25%): 319 equip (7.4k), 318 skill (2.7k), 301 battle-start (2.7k), 333/337/339/336/340/334/335 enemy ops, 320 actor name, 331/332 enemy HP/MP, 283 battleback, 282 tileset, 303 name input, 324, 353 game-over, 217, 315 exp, 352 save, + ~11 rarer. NEXT: actor/enemy/battle-start cluster, then EBOOT link.

### 2026-09-19 — Interpreter COMPLETE: 89/89 codes, 100.00% occurrences, 37/37 goldens
> FINAL ROUND (all remaining codes with occurrences + dead-code stubs): 301 battle setup (+605 shop-goods consumption by lookahead, exactly like the engine's `nextEventCode` loop), 302/303/351-354 flow+scenes, 314-321/324-326 actor admin (exp/level/params/skills/equip/names/class/nick/profile/TP), 331-340/342 enemy ops over a harness-sized troop (HP alive-only + allowDeath, states, appear/transform, battle anims, force-action slot, abort), 103/104/105 inputs, 133/134/136-140 + 281-285 system/map (victory/defeat ME, save/encounter/formation toggles, window tone, tileset, battlebacks, parallax, location-info recorded pending map ctx, vehicle, gather), 202/206/217 movement misc, 241-251 audio log (BGM/BGS/ME names, fades, SE-stop, save/replay), 261 movie name.
> Corrections during build: 317's param order differs from 315/316 (paramId is p[2]); 232 skips `_params[1]`; 236 type is a string; `FhCmd.p` 6→10; engine defaults (save/encounter/formation/namedisplay/followers ON) instead of memset-zero; audio names need 64-char caps (53-char ME names observed); troop pages added to golden candidates (enemy ops tested in real troop context).
> DONE goldens T19–T25c (equip, battle setup, troop enemy ops+anims, actor admin, game-over, ME/BGS/stop-SE, battleback, tileset, appear). **37/37 ALL PASS**, zero warnings. The interpreter now executes every event list in the game; remaining `unknown`s are only codes awaiting their systems (movement execution, map ctx, battle). NEXT per build order: link interp + text into the EBOOT (bitmap font), then the exact map renderer.

---

## 12. Status: done vs todo (living section — updated in place, plan above untouched)

### DONE (all verified, artifacts in repo)
- Phase 0: audit (`tools/audit.py`), override map (`tools/override_map.py`, 92 multi-touch methods), formula coverage (`tools/formula_coverage.py`, 100% VM-safe).
- Converter all 6 items: images (`convert_assets.py`, 1,726 files → 264 MB T8), audio (`convert_audio.py`, BGM 7/7 + samples verified), data (`pack_data.py`, 183 files → 123.6 MB pak, 0 roundtrip errors), snippets (`compile_snippets.py`, 188/188 → 4.9 KB), bake (`bake.py`: 271,361 jumps, 169 passability maps, autotile tables, 310 anims).
- Demos: PC pygame (`demo/feas_demo.py`, 62 fps PC) + PSP EBOOT (`psp/gu_demo`, procedural + real-T8 modes, ~150 KB).
- Interpreter: **89/89 codes, 100.00% occurrences, 37/37 goldens** (`runtime/interp.*`, `runtime/text.*`, `tests/`). Escape decoder token-exact on real dialogue.
- Size estimate: ~536 MB unoptimized (~400 MB with planned opts); RAM streams per §7.

### DEFERRED (recorded, not forgotten — each blocks a listed todo below)
- D1 movement execution (205/204/213 routes queued; followers gather flag; vehicle in/out).
- D2 map context (285 location-info needs terrain/region reads; 285 records request only).
- D3 battle system (301 reservation recorded; 601-603 branch on harness value; damage via `formulas.bin`; states/buffs; limb troops; force-action slot; abort).
- D4 actor/enemy DB (HP/MP caps, mhp/mmp for recoverAll, exp curves, equip slots, class data; changeHp/exp/level act on harness-seeded values).
- D5 audio backend (BGM/BGS/ME/SE names logged; MP3/PCM files converted and waiting).
- D6 message layout (YEP_MessageCore positioning, GabWindow, choice windows, bitmap font §4.1).
- D7 picture/weather rendering (state recorded; needs the renderer).
- D8 transfer execution (201 recorded; needs map loading + fade).
- D9 overworld encounters (301 mode 2 → troop -1; needs encounter tables).
- D10 shop/menu/save/title scenes (requests recorded as `scene_req`).

### TODO (md build order from here)
- [x] Map renderer reference + C port + PSP wiring (below)
- [x] EBOOT link v1: interp + text on screen (below; bitmap font still open)
- [ ] Lighting/fog: full §5.5 path, gated on benchmark A (needs HW fps — user).
- [ ] Battle: D3 + D4 + troop events in battle.
- [ ] Menus/synthesis/saves: D6 + D10 + hunger vars.
- [ ] Plugin backends: Gab, Terrax lights per-map counts, chase AI on real maps, filters (switch 3520), footstep SE.
- [ ] Phase 2 slice: boot map 72 → title → walk → talk → save/load → one limb troop, 15–30 fps on HW.
- [ ] Optimizations: SE→ADPCM, jumps binary, u16 tile ids, sv_actors review.

### 2026-09-19 — Map renderer reference DONE (`tools/render_map.py` → `render_out/Map030.png`)
> Ports `Tilemap._drawNormalTile/_drawAutotile` (rpg_core.js:5010/5038): tile ids → sheet slot + source rects on the converted `.t8` sheets (deswizzled + CLUT in Python), blitted per layer to a 24px-tile PNG. Map030: **15,522 quads == 15,522 in-data tiles, 0 missing sheets** — exact.
> Two findings: (1) **game-wide max tile id is 1663 < 2048 (A1) — this game uses ZERO A1–A4 autotiles.** The autotile subsystem (tables, kind→cell, water animation) is dead for this shipment; only A5/B/C/D/E normal tiles render. Baked tables stay as insurance. (2) False alarm, resolved by verification: the render looked 75% black, but source-cell comparison proved 58% of tiles are void-black 1536 (map borders/pits) — the output is correct, not broken. Lesson logged: compare against source pixels before suspecting the pipeline.
> This PNG is the golden master for the C port (§3.5 loop): C must emit identical quad lists. NEXT: C port (`runtime/map.*`) + PSP wiring (sheet streaming per §7), then EBOOT link.

### 2026-09-19 — Map renderer C port + PSP wiring DONE (EBOOT v3)
> DONE `runtime/map.h/.c` (portable, data-free): normal-tile resolver + autotile kind/shape/cell resolver, both ported line-by-line from rpg_core.js:5010/5038. Tables live in generated `runtime/autotile_tables.h` (`tools/emit_autotile_h.py`, sizes asserted). Engine quirks replicated: A1-relative kind numbering for all families (caught by unit test, not by reading twice), out-of-range WALL/WATERFALL shapes draw nothing (JS-undefined guard).
> DONE proofs: `tests/test_map.c` unit green (hand-computed B/C/E cells, A1/A2/A3 families); `tests/dump_quads.c` emits **15,522 quads byte-identical to the PNG golden** (`diff` clean).
> DONE EBOOT v3 (~151 KB, zero warnings): real Map030 on device — 5 T8 sheets (664 KB) + layers (122 KB) + passability in RAM, resolved per-frame on-device via map.c (copy in `psp/gu_demo/map_runtime.*`), batched per sheet, passability tint kept, camera clamped to map bounds. Copy `psp/gu_demo/data/` next to the EBOOT. NEXT: EBOOT link (interp + message box + choices).

### 2026-09-19 — EBOOT link v1 DONE (message mode on device, EBOOT v4 ~168 KB)
> SELECT toggles a message mode that runs **the real Map001 ev37 (rotten-meat table, 21 cmds)** through the actual interpreter on the PSP: live text buffer decoded for display, waits pacing frames (230s), D-pad + Cross/Circle choice selection, END screen proving state (item 21 granted). 102 needed a UI handshake, so the core grew `await_choice`/`choice_text`/`choice_n` + `FH_RUN_CHOICE`: `step()` callers (EBOOT) resolve interactively, `run()` (tests) auto-resolves presets — traces identical either way. Event embedded via `tools/emit_event_h.py` (refuses codes outside the demo's set, keeping it honest); runtime sources copied into the EBOOT with provenance headers (PSP `int`≠`int32_t` caught once: `FhEscCtx.vars` is now `int32_t`). Text renders via debug screen (runs only; color codes skipped) — the §4.1 bitmap font is the next text step.
> Goldens T26/T26b (take/leave paths). **39/39 ALL PASS**, zero warnings on both compilers.
> NEXT: lighting/fog full path (needs your HW fps from this EBOOT), then battle.

### 2026-09-19 — Single-file EBOOT (data embedded via bin2o)
> User asked: pack data INTO the EBOOT instead of loose files. DONE: 12 blobs (5 T8 + 5 CLUT + layers + passability, ~800 KB) embedded with `bin2o -i` (pattern from `sdk/samples/gu/sprite`: `label_start[]`), loaded by `memcpy` at boot. EBOOT v4 is now **one 970 KB file, zero MS file I/O, zero warnings**. Two build lessons: custom rules must go AFTER `include build.mak` (first explicit target hijacks make's default goal), and `FHDEMO.zip` is EBOOT-only again. Trade-off logged: embed-by-rebuild is right at demo scale; the full game streams from MS (§7).

### 2026-09-19 — Stress scaffolding removed (user report: green rects + battle box still on screen)
> User confirmed with screenshots: the event rectangles, battle box, fog bands and procedural mode were still rendering over the real map — all Phase-1 scaffolding, all removed. `main.c` rewritten clean: pure tilemap + message mode + fps HUD only. Also removed along the way: chase sim + BFS, passability red tint (pure REPLACE now), `CVert`/`quad2d`, light/fog/battle overlays and their toggles, `map_pass` embed (−15 KB). Boot moved to dense art at tile (70,8) — corner (0,0) is void-black and looked broken on boot. Title renamed "F&H Port".
> OPEN RENDER ISSUE (from the same screenshots): black seams + red grid lines between tiles on PPSSPP. Analysis: our UVs are integer-exact under hardware NEAREST (no bleed possible on device), which points at PPSSPP texture upscaling/filtering bleeding neighbor texels — to confirm, retest with PPSSPP Settings → Graphics → Texture filtering = Nearest (and texture scaling OFF). If seams persist under Nearest, the cause is on our side and gets its own entry. No GUI apps will be launched on the dev machine for repro without asking (lesson: stay off the user's monitors).

### 2026-09-19 — Placement bug hunt: user reports uniform tile shift on device
> User verdict on the clean build: tiles render SHIFTED uniformly (not seams) — a real placement bug, still open. Quad math is proven correct three ways, so suspicion is on device-side UV/sheet state. Per user instruction (no questions, no interactive testing): added a PASSIVE trace — the EBOOT writes `ms0:/fh_trace.txt` once at boot with camera/origin, sheet dims + head bytes + CLUT[0], raw layer samples, and resolved (tid, slot, su, sv) for the 4×4 corner cells. Run it, share the file (flatpak PPSSPP: `~/.var/app/org.ppsspp.PPSSPP/.config/ppsspp/PSP/fh_trace.txt`); the values diff directly against the golden.

### 2026-09-19 — Placement bug FOUND and fixed: vertex struct padding
> The user's screenshot showed the signature: tile soup, scrambling worse down-screen. Root cause: `TVert {short,short,u32,short,short,short}` is 14 bytes of fields but `sizeof == 16` (compiler pads to 4-byte alignment), while GU advances exactly 14 bytes/vertex per the format flags. Every vertex after the first read 2 bytes off, drifting further down the buffer — progressive scramble, exactly as photographed. The blit sample never hit this (its 10-byte vertex needs no padding). Fix: `__attribute__((packed))` + `_Static_assert(sizeof(TVert) == 14)` so it can never regress. (The passive trace file was unnecessary in the end — keeping the writer in; it's 30 lines and may help again.)
> Fresh `FHDEMO.zip` is up. Expected now: the exact reference map. If anything is still off, it should look qualitatively different (wrong art, not soup).

### 2026-09-19 — Trace verdict + layer-order fix
> The user's `fh_trace.txt` (from their own run — the passive trace works) shows device-side resolution **exactly matching the golden** (e.g. cell 70,8,z3 tid 450 → slot 6, su 480, sv 384; sheet heads/CLUT[0] sane). So data, sheets, resolver are correct on device — the remaining fault is draw order: the per-sheet buckets drew all layers of sheet 4 before sheet 5, burying upper-layer tiles from earlier sheets. Fixed to z-major outer loop (matches the reference compositor, which the user approved). Also added a vertex-byte witness to the trace (first quad raw bytes + `sizeof`) to settle any future layout doubt without screenshots.
> Green-square note: Mines_E/_D legitimately contain green palette entries in use — a misplaced green tile is a symptom of misordering, not corruption. Fresh zip up.

### 2026-09-19 — Black holes fixed: alpha blend was off
> Latest screenshot showed coherent chunks with black rectangular holes — transparent texels of upper-layer tiles painting black over lower layers (blend was disabled; alpha ignored). One-line-class fix: `GU_BLEND` + standard alpha func around the tile draws (opaque tiles unaffected). Matches the reference compositor. Fresh zip up.

### 2026-09-19 — Docs check + PC pixel-harness (no emulator needed)
> PSPSDK GU docs verified: blend setup is the canonical transparent-sprite pattern; `ClutLoad` requires 16-byte-aligned palette memory (ours: static `.bss` — TBD whether the toolchain aligns it, flagged); `TexScale`/`TexOffset` have no documented identity default, and the official clut sample sets them explicitly — so the EBOOT now does too (prime suspect for any residual uniform shift).
> DONE `tests/draw_ppm.c`: the REAL `fh_collect()` (new portable collector in `runtime/map.c`, now also used by the EBOOT) + software compositing → PPM, diffed against the golden PNG crop. **130,560/130,560 pixels bit-identical.** Our full draw path (collection, z-order, alpha) is proven on PC; remaining device-only surface is vertex submission + projection. Along the way: my C deswizzle ran backwards (copied the upload direction from the blit sample) — PC-tooling-only bug, hardware path unaffected; fixed. Also fixed for real: `sheet_cl` was 4-aligned (`...674`) but `ClutLoad` requires 16 — now `aligned(16)` (verified `...680` in the ELF).
> Fresh zip up (EBOOT runs the tested collector + explicit texture state).

### 2026-09-19 — Placement bug: data exonerated, vertex witness added
> Verified three more stages on PC: (1) device trace tile IDs == Map030.json exactly (cell 70,8 → [1576,0,0,450] both sides); (2) non-void A5 sheet cells contain real art (brightness ~200-330, only 1536 is void-black); (3) green palette entries legitimately exist in Mines_E/_D. So: bytes, resolver, sheets, order, blend, tex state all check out remotely. What remains unverifiable without eyes: vertex bytes on the wire + GE draw state. The current EBOOT's trace already includes a vertex-byte witness (first quad raw bytes + sizeof) — one `fh_trace.txt` from the current build settles it with zero interaction.

### 2026-09-19 — Revert + bin2o exonerated (readelf proof)
> The packed struct made things strictly worse (streaks), which forced a re-think: the GE fetches whole 32-bit words, so a 14-byte vertex still occupies 16 bytes on the bus — the ORIGINAL unpacked stride was correct, and packing misaligned every odd vertex. Reverted + documented in code. Separately chased the `bin2o -i` header theory (16-byte prefix in `.data` looked guilty for a uniform shift): `psp-readelf` proves `d_a1_start = 0x10` with size 73728, i.e. the label points PAST the header at the true content — embedded bytes are correct, `bin2o` innocent. So: data right, quads right, stride right; the tint/voids/filtering artifacts explain the earlier reports. Fresh zip up; waiting on one screenshot of this build to close the issue.

### 2026-09-19 — Scope clarification + full converted-size estimate
> SCOPE (user asked, confirmed): we do NOT port 2M occurrences — we port **89 distinct command codes once**, and the data drives them. 1,718,042 occurrences collapse to 89 handlers; core v1 already covers the structural spine (~1.1M occurrences: branches/switches/vars/text/choices/waits). Remaining ~30 codes in frequency order; each is one small handler + one golden test.
> SIZE ESTIMATE (measured, `--tile 24`, no optimization yet):
> | Asset | Converted | Note |
> |---|---|---|
> | Images T8+CLUT (1,726 files) | ~264 MB | sv_actors alone 193 MB (267 sheets); characters 31, tilesets 15, pictures 11, enemies 9 |
> | Audio: BGM 7/7 | 12 MB | MP3 96k, complete |
> | Audio: BGS/ME (est. from 0.29 ratio) | ~40 MB | 23 BGS (123 MB src) + 3 ME |
> | Audio: SE 400 files (est. PCM) | ~90 MB | ogg 27 MB expands ~3.4× to 22 kHz PCM |
> | Data game.pak (183 files) | 124 MB | map tile arrays dominate |
> | Baked (jumps text 5.3 + anims + pass) | ~6 MB | jumps→binary later (~1 MB) |
> | **Total on Memory Stick** | **~536 MB** | fits any 1 GB+ stick; 512 MB with optimization |
> | Planned optimizations | ~−130 MB | SE→PSP ADPCM (~10 MB not 90), jumps binary, u16 tile ids (~−40 MB pak) → **~400 MB, stretch ~250 MB if sv_actors reviewed** |
> | RAM at runtime | streaming | 1–2 sheets + 1 map + PCM SE resident (plan §7); EBOOT heap 20 MB unchanged |
> VERDICT: size is a non-issue for distribution (Memory Stick), and RAM is unaffected (streaming). The outlier to revisit is sv_actors at 193 MB — check in-game whether front-view even uses them before paying for aggressive downscale.

### 2026-09-20 — 4-black-cell residual: full exoneration log + handoff (EBOOT clean, FHDEMO.zip repacked)
> STATE: device renders Map030 at boot tile (70,8) with 216/220 cells correct; 4 cells black-but-art at view (7,8),(8,8),(18,8),(19,8) = map (77,16),(78,16),(88,16),(89,16), all y=192, tids z0 {1635,1636} + z2/z3 {2653,2404,3487,3319,4583×2} on Mines_A1/_B/_E. Deterministic across runs (0 px run-to-run diff), identical on Vulkan + software vertex path. PC pipeline is 100%: golden from converted .t8/.clut, dump_quads byte-identical, draw_ppm 130560/130560 px.
> EXONERATED (each by a dedicated device build + capture): blend off/on, alpha-test off/on, TexFlush/TexSync present/removed (no-flush build: same 4), sprite-vs-triangle prims, z0-only vs full (z0-only: same spots black), flat untextured quads (cells DRAW blue → positions + rasterization OK), UVSWAP (cells draw rock art → position path OK), per-cell binds, one-by-one submission, reversed draw order, VSHIFT y+24 (black follows the quad → moves with verts), full-framebuffer cell test (black is in fb, not capture), JSON tids == device restid, swizzle-aware FNV of device RAM rects == converted file, submitted-vert vdump (quads 183/184/194/195 decode EXACT u/v/x/y), HardwareTransform=False in PPSSPP (same 4), UNSWIZZLE linear upload (same 4 → swizzle path innocent).
> DECISIVE TEST — WHITEFILL (`-DUNSWIZZLE -DWHITEFILL`: every sheet texel = idx1 opaque gray): rest of scene goes gray, the 4 cells STAY black (0–14 avg). Content-independent → sampling/addressing/rasterization of those specific quads fails on PPSSPP's GE path despite byte-correct verts + texture + CLUT. Prime suspects for next session: PPSSPP T8 texture-cache edge case at those texel blocks (rule in/out with full software rendering `SoftwareRendering=True`, or by nudging failing UVs ±1 texel), real-hardware check (may render fine), vertex-buffer aliasing across group draws. Invisible events 138/237 at (78,16)/(89,16) confirmed non-visual (empty characterName).
> SHIP STATE: `psp/gu_demo/main.c` clean flags `CFLAGS = -Wall -O2 -G0 -DALPHATEST_ONLY -DSHOTCAP` (zero warnings); EBOOT 974148 bytes sha1 19b0f596a021; `FHDEMO.zip` repacked EBOOT-only (416892 bytes). Source keeps #ifdef-gated diagnostics for reuse (all OFF): DOFLUSH, UVPROBE, UVSWAP, ONLYFAILING, FLAT_QUADS(+FLATSOLO), PERCELL, TRIS, VSHIFT, ONE..., REVERSE_ORDER, DRAWTWICE, PERSYNC, SHEET_TINT, UNSWIZZLE, WHITEFILL. Evidence: `render_out/device_baseline.ppm` (clean, 4 black), `render_out/whitefill_probe.ppm` (all-gray except 4 black), `/tmp/opencode/EBOOT_FINAL_CLEAN.PBP` == shipped EBOOT. SUGGESTED NEXT: (1) real-PSP photo of FHDEMO.zip to see if the 4 cells are PPSSPP-only; (2) then continue build order (message-mode goldens → battle) rather than more emulator forensics.

### 2026-09-20 — Real-hardware boot FIXED (kernel imports) + character sprites/z-order/player timing
> USER VERDICT on PSP-2000/ARK: prior builds black-screen froze (hard reset); PPSSPP ran them fine. Exonerated in order: file size/transfer (byte-exact on stick), plugins (none), heap (cut 20MB→1MB, ~5MB total), SFO/PBP/ELF/entry (all valid, entry = genuine SDK _start, startup mirrors samples). ROOT CAUSE: our link (`-lpspkernel` + SDK-default `-lpspnet`) bound kernel-only imports (ThreadMan/Sysclib/Utils/LoadExec/ModuleMgr/InterruptManager-ForKernel) into a user-mode homebrew — tolerated by PPSSPP, fatal on real firmware before main() (no red, no ms0 trace, freeze). FIX (psp/gu_demo/Makefile): `LIBS = -lpspgu -lpspctrl -lpspdisplay -lpspge -lpsprtc -lpspdebug` + filter SDK net libs; newlib covers strings statically. Proof: zero ForKernel strings, zero fixup warnings — import surface now matches the known-good controller sample module-for-module (plus user sceRtc). USER CONFIRMED: game boots on real PSP-2000.
> Same session: character sheets fixed (12×8 grid, 40×55 cells; rebuilt 512×512 swizzled from deswizzled originals — prior pad scripts had double-swizzled), walking = charIndex block 0, pattern map step 0→1/1→0/2→2; default = PLAIN sheets (Actors.json + Map030 EV056; _torch is situational); movement/anim ported exact (speed 4 → 1.5px/frame, 16 frames/tile, wait 15, pattern cycle 1-2-1-0); z-ordering per Tilemap (baked higher.bin ★ mask, lower→chars z3→upper). FHDEMO.zip + FHDIAG.zip repacked. NEXT per build order: lighting/fog (HW fps now measurable on device), then battle.

### 2026-09-20 — NPCs + talk (spawn-area slice)
> RENDERER REUSE (user asked: yes): `render_player_sprite` split into `char_cell` (OG Sprite_Character addressing incl. `$` singles) + `render_character_cell` (proven texture state); player and NPCs share it. NPCs Y-sort with the player by feet (insertion sort, culled off-screen); upper star-tiles still cover all (z=4 > chars z=3).
> TRIGGER (rpg_objects.js `triggerButtonAction`): CROSS when standing → Here `[0]` prio-0 (corpses underfoot) then facing `[0,1,2]` prio-1 (faced NPCs); directionFix respected (EV020 torch never turns). Talk runs the baked list through the existing message/choice infra (SELECT demo generalized to any list).
> DATA: 15 NPCs baked from Map030 pg0 (positions/triggers/priorities/patterns as shipped); sheets `!Flame/!creature/!map_objects2` (288×192 non-$, 24px cells) + `$minerghost2` (120×220 single) via generalized `pad_chars_pow2` (meta-driven) + Makefile bin2o (`$$` escaping needed for `$` names).
> INTERP: `111` type 8 (party-has-item, EV020's tinderbox check) added to C + Python sim + emitter (id now carried); 3 goldens' unknown counts regenerated 1→0 by `emit_interp_test.py`, traces byte-identical, **ALL PASS**. Emitter generalized to multi-event (`DEMO_EV` + `M30_EV213` + `M30_EV020` incl. 111/121/127/356/411/412). PC-verified all 5 talk paths end-to-end (incl. `Light on 27` dispatch). FHDEMO.zip repacked (~2.98MB EBOOT).
> NEXT: torch-light backend + NPC roster beyond spawn slice, then battle.

### 2026-09-20 — Torch backend + conversation state
> EV020's tinderbox choice now has a visible consequence: lighting the torch
> sets switch 501, latched every map frame into `torch_lit`, which swaps all
> four characters to their `_torch` sheets (same layout, staged + verified)
> and re-enables the Fire radius flicker (±7, Terrax default).
> Conversations share one persistent world state (switches/inventory carried
> across `msg_open`), so items and the torch stay lit between talks.
> FHDEMO.zip repacked (~4.1MB EBOOT).

### 2026-09-20 — Corpses below, NPC blocking, debug spawn, SELECT retired
> Z-CORRECTION: priority-0 events (corpses) now draw right after the lower
> tiles, always under the player (z=1 < chars z=3); only prio-1 NPCs join
> the feet-Y sort. Matches screenZ = priorityType*2+1.
> COLLISION (partial, honest): same-priority events block via a baked solid
> grid (OG isCollidedWithCharacters; prio-0 never blocks); tile open/shut
> as before. Still missing: 4-dir passage bits, counters, followers.
> DEBUG SPAWN moved to (58,13) facing the saw-corpse cluster; SELECT
> message-demo entry removed (talk via O is the path; DEMO_EV unbaked).
> FHDEMO.zip repacked.

### 2026-09-20 — Dialogue behaves like the OG (pages, map behind, O clicks)
> Three fixes from device feedback: (1) message mode now renders the FROZEN
> map + NPCs behind the window (was a modal black clear); (2) the
> interpreter pauses per page (new FH_RUN_PAGE: 101-while-open and
> end-with-text pause; O resumes; END exits on O) instead of fast-forward;
> choices already used O. (3) OK button = O everywhere (talk/confirm),
> Cross = cancel. Text wraps at ~52 cols like the OG proportions.
> CORE SAFETY: PAGE adds no trace entries (skip flag so pause/resume logs
> one execution); fh_interp_run treats PAGE as continue — all 37 goldens
> green with byte-identical traces; talk paths re-verified with page-aware
> driving (EV213 needs 2 O-presses, EV020 single page + choice).
> FHDEMO.zip repacked.

### 2026-09-20 — OG skin dialog box + trap hunt instrumentation
> BOX (user: port the OG one): the window now draws the real Window.png
> skin 9-sliced exactly per rpg_core.js Window._refreshFrame (parts at
> skin offset 96,96, 24px margins halved to 12; baked via new
> tools/bake_window.py, palettised+swizzled like tiles).
> TRAP (user: stuck on first dialogue): state machine + input edges both
> verify clean on PC (UI-loop harness over all 5 talk paths: every path
> reaches END, unknowns 0). Suspects addressed anyway: page_wait now
> clears on pick/cancel (was one stale extra press), and the window shows
> a temporary dim state row (pc/await/pagewait/ended/choice) so one device
> screenshot pinpoints any residual stall. FHDEMO.zip repacked.

### 2026-09-20 — Real font, real colors, word wrap, bg/pos modes
> FONT (user: copy the OG one): the game does NOT use mplus — gamefont.css
> selects Eczar-Regular. Rebaked the atlas from Eczar (512x512, 16x32
> cells, proportional advances in font_adv.bin). Old mplus atlas retired.
> COLOR: full 32-entry text palette sampled from the game's Window.png
> grid (exact rpg_windows.js coords); \C[2] item orange verified.
> WRAP: pixel-width word wrap (was mid-word char wrap). BG/POS: 101
> background (skin/dim/transparent) and position (top/mid/bottom) now
> honored from baked params. TRIGGER (user: are corpse chats right?):
> verified identical to Game_Player.startMapEvent — prio-0 corpses fire
> only standing on their tile, both here and in the OG; EV212 is an empty
> list in the shipped data (silent in both). FHDEMO.zip repacked.

### 2026-09-21 — Eczar font, page conditions, true fill, bg/pos
> FONT: gamefont.css selects Eczar-Regular, not mplus — rebaked 512x512
> (16x32 cells) + per-glyph advances (font_adv.bin, half-px units);
> proportional layout + pixel word-wrap; C1 controls map to '?'.
> PAGES: talk resolves OG last-first page scan by switch (EV213-pg1 on
> 2640; EV020-pg1/pg2 silent once lit/taken) — no more re-light loop.
> FILL: was byte-swapped blue (0xdc2a1c1a); now the sampled skin pattern
> (66,60,52). 101 background (skin/dim/none) + position (top/mid/bottom)
> honored from baked params. TRIGGERS verified identical to
> startMapEvent (corpses fire underfoot; EV212 empty in shipped data).
> No Map030 nameboxes exist (0 \n<> codes) — names wait for maps that
> have them. Typewriter + namebox still open.
> FHDEMO.zip repacked.

### 2026-09-21 — Eczar, typewriter, namebox, page conditions
> FONT: gamefont.css proved the game uses Eczar-Regular, not mplus —
> rebaked 512x512 (16x32 cells) + per-glyph advances; proportional layout
> + pixel word-wrap; C1 controls map to '?'.
> TYPEWRITER: 2 glyphs/frame; O completes, then advances/picks/exits;
> choices pop once the body is complete.
> NAMEBOX: Yanfly \n<Name> captured in the render collector (decoder +
> goldens untouched — zero such codes on current maps); mini skin box,
> palette-6 name. PAGES: OG last-first switch scan (EV213-pg1, EV020
> silent once lit/taken). FILL: byte-swap bug fixed to sampled skin
> pattern. 101 bg/pos modes honored. TRIGGERS verified identical to
> startMapEvent. FHDEMO.zip repacked.

### 2026-09-21 — Box trims to content; Guard1 art staged
> Removed the temporary trap-diagnostic row: with no namebox active the
> window now sizes exactly to its content (no reserved name space).
> Started enemies: Guard1 troop (first battle, 7 limbs) sheets staged to
> psp data after decode verification (already pow2-aligned, direct copy).
> FHDEMO.zip repacked.
