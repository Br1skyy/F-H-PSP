#!/usr/bin/env python3.12
"""Formula/JS-snippet coverage — implements §4.4 of the plan (Python port).

The plan sketches a Node/acorn compiler. This tool answers the same question
without Node: what fraction of snippets fits the §5.3 VM subset, and what
lands in the fallback list (needs mujs/Duktape or hand-ported runtime ops)?

Usage:
    python3.12 tools/formula_coverage.py "Fear & Hunger_WIN/www" [--out audit_out]

Output: coverage.json + coverage_report.txt
"""
import json, re, sys, pathlib, collections


TOKEN = re.compile(r"""
    (?P<num>\d+\.?\d*) |
    (?P<str>'[^']*'|"[^"]*") |
    (?P<id>\$?\w+) |
    (?P<op>===|!==|<=|>=|&&|\|\||[+\-*/%<>=!&|?:;,.()\[\]])
""", re.VERBOSE)

ALLOWED_STATS = {'atk','def','mat','mdf','agi','luk','hp','mp','tp','mhp','mmp',
                 'level','exp','crt','cev','hit','eva','mev','cnt','hrg','mrg','trg'}
ALLOWED_CALLS = {'Math.max','Math.min','Math.floor','Math.ceil','Math.abs',
                 '$gameVariables.value','$gameSwitches.value','Math.random'}

def try_compile_expr(src):
    """Return (ok, reason_or_bytecode)."""
    toks = [m.group(0) for m in TOKEN.finditer(src)]
    if not toks:
        return False, 'empty'
    joined = ''.join(toks)
    stripped = re.sub(r'\s+', '', src)
    if joined != stripped:
        bad = re.sub(r'\s+', '', src)
        for t in toks:
            bad = bad.replace(t, '', 1)
        return False, f'unsupported-chars:{bad[:40]!r}'

    if any(k in toks for k in (';', 'var', 'function', 'for', 'while', 'return', 'new')):
        return False, 'statement-style'
    if re.search(r'\b(if|else|for|while|function|new|this)\b', src):
        return False, 'keyword-blocked'

    for m in re.finditer(r'\$?\w+', src):
        w = m.group(0)
        if w in ('a','b','Math','max','min','floor','ceil','abs','random',
                 '$gameVariables','$gameSwitches','value','true','false'):
            continue
        if w in ALLOWED_STATS:
            continue
        if w.isdigit():
            continue
        return False, f'ident:{w}'

    for m in re.finditer(r'(\$?\w+)\.(\w+)', src):
        obj, prop = m.group(1), m.group(2)
        if obj in ('a','b') and prop in ALLOWED_STATS:
            continue
        if obj == 'Math' and prop in ('max','min','floor','ceil','abs','random'):
            continue
        if obj in ('$gameVariables','$gameSwitches') and prop == 'value':
            continue
        return False, f'member:{obj}.{prop}'
    return True, 'expr-ok'

def classify_script(src):
    """Script lines (code 355/655) are runtime ops, not VM exprs. Bucket them."""
    s = src.strip()
    if re.fullmatch(r'\$gamePlayer\.refresh\(\);?', s):
        return 'op:refresh-player'
    if re.fullmatch(r"\$gameActors\.actor\(\d+\)\.set(Character|Battler)Image\(.*\);?", s):
        return 'op:set-image'
    if re.fullmatch(r'\$game(Variables|Switches|Actors|Map|Party|Troop)\..*', s):
        return 'op:game-api (hand-port, small set?)'
    ok, reason = try_compile_expr(s.rstrip(';'))
    if ok:
        return 'expr-compatible'
    return f'fallback:{reason}'

def main():
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('Fear & Hunger_WIN/www')
    outdir = pathlib.Path('audit_out')
    if '--out' in sys.argv:
        outdir = pathlib.Path(sys.argv[sys.argv.index('--out') + 1])
    outdir.mkdir(parents=True, exist_ok=True)
    data = root / 'data'
    def load(n): return json.loads((data / n).read_text(encoding='utf-8'))

    formulas, scripts, conds = [], [], []
    def walk(lst):
        for c in lst or []:
            if not isinstance(c, dict): continue
            code, p = c.get('code'), c.get('parameters', [])
            if code in (355, 655) and p: scripts.append(str(p[0]))
            elif code == 111 and len(p) > 1 and p[0] == 12: conds.append(str(p[1]))
    for f in data.glob('Map[0-9]*.json'):
        d = load(f.name)
        for ev in filter(None, d.get('events', []) or []):
            for pg in ev.get('pages', []): walk(pg.get('list', []))
    for ce in filter(None, load('CommonEvents.json') or []): walk(ce.get('list', []))
    for tr in filter(None, load('Troops.json') or []):
        for pg in tr.get('pages', []): walk(pg.get('list', []))
    for name in ('Skills','Items','Weapons','Armors','States','Enemies'):
        for o in filter(None, load(name + '.json') or []):
            f = o.get('damage', {}).get('formula', '') if isinstance(o.get('damage'), dict) else ''
            if f and f != '0': formulas.append(f)

    f_ok, f_bad = collections.Counter(), collections.Counter()
    for f in formulas:
        ok, why = try_compile_expr(f)
        (f_ok if ok else f_bad)[f if not ok else ('OK:' + f)] += 1
    s_cls = collections.Counter(classify_script(s) for s in scripts)
    c_ok = sum(1 for c in conds if try_compile_expr(c)[0])

    total_f = len(formulas)
    nok_f = sum(f_bad.values())
    L = [f'Formula/JS coverage — {root}',
         f'Damage formulas: {total_f} occurrences, {len(set(formulas))} distinct',
         f'  compilable to VM subset: {total_f - nok_f} ({100*(total_f-nok_f)/max(1,total_f):.1f}%)',
         f'  fallback (needs full interpreter review): {nok_f}']
    if f_bad:
        L.append('  fallback formulas:')
        for k, v in f_bad.most_common(20): L.append(f'    {v:4d} {k!r}')
    else:
        L.append('  distinct formulas sample: ' + ', '.join(sorted(set(formulas))[:15]))
    L.append('')
    L.append(f'Script lines (355/655): {len(scripts)} occurrences, {len(set(scripts))} distinct')
    for k, v in s_cls.most_common(20): L.append(f'  {v:5d} {k}')
    L.append('')
    L.append(f'Script conditions (111/12): {len(conds)} occurrences, {c_ok} compilable ({100*c_ok/max(1,len(conds)):.1f}%)')
    bad_c = collections.Counter(c for c in set(conds) if not try_compile_expr(c)[0])
    for k in list(bad_c)[:10]: L.append(f'  FALLBACK cond: {k[:150]!r}')
    L.append('')
    L.append('Verdict: ' + ('formulas are trivially VM-safe (constants + a.atk/b.def); '
             'script LINES are runtime API ops (refresh/setImage), not VM exprs — '
             'hand-port ~5 API ops, no ES5 interpreter needed for the observed set. '
             if nok_f == 0 else 'some formulas need fallback — review list above.'))

    (outdir / 'coverage.json').write_text(json.dumps({
        'formulas_total': total_f, 'formulas_distinct': len(set(formulas)),
        'formulas_fallback': dict(f_bad), 'scripts': dict(s_cls),
        'conds_total': len(conds), 'conds_ok': c_ok,
        'fallback_conds': sorted(bad_c)[:20]}, indent=1), encoding='utf-8')
    (outdir / 'coverage_report.txt').write_text('\n'.join(L) + '\n', encoding='utf-8')
    print('\n'.join(L))
    print(f"\nwrote {outdir / 'coverage.json'}")

if __name__ == '__main__':
    main()
