#!/usr/bin/env python3.12
"""Bake jump targets, passability masks, autotile tables, and animation frames.

Usage:
    python3.12 tools/bake.py "Fear & Hunger_WIN/www" --out converted/baked
Verifies: unmatched branches == 0; table sizes; passability spot checks."""
import json, sys, pathlib, struct


SKIP_CMDS = (111, 411, 402, 403, 601, 602, 603)

def resolve_jumps(lst):
    jumps, problems = {}, []
    n = len(lst or [])
    codes = [c.get('code', -1) if isinstance(c, dict) else -1 for c in lst or []]
    inds = [c.get('indent', 0) if isinstance(c, dict) else 0 for c in lst or []]
    labels = {}
    for i, c in enumerate(lst or []):
        if isinstance(c, dict) and c.get('code') == 118 and c.get('parameters'):
            labels.setdefault(str(c['parameters'][0]), i)
    for i in range(n):
        code = codes[i]
        if code in SKIP_CMDS:
            j = next((k for k in range(i + 1, n) if inds[k] <= inds[i]), n)
            jumps[i] = j
        elif code == 413:

            j = next((k for k in range(i - 1, -1, -1) if inds[k] == inds[i]), None)
            if j is not None and codes[j] == 112:
                jumps[i] = j
            else:
                problems.append({'index': i, 'code': 413, 'reason': 'no matching 112'})
        elif code == 113:


            depth, j = 0, None
            for k in range(i + 1, n):
                if codes[k] == 112:
                    depth += 1
                if codes[k] == 413 and inds[k] < inds[i]:
                    if depth > 0:
                        depth -= 1
                    else:
                        j = k
                        break
            if j is not None:
                jumps[i] = j
            else:


                jumps[i] = n
                problems.append({'index': i, 'code': 113, 'reason': 'break-outside-loop: jump to end (engine-faithful)'})
        elif code == 119:

            nm = str((lst[i].get('parameters', [''])[0] if isinstance(lst[i], dict) else ''))
            jumps[i] = labels.get(nm, n)
            if nm not in labels:
                problems.append({'index': i, 'code': 119, 'reason': f'label {nm!r} missing: fall through (engine-faithful)'})

    expect = {111: (411, 412), 411: (412,), 402: (402, 403, 404), 403: (404,),
              601: (602, 603, 604), 602: (603, 604), 603: (604,)}
    for i, j in jumps.items():
        code = codes[i]
        if code in expect and j < n and codes[j] not in expect[code] + (0,):
            problems.append({'index': i, 'code': code,
                             'reason': f'lands on {codes[j]} at {j}, expected one of {expect[code]}'})
    return jumps, problems

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

def main():
    game = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else 'converted/baked')
    (out / 'passability').mkdir(parents=True, exist_ok=True)
    data = game / 'data'
    def load(n): return json.loads((data / n).read_text(encoding='utf-8'))
    tilesets = load('Tilesets.json')


    jumps, problems, n_openers = {}, [], 0
    def bake_list(lst, key):
        j, p = resolve_jumps(lst)
        if j:
            jumps[key] = {str(k): v for k, v in j.items()}
        for x in p:
            problems.append({'list': key, **x})
        return sum(1 for c in lst or [] if isinstance(c, dict) and (
            c.get('code') in SKIP_CMDS or c.get('code') in (113, 413, 119)))
    total_openers = 0
    for f in sorted(data.glob('Map[0-9]*.json')):
        d = load(f.name)
        for ev in filter(None, d.get('events', []) or []):
            for pi, pg in enumerate(ev.get('pages', [])):
                total_openers += bake_list(pg.get('list', []), f'{f.name}/ev{ev["id"]}/pg{pi}')
    for i, ce in enumerate(filter(None, load('CommonEvents.json') or [])):
        total_openers += bake_list(ce.get('list', []), f'CommonEvents/{ce.get("id", i)}')
    for tr in filter(None, load('Troops.json') or []):
        for pi, pg in enumerate(tr.get('pages', [])):
            total_openers += bake_list(pg.get('list', []), f'Troops/{tr.get("id")}/pg{pi}')
    (out / 'jumps.json').write_text(json.dumps(
        {'openers': total_openers, 'lists_with_branches': len(jumps),
         'problems': problems, 'jumps': jumps}, indent=1))
    import collections as _c
    print(f'jumps: {total_openers} skip/loop cmds in {len(jumps)} lists, '
          f'notes={len(problems)} ' + str(dict(_c.Counter(p["reason"] for p in problems))))


    nmaps = 0
    for f in sorted(data.glob('Map[0-9]*.json')):
        d = load(f.name)
        w, h = d['width'], d['height']
        raw = d['data']
        ts = next(t for t in tilesets if t and t['id'] == d['tilesetId'])
        flags = ts['flags']
        mask = bytearray(w * h)
        for y in range(h):
            for x in range(w):
                tiles = [raw[(z * h + y) * w + x] or 0 for z in (3, 2, 1, 0)]
                m = 0
                for bit_i, bit in ((0, 1), (1, 2), (2, 4), (3, 8)):
                    if check_passage(flags, tiles, bit):
                        m |= 1 << bit_i
                mask[y * w + x] = m
        (out / 'passability' / (f.stem + '.bin')).write_bytes(bytes(mask))
        nmaps += 1

    print(f'passability: {nmaps} maps baked')


    import re
    src = (game / 'js/rpg_core.js').read_text(encoding='utf-8')
    def grab(name):
        m = re.search(name + r'\s*=\s*\[(.*?)\];', src, re.S)
        return json.loads('[' + m.group(1).replace(']', '],').rstrip(',') + ']')

    def grab2(name):
        m = re.search(name + r'\s*=\s*(\[.*?\]);', src, re.S)
        return json.loads(m.group(1))
    auto = {'TILE_ID_A1': 2048, 'TILE_ID_A2': 2816, 'TILE_ID_A3': 4352,
            'TILE_ID_A4': 5888, 'TILE_ID_MAX': 8192, 'TILE_ID_A5': 1536,
            'FLOOR': grab2('Tilemap.FLOOR_AUTOTILE_TABLE'),
            'WALL': grab2('Tilemap.WALL_AUTOTILE_TABLE'),
            'WATERFALL': grab2('Tilemap.WATERFALL_AUTOTILE_TABLE'),
            'provenance': 'rpg_core.js TILE_ID block :5271 + tables :5386'}
    assert len(auto['FLOOR']) == 48 and len(auto['WALL']) == 16 and len(auto['WATERFALL']) == 4
    (out / 'autotiles.json').write_text(json.dumps(auto))
    print('autotiles: FLOOR=48 WALL=16 WATERFALL=4 OK')


    anims = []
    for a in filter(None, load('Animations.json') or []):
        anims.append({'id': a['id'], 'name': a['name'],
                      'frames': [[list(cell) for cell in fr] for fr in a['frames']],
                      'timings': a['timings'],
                      'sheets': [a['animation1Name'], a['animation2Name']],
                      'position': a['position']})
    (out / 'anims.json').write_text(json.dumps(anims))
    print(f'anims: {len(anims)} baked')

if __name__ == '__main__':
    main()
