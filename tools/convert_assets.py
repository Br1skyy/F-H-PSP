#!/usr/bin/env python3.12
"""Convert game images to swizzled T8 + CLUT + meta.

Usage:
    python3.12 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted \
        --tile 24 --only tilesets/Ancient_A --only characters/mercenary
    python3.12 tools/convert_assets.py "Fear & Hunger_WIN/www" --out converted --tile 24

Output layout:
    converted/<category>/<name>.t8        swizzled 8-bit indices
    converted/<category>/<name>.clut      256 x RGBA8888 (little-endian u32)
    converted/<category>/<name>.meta.json {w,h,tex_w,tex_h,scale,palette_size}
    converted/manifest.json               all entries + totals + VRAM estimate"""
import json, sys, pathlib, struct, math
from PIL import Image

def next_pow2(n):
    p = 16
    while p < n:
        p *= 2
    return p

def swizzle8(out: bytearray, inp: bytes, w: int, h: int):
    assert w % 16 == 0 and h % 8 == 0, f'stride {w}x{h} not 16x8 aligned'
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[dst:dst + 16] = inp[src:src + 16]
                dst += 16

def decrypt_blob(blob: bytes, key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    body = bytearray(blob[16:])
    for i in range(16):
        body[i] ^= key[i]
    return bytes(body)

def convert_one(src_bytes: bytes, scale: float):
    im = Image.open(__import__('io').BytesIO(src_bytes)).convert('RGBA')
    ow, oh = im.size
    nw, nh = max(1, round(ow * scale)), max(1, round(oh * scale))
    if (nw, nh) != (ow, oh):
        im = im.resize((nw, nh), Image.LANCZOS)
    alpha = im.getchannel('A')

    mask = alpha.point(lambda a: 0 if a < 128 else 255, mode='L')
    rgb = im.convert('RGB').quantize(colors=255, method=Image.MEDIANCUT)
    pal = rgb.getpalette()[:255 * 3]
    idx = rgb.tobytes()

    idx = bytes(b + 1 if m else 0 for b, m in zip(idx, mask.tobytes()))

    clut = struct.pack('<I', 0x00000000)
    for i in range(255):
        r, g, b = pal[i * 3:(i + 1) * 3]
        clut += struct.pack('<I', 0xFF000000 | (b << 16) | (g << 8) | r)

    tex_w = ((nw + 15) // 16) * 16
    tex_h = ((nh + 7) // 8) * 8
    padded = bytearray(tex_w * tex_h)
    for y in range(nh):
        padded[y * tex_w:(y * tex_w) + nw] = idx[y * nw:(y + 1) * nw]
    swiz = bytearray(tex_w * tex_h)
    swizzle8(swiz, bytes(padded), tex_w, tex_h)
    return bytes(swiz), clut, nw, nh, tex_w, tex_h

WORLD_CATS = {'tilesets', 'characters'}

def main():
    args = sys.argv[1:]
    game = pathlib.Path(args[0])
    out = pathlib.Path(args[args.index('--out') + 1] if '--out' in args else 'converted')
    tile = int(args[args.index('--tile') + 1] if '--tile' in args else 24)
    only = [a for a in args if False]
    only = []
    for i, a in enumerate(args):
        if a == '--only' and i + 1 < len(args):
            only.append(args[i + 1])
    sysj = json.loads((game / 'data/System.json').read_text(encoding='utf-8'))
    key = sysj.get('encryptionKey', '')
    scale_world = tile / 48.0


    jobs = []
    for cat in ('tilesets', 'characters', 'enemies', 'faces', 'sv_actors',
                'battlebacks1', 'parallaxes', 'pictures'):
        d = game / 'img' / cat
        if not d.exists():
            continue
        for f in sorted(d.glob('*.rpgmvp')) + sorted(d.glob('*.png')):
            rel = f'{cat}/{f.stem}'
            if only and not any(rel.startswith(o) for o in only):
                continue

            if f.suffix == '.png' and (f.parent / (f.stem + '.rpgmvp')).exists():
                continue
            jobs.append((cat, f))

    out.mkdir(parents=True, exist_ok=True)
    entries, total_t8 = [], 0
    for cat, f in jobs:
        raw = f.read_bytes()
        if f.suffix == '.rpgmvp':
            raw = decrypt_blob(raw, key)
        scale = scale_world if cat in WORLD_CATS else scale_world
        try:
            t8, clut, w, h, tw, th = convert_one(raw, scale)
        except Exception as e:
            print(f'SKIP {cat}/{f.name}: {e}')
            continue
        cdir = out / cat
        cdir.mkdir(parents=True, exist_ok=True)
        (cdir / (f.stem + '.t8')).write_bytes(t8)
        (cdir / (f.stem + '.clut')).write_bytes(clut)
        meta = {'name': f.stem, 'cat': cat, 'tile': tile, 'scale': scale,
                'w': w, 'h': h, 'tex_w': tw, 'tex_h': th,
                't8_bytes': len(t8), 'clut_bytes': len(clut)}
        (cdir / (f.stem + '.meta.json')).write_text(json.dumps(meta, indent=1))
        entries.append(meta)
        total_t8 += len(t8)
        print(f'OK {cat}/{f.stem} {w}x{h} (stride {tw}x{th}) {len(t8)//1024}KB')

    vram_free = 2 * 1024 * 1024 - 550 * 1024
    manifest = {'tile': tile, 'n': len(entries),
                't8_total_bytes': total_t8,
                'vram_texture_budget_bytes': vram_free,
                'fits_vram_simultaneously': total_t8 <= vram_free,
                'note': 'Full-set T8 total exceeds VRAM by design; runtime streams 1-2 sheets (plan §7).',
                'entries': entries}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=1))
    print(f'\n{len(entries)} files, T8 total {total_t8/1e6:.1f}MB '
          f'(VRAM texture budget ~{vram_free/1e6:.1f}MB)')

if __name__ == '__main__':
    main()
