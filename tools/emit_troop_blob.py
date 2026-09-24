#!/usr/bin/env python3
"""Pack all troops and shared skill CEs into indexed binary blobs.

Usage (from repo root):
    python3 tools/emit_troop_blob.py "Fear & Hunger_WIN/www" \
        --out psp/gu_demo/data --ce-out psp/gu_demo/data

Writes troops.blob (one payload per troop: page conditions, command
streams, troop CEs, actor seeds, string pool) and skillce.blob (every
common event any skill or item can invoke, each with its own pool).
Encoding of single commands is shared with emit_troop_h.encode_cmd;
runtime/battle_blob.c decodes both files.
"""
import json
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from emit_troop_h import encode_cmd

MAGIC = 0x52544846
VERSION = 1
CE_MAGIC = 0x45434B53
CE_VERSION = 1
NO_STRING = 0xFFFFFFFF


def encode_list(lst, jm, sym):
    cmds = []
    n = len(lst)
    for i, c in enumerate(lst):
        jump = int(jm.get(str(i), n))
        cmds.append(encode_cmd(c, i, jump, sym))
    return cmds


def page_cond(page):
    c = page['conditions']
    return (page['span'],
            1 if c['turnEnding'] else 0, 1 if c['turnValid'] else 0,
            c['turnA'], c['turnB'],
            1 if c['enemyValid'] else 0, c['enemyIndex'], c['enemyHp'],
            1 if c['actorValid'] else 0, c['actorId'], c['actorHp'],
            1 if c['switchValid'] else 0, c['switchId'])


def pack_cmds(cmds, pool, pool_index):
    buf = bytearray()
    for code, indent, jump, op, pp, raw in cmds:
        if raw is None:
            soff = NO_STRING
        else:
            if raw not in pool_index:
                pool_index[raw] = len(pool)
                pool += raw.encode('utf-8') + b'\x00'
            soff = pool_index[raw]
        buf += struct.pack('<H B H B 10i I', code & 0xFFFF, indent & 0xFF,
                           jump & 0xFFFF, op & 0xFF,
                           *[int(v) for v in pp], soff)
    return bytes(buf)


def pack_ce_list(ces, pool, pool_index):
    buf = bytearray()
    buf += struct.pack('<H', len(ces))
    for cid, cmds in ces:
        cbuf = pack_cmds(cmds, pool, pool_index)
        buf += struct.pack('<H H', cid, len(cmds))
        buf += cbuf
    return bytes(buf)


def pack_ce_self(cid, cmds):
    pool = bytearray()
    pool_index = {}
    cbuf = pack_cmds(cmds, pool, pool_index)
    buf = bytearray()
    buf += struct.pack('<H H', cid, len(cmds))
    buf += cbuf
    buf += struct.pack('<I', len(pool))
    buf += pool
    return bytes(buf)


def collect_ces(ce_ids, ce_map, baked):
    ce_ids = sorted(ce_ids)
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
    out = []
    for cid in ce_ids:
        e = ce_map.get(cid)
        if not e:
            continue
        jm = baked.get(f'CommonEvents/{cid}', {})
        out.append((cid, encode_list(e['list'], jm, f'CEB_{cid}')))
    return out


def skill_ce_seeds(game_data):
    seeds = set()
    for fn in ('Skills.json', 'Items.json'):
        for x in json.loads((game_data / fn).read_text(encoding='utf-8')):
            if not x:
                continue
            for e in x.get('effects', []):
                if e.get('code') == 44 and e.get('dataId'):
                    seeds.add(e['dataId'])
    return seeds


def main() -> None:
    game = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[sys.argv.index('--out') + 1])
    ce_out = pathlib.Path(sys.argv[sys.argv.index('--ce-out') + 1])
    baked = json.loads(
        pathlib.Path('converted/baked/jumps.json').read_text())['jumps']
    data = game / 'data'
    troops = json.loads((data / 'Troops.json').read_text(encoding='utf-8'))
    ces = json.loads((data / 'CommonEvents.json').read_text(encoding='utf-8'))
    actors = json.loads((data / 'Actors.json').read_text(encoding='utf-8'))
    ce_map = {x['id']: x for x in ces if x}

    names = []
    payloads = []
    biggest = (0, 0)
    for troop in troops:
        if not troop:
            continue
        tid = troop['id']
        names.append((tid, troop['name']))
        pool = bytearray()
        pool_index = {}
        body = bytearray()
        pages = troop['pages']
        body += struct.pack('<H', len(pages))
        for i, page in enumerate(pages):
            jm = baked.get(f'Troops/{tid}/pg{i}', {})
            cmds = encode_list(page['list'], jm, f'T{i}_{i}')
            body += struct.pack('<13i', *page_cond(page))
            body += struct.pack('<H', len(cmds))
            body += pack_cmds(cmds, pool, pool_index)
        ids = sorted({c['parameters'][0] for p in pages
                      for c in p['list'] if c.get('code') == 117
                      and c.get('parameters')})
        ces_out = collect_ces(ids, ce_map, baked)
        body += pack_ce_list(ces_out, pool, pool_index)
        seeds = [(a['id'], a.get('classId', 0),
                  (list(a.get('equips', []))[:8] + [0] * 8)[:8])
                 for a in actors if a]
        body += struct.pack('<H', len(seeds))
        for sid, cls, eq in seeds:
            body += struct.pack('<10i', sid, cls, *eq)
        body += struct.pack('<I', len(pool))
        body += pool
        payloads.append((tid, bytes(body)))
        if len(body) > biggest[1]:
            biggest = (tid, len(body))

    names_blob = bytearray()
    name_offs = []
    for tid, nm in names:
        name_offs.append((tid, len(names_blob)))
        names_blob += nm.encode('utf-8') + b'\x00'
    index = bytearray()
    off = 16 + len(payloads) * 16 + len(names_blob)
    for (tid, _), (pid, pay) in zip(names, payloads):
        assert tid == pid
        index += struct.pack('<4I', tid, off, len(pay),
                             dict(name_offs)[tid])
        off += len(pay)
    blob = bytearray()
    blob += struct.pack('<4I', MAGIC, VERSION, len(payloads),
                        len(names_blob))
    blob += index
    blob += names_blob
    for _, pay in payloads:
        blob += pay
    (out / 'troops.blob').write_bytes(bytes(blob))

    shared = collect_ces(skill_ce_seeds(data), ce_map, baked)
    ceb = bytearray()
    ceb += struct.pack('<I H H', CE_MAGIC, CE_VERSION, len(shared))
    for cid, cmds in shared:
        ceb += pack_ce_self(cid, cmds)
    (ce_out / 'skillce.blob').write_bytes(bytes(ceb))

    print(f'troops: {len(payloads)} payloads, biggest troop '
          f'{biggest[0]} {biggest[1]} bytes, blob {len(blob)} bytes')
    print(f'shared CEs: {len(shared)} ids, '
          f'biggest {max(len(c) for _, c in shared)} cmds, '
          f'blob {len(ceb)} bytes')


if __name__ == '__main__':
    main()
