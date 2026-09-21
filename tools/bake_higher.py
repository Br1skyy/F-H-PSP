#!/usr/bin/env python3
"""Bake the higher-tile (star) mask for a map's tileset.

The original engine draws tiles with flags[tid] & 0x10 set on the UPPER
bitmap (z=4, above same-priority characters); everything else goes to the
LOWER bitmap (z=0, below). See rpg_core.js Tilemap._isHigherTile.

Usage (from repo root):
    python3 tools/bake_higher.py --map Map030 --out psp/gu_demo/data/map030/higher.bin

Output: one byte per tile id (1 = higher), length = len(tileset flags).
Requires an owned copy of the game (reads Fear & Hunger_WIN/www/data).
"""
import argparse
import json
import pathlib
import sys


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--map', default='Map030')
    ap.add_argument('--game', default='Fear & Hunger_WIN/www/data')
    ap.add_argument('--out',
                    default='psp/gu_demo/data/map030/higher.bin')
    args = ap.parse_args()

    game = pathlib.Path(args.game)
    m = json.loads((game / f'{args.map}.json').read_text(encoding='utf-8'))
    ts = json.loads((game / 'Tilesets.json').read_text(encoding='utf-8'))
    t = next(t for t in ts if t and t['id'] == m['tilesetId'])
    flags = t['flags']
    mask = bytes(1 if (f & 0x10) else 0 for f in flags)

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(mask)
    print(f'{t["name"]}: {len(mask)} tids, {sum(mask)} higher -> {out}')


if __name__ == '__main__':
    sys.exit(main())
