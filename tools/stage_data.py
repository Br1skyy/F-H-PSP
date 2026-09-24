#!/usr/bin/env python3
"""Stage battle and map data for the PSP build.

Copies everything psp/gu_demo/Makefile embeds from converted/ into
psp/gu_demo/data/, regenerates the game-text headers (battle DB, troop
pages, animations, events, test vectors) from the owned copy, runs the
quick bakers that write into data/ directly, and fails loudly listing
anything still missing. Run after the heavy converters (see
docs/pipeline.md). Must run from the repo root.

Usage:
    python3 tools/stage_data.py "Fear & Hunger_WIN/www" [--map Map030]
"""
import argparse
import pathlib
import re
import subprocess
import sys


def swizzle8(inp, w, h):
    out = bytearray(w * h)
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[dst:dst + 16] = inp[src:src + 16]
                dst += 16
    return bytes(out)


def deswizzle8(data, w, h):
    out = bytearray(w * h)
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[src:src + 16] = data[dst:dst + 16]
                dst += 16
    return bytes(out)


def pad_height(t8, w, h, hn):
    lin = deswizzle8(t8, w, h)
    pad = bytearray(w * hn)
    pad[0:w * h] = lin
    return swizzle8(bytes(pad), w, hn)


def next_pow2(n):
    p = 16
    while p < n:
        p *= 2
    return p


def pad_pow2(t8, w, h, tw, th):
    """Pad a swizzled texture to power-of-2 dims (GE requirement).

    The converter only aligns to 16x8 blocks, so e.g. enemies come out
    112x160 while the renderer uploads 128x256: the short upload reads
    past the buffer and bands across the sprite. Identity when already
    pow2.
    """
    nw, nh = next_pow2(tw), next_pow2(th)
    if nw == tw and nh == th:
        return t8
    lin = deswizzle8(t8, tw, th)
    art = bytearray(nw * nh)
    for y in range(th):
        art[y * nw:y * nw + tw] = lin[y * tw:y * tw + tw]
    return swizzle8(bytes(art), nw, nh)


def converted_dims(src):
    import json as _json
    meta = _json.loads((src.parent / (src.stem + '.meta.json'))
                       .read_text(encoding='utf-8'))
    return meta['w'], meta['h'], meta['tex_w'], meta['tex_h']


def stage_t8(src, dst):
    """Stage one texture: byte copy, pow2-padded when needed.

    Character sheets pad to 512x512 and everything else to the next
    pow2 of its converted stride; already-pow2 files copy through
    untouched. Padding happens here, never in converted/: re-running
    the converter used to silently un-pad the cache and break the
    build. Small NPC sheets keep their own stride (the renderer
    addresses them directly), which is already pow2.
    """
    import json as _json
    raw = src.read_bytes()
    if dst.suffix == '.t8':
        meta = _json.loads((src.parent / (src.stem + '.meta.json'))
                           .read_text(encoding='utf-8'))
        raw = pad_pow2(raw, meta['w'], meta['h'],
                       meta['tex_w'], meta['tex_h'])
    dst.write_bytes(raw)


RENAMES = {
    'bal_ballista': 'enemies/ballista_ballista',
    'bal_head': 'enemies/ballista_head',
    'bal_legL': 'enemies/ballista_legL',
    'bal_legR': 'enemies/ballista_legR',
    'bal_stinger': 'enemies/ballista_stinger',
    'bal_torso': 'enemies/ballista_torso',
    'guard1': 'characters/guard1',
    'knight': 'characters/knight',
    'mercenary': 'characters/mercenary',
}

BATTLERS = ['Actor1_1', 'knight1_1', 'darkpriest1_1', 'outlander1_1']
ANIMS = ['blood_shot', 'bloodsplurt', 'bugs1', 'bugs2', 'coin_flip',
         'needle_worm', 'pinecone_pig', 'slash1']


def needed_data():
    mk = pathlib.Path('psp/gu_demo/Makefile').read_text(encoding='utf-8')
    names = set()
    for tok in re.findall(r'data/\S+?\.(?:t8|clut|bin)', mk):
        name = tok.split('data/')[1].replace('$$', '$').rstrip('"\';')
        names.add(name)
    return sorted(names)


def run_emitters(game, map_name):
    jobs = [
        ['tools/bake_battle_db.py', '--out', 'psp/gu_demo/battle_db.h'],
        ['tools/emit_troop_h.py', str(game), '--troop', '1', '--out',
         'psp/gu_demo/troop1.h'],
        ['tools/emit_troop_h.py', str(game), '--troop', '44', '--out',
         'psp/gu_demo/troop44.h'],
        ['tools/emit_troop_h.py', str(game), '--out',
         'psp/gu_demo/itemce.h', '--ces-only', '4', '14', '15', '18',
         '27', '32', '36', '46', '56', '64', '76', '92', '138', '145',
         '147', '150', '153', '185', '189', '190', '216', '234', '235',
         '240', '241', '244'],
        ['tools/emit_anim_h.py', str(game), '--out',
         'psp/gu_demo/anim_data.h'],
        ['tools/emit_event_h.py', str(game), '--out',
         'psp/gu_demo/event_demo.h'],
        ['tools/emit_lights_h.py', str(game), '--map', map_name, '--out',
         'psp/gu_demo/map030_lights.h'],
        ['tools/emit_interp_test.py', str(game), '--out', 'tests'],
    ]
    for cmd in jobs:
        print('running:', ' '.join(cmd))
        r = subprocess.run([sys.executable] + cmd,
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:] if r.stdout else '')
            print(r.stderr[-2000:] if r.stderr else '')
            sys.exit(f'emitter failed: {cmd[0]}')


def run_bakers(game, map_name, data):
    jobs = [        (['tools/bake_battlers.py', '--out', 'psp/gu_demo/data'],
         [f'bv_{b}{s}.{e}' for b in BATTLERS for s in 'ab'
          for e in ('t8', 'clut')]),
        (['tools/bake_anims.py', '--out', 'psp/gu_demo/data'],
         [f'anim_{a}.{e}' for a in ANIMS for e in ('t8', 'clut')]),
        (['tools/bake_font.py', '--font',
          f'{game}/fonts/Eczar-Regular.ttf', '--out',
          'psp/gu_demo/data/font'],
         ['font.t8', 'font.clut', 'font_adv.bin']),
        (['tools/bake_window.py', '--src',
          f'{game}/img/system/Window.png', '--out',
          'psp/gu_demo/data/window'],
         ['window.t8', 'window.clut']),
        (['tools/extract_layers.py', str(game), '--out',
          'psp/gu_demo/data/map030'],
         ['map030/layers.bin']),
        (['tools/bake_higher.py', '--map', map_name, '--game',
          f'{game}/data'],
         ['map030/higher.bin']),
    ]
    for cmd, outputs in jobs:
        if all((data / o).exists() for o in outputs):
            continue
        print('running:', ' '.join(cmd))
        r = subprocess.run([sys.executable] + cmd)
        if r.returncode != 0:
            sys.exit(f'baker failed: {cmd[0]}')


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('game')
    ap.add_argument('--map', default='Map030')
    ap.add_argument('--force', action='store_true',
                    help='re-stage even files already present')
    args = ap.parse_args()
    if not pathlib.Path('psp/gu_demo/Makefile').exists():
        sys.exit('run from the repo root')
    game = pathlib.Path(args.game)
    converted = pathlib.Path('converted')
    data = pathlib.Path('psp/gu_demo/data')
    (data / 'map030').mkdir(parents=True, exist_ok=True)

    print('padding converted textures to pow2')
    r = subprocess.run([sys.executable, 'tools/pad_pow2.py'])
    if r.returncode != 0:
        sys.exit('pad_pow2.py failed')
    run_bakers(game, args.map, data)
    run_emitters(game, args.map)

    missing = []
    for name in needed_data():
        dst = data / name
        stem = pathlib.Path(name).stem
        if (stem.startswith(('bv_', 'anim_')) or stem in ('font', 'window')
                or name in ('font_adv.bin', 'map030/layers.bin',
                            'map030/higher.bin')):
            if dst.exists():
                continue
            missing.append(f'{name} (its baker did not produce it)')
            continue
        if dst.exists() and not args.force:
            continue
        parent = pathlib.Path(name).parent
        if name == 'map030/Map030.bin':
            src = converted / 'baked/passability' / f'{args.map}.bin'
            if src.exists():
                dst.write_bytes(src.read_bytes())
                print(f'staged {name}')
            else:
                missing.append(f'{name} (run tools/bake.py first)')
            continue
        if stem == 'floor1':
            src = converted / 'battlebacks1/floor1.t8'
            if src.exists():
                dst.write_bytes(pad_height(src.read_bytes(), 512, 376, 512))
                (data / 'floor1.clut').write_bytes(
                    (converted / 'battlebacks1/floor1.clut').read_bytes())
                print(f'staged {name} (height-padded to 512)')
            else:
                missing.append(f'{name} (run tools/convert_assets.py first)')
            continue
        if stem == 'icon_IconSet':
            sys.path.insert(0, 'tools')
            from convert_assets import decrypt_blob, convert_one
            import json as _json
            enc = game / 'img/system/IconSet.rpgmvp'
            if enc.exists():
                key = _json.loads((game / 'data/System.json')
                                  .read_text(encoding='utf-8'))['encryptionKey']
                t8, clut, _, _, _, _ = convert_one(
                    decrypt_blob(enc.read_bytes(), key), 0.5)
                dst.write_bytes(pad_height(t8, 256, 320, 512))
                (data / 'icon_IconSet.clut').write_bytes(clut)
                print(f'staged {name} (converted at half scale)')
            else:
                missing.append(f'{name} (missing in game copy?)')
            continue
        if stem in RENAMES:
            for ext in ('.t8', '.clut'):
                s = converted / (RENAMES[stem] + ext)
                d = data / parent / (stem + ext)
                if s.exists():
                    stage_t8(s, d)
                else:
                    missing.append(f'{name} (missing {RENAMES[stem]}{ext})')
            print(f'staged {name} (renamed)')
            continue
        hits = sorted(converted.glob(f'*/{stem}.t8'))
        if len(hits) == 1:
            for ext in ('.t8', '.clut'):
                s = hits[0].parent / (hits[0].stem + ext)
                d = data / parent / (stem + ext)
                if s.exists():
                    stage_t8(s, d)
            print(f'staged {name}')
        elif not hits:
            missing.append(f'{name} (no converted match, run converters)')
        else:
            missing.append(f'{name} (ambiguous: {[str(x) for x in hits]})')

    form = converted / 'code/formulas.bin'
    if not form.exists():
        missing.append('converted/code/formulas.bin (run compile_snippets)')

    if missing:
        print('STILL MISSING:')
        for m in missing:
            print('  ', m)
        sys.exit(1)
    print(f'staged {len(needed_data())} files into psp/gu_demo/data/')


if __name__ == '__main__':
    main()
