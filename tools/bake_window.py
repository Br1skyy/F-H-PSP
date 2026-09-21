#!/usr/bin/env python3
"""Bake the message-window skin for the PSP dialog box.

Source: Fear & Hunger/www/img/system/Window.png (192x192, plain PNG).
Output: 256-stride T8 + CLUT, swizzled (frame parts live at skin offset
96,96 with 24px margins per rpg_core.js Window._refreshFrame; padding is
transparent index 0). Alpha is preserved (the skin is translucent).

Usage (from repo root):
    python3 tools/bake_window.py --out psp/gu_demo/data/window
"""
import argparse
import pathlib
import struct
import sys
from PIL import Image


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
    ap.add_argument('--src',
                    default='Fear & Hunger_WIN/www/img/system/Window.png')
    ap.add_argument('--out', default='psp/gu_demo/data/window')
    args = ap.parse_args()

    im = Image.open(args.src).convert('RGBA')
    w, h = im.size
    assert (w, h) == (192, 192), f'unexpected skin size {w}x{h}'
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

    stride = 256
    tex_h = 256  # pad 192 -> pow2 height for TexImage
    padded = bytearray(stride * tex_h)
    for y in range(h):
        padded[y * stride:y * stride + w] = idx[y * w:(y + 1) * w]
    swiz = swizzle8(bytes(padded), stride, tex_h)

    out = pathlib.Path(args.out)
    (out.parent / (out.name + '.t8')).write_bytes(bytes(swiz))
    (out.parent / (out.name + '.clut')).write_bytes(clut)
    print(f'wrote {out}.t8 ({len(swiz)}B) + {out.name}.clut')


if __name__ == '__main__':
    sys.exit(main())
