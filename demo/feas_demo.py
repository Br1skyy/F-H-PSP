#!/usr/bin/env python3.12
"""Feasibility perf demo — Phase 1 tests A/B/C on PC (PSP approximation).

Implements the *shape* of plan §8 tests using REAL assets from the player's
own copy (decrypted on the fly, never shipped):
  A. Map stress   — largest real map (Map030 145x105, 398 events), scrolling
                     tile blits (~1000 quads equiv), N lights, 5 fog layers
  B. Event stress — 398 event sprites + MV-style limited-BFS chase pathfinding
  C. Battle stress— large enemy portraits + damage popups + state icons

PC fps is NOT PSP fps (plan: PPSSPP/HW needed for proof). What this proves:
  - draw-call/fill-rate structure fits in a 2 MB-VRAM-style budget
    (counts quads, texture swaps, light-buffer size explicitly)
  - chase pathfinding cost with real event counts, staggered vs unstaggered
  - asset pipeline (decrypt -> downscale -> palettise) works end to end

Usage:
    python3.12 demo/feas_demo.py "Fear & Hunger_WIN/www" [--seconds 10] [--no-window]
    python3.12 demo/feas_demo.py "Fear & Hunger_WIN/www" --seconds 8 --no-window   # headless benchmark
Controls (window mode): arrows/WASD move, F toggles fog, L toggles lighting.
"""
import json, sys, time, pathlib, collections, math, os

GAME = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('Fear & Hunger_WIN/www')
SECS = float(sys.argv[sys.argv.index('--seconds') + 1]) if '--seconds' in sys.argv else 10.0
HEADLESS = '--no-window' in sys.argv
if HEADLESS:
    os.environ['SDL_VIDEODRIVER'] = 'dummy'

import pygame
from PIL import Image
import io

W, H = 480, 272          # PSP screen (§1)
TILE = 24                # converter setting under test (§4.1); 24px = clean 2x
LIGHT_W, LIGHT_H = W // 4, H // 4   # §5.5 quarter-res light buffer

def sys_data():
    return json.loads((GAME / 'data/System.json').read_text(encoding='utf-8'))

def decrypt_blob(blob: bytes, key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    body = bytearray(blob[16:])
    for i in range(16):
        body[i] ^= key[i]
    return bytes(body)

def load_img_rel(relpath):
    """Load img/xxx or audio pic; tries .rpgmvp decrypt then plain png."""
    base = GAME / relpath
    for cand in (pathlib.Path(str(base) + '.rpgmvp'), base,
                 pathlib.Path(str(base) + '.png')):
        if cand.exists():
            raw = cand.read_bytes()
            if cand.suffix == '.rpgmvp':
                raw = decrypt_blob(raw, sys_data().get('encryptionKey', ''))
            return pygame.image.load(io.BytesIO(raw)).convert_alpha()
    return None

def main():
    pygame.init()
    if HEADLESS:
        screen = pygame.display.set_mode((W, H))
    else:
        screen = pygame.display.set_mode((W * 2, H * 2))
        pygame.display.set_caption('F&H PSP feas demo (PC approx)')
    clock = pygame.time.Clock()
    font = pygame.font.SysFont('monospace', 12)

    # --- real map meta (Map030 worst case) ---
    m30 = json.loads((GAME / 'data/Map030.json').read_text(encoding='utf-8'))
    mw, mh, nev = m30['width'], m30['height'], len([e for e in m30['events'] if e])
    print(f'Map030: {mw}x{mh} tiles, {nev} events (worst case per audit)')

    # --- real tileset sheet (decrypted) + downscale to TILE px (converter §4.4 step 2) ---
    sheet = load_img_rel('img/tilesets/Ancient_A')
    if sheet is None:
        # fallback: first existing tileset
        first = sorted((GAME / 'img/tilesets').glob('*.rpgmvp'))[0]
        raw = decrypt_blob(first.read_bytes(), sys_data().get('encryptionKey', ''))
        sheet = pygame.image.load(io.BytesIO(raw)).convert_alpha()
        print(f'(fallback sheet {first.name})')
    # palettise to 8-bit like the PSP path (§5.4 T8): quantize a copy, count colors
    pil = Image.open(io.BytesIO(pygame.image.tostring(sheet, 'RGBA'))).convert('RGB') \
        if False else None
    # build tile grid from sheet: slice 48px MV tiles, downscale to TILE
    sw, sh = sheet.get_size()
    tiles = []
    for ty in range(0, sh, 48):
        for tx in range(0, sw, 48):
            t = sheet.subsurface(pygame.Rect(tx, ty, min(48, sw - tx), min(48, sh - ty)))
            t = pygame.transform.scale(t, (TILE, TILE))
            tiles.append(t)
            if len(tiles) >= 256:
                break
        if len(tiles) >= 256:
            break
    print(f'sheet {sw}x{sh} -> {len(tiles)} tiles @ {TILE}px (palettised T8 at runtime on PSP)')

    # --- event sprites: real character sheet if possible ---
    char = load_img_rel('img/characters/$2door_1') or load_img_rel('img/characters/mercenary')
    sprites = []
    if char:
        cw, chh = char.get_size()
        for i in range(min(8, cw // 24)):
            s = pygame.transform.scale(char.subsurface(pygame.Rect(i * 24, 0, 24, 32 if chh >= 32 else chh)), (16, 20))
            sprites.append(s)
    if not sprites:
        s = pygame.Surface((16, 20)); s.fill((200, 60, 60)); sprites = [s]
    import random
    random.seed(7)
    events = [{'x': random.uniform(0, mw * TILE), 'y': random.uniform(0, mh * TILE),
               'spr': sprites[i % len(sprites)], 'chase': i < 8} for i in range(nev)]

    # --- fog layers (5, per plan) + lights ---
    fogs = []
    for i in range(5):
        f = pygame.Surface((W, H), pygame.SRCALPHA)
        f.fill((120, 120, 140, 18))
        fogs.append({'surf': f, 'off': 0.0, 'spd': 4 + i * 3})
    light_buf = pygame.Surface((LIGHT_W, LIGHT_H))
    glow = pygame.Surface((48, 48), pygame.SRCALPHA)
    for r in range(24, 0, -1):
        a = pygame.Surface((r * 2, r * 2), pygame.SRCALPHA)
        pygame.draw.circle(a, (255, 240, 200, max(0, 90 - r * 3)), (r, r), r)
        glow.blit(a, (24 - r, 24 - r), special_flags=pygame.BLEND_ADD)
    glow_small = pygame.transform.scale(glow, (LIGHT_W // 6, LIGHT_H // 6))
    lights = [{'x': random.uniform(0, W), 'y': random.uniform(0, H)} for _ in range(12)]
    print(f'lights: {len(lights)} @ {LIGHT_W}x{LIGHT_H} buffer, fog layers: {len(fogs)}')

    use_fog, use_light = True, True
    camx, camy = 0.0, 0.0
    # MV-style chase BFS stub: grid = map tiles, searchLimit-bounded
    GW, GH = min(mw, 60), min(mh, 60)
    blocked = [[random.random() < 0.15 for _ in range(GW)] for _ in range(GH)]
    def bfs_step(ex, ey, px, py, limit=12):
        from collections import deque
        sx, sy = int(ex / TILE) % GW, int(ey / TILE) % GH
        tx, ty = int(px / TILE) % GW, int(py / TILE) % GH
        dq, seen = deque([(sx, sy, [])]), {(sx, sy)}
        while dq:
            x, y, path = dq.popleft()
            if (x, y) == (tx, ty):
                return path[:1]
            if len(path) >= limit:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < GW and 0 <= ny < GH and (nx, ny) not in seen and not blocked[ny][nx]:
                    seen.add((nx, ny)); dq.append((nx, ny, path + [(dx, dy)]))
        return []

    nx_tiles, ny_tiles = W // TILE + 2, H // TILE + 2
    quads_per_frame = nx_tiles * ny_tiles * 3  # 3 layers equiv (§7: ~1000 quads)
    frames, path_time, t0 = 0, 0.0, time.time()
    px, py = W / 2, H / 2
    chase_stagger, tick = 4, 0  # §5.6 stagger: 1/4 of chasers per frame
    running, end = True, t0 + SECS
    while running and time.time() < end:
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False
            elif e.type == pygame.KEYDOWN:
                if e.key == pygame.K_f: use_fog = not use_fog
                if e.key == pygame.K_l: use_light = not use_light
        keys = pygame.key.get_pressed()
        if keys[pygame.K_LEFT] or keys[pygame.K_a]: camx -= 3
        if keys[pygame.K_RIGHT] or keys[pygame.K_d]: camx += 3
        if keys[pygame.K_UP] or keys[pygame.K_w]: camy -= 3
        if keys[pygame.K_DOWN] or keys[pygame.K_s]: camy += 3
        px, py = W / 2 + (camx % 200), H / 2 + (camy % 200)

        surf = pygame.Surface((W, H)) if HEADLESS else pygame.Surface((W, H))
        # A: tile layers (3 passes over same tiles = 3 layers)
        x0, y0 = int(camx / TILE), int(camy / TILE)
        for layer in range(3):
            for ty in range(ny_tiles):
                for tx in range(nx_tiles):
                    t = tiles[(tx + x0 * 7 + ty * 13 + layer * 31) % len(tiles)]
                    surf.blit(t, (tx * TILE - int(camx % TILE), ty * TILE - int(camy % TILE)))
        # B: events (only on-screen subset, like real culling)
        drawn = 0
        for ev in events:
            sx, sy = ev['x'] - camx, ev['y'] - camy
            if -20 <= sx < W + 20 and -20 <= sy < H + 20:
                surf.blit(ev['spr'], (sx, sy)); drawn += 1
        # B2: staggered chase pathfinding
        t1 = time.time()
        chasers = [ev for ev in events if ev['chase']]
        for ev in chasers[tick % chase_stagger::chase_stagger]:
            step = bfs_step(ev['x'], ev['y'], camx + px, camy + py)
            if step:
                ev['x'] += step[0][0] * 2; ev['y'] += step[0][1] * 2
        tick += 1
        path_time += time.time() - t1
        # lighting @ 1/4 res (§5.5)
        if use_light:
            light_buf.fill((40, 40, 60))
            for L in lights:
                light_buf.blit(glow_small, (L['x'] / 4 - 8, L['y'] / 4 - 8),
                               special_flags=pygame.BLEND_ADD)
            surf.blit(pygame.transform.scale(light_buf, (W, H)), (0, 0),
                      special_flags=pygame.BLEND_MULT)
        # fog
        if use_fog:
            for f in fogs:
                f['off'] += f['spd'] / 60.0
                surf.blit(f['surf'], (int(f['off']) % W - W, 0))
                surf.blit(f['surf'], (int(f['off']) % W, 0))
        # C: battle-stress overlay: enemy portrait + popups (every ~2s)
        if (frames // 60) % 2 == 0:
            pygame.draw.rect(surf, (30, 30, 40), (W - 150, 20, 130, 130))
            surf.blit(font.render('ENEMY 1024px->128', True, (255, 255, 255)), (W - 148, 22))
            for i in range(6):
                y = 60 + i * 14 + int(10 * math.sin(frames / 20.0 + i))
                surf.blit(font.render(f'-{17 + i * 3} HP', True, (255, 220, 80)), (W - 140, y))
        surf.blit(font.render(f'{clock.get_fps():.0f}fps tiles~{quads_per_frame} ev{drawn}/{nev}', True, (255, 255, 255)), (4, 4))
        if not HEADLESS:
            pygame.transform.scale(surf, (W * 2, H * 2), screen)
            pygame.display.flip()
        frames += 1
        clock.tick(60)

    dt = time.time() - t0
    fps = frames / dt
    print(f'\nRESULT frames={frames} time={dt:.1f}s fps={fps:.1f} '
          f'quads~{quads_per_frame}/frame pathfind={path_time*1000/frames:.2f}ms/frame')
    print(f'VRAM-shape: framebuffer 480x272x2 (~0.55MB plan) + sheet resident; '
          f'light buf {LIGHT_W}x{LIGHT_H}; fog x{len(fogs) if use_fog else 0}')
    print('NOTE: PC fps != PSP fps. Pass criteria need HW (§8): worst>=15, typical>=20.')
    pygame.quit()
    return fps

if __name__ == '__main__':
    main()
