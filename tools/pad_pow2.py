#!/usr/bin/env python3
"""Pad converted textures to power-of-2 strides, in place.

The PSP GE only accepts pow2 textures. The converter aligns to 16x8
blocks, which is not enough (enemies come out 112x160 while the game
needs 128x256). Run after convert_assets.py, before baking anything
that reads texture dims. Idempotent: already-pow2 files are untouched.
Covers tilesets, characters, and enemies (the staged categories).

Usage (from repo root):
    python3 tools/pad_pow2.py
"""
import json
import pathlib


def deswizzle8(data, w, h):
    out = bytearray(w * h)
    dst = 0
    for by in range(0, h, 8):
        for bx in range(0, w, 16):
            for row in range(8):
                src = (by + row) * w + bx
                out[src:src + 16] = data[dst:dst + 16]
                dst += 16
    return bytes(out)


def swizzle8(inp, w, h):
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


def next_pow2(n):
    p = 16
    while p < n:
        p *= 2
    return p


CATS = ('tilesets', 'characters', 'enemies')


def main() -> None:
    conv = pathlib.Path('converted')
    n_pad = n_same = 0
    for cat in CATS:
        d = conv / cat
        if not d.exists():
            continue
        for t8 in sorted(d.glob('*.t8')):
            meta_path = t8.parent / (t8.stem + '.meta.json')
            meta = json.loads(meta_path.read_text(encoding='utf-8'))
            tw, th = meta['tex_w'], meta['tex_h']
            nw, nh = next_pow2(tw), next_pow2(th)
            raw = t8.read_bytes()
            assert len(raw) == tw * th, f'{t8}: {len(raw)} != {tw}x{th}'
            if nw == tw and nh == th:
                n_same += 1
                continue
            lin = deswizzle8(raw, tw, th)
            art = bytearray(nw * nh)
            for y in range(th):
                art[y * nw:y * nw + tw] = lin[y * tw:y * tw + tw]
            t8.write_bytes(bytes(swizzle8(bytes(art), nw, nh)))
            meta['tex_w'] = nw
            meta['tex_h'] = nh
            meta['t8_bytes'] = nw * nh
            meta_path.write_text(json.dumps(meta, indent=1))
            n_pad += 1
            print(f'padded {cat}/{t8.stem}: {tw}x{th} -> {nw}x{nh}')
    print(f'done: {n_pad} padded, {n_same} already pow2')


if __name__ == '__main__':
    main()
