#!/usr/bin/env python3
"""Portable build driver for the PSP demo (no make, no sh).

Replicates psp/gu_demo/Makefile using only Python plus the PSP
toolchain binaries on PATH (psp-gcc, bin2o, psp-fixup-imports,
mksfoex, psp-strip, pack-pbp, psp-config). Works anywhere those
exist: Linux, macOS, Windows via WSL, or native Windows through
MSYS2 (psp-gcc and friends resolve as .exe automatically).

Usage (from the repo root):
    python3 tools/build.py            build EBOOT.PBP + fill Build/
    python3 tools/build.py --clean    remove build artifacts

The object list, data embeds, and link libs are parsed from the
Makefile so the two drivers cannot drift apart. Timestamps drive
rebuilds; generated headers are explicit dependencies so a
regenerated header always rebuilds what includes it.
"""
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEMO = ROOT / 'psp' / 'gu_demo'
RUNTIME = ROOT / 'runtime'
TARGET = 'fh_demo'
TITLE = 'F&H Port'


def need(prog):
    if shutil.which(prog) is None:
        sys.exit(f'missing tool: {prog} (install the PSP toolchain and'
                 f' put it on PATH, see docs/pipeline.md)')
    return prog


def run(cmd):
    print('+', ' '.join(str(c) for c in cmd))
    r = subprocess.run([str(c) for c in cmd], cwd=DEMO)
    if r.returncode != 0:
        sys.exit(f'failed: {cmd[0]}')
    return r


def read_make(path):
    text = path.read_text(encoding='utf-8')
    text = re.sub(r'\\\n', ' ', text)
    out = {}
    for line in text.splitlines():
        m = re.match(r'^([A-Z_]+)\s*=\s*(.*)$', line.strip())
        if m:
            out[m.group(1)] = m.group(2).strip()
    rules = {}
    for m in re.finditer(r'^([A-Za-z0-9_./$%-]+\.o):\s*(\S+)\s*$',
                         text, re.M):
        rules[m.group(1).replace('$$', '$')] = \
            m.group(2).replace('$$', '$')
    return out, rules


def write_if_changed(path, data):
    if path.exists() and path.read_bytes() == data:
        return False
    path.write_bytes(data)
    print(f'wrote {path.relative_to(ROOT)}')
    return True


def refresh_copies():
    rt = RUNTIME
    jobs = [
        ('interp_rt.c', rt / 'interp.c', None),
        ('text_rt.c', rt / 'text.c', None),
        ('map_runtime.c', rt / 'map.c', None),
    ]
    changed = False
    for local, src, _ in jobs:
        lines = src.read_text(encoding='utf-8').splitlines(keepends=True)
        lines[0] = ('/* COPY of runtime/%s - see runtime/. Refresh with'
                    ' cp. */\n' % src.name)
        text = ''.join(lines).replace('#include "interp.h"',
                                      '#include "interp_rt.h"')
        text = text.replace('#include "text.h"',
                            '#include "text_rt.h"')
        text = text.replace('#include "map.h"',
                            '#include "map_runtime.h"')
        changed |= write_if_changed(DEMO / local, text.encode())
    shim = (b'#ifndef FH_INTERP_RT_H\n#define FH_INTERP_RT_H\n\n'
            b'#include "../../runtime/interp.h"\n\n#endif\n')
    changed |= write_if_changed(DEMO / 'interp_rt.h', shim)
    for local, orig, new in (('text_rt.h', 'FH_TEXT_H', 'FH_TEXT_RT_H'),
                             ('map_runtime.h', 'FH_MAP_H',
                              'FH_MAP_RUNTIME_H')):
        src = {'text_rt.h': rt / 'text.h',
               'map_runtime.h': rt / 'map.h'}[local]
        text = src.read_text(encoding='utf-8').replace(orig, new)
        changed |= write_if_changed(DEMO / local, text.encode())
    return changed


SPECIAL_C = {
    'player_rt.o': ('../../runtime/player.c', ['-I../../runtime']),
    'battle_rt.o': ('../../runtime/battle.c', ['-I../../runtime']),
    'battle_blob.o': ('../../runtime/battle_blob.c', ['-I../../runtime']),
    'interp_rt.o': ('interp_rt.c', []),
    'text_rt.o': ('text_rt.c', []),
    'map_runtime.o': ('map_runtime.c', []),
}

HEADER_DEPS = {
    'main.o': ['main.c', 'battle_db.h', 'troop1.h', 'troop44.h',
               'itemce.h', 'anim_data.h', 'event_demo.h',
               'map030_lights.h', 'render.h', 'interp_rt.h',
               '../../runtime/battle.h', '../../runtime/battle_blob.h',
               '../../runtime/interp.h'],
    'render.o': ['render.c', 'render.h', 'anim_data.h', 'map_runtime.h',
                 'interp_rt.h', 'text_rt.h'],
    'input.o': ['input.c', 'input.h'],
    'map_runtime.o': ['map_runtime.c', 'map_runtime.h',
                      '../../runtime/map.h'],
    'interp_rt.o': ['interp_rt.c', 'interp_rt.h',
                    '../../runtime/interp.h'],
    'text_rt.o': ['text_rt.c', 'text_rt.h', '../../runtime/text.h'],
    'player_rt.o': ['../../runtime/player.c', '../../runtime/player.h'],
    'battle_rt.o': ['../../runtime/battle.c', '../../runtime/battle.h'],
    'battle_blob.o': ['../../runtime/battle_blob.c',
                      '../../runtime/battle_blob.h',
                      '../../runtime/battle.h', '../../runtime/interp.h'],
}


def newer(deps, target):
    t = DEMO / target
    if not t.exists():
        return True
    mt = t.stat().st_mtime
    for d in deps:
        p = DEMO / d
        if p.exists() and p.stat().st_mtime > mt:
            return True
    return False


def main() -> None:
    if '--clean' in sys.argv:
        for pat in ('*.o', '*.elf', '*.SFO', '*.PBP', '*_strip.elf'):
            for p in DEMO.glob(pat):
                p.unlink()
                print(f'removed {p.name}')
        return
    for prog in ('psp-gcc', 'bin2o', 'psp-fixup-imports', 'mksfoex',
                 'psp-strip', 'pack-pbp', 'psp-config'):
        need(prog)
    vars_, rules = read_make(DEMO / 'Makefile')
    objs = vars_['OBJS'].split()
    libs = [l for l in vars_.get('LIBS', '').split()
            if 'pspnet' not in l]
    cflags = vars_.get('CFLAGS', '-Wall -O2 -G0').split()
    r = subprocess.run(['psp-config', '--pspsdk-path'], capture_output=True,
                       text=True)
    if r.returncode != 0:
        sys.exit('psp-config failed; is PSPDEV set up?')
    sdk = pathlib.Path(r.stdout.strip())
    prefix = sdk.parent
    inc = [f'-I{DEMO}', f'-I{prefix / "include"}',
           f'-I{sdk / "include"}']
    refresh_copies()
    for obj in objs:
        if obj in SPECIAL_C:
            src, extra = SPECIAL_C[obj]
            deps = HEADER_DEPS.get(obj, [src])
        elif obj in rules:
            continue
        else:
            src, extra = obj.replace('.o', '.c'), []
            deps = HEADER_DEPS.get(obj, [src])
        if not newer(deps, obj):
            continue
        run(['psp-gcc'] + cflags + ['-D_PSP_FW_VERSION=600'] + inc +
            extra + ['-c', src, '-o', obj])
    for obj in objs:
        if obj not in rules:
            continue
        src = rules[obj]
        if not newer([src], obj):
            continue
        if (DEMO / obj).exists():
            (DEMO / obj).unlink()
        run(['bin2o', '-i', src, obj,
             obj[:-2] if obj.endswith('.o') else obj])
    elf = f'{TARGET}.elf'
    if newer(objs, elf):
        run(['psp-gcc'] + [o for o in objs] +
            [f'-L{DEMO}', f'-L{prefix / "lib"}', f'-L{sdk / "lib"}',
             '-Wl,-zmax-page-size=128'] + libs + ['-o', elf])
        run(['psp-fixup-imports', elf])
    sfo = 'PARAM.SFO'
    if not (DEMO / sfo).exists():
        run(['mksfoex', TITLE, sfo])
    if newer([elf, sfo], 'EBOOT.PBP'):
        run(['psp-strip', elf, '-o', f'{TARGET}_strip.elf'])
        run(['pack-pbp', 'EBOOT.PBP', sfo, 'NULL', 'NULL', 'NULL',
             'NULL', 'NULL', f'{TARGET}_strip.elf', 'NULL'])
        (DEMO / f'{TARGET}_strip.elf').unlink(missing_ok=True)
    dist = ROOT / 'Build'
    dist.mkdir(exist_ok=True)
    shutil.copy2(DEMO / 'EBOOT.PBP', dist / 'EBOOT.PBP')
    data = DEMO / 'data'
    if (data / 'troops.blob').exists():
        (dist / 'data').mkdir(exist_ok=True)
        for name in ('troops.blob', 'skillce.blob'):
            shutil.copy2(data / name, dist / 'data' / name)
        for sub in ('enemies', 'battlebacks'):
            if (data / sub).exists():
                shutil.copytree(data / sub, dist / 'data' / sub,
                                dirs_exist_ok=True)
    print(f'dist ready at {dist}')


if __name__ == '__main__':
    main()
