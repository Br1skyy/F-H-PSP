#!/usr/bin/env python3
"""Bake the game's own message font into a PSP-ready atlas.

Source: Fear & Hunger/www/fonts/Eczar-Regular.ttf (ships with the game,
selected by fonts/gamefont.css as GameFont — NOT mplus). Proportional
font: advances baked alongside (half-px units) into font_adv.bin.
Output: 512x512 T8 (Latin-1 codepoints 1:1 into 32x16 cells of 16x32px)
+ white-ramp CLUT, swizzled. Glyph coverage = palette index.

Usage (from repo root):
    python3 tools/bake_font.py --out psp/gu_demo/data/font
"""
import argparse
import pathlib
import struct
import sys
from PIL import Image, ImageFont, ImageDraw

COLS, CW, CH, SIZE, YOFF = 32, 16, 32, 18, 0


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


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--font',
                    default='Fear & Hunger_WIN/www/fonts/Eczar-Regular.ttf')
    ap.add_argument('--out', default='psp/gu_demo/data/font')
    args = ap.parse_args()

    font = ImageFont.truetype(args.font, SIZE)
    W, H = COLS * CW, 16 * CH
    linear = bytearray(W * H)
    adv = bytearray(256)
    for cp in range(256):
        if cp < 32:
            adv[cp] = 0
            continue
        img = Image.new('L', (CW, CH), 0)
        ImageDraw.Draw(img).text((1, YOFF), chr(cp), font=font, fill=255)
        px = img.load()
        cx, cy = (cp % COLS) * CW, (cp // COLS) * CH
        for y in range(CH):
            for x in range(CW):
                linear[(cy + y) * W + cx + x] = px[x, y]
        adv[cp] = max(1, min(255, int(round(font.getlength(chr(cp)) * 2))))
    print(f'max advance: {max(adv)/2}px')

    swiz = swizzle8(bytes(linear), W, H)
    out = pathlib.Path(args.out)
    (out.parent / (out.name + '.t8')).write_bytes(bytes(swiz))
    clut = b''.join(struct.pack('<I', (i << 24) | 0x00FFFFFF)
                    for i in range(256))
    (out.parent / (out.name + '.clut')).write_bytes(clut)
    (out.parent / (out.name + '_adv.bin')).write_bytes(bytes(adv))
    print(f'wrote {out}.t8 ({len(swiz)}B) + clut + adv')


if __name__ == '__main__':
    sys.exit(main())
