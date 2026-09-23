#!/usr/bin/env python3.12
"""Pad converted tileset .t8 files to power-of-2 strides."""
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


def pad_sheet(cat: str, name: str) -> None:
    base = CONV / cat / name
    meta_path = pathlib.Path(str(base) + '.meta.json')
    t8_path   = pathlib.Path(str(base) + '.t8')

    meta = json.loads(meta_path.read_text())
    old_w: int = meta['tex_w']
    old_h: int = meta['tex_h']
    new_w: int = next_pow2(old_w)
    new_h: int = next_pow2(old_h)

    if new_w == old_w and new_h == old_h:
        print(f'{name}: already {old_w}x{old_h} (power-of-2), skipping')
        return

    print(f'{name}: {old_w}x{old_h} -> {new_w}x{new_h}')

    raw = t8_path.read_bytes()
    assert len(raw) == old_w * old_h, \
        f'{name}: expected {old_w*old_h} bytes, got {len(raw)}'


    linear_old = deswizzle8(raw, old_w, old_h)


    linear_new = bytearray(new_w * new_h)
    for y in range(old_h):
        src_row = y * old_w
        dst_row = y * new_w
        linear_new[dst_row:dst_row + old_w] = linear_old[src_row:src_row + old_w]


    swizzled_new = swizzle8(bytes(linear_new), new_w, new_h)


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
