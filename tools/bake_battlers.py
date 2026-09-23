#!/usr/bin/env python3
"""Bake side-view battler sheets for the PSP battle scene.

Source: Fear & Hunger/www/img/sv_actors/*.rpgmvp (decrypted with the
System key). SV layout is 9 cols x 6 rows (source cells ~230px).

Only the motions the battle ever plays are baked, at 112px cells
(1:1 on screen — the old 56px cells upscaled 2x looked blocky):
rows 1-4, cols 0-2 ("a" sheet) and cols 3-5 ("b" sheet). Each sheet is
336x448 art padded to 512x512 (GE texture limit), palette 255 colors +
transparent index 0 (same convention as convert_assets.py).

Motion cells (rpg_sprites.js Sprite_Actor.updateFrame + main.c
btl_motion): wait (0,1), guard (0,3), swing (3,1), missile (3,2),
skill (3,3), damage (0,4); pattern adds 0-2 to the column. Render
picks sheet A/B by column and rows are stored minus 1 (see render.c).
If a new motion outside rows 1-4 / cols 0-5 is ever queued, extend
WANT_ROWS/WANT_COLS here and in render_battle.

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

CELL = 112
COLS, ROWS = 9, 6

WANT_ROWS = (1, 2, 3, 4)
WANT_COLS_A = (0, 1, 2)
WANT_COLS_B = (3, 4, 5)


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


def palettise(im: Image.Image) -> tuple:
    """RGBA -> (indices with 0 = transparent, clut). Shared by sheets."""
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
    return idx, clut


def bake_one(src: pathlib.Path, dst: pathlib.Path, key: str) -> None:
    raw = src.read_bytes()
    if src.suffix == '.rpgmvp':
        raw = decrypt_blob(raw, key)
    im = Image.open(io.BytesIO(raw)).convert('RGBA')
    sw, sh = im.size
    cw, ch = sw // COLS, sh // ROWS
    for suffix, cols in (('a', WANT_COLS_A), ('b', WANT_COLS_B)):
        sheet = Image.new('RGBA', (len(cols) * CELL, len(WANT_ROWS) * CELL))
        for dx, c in enumerate(cols):
            for dy, r in enumerate(WANT_ROWS):
                part = im.crop((c * cw, r * ch, (c + 1) * cw,
                                (r + 1) * ch))
                part = part.resize((CELL, CELL), Image.LANCZOS)
                sheet.paste(part, (dx * CELL, dy * CELL))
        idx, clut = palettise(sheet)
        tw, th = sheet.size
        tex_w, tex_h = next_pow2(tw), next_pow2(th)
        padded = bytearray(tex_w * tex_h)
        for y in range(th):
            padded[y * tex_w:y * tex_w + tw] = idx[y * tw:(y + 1) * tw]
        swiz = swizzle8(bytes(padded), tex_w, tex_h)
        (dst.parent / (dst.name + suffix + '.t8')).write_bytes(bytes(swiz))
        (dst.parent / (dst.name + suffix + '.clut')).write_bytes(clut)
        print(f'{src.stem}{suffix}: src cell {cw}x{ch} '
              f'-> {len(cols) * CELL}x{len(WANT_ROWS) * CELL} '
              f'(stride {tex_w}x{tex_h})')


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
