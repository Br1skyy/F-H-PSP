#!/usr/bin/env python3
"""Bake the game's own message font into a PSP-ready atlas.

Source: Fear & Hunger/www/fonts/mplus-1m-regular.ttf (OFL, ships with the
game — bake from your owned copy). Output: 256x256 T8 (Latin-1 codepoints
mapped 1:1 to 16x16 cells of 16x16px) + white-ramp CLUT, swizzled.
Glyph coverage becomes the palette index (0 = transparent).

Usage (from repo root):
    python3 tools/bake_font.py --out psp/gu_demo/data/font
"""
import argparse
import pathlib
import struct
import sys
from PIL import Image, ImageFont, ImageDraw


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


CELL, COLS, SIZE = 16, 16, 13


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--font',
                    default='Fear & Hunger_WIN/www/fonts/mplus-1m-regular.ttf')
    ap.add_argument('--out', default='psp/gu_demo/data/font')
    args = ap.parse_args()

    font = ImageFont.truetype(args.font, SIZE)
    # mplus-1m is monospace: one advance for all glyphs.
    adv = int(round(font.getlength('M')))
    print(f'advance={adv}px at {SIZE}px')

    linear = bytearray(256 * 256)
    for cp in range(32, 256):
        img = Image.new('L', (CELL, CELL), 0)
        d = ImageDraw.Draw(img)
        d.text((1, 0), chr(cp), font=font, fill=255)
        px = img.load()
        cx, cy = (cp % COLS) * CELL, (cp // COLS) * CELL
        for y in range(CELL):
            for x in range(CELL):
                linear[(cy + y) * 256 + cx + x] = px[x, y]

    swiz = swizzle8(bytes(linear), 256, 256)
    out = pathlib.Path(args.out)
    (out.parent / (out.name + '.t8')).write_bytes(bytes(swiz))
    # CLUT entry i = white with alpha i (glyph coverage drives blending).
    clut = b''.join(struct.pack('<I', (i << 24) | 0x00FFFFFF)
                    for i in range(256))
    (out.parent / (out.name + '.clut')).write_bytes(clut)
    print(f'wrote {out}.t8 ({len(swiz)}B) + {out.name}.clut')


if __name__ == '__main__':
    sys.exit(main())
