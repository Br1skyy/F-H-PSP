#!/usr/bin/env python3
"""Bake battle-animation sheets for the PSP animation renderer.

Source: Fear & Hunger/www/img/animations/*.rpgmvp (decrypted with the
System key). Sheets hold 192px cells (5 cols); baked at half scale like
enemy art (96px cells) into pow2-padded swizzled T8 + RGBA8888 CLUT
(index 0 = transparent, same convention as convert_assets.py).

Usage (from repo root):
    python3 tools/bake_anims.py --out psp/gu_demo/data
"""
import argparse
import io
import json
import pathlib
import struct
import sys
from PIL import Image

SCALE = 0.5


def decrypt_blob(blob: bytes, key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    body = bytearray(blob[16:])
    for i in range(16):
        body[i] ^= key[i]
    return bytes(body)


def swizzle8(inp: bytes, w: int, h: int) -> bytearray:
    assert w % 16 == 0 and h % 8 == 0
    out = bytearray(w * h)
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[dst:dst + 16] = inp[src:src + 16]
                dst += 16
    return out


def next_pow2(n: int) -> int:
    p = 16
    while p < n:
        p *= 2
    return p


def bake_one(src: pathlib.Path, dst: pathlib.Path, key: str) -> None:
    raw = src.read_bytes()
    if src.suffix == '.rpgmvp':
        raw = decrypt_blob(raw, key)
    im = Image.open(io.BytesIO(raw)).convert('RGBA')
    w, h = im.size
    tw, th = max(16, int(w * SCALE)), max(8, int(h * SCALE))
    im = im.resize((tw, th), Image.LANCZOS)
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
    tex_w, tex_h = next_pow2(tw), next_pow2(th)
    padded = bytearray(tex_w * tex_h)
    for y in range(th):
        padded[y * tex_w:y * tex_w + tw] = idx[y * tw:(y + 1) * tw]
    swiz = swizzle8(bytes(padded), tex_w, tex_h)
    (dst.parent / (dst.name + '.t8')).write_bytes(bytes(swiz))
    (dst.parent / (dst.name + '.clut')).write_bytes(clut)
    meta = {'w': tw, 'h': th, 'tex_w': tex_w, 'tex_h': tex_h,
            'cell': int(192 * SCALE)}
    (dst.parent / (dst.name + '.meta.json')).write_text(json.dumps(meta))
    print(f'{src.stem}: {w}x{h} -> {tw}x{th} (tex {tex_w}x{tex_h})')


SHEETS = ['coin_flip', 'pinecone_pig', 'bloodsplurt', 'blood_shot',
          'bugs1', 'bugs2', 'slash1', 'needle_worm']


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='psp/gu_demo/data')
    ap.add_argument('--only', default='')
    ap.add_argument('--dir', default='img/animations')
    ap.add_argument('--prefix', default='anim_')
    args = ap.parse_args()
    game = pathlib.Path('Fear & Hunger_WIN/www')
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    names = [args.only] if args.only else SHEETS
    imgdir = pathlib.Path(getattr(args, 'dir', 'img/animations'))
    for name in names:
        src = game / imgdir / f'{name}.rpgmvp'
        if not src.exists():
            src = game / imgdir / f'{name}.png'
        if not src.exists():
            print(f'{name}: missing, skipping')
            continue
        bake_one(src, pathlib.Path(args.out) / f'{args.prefix}{name}', key)


if __name__ == '__main__':
    sys.exit(main())
