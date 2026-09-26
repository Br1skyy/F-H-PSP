#!/usr/bin/env python3
"""Bake menu bust portraits (img/pictures troop Actor busts) to T8.

The Galv BustMenu layout draws one bust per party member, named after
the actor's face: pictures/<FaceName>_<FaceIndex+1>. Only the base
sheets referenced by Actors.json faces are baked (L/R variants are
unused by actor data). Output lands in psp/gu_demo/data/busts/ and is
streamed at menu time like enemy art.

Usage (from the repo root):
    python3 tools/bake_busts.py "Fear & Hunger_WIN/www"
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from bake_anims import bake_one


def main() -> None:
    game = pathlib.Path(sys.argv[1])
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    out = pathlib.Path('psp/gu_demo/data/busts')
    out.mkdir(parents=True, exist_ok=True)
    actors = json.loads((game / 'data/Actors.json').read_text())
    need = set()
    for a in actors:
        if not a:
            continue
        fn = str(a.get('faceName') or '')
        if fn in ('Actor1', 'Actor2', 'Actor3'):
            need.add((fn, int(a.get('faceIndex', 0)) + 1))
    n = 0
    for fn, idx in sorted(need):
        src = game / 'img/pictures' / f'{fn}_{idx}.rpgmvp'
        if not src.exists():
            src = game / 'img/pictures' / f'{fn}_{idx}.png'
        if not src.exists():
            print(f'{fn}_{idx}: missing, skipping')
            continue
        bake_one(src, out / f'bust_{fn}_{idx}', key)
        n += 1
    print(f'baked {n} busts into {out}')


if __name__ == '__main__':
    main()
