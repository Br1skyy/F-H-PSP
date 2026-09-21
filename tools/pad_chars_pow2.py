#!/usr/bin/env python3
"""Pad converted character sheets to power-of-2 for the PSP GE.

convert_assets.py outputs SWIZZLED 480x440 textures. The PSP GE needs
power-of-2 strides, so this deswizzles, pads rows to 512 (and height to
512), then re-swizzles at 512x512. (Padding swizzled bytes directly, as
the old pad_char_*.py scripts did, scrambles every row — do not do that.)

Usage (from repo root):
    python3 tools/pad_chars_pow2.py --only mercenary_torch
    python3 tools/pad_chars_pow2.py   # all *_torch + plain sheets used by demo
"""
import argparse
import json
import pathlib
import sys

CHAR_DIR = pathlib.Path('converted/characters')
# (converted stem, half-scale w, half-scale h)
SHEETS = [
    ('mercenary_torch', 480, 440),
    ('outlander_torch', 480, 440),
    ('dark_priest_torch', 480, 440),
    ('knight_torch', 480, 440),
    ('mercenary', 480, 440),
    ('outlander', 480, 440),
    ('dark_priest', 480, 440),
    ('knight', 480, 440),
]

NEW_W, NEW_H = 512, 512


def deswizzle8(inp: bytes, w: int, h: int) -> bytearray:
    out = bytearray(w * h)
    src = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                dst = (by + row) * w + bx
                out[dst:dst + 16] = inp[src:src + 16]
                src += 16
    return out


def swizzle8(inp: bytes, w: int, h: int) -> bytearray:
    assert w % 16 == 0 and h % 8 == 0, f'dim {w}x{h} not 16x8 aligned'
    out = bytearray(w * h)
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[dst:dst + 16] = inp[src:src + 16]
                dst += 16
    return out


def pad_sheet(name: str, w: int, h: int) -> None:
    t8 = CHAR_DIR / f'{name}.t8'
    meta_p = CHAR_DIR / f'{name}.meta.json'
    raw = t8.read_bytes()
    assert len(raw) == w * h, f'{name}: expected {w*h}, got {len(raw)}'
    linear_old = deswizzle8(raw, w, h)
    linear_new = bytearray(NEW_W * NEW_H)  # zero = transparent index 0
    for y in range(h):
        linear_new[y * NEW_W:y * NEW_W + w] = linear_old[y * w:(y + 1) * w]
    swiz = swizzle8(bytes(linear_new), NEW_W, NEW_H)
    t8.write_bytes(bytes(swiz))
    meta = json.loads(meta_p.read_text())
    meta['tex_w'], meta['tex_h'], meta['t8_bytes'] = NEW_W, NEW_H, len(swiz)
    meta_p.write_text(json.dumps(meta, indent=1))
    print(f'{name}: {w}x{h} -> {NEW_W}x{NEW_H}')


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    args = ap.parse_args()
    for name, w, h in SHEETS:
        if args.only and name != args.only:
            continue
        if not (CHAR_DIR / f'{name}.t8').exists():
            print(f'{name}: missing, skipping')
            continue
        pad_sheet(name, w, h)


if __name__ == '__main__':
    sys.exit(main())
