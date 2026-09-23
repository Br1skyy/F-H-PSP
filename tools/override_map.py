#!/usr/bin/env python3.12
"""List every prototyped method per plugin, in load order.

Usage:
    python3.12 tools/override_map.py "Fear & Hunger_WIN/www" [--out audit_out]

Lists every `Class.prototype.method` definition per enabled plugin, in load
order, so methods touched by 2+ plugins (effective-behavior risk) stand out.
Also flags unreadable/minified (possibly obfuscated) plugins - Q8."""
import re, json, sys, pathlib, collections

pat = re.compile(r'(\w+)\.prototype\.(\w+)\s*=\s*function')

def main():
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('Fear & Hunger_WIN/www')
    outdir = pathlib.Path('audit_out')
    if '--out' in sys.argv:
        outdir = pathlib.Path(sys.argv[sys.argv.index('--out') + 1])
    outdir.mkdir(parents=True, exist_ok=True)

    txt = (root / 'js/plugins.js').read_text(encoding='utf-8')
    plugins = json.loads(txt[txt.index('['): txt.rindex(']') + 1])
    touch = collections.defaultdict(list)
    obfuscated = []
    for p in plugins:
        if not p.get('status'):
            continue
        f = root / 'js/plugins' / (p['name'] + '.js')
        if not f.exists():
            touch['<MISSING FILE>'].append(p['name'])
            continue
        src = f.read_text(encoding='utf-8', errors='ignore')
        for cls, meth in pat.findall(src):
            touch[f'{cls}.{meth}'].append(p['name'])

        lines = src.splitlines()
        avglen = sum(len(l) for l in lines[:50]) / max(1, len(lines[:50]))
        if (len(lines) < 30 and avglen > 300) or '_0x' in src[:2000]:
            obfuscated.append(p['name'])

    multi = {k: v for k, v in touch.items() if len(v) >= 2}
    L = [f'Plugin override map - {root}',
         f'Enabled plugins: {sum(1 for p in plugins if p.get("status"))}',
         '',
         f'Methods touched by 2+ plugins ({len(multi)} - read these first):']
    for k in sorted(multi):
        L.append(f"  {k:55s} {' -> '.join(multi[k])}")
    L.append('')
    L.append(f'All touched methods ({len(touch)}):')
    for k in sorted(touch):
        L.append(f"  {k:55s} {' -> '.join(touch[k])}")
    L.append('')
    L.append(f'Possibly obfuscated/minified plugins (heuristic): {obfuscated or "none found"}')
    (outdir / 'override_map.txt').write_text('\n'.join(L) + '\n', encoding='utf-8')
    (outdir / 'override_map.json').write_text(
        json.dumps({'multi': multi, 'all': dict(touch), 'obfuscated': obfuscated}, indent=1),
        encoding='utf-8')
    print('\n'.join(L[:40]))
    print(f'... wrote {outdir / "override_map.txt"} ({len(touch)} methods, {len(multi)} multi-touch)')

if __name__ == '__main__':
    main()
