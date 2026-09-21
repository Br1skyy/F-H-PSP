#!/usr/bin/env python3.12
"""Phase 0 data audit — implements §4.2 of the plan (extended).

Usage:
    python3.12 tools/audit.py "Fear & Hunger_WIN/www" [--out audit_out]

Reads the game folder and writes:
    audit.json        — raw counters (commands, plugin cmds, note tags, snippets, maps, switches/vars)
    audit_report.txt  — human-readable summary answering open questions Q1,Q3,Q5,Q6,Q7

Only reads the user's own copy. Never ships assets.
"""
import json, re, sys, pathlib, collections

def load_json(p):
    return json.loads(pathlib.Path(p).read_text(encoding='utf-8'))

def main():
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('Fear & Hunger_WIN/www')
    outdir = pathlib.Path(sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--out' else 'audit_out')
    # allow: audit.py GAME [--out DIR]
    if '--out' in sys.argv:
        outdir = pathlib.Path(sys.argv[sys.argv.index('--out') + 1])
    outdir.mkdir(parents=True, exist_ok=True)
    data = root / 'data'

    cmds = collections.Counter()
    plug = collections.Counter()
    notes = collections.Counter()
    snippets = collections.Counter()   # damage formulas + script lines + script conds

    def walk(lst):
        for c in lst or []:
            if not isinstance(c, dict):
                continue
            code, p = c.get('code', -1), c.get('parameters', [])
            cmds[code] += 1
            try:
                if code == 356 and p:
                    plug[str(p[0]).split(' ')[0]] += 1
                elif code in (355, 655) and p:
                    snippets['script:' + str(p[0])[:200]] += 1
                elif code == 111 and len(p) > 1 and p[0] == 12:
                    snippets['cond:' + str(p[1])[:200]] += 1
            except Exception:
                pass

    def load(name):
        return load_json(data / name)

    n_maps = 0
    for f in sorted(data.glob('Map[0-9]*.json')):
        n_maps += 1
        d = load(f.name)
        for ev in filter(None, d.get('events', []) or []):
            for pg in ev.get('pages', []):
                walk(pg.get('list', []))

    for ce in filter(None, load('CommonEvents.json') or []):
        walk(ce.get('list', []))
    for tr in filter(None, load('Troops.json') or []):
        for pg in tr.get('pages', []):
            walk(pg.get('list', []))

    for name in ('Skills', 'Items', 'Weapons', 'Armors', 'States', 'Enemies', 'Actors', 'Classes'):
        try:
            arr = load(name + '.json')
        except FileNotFoundError:
            continue
        for o in filter(None, arr or []):
            if not isinstance(o, dict):
                continue
            dmg = o.get('damage', {})
            if isinstance(dmg, dict) and dmg.get('formula') not in ('', '0', None):
                snippets['formula:' + str(dmg['formula'])[:200]] += 1
            for t in re.findall(r'<([^:>\s]+)', o.get('note', '') or ''):
                notes[t] += 1

    # map sizes + event counts (worst-case maps for benchmarks)
    maps = []
    for f in sorted(data.glob('Map[0-9]*.json')):
        d = load(f.name)
        w, h = d.get('width', 0), d.get('height', 0)
        nev = len([e for e in (d.get('events') or []) if e])
        maps.append({'file': f.name, 'w': w, 'h': h,
                     'tiles': w * h, 'events': nev,
                     'name': d.get('displayName', '')})
    maps.sort(key=lambda m: m['tiles'], reverse=True)

    # switches / variables of interest (hunger, switch 3520)
    sysj = load('System.json')
    switches = sysj.get('switches', [])
    variables = sysj.get('variables', [])
    def named(lst, pred):
        return [(i, v) for i, v in enumerate(lst) if v and pred(v)]
    hunger_vars = named(variables, lambda v: 'hung' in v.lower() or 'food' in v.lower())
    sw3520 = switches[3520] if len(switches) > 3520 else None
    filter_sw = named(switches, lambda s: 'filter' in s.lower())

    # encryption + asset sizes (Q6)
    enc = {'hasEncryptedImages': sysj.get('hasEncryptedImages'),
           'hasEncryptedAudio': sysj.get('hasEncryptedAudio'),
           'hasKey': bool(sysj.get('encryptionKey'))}
    def dir_size(p):
        p = root / p
        if not p.exists():
            return None
        return sum(f.stat().st_size for f in p.rglob('*') if f.is_file())
    asset_sizes = {k: dir_size(k) for k in ('img', 'audio', 'movies', 'data', 'js')}

    # plugin list (enabled set)
    try:
        txt = (root / 'js/plugins.js').read_text(encoding='utf-8')
        plugins = json.loads(txt[txt.index('['): txt.rindex(']') + 1])
        plugstat = [{'name': p['name'], 'status': p['status']} for p in plugins]
    except Exception as e:
        plugstat = [{'error': str(e)}]

    result = {
        'game_root': str(root),
        'n_map_files': n_maps,
        'commands': dict(sorted(cmds.items(), key=lambda kv: kv[0])),
        'plugin_commands': dict(plug.most_common()),
        'note_tags': dict(notes.most_common()),
        'js_snippets_distinct': len(snippets),
        'js_snippets_top': snippets.most_common(60),
        'maps_top20_by_tiles': maps[:20],
        'switches_total': len(switches),
        'variables_total': len(variables),
        'switch_3520': sw3520,
        'filter_switches': filter_sw,
        'hunger_variables': hunger_vars,
        'encryption': enc,
        'asset_sizes_bytes': asset_sizes,
        'plugins': plugstat,
    }
    (outdir / 'audit.json').write_text(json.dumps(result, indent=1), encoding='utf-8')

    # human report
    L = []
    L.append(f'Phase 0 audit — {root}')
    L.append(f'Map files: {n_maps}, event-command occurrences total: {sum(cmds.values())}')
    L.append('')
    L.append('Top event codes (code: count):')
    for code, n in cmds.most_common(25):
        L.append(f'  {code}: {n}')
    L.append('')
    L.append('Plugin commands:')
    for k, v in plug.most_common():
        L.append(f'  {v:6d} {k}')
    L.append('')
    L.append('Note tags:')
    for k, v in notes.most_common(30):
        L.append(f'  {v:4d} {k}')
    L.append('')
    L.append('Largest maps (file WxH tiles events):')
    for m in maps[:12]:
        L.append(f"  {m['file']} {m['w']}x{m['h']} tiles={m['tiles']} events={m['events']}")
    L.append('')
    L.append(f"Switches: {len(switches)}, Variables: {len(variables)}")
    L.append(f'Switch 3520 = {sw3520!r}')
    L.append(f'Filter switches: {filter_sw}')
    L.append(f'Hunger variables: {hunger_vars}')
    L.append(f"Encryption: {enc}")
    L.append(f"Asset sizes (bytes): {asset_sizes}")
    L.append(f"Distinct JS snippets (formulas+scripts+conds): {len(snippets)}")
    L.append('Top snippets:')
    for k, v in snippets.most_common(20):
        L.append(f'  {v:5d} {k[:160]!r}')
    (outdir / 'audit_report.txt').write_text('\n'.join(L) + '\n', encoding='utf-8')
    print('\n'.join(L))
    print(f'\nWrote {outdir / "audit.json"} and {outdir / "audit_report.txt"}')

if __name__ == '__main__':
    main()
