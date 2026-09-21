#!/usr/bin/env python3
"""Bake side-view battler sheets for the PSP battle scene.

Source: Fear & Hunger/www/img/sv_actors/*.rpgmvp (decrypted with the
System key). SV layout is 9 cols x 6 rows; cells are ~115px at half
scale, so sheets are re-scaled to 56px cells (504x336 art) then padded
to 512x512 and swizzled. Palette: 255 colors + transparent index 0
(same convention as convert_assets.py).

Motion cells (rpg_sprites.js Sprite_Actor.updateFrame):
  col = floor(motionIndex/6)*3 + pattern, row = motionIndex%6
  wait=1 idle:(1,1)  guard=3:(4,3)  thrust=6:(4,0)  swing=7:(4,1)
  missile=8:(4,2), middle pattern shown for static poses.

Usage (from repo root):
    python3 tools/bake_battlers.py --out psp/gu_demo/data
"""
import argparse
import io
import json
import pathlib
import struct
import sys
from PIL import Image

CELL = 64
COLS, ROWS = 9, 6


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
    target = (COLS * CELL, ROWS * CELL)
    im = im.resize(target, Image.LANCZOS)
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
    tex_w, tex_h = next_pow2(target[0]), next_pow2(target[1])
    padded = bytearray(tex_w * tex_h)
    for y in range(target[1]):
        padded[y * tex_w:y * tex_w + target[0]] = idx[y * target[0]:(y + 1) * target[0]]
    swiz = swizzle8(bytes(padded), tex_w, tex_h)
    (dst.parent / (dst.name + '.t8')).write_bytes(bytes(swiz))
    (dst.parent / (dst.name + '.clut')).write_bytes(clut)
    print(f'{src.stem}: {im.size} -> {tex_w}x{tex_h}')


BATTLERS = ['Actor1_1', 'knight1_1', 'darkpriest1_1', 'outlander1_1']


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='psp/gu_demo/data')
    ap.add_argument('--only', default='')
    args = ap.parse_args()
    game = pathlib.Path('Fear & Hunger_WIN/www')
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    names = [args.only] if args.only else BATTLERS
    for name in names:
        src = game / 'img/sv_actors' / f'{name}.rpgmvp'
        if not src.exists():
            src = game / 'img/sv_actors' / f'{name}.png'
        if not src.exists():
            print(f'{name}: missing, skipping')
            continue
        bake_one(src, pathlib.Path(args.out) / f'bv_{name}', key)


if __name__ == '__main__':
    sys.exit(main())
