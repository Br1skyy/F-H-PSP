#!/usr/bin/env python3.12
"""Pad converted tileset .t8 files to power-of-2 row strides.

The PSP GE requires texture strides to be powers of 2.  The original
convert_assets.py output uses the exact image width (192 for Mines_A1,
384 for the four 384-wide tilesets) — both are non-power-of-2, so the
GE mis-addresses every row past the first.

This tool:
  1. Deswizzles the existing .t8 (16x8 blocks at the old width).
  2. Re-pads each row to the next power of 2 (192→256, 384→512).
  3. Re-swizzles at the new stride.
  4. Overwrites the .t8 in place and updates the meta.json tex_w field.

The UV coordinates (su, sv from fh_normal_quad) are unchanged; they still
address within the original image columns, which are now at the correct
offset within the wider padded row.

Usage (run from the F&H PSP root):
    python3.12 tools/pad_sheets_pow2.py
"""
import json, pathlib, struct

SHEETS = [
    ('tilesets', 'Mines_A1'),
    ('tilesets', 'Mines_B'),
    ('tilesets', 'Mines_E'),
    ('tilesets', 'Inside_B'),
    ('tilesets', 'Mines_D'),
]
CONV = pathlib.Path('converted')


def next_pow2(n: int) -> int:
    p = 16
    while p < n:
        p *= 2
    return p


def deswizzle8(inp: bytes, w: int, h: int) -> bytearray:
    """Undo PSP 16x8 swizzle: swizzled -> linear rows."""
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
    """PSP 16x8 swizzle: linear rows -> swizzled."""
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


def pad_sheet(cat: str, name: str) -> None:
    base = CONV / cat / name
    meta_path = pathlib.Path(str(base) + '.meta.json')
    t8_path   = pathlib.Path(str(base) + '.t8')

    meta = json.loads(meta_path.read_text())
    old_w: int = meta['tex_w']
    old_h: int = meta['tex_h']
    new_w: int = next_pow2(old_w)
    new_h: int = next_pow2(old_h)   # height is usually already pow2, but be safe

    if new_w == old_w and new_h == old_h:
        print(f'{name}: already {old_w}x{old_h} (power-of-2), skipping')
        return

    print(f'{name}: {old_w}x{old_h} -> {new_w}x{new_h}')

    raw = t8_path.read_bytes()
    assert len(raw) == old_w * old_h, \
        f'{name}: expected {old_w*old_h} bytes, got {len(raw)}'

    # 1. Deswizzle at old stride.
    linear_old = deswizzle8(raw, old_w, old_h)

    # 2. Copy each row into a wider (new_w) buffer; extra columns stay 0
    #    (index 0 = transparent in our CLUT convention).
    linear_new = bytearray(new_w * new_h)  # zero-init → transparent
    for y in range(old_h):
        src_row = y * old_w
        dst_row = y * new_w
        linear_new[dst_row:dst_row + old_w] = linear_old[src_row:src_row + old_w]

    # 3. Re-swizzle at new stride.
    swizzled_new = swizzle8(bytes(linear_new), new_w, new_h)

    # 4. Write back.
    t8_path.write_bytes(bytes(swizzled_new))
    meta['tex_w'] = new_w
    meta['tex_h'] = new_h
    meta['t8_bytes'] = len(swizzled_new)
    meta_path.write_text(json.dumps(meta, indent=1))
    print(f'  -> wrote {len(swizzled_new)} bytes, meta updated')


def main() -> None:
    for cat, name in SHEETS:
        pad_sheet(cat, name)
    print('done')


if __name__ == '__main__':
    main()
