#!/usr/bin/env python3
"""Bake actor face sheets used by the menu status window.

Usage (from the repo root):
    python3 tools/bake_faces.py "Fear & Hunger_WIN/www"
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from bake_anims import bake_one


def main() -> None:
    game = pathlib.Path(sys.argv[1])
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    out = pathlib.Path('psp/gu_demo/data')
    names = set()
    actors = json.loads((game / 'data/Actors.json').read_text())
    for a in actors:
        if a and a.get('faceName'):
            names.add(a['faceName'])
    for name in sorted(names):
        src = game / 'img/faces' / f'{name}.rpgmvp'
        if not src.exists():
            src = game / 'img/faces' / f'{name}.png'
        if not src.exists():
            print(f'{name}: missing, skipping')
            continue
        bake_one(src, out / f'face_{name}', key)


if __name__ == '__main__':
    main()
