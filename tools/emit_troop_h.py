#!/usr/bin/env python3
"""Bake troop pages and referenced common events to a C header."""
import json
import sys
import pathlib


def cstr(s):
    out = ['"']
    for b in s.encode('utf-8'):
        ch = chr(b)
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ch == '\n':
            out.append('\\n')
        elif 32 <= b < 127:
            out.append(ch)
        else:
            out.append(f'\\x{b:02x}')
    out.append('"')
    return ''.join(out)


def num(p, k, default=0):
    v = p[k] if len(p) > k and isinstance(p[k], (int, float)) else default
    return int(v)


def se_name(p0):
    if isinstance(p0, dict):
        return str(p0.get('name', ''))
    return str(p0)


PLAIN = (0, 105, 108, 112, 113, 115, 118, 404, 411, 412, 413, 604, 505,
         605, 403, 404, 123, 235, 334, 335, 340)


def encode_cmd(c, i, jump, sym):
    code = c.get('code', -1)
    p = c.get('parameters', [])
    indent = c.get('indent', 0)
    op, pp, raw = 0, [0] * 10, None

    if code in PLAIN:
        if code == 123 and len(p) >= 2:
            pp[0] = 'ABCD'.index(p[0]) if p[0] in 'ABCD' else 0
            pp[1] = num(p, 1)
        elif code == 235 and p:
            pp[0] = num(p, 0)
        elif code in (334, 335) and p:
            pp[0] = num(p, 0)
    elif code == 119:
        pass
    elif code == 401 and p:
        raw = str(p[0])
    elif code == 405 and p:
        raw = str(p[0])
    elif code == 101 and p:
        pp = [num(p, 1), num(p, 2), num(p, 3)] + [0] * 7
        raw = str(p[0])
    elif code == 102 and p:
        raw = '\n'.join(str(x) for x in p[0])
    elif code == 402 and p:
        pp[0] = num(p, 0)
    elif code == 111 and p:
        op = num(p, 0)
        assert op in (0, 1, 4, 5, 8, 9, 10, 11, 13), \
            f'{sym}[{i}]: 111 type {op}'
        if op == 0:
            pp = [num(p, 1), num(p, 2)] + [0] * 8
        elif op == 1:
            pp = [num(p, 1), num(p, 2), num(p, 3), num(p, 4)] + [0] * 6
        elif op == 4:
            pp = [num(p, 1), num(p, 2), num(p, 3)] + [0] * 7
            if num(p, 2) == 1 and len(p) > 3:
                raw = str(p[3])
        elif op == 5:
            pp = [num(p, 1), num(p, 2), num(p, 3)] + [0] * 7
        elif op == 8:
            pp = [num(p, 1)] + [0] * 9
        elif op == 9 or op == 10:
            pp = [num(p, 1), 1 if len(p) > 2 and p[2] else 0] + [0] * 8
        elif op == 13:
            pp = [num(p, 1)] + [0] * 9
    elif code == 117 and p:
        pp[0] = num(p, 0)
    elif code == 121 and len(p) >= 3:
        pp = [num(p, 0), num(p, 1), num(p, 2)] + [0] * 7
    elif code == 122 and len(p) >= 5:
        op = num(p, 2)
        v = p[4]
        pp = [num(p, 0), num(p, 1), num(p, 3),
              int(v) if isinstance(v, (int, float)) else 0,
              num(p, 5) if len(p) > 5 else 0] + [0] * 5
    elif code in (126, 127) and len(p) >= 4:
        op = num(p, 1)
        v = p[3]
        pp = [num(p, 0), 0, num(p, 2),
              int(v) if isinstance(v, (int, float)) else 0] + [0] * 6
    elif code == 128 and len(p) >= 4:
        op = num(p, 1)
        v = p[3]
        pp = [num(p, 0), 0, num(p, 2),
              int(v) if isinstance(v, (int, float)) else 0] + [0] * 6
    elif code == 129 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1),
              num(p, 2) if len(p) > 2 else 0] + [0] * 7
    elif code == 132 and p:
        raw = se_name(p[0])
    elif code == 135 and p:
        pp[0] = num(p, 0)
    elif code == 201 and len(p) >= 6:
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3), num(p, 4),
              num(p, 5)] + [0] * 4
    elif code == 205 and len(p) >= 2:
        route = p[1] if isinstance(p[1], dict) else {}
        pp = [num(p, 0), 0, 1 if route.get('wait') else 0] + [0] * 7
    elif code == 211 and p:
        pp[0] = num(p, 0)
    elif code == 212 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1),
              1 if len(p) > 2 and p[2] else 0, 0] + [0] * 6
    elif code == 216 and p:
        pp[0] = num(p, 0)
    elif code == 217:
        pass
    elif code == 225 and len(p) >= 3:
        pp = [num(p, 0), num(p, 1), num(p, 2)] + [0] * 7
        op = 1 if len(p) > 3 and p[3] else 0
    elif code == 230 and p:
        pp[0] = num(p, 0)
    elif code == 231 and len(p) >= 10:
        op = 0
        pp = [num(p, 0), num(p, 2), num(p, 3),
              num(p, 4), num(p, 5),
              num(p, 6), num(p, 7), num(p, 8), num(p, 9), 0]
        raw = str(p[1])
    elif code == 232 and len(p) >= 12:
        op = 1 if p[11] else 0
        pp = [num(p, 0), num(p, 2), num(p, 3),
              num(p, 4), num(p, 5),
              num(p, 6), num(p, 7), num(p, 8), num(p, 9),
              num(p, 10)]
    elif code == 233 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1)] + [0] * 8
    elif code == 234 and len(p) >= 3:
        tone = p[1] if isinstance(p[1], list) else [0, 0, 0, 0]
        pp = [num(p, 0),
              int(tone[0]) if len(tone) > 0 else 0,
              int(tone[1]) if len(tone) > 1 else 0,
              int(tone[2]) if len(tone) > 2 else 0,
              int(tone[3]) if len(tone) > 3 else 0,
              num(p, 2)] + [0] * 4
        op = 1 if num(p, 3) else 0
    elif code == 236 and len(p) >= 3:
        tmap = {'none': 0, 'rain': 1, 'storm': 2, 'snow': 3}
        pp = [tmap.get(str(p[0]), 0), num(p, 1), num(p, 2)] + [0] * 7
        op = 1 if num(p, 3) else 0
    elif code == 250 and p:
        raw = se_name(p[0])
    elif code == 251:
        pass
    elif code == 223 and len(p) >= 2:
        tone = p[0] if isinstance(p[0], list) else [0, 0, 0, 0]
        pp = [int(tone[0]) if len(tone) > 0 else 0,
              int(tone[1]) if len(tone) > 1 else 0,
              int(tone[2]) if len(tone) > 2 else 0,
              int(tone[3]) if len(tone) > 3 else 0,
              num(p, 1)] + [0] * 5
        op = 1 if num(p, 2) else 0
    elif code == 282 and p:
        pp[0] = num(p, 0)
    elif code == 301 and len(p) >= 4:
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3)] + [0] * 6
    elif code == 303 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1)] + [0] * 8
    elif code == 311 and len(p) >= 6:
        v = p[4]
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3),
              int(v) if isinstance(v, (int, float)) else 0,
              num(p, 5)] + [0] * 4
    elif code == 312 and len(p) >= 5:
        v = p[4]
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3),
              int(v) if isinstance(v, (int, float)) else 0, 0] + [0] * 4
    elif code == 313 and len(p) >= 4:
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3)] + [0] * 6
    elif code == 314 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1)] + [0] * 8
    elif code in (315, 316) and len(p) >= 6:
        v = p[4]
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3),
              int(v) if isinstance(v, (int, float)) else 0,
              num(p, 5)] + [0] * 4
    elif code == 317 and len(p) >= 6:
        v = p[4]
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3),
              int(v) if isinstance(v, (int, float)) else 0,
              num(p, 5)] + [0] * 4
    elif code == 318 and len(p) >= 4:
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3)] + [0] * 6
    elif code == 319 and len(p) >= 3:
        pp = [num(p, 0), num(p, 1), num(p, 2)] + [0] * 7
    elif code == 320 and p:
        pp = [num(p, 0)] + [0] * 9
        raw = str(p[1]) if len(p) > 1 else ''
    elif code == 322 and len(p) >= 6:
        pp = [num(p, 0)] + [0] * 9
        raw = '\n'.join([str(p[1]), str(num(p, 2)), str(p[3]),
                         str(num(p, 4)), str(p[5])])
    elif code == 324 and p:
        pp = [num(p, 0)] + [0] * 9
        raw = str(p[1]) if len(p) > 1 else ''
    elif code in (331, 332, 342) and len(p) >= 5:
        v = p[3]
        pp = [num(p, 0), num(p, 1), num(p, 2),
              int(v) if isinstance(v, (int, float)) else 0,
              num(p, 4)] + [0] * 5
    elif code == 333 and len(p) >= 3:
        pp = [num(p, 0), num(p, 1), num(p, 2)] + [0] * 7
    elif code == 336 and len(p) >= 2:
        pp = [num(p, 0), num(p, 1)] + [0] * 8
    elif code == 337 and len(p) >= 3:
        pp = [num(p, 0), num(p, 1), 1 if p[2] else 0] + [0] * 7
    elif code == 339 and len(p) >= 4:
        pp = [num(p, 0), num(p, 1), num(p, 2), num(p, 3)] + [0] * 6
    elif code in (351, 352, 353, 354):
        pass
    elif code == 355 and p:
        raw = str(p[0])
    elif code == 356 and p:
        raw = str(p[0])
    elif code in (601, 602, 603):
        pass
    else:
        raise AssertionError(f'{sym}[{i}]: code {code} not encodable')
    return code, indent, jump, op, pp, raw


def emit_list(lst, jm, sym):
    n = len(lst)
    L = [f'static const FhCmd {sym}[] = {{']
    for i, c in enumerate(lst):
        jump = int(jm.get(str(i), n))
        code, indent, _, op, pp, raw = encode_cmd(c, i, jump, sym)
        s = cstr(raw) if raw is not None else 'NULL'
        L.append(f'  {{{code},{indent},{jump},{op},'
                 f'{{{",".join(map(str, pp))}}},{s}}},')
    L += ['};', f'static const int {sym}_LEN = {n};']
    return L, n


def main():
    game = pathlib.Path(sys.argv[1])
    troop_id = int(sys.argv[sys.argv.index('--troop') + 1]) if '--troop' in sys.argv else 0
    out = pathlib.Path(sys.argv[sys.argv.index('--out') + 1])
    baked = json.loads(
        pathlib.Path('converted/baked/jumps.json').read_text())['jumps']
    data = game / 'data'
    ces = json.loads((data / 'CommonEvents.json').read_text(encoding='utf-8'))
    if '--ces-only' in sys.argv:

        ids = sorted({int(x) for x in sys.argv[sys.argv.index('--ces-only') + 1:] if x.isdigit()})
        L = ['/* AUTO-GENERATED by tools/emit_troop_h.py --ces-only.',
         ' * Do not hand-edit. Encoding matches runtime/interp.c. */']
        syms = []
        for cid in ids:
            e = next(x for x in ces if x and x['id'] == cid)
            sym = f'CEB_{cid}'
            jm = baked.get(f'CommonEvents/{cid}', {})
            lines, _ = emit_list(e['list'], jm, sym)
            L += lines
            syms.append((cid, sym))
        L.append('static const int CEB_IDS[] = '
                 f'{{{",".join(str(c) for c, _ in syms)}}};')
        L.append('static const FhCmd *CEB_LIST[] = '
                 f'{{{",".join(s for _, s in syms)}}};')
        L.append('static const int CEB_LEN[] = '
                 f'{{{",".join(s + "_LEN" for _, s in syms)}}};')
        L.append(f'static const int CEB_N = {len(syms)};')
        out.write_text('\n'.join(L) + '\n')
        print(f'wrote {out} (CEs {[c for c, _ in syms]})')
        return

    troops = json.loads((data / 'Troops.json').read_text(encoding='utf-8'))
    troop = next(t for t in troops if t and t['id'] == troop_id)


    ce_ids = sorted({c['parameters'][0] for p in troop['pages']
                     for c in p['list'] if c.get('code') == 117
                     and c.get('parameters')})
    ce_map = {x['id']: x for x in ces if x}
    prev = None
    while prev != ce_ids:
        prev = list(ce_ids)
        for cid in prev:
            e = ce_map.get(cid)
            if not e:
                continue
            for c in e['list']:
                if c.get('code') == 117 and c.get('parameters'):
                    nid = c['parameters'][0]
                    if nid not in ce_ids:
                        ce_ids.append(nid)
        ce_ids = sorted(ce_ids)

    L = ['/* AUTO-GENERATED by tools/emit_troop_h.py. Do not hand-edit.',
         f' * Troop {troop_id} ({troop["name"]}): page conditions carry',
         ' * BtPageCond fields in order; lists encode runtime/interp.c. */']
    conds = []
    for i, page in enumerate(troop['pages']):
        c = page['conditions']
        conds.append((page['span'],
                      1 if c['turnEnding'] else 0, 1 if c['turnValid'] else 0,
                      c['turnA'], c['turnB'],
                      1 if c['enemyValid'] else 0, c['enemyIndex'], c['enemyHp'],
                      1 if c['actorValid'] else 0, c['actorId'], c['actorHp'],
                      1 if c['switchValid'] else 0, c['switchId']))
        sym = f'TROOP{troop_id}_PG{i}'
        jm = baked.get(f'Troops/{troop_id}/pg{i}', {})
        lines, _ = emit_list(page['list'], jm, sym)
        L += lines
    L.append(f'static const int TROOP{troop_id}_NPAGES = {len(troop["pages"])};')
    L.append('static const BtPageCond '
             f'TROOP{troop_id}_COND[] = ' + '{')
    for co in conds:
        L.append('    {%s},' % ','.join(map(str, co)))
    L.append('};')
    L.append('static const FhCmd *TROOP%d_LIST[] = {' % troop_id)
    for i in range(len(troop['pages'])):
        L.append(f'    TROOP{troop_id}_PG{i},')
    L.append('};')
    L.append('static const int TROOP%d_LEN[] = {' % troop_id)
    for i in range(len(troop['pages'])):
        L.append(f'    TROOP{troop_id}_PG{i}_LEN,')
    L.append('};')

    ce_syms = []
    for cid in ce_ids:
        e = next(x for x in ces if x and x['id'] == cid)
        sym = f'TROOP{troop_id}_CE{cid}'
        jm = baked.get(f'CommonEvents/{cid}', {})
        lines, _ = emit_list(e['list'], jm, sym)
        L += lines
        ce_syms.append((cid, sym))
    L.append(f'static const int TROOP{troop_id}_CE_IDS[] = '
             f'{{{",".join(str(c) for c, _ in ce_syms)}}};')
    L.append(f'static const FhCmd *TROOP{troop_id}_CE_LIST[] = '
             f'{{{",".join(s for _, s in ce_syms)}}};')
    L.append(f'static const int TROOP{troop_id}_CE_LEN[] = '
             f'{{{",".join(s + "_LEN" for _, s in ce_syms)}}};')
    L.append(f'static const int TROOP{troop_id}_CE_N = {len(ce_syms)};')


    actors = json.loads((data / 'Actors.json').read_text(encoding='utf-8'))
    L.append('static const struct { int id, cls, eq[8]; } '
             f'TROOP{troop_id}_ACTORS[] = ' + '{')
    for a in actors:
        if not a:
            continue
        eq = list(a.get('equips', []))[:8] + [0] * 8
        L.append('    {%d,%d,{%s}},' % (
            a['id'], a.get('classId', 0), ','.join(map(str, eq[:8]))))
    L.append('    {0,0,{0}},')
    L.append('};')
    out.write_text('\n'.join(L) + '\n')
    print(f'wrote {out} ({len(troop["pages"])} pages, CEs {ce_ids})')


if __name__ == '__main__':
    sys.exit(main())
