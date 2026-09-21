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
# Converted stems to pad (dims + stride read from each .meta.json).
SHEETS = [
    'mercenary_torch', 'outlander_torch', 'dark_priest_torch', 'knight_torch',
    'mercenary', 'outlander', 'dark_priest', 'knight',
    '!Flame', '!creature', '!map_objects2', '$minerghost2', 'guard1',
]

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


def next_pow2(n: int) -> int:
    p = 16
    while p < n:
        p *= 2
    return p


def pad_sheet(name: str) -> None:
    t8 = CHAR_DIR / f'{name}.t8'
    meta_p = CHAR_DIR / f'{name}.meta.json'
    meta0 = json.loads(meta_p.read_text())
    w, h = meta0['tex_w'], meta0['tex_h']
    new_w, new_h = next_pow2(w), next_pow2(h)
    if (new_w, new_h) == (w, h):
        print(f'{name}: already {w}x{h}, skipping')
        return
    raw = t8.read_bytes()
    assert len(raw) == w * h, f'{name}: expected {w*h}, got {len(raw)}'
    linear_old = deswizzle8(raw, w, h)
    linear_new = bytearray(new_w * new_h)  # zero = transparent index 0
    for y in range(h):
        linear_new[y * new_w:y * new_w + w] = linear_old[y * w:(y + 1) * w]
    swiz = swizzle8(bytes(linear_new), new_w, new_h)
    t8.write_bytes(bytes(swiz))
    meta = json.loads(meta_p.read_text())
    meta['tex_w'], meta['tex_h'], meta['t8_bytes'] = new_w, new_h, len(swiz)
    meta_p.write_text(json.dumps(meta, indent=1))
    print(f'{name}: {w}x{h} -> {new_w}x{new_h}')


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    args = ap.parse_args()
    for name in SHEETS:
        if args.only and name != args.only:
            continue
        if not (CHAR_DIR / f'{name}.t8').exists():
            print(f'{name}: missing, skipping')
            continue
        pad_sheet(name)


if __name__ == '__main__':
    sys.exit(main())
