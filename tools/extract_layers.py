#!/usr/bin/env python3.12
"""Extract the usable map layers as u16 LE binary.

Usage:
    python3.12 tools/extract_layers.py "Fear & Hunger_WIN/www" --out psp/gu_demo/data/map030"""
import json, sys, pathlib, struct

def main():
    args = sys.argv[1:]
    game = pathlib.Path(args[0])
    out = pathlib.Path(args[args.index('--out') + 1] if '--out' in args else 'psp/gu_demo/data/map030')
    out.mkdir(parents=True, exist_ok=True)


    data = json.loads((game / 'data/Map030.json').read_text(encoding='utf-8'))
    w, h = data['width'], data['height']
    raw = data['data']

    print(f'Map030: {w}x{h}, total data items: {len(raw)}')
    print(f'Layers in data: {len(raw) // (w * h)}')


    layers = bytearray()
    for z in range(4):
        for y in range(h):
            for x in range(w):
                idx = (z * h + y) * w + x
                if idx < len(raw):
                    tid = raw[idx] or 0
                    layers += struct.pack('<H', tid)


    (out / 'layers.bin').write_bytes(bytes(layers))
    print(f'Wrote {len(layers)} bytes ({len(layers)//2} tiles) to {out / "layers.bin"}')
    print(f'Expected: {w * h * 4 * 2} bytes for 4 layers')


    ts = next(t for t in json.loads((game / 'data/Tilesets.json').read_text()) if t and t['id'] == data['tilesetId'])
    flags = ts['flags']

    def check_passage(flags, tiles, bit):
        for t in tiles:
            flag = flags[t] if t < len(flags) else 0
            if flag & 0x10:
                continue
            if (flag & bit) == 0:
                return True
            if (flag & bit) == bit:
                return False
        return False

    mask = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            tiles = [raw[(z * h + y) * w + x] or 0 for z in (3, 2, 1, 0)]
            m = 0
            for bit_i, bit in ((0, 1), (1, 2), (2, 4), (3, 8)):
                if check_passage(flags, tiles, bit):
                    m |= 1 << bit_i
            mask[y * w + x] = m

    (out / 'Map030.bin').write_bytes(bytes(mask))
    print(f'Wrote passability: {len(mask)} bytes to {out / "Map030.bin"}')

if __name__ == '__main__':
    main()