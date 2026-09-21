# TODO (build order)

## Done (verified)

- [x] Converter: images/audio/data/snippets/bake (1,726 files, 37/37 goldens)
- [x] Interpreter 89/89 codes + text decoder (token-exact)
- [x] Map renderer: reference PNG → C port → PSP (15,522 quads exact)
- [x] EBOOT link v1: message mode (Map001 ev37) on device
- [x] Player: 40×55 single-cell sprites, plain default sheets, OG timing
- [x] Z-order: higher-mask split (lower → chars → upper)
- [x] Real-hardware boot (user-link fix); 60 fps locked on PSP-2000
- [x] Walk cycle persists across tiles (all 3 frames, OG rhythm)
- [x] NPCs render (15 baked, Y-sorted) + talk trigger + EV213/EV020 dialogue
- [x] 111-type-8 (has-item) in C + sim, goldens green

## Next

- [ ] **Lighting/fog** (full §5.5 path; benchmark on HW — headroom confirmed)
- [ ] Battle: damage VM (D3) + actor/enemy DB (D4) + troop events in battle
- [ ] Menus / synthesis / saves (D6 + D10) + hunger vars
- [ ] Plugin backends: Gab, Terrax light counts, chase AI, filters (keep
      switch 3520), footstep SE
- [ ] NPC sprites + Y-sorting between characters; torch-graphic swap on
      tinderbox event (322 data mapped, sheets converted)
- [ ] Message layout: bitmap font (§4.1), YEP_MessageCore positioning,
      choice windows
- [ ] Phase 2 slice: boot map 72 → title → walk → talk → save/load →
      one limb troop, 15–30 fps on HW
- [ ] Optimizations: SE→ADPCM, jumps binary, u16 tile ids, sv_actors review

## Deferred (recorded, not forgotten)

D1 movement execution · D2 map context · D3/D4 battle+DB · D5 audio
backend · D6 message layout · D7 pictures/weather · D8 transfer execution ·
D9 overworld encounters · D10 shop/menu/save/title scenes.
(See progress log for details.)
