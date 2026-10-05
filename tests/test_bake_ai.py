#!/usr/bin/env python3
"""Baker regression: enemy AI HP/MP windows must survive the int table."""
import json, os, pathlib, re, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

def main():
    with tempfile.TemporaryDirectory() as d:
        data = pathlib.Path(d) / 'Fear & Hunger_WIN' / 'www' / 'data'
        data.mkdir(parents=True)
        params = [[10 + i for i in range(100)] for _ in range(8)]
        json.dump([None, {"id": 1, "name": "C", "params": params,
                          "expParams": [30, 20, 30, 30]}], open(data / 'Classes.json', 'w'))
        json.dump([None, {"id": 1, "name": "T", "members": [], "pages": []}],
                  open(data / 'Troops.json', 'w'))
        for n in ['Actors', 'Armors', 'Items', 'Skills', 'States', 'Weapons']:
            json.dump([None], open(data / (n + '.json'), 'w'))
        acts = [
            (1, 5, 0, 0, 0), (7, 7, 2, 0, 0.5), (8, 6, 1, 2, 3),
            (9, 6, 3, 0.25, 1), (10, 6, 4, 13, 0), (11, 6, 5, 4, 0),
        ]
        json.dump([None, {"id": 1, "name": "G", "battlerName": "",
                          "params": [100, 0, 10, 5, 10, 5, 10, 10], "traits": [],
                          "actions": [{"skillId": s, "rating": r, "conditionType": t,
                                       "conditionParam1": a, "conditionParam2": b}
                                      for s, r, t, a, b in acts]}],
                  open(data / 'Enemies.json', 'w'))
        out = pathlib.Path(d) / 'db.h'
        subprocess.run([sys.executable, str(ROOT / 'tools' / 'bake_battle_db.py'),
                        '--out', str(out)], cwd=d, check=True, capture_output=True)
        text = out.read_text()
    block = text[text.index('FOE_AI[]'):]
    block = block[:block.index('};')]
    rows = [tuple(int(x) for x in m.split(','))
            for m in re.findall(r'\{(\d+,\d+,\d+,\d+,\d+,\d+)\}', block)]
    want = [(1, 1, 5, 0, 0, 0), (1, 7, 7, 2, 0, 50), (1, 8, 6, 1, 2, 3),
            (1, 9, 6, 3, 25, 100), (1, 10, 6, 4, 13, 0), (1, 11, 6, 5, 4, 0)]
    if rows != want:
        print('FAIL: FOE_AI rows', rows, 'want', want)
        return 1
    print('ALL PASS')
    return 0

if __name__ == '__main__':
    sys.exit(main())
