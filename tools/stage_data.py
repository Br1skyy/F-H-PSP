#!/usr/bin/env python3
"""Stage battle and map data for the PSP build.

Copies everything psp/gu_demo/Makefile embeds from converted/ into
psp/gu_demo/data/, runs the quick bakers that write there directly,
and fails loudly listing anything still missing. Run after the heavy
converters (see docs/pipeline.md). Must run from the repo root.

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


SMALL_NPC = {'!Flame', '!creature', '!map_objects2', '$minerghost2'}


def put_character(src, dst):
    """Stage a character sheet.

    Big sheets go out padded to 512x512 (the player renderer uploads
    with a 512 stride). Padding happens here, never in converted/:
    re-running the converter used to silently un-pad the cache and
    break the build. Meta dims drive the pad, so already-padded sheets
    copy through byte-identical. Small NPC sheets keep their own
    stride (the renderer addresses them directly).
    """
    import json as _json
    if (dst.suffix == '.clut' or 'characters/' not in src.as_posix()
            or src.stem in SMALL_NPC):
        dst.write_bytes(src.read_bytes())
        return
    meta = _json.loads((src.parent / (src.stem + '.meta.json'))
                       .read_text(encoding='utf-8'))
    w, h, tw, th = meta['w'], meta['h'], meta['tex_w'], meta['tex_h']
    raw = src.read_bytes()
    lin = deswizzle8(raw, tw, th)
    art = bytearray(512 * 512)
    for y in range(h):
        art[y * 512:y * 512 + w] = lin[y * tw:y * tw + w]
    dst.write_bytes(swizzle8(bytes(art), 512, 512))


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


def run_bakers(game, map_name, data):
    jobs = [
        (['tools/bake_battlers.py', '--out', 'psp/gu_demo/data'],
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
    args = ap.parse_args()
    if not pathlib.Path('psp/gu_demo/Makefile').exists():
        sys.exit('run from the repo root')
    game = pathlib.Path(args.game)
    converted = pathlib.Path('converted')
    data = pathlib.Path('psp/gu_demo/data')
    (data / 'map030').mkdir(parents=True, exist_ok=True)

    run_bakers(game, args.map, data)

    missing = []
    for name in needed_data():
        dst = data / name
        if dst.exists():
            continue
        stem = pathlib.Path(name).stem
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
                    put_character(s, d)
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
                    put_character(s, d)
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
