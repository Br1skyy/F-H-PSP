#!/usr/bin/env python3.12
"""Golden-master emitter — md §3.5 in miniature.

Takes REAL event lists from the player's copy, converts them to FhCmd rows
(using converted/baked/jumps.json), runs a Python reference sim, and emits
tests/test_vec.h with the vectors + expected traces. The C harness
(tests/test_interp.c) must reproduce them exactly.

Usage:
    python3.12 tools/emit_interp_test.py "Fear & Hunger_WIN/www" --out tests
"""
import json, sys, pathlib

def cstr(s):

    out = ['"']
    for b in s.encode('utf-8'):
        ch = chr(b)
        if ch == '"': out.append('\\"')
        elif ch == '\\': out.append('\\\\')
        elif ch == '\n': out.append('\\n')
        elif 32 <= b < 127: out.append(ch)
        else: out.append(f'\\x{b:02x}')
    out.append('"')
    return ''.join(out)

_ANIM_FRAMES = None
def frames_of(anim_id):
    """Baked frame count for 212 waits (duration = frames*4+1, rpg_sprites)."""
    global _ANIM_FRAMES
    if _ANIM_FRAMES is None:
        _ANIM_FRAMES = {a['id']: len(a['frames'])
                        for a in json.loads(pathlib.Path('converted/baked/anims.json').read_text())}
    return _ANIM_FRAMES.get(anim_id, 0)

def canon_esc(s):
    """Canonical escaping for token literals (mirrored in test_interp.c).
    ASCII-only: UTF-8 continuation bytes always take the \\xNN path, exactly
    like the C side's byte-wise isalnum."""
    out = []
    for b in s.encode('utf-8'):
        ch = chr(b)
        if ch.isascii() and (ch.isalnum() or ch in " ., '\"!?()-_:;/"):
            out.append(ch)
        else:
            out.append(f'\\x{b:02x}')
    return ''.join(out)

def py_decode(text, variables, actor_names, party, currency):
    """Mirror of runtime/text.c fh_decode_escapes. Returns canonical literal."""
    s = text.replace('\\', '\x1b').replace('\x1b\x1b', '\\')
    def sub(s, fam, fn):
        out = []
        i = 0
        while i < len(s):
            if s[i] == '\x1b' and i + 1 < len(s) and s[i + 1] in (fam, fam.lower()):
                j = i + 2
                num = None
                if j < len(s) and s[j] == '[':
                    k = s.find(']', j)
                    if k > j:
                        num = s[j + 1:k]
                        j = k + 1
                v = fn(num)
                if v is None:
                    out.append(s[i:i + 2])
                    i += 2
                else:
                    out.append(v)
                    i = j
            else:
                out.append(s[i])
                i += 1
        return ''.join(out)
    def vfn2(num):
        if num is not None and num.isdigit() and int(num) < len(variables):
            return str(variables[int(num)])
        return None
    def nfn(num):
        if num is not None and num.isdigit() and 1 <= int(num) <= len(actor_names):
            return actor_names[int(num) - 1]
        return None
    def pfn(num):
        if num is not None and num.isdigit() and 1 <= int(num) <= len(party):
            aid = party[int(num) - 1]
            if 1 <= aid <= len(actor_names):
                return actor_names[aid - 1]
            return ''
        return None
    s = sub(s, 'V', vfn2)
    s = sub(s, 'V', vfn2)
    s = sub(s, 'N', nfn)
    s = sub(s, 'P', pfn)
    s = sub(s, 'G', lambda num: currency)
    toks = []
    i = 0
    syms = set('$ . | ^ ! > < { } \\'.split())
    while i < len(s):
        if s[i] == '\x1b':
            i += 1
            code = ''
            if i < len(s) and s[i] in syms:
                code = s[i]
                i += 1
            else:
                while i < len(s) and s[i].isalpha():
                    code += s[i].upper()
                    i += 1
            param = -1
            if i < len(s) and s[i] == '[':
                k = s.find(']', i)
                if k > i and s[i + 1:k].isdigit():
                    param = int(s[i + 1:k])
                    i = k + 1
            toks.append(f'C{code},{param};')
        else:
            j = i
            while j < len(s) and s[j] != '\x1b':
                j += 1
            toks.append('T' + canon_esc(s[i:j]) + ';')
            i = j
    return ''.join(toks)

def to_cmd(c, jump):
    code = c.get('code', -1)
    p = c.get('parameters', [])
    num = lambda i: p[i] if isinstance(p[i], (int, float)) else 0
    op, pp, s = 0, [0, 0, 0, 0, 0, 0], None
    if code == 111 and p:
        op = int(p[0])
        if op == 0: pp = [int(p[1]), int(p[2]), 0, 0, 0, 0]
        elif op == 1: pp = [int(p[1]), int(p[2]), int(p[3] if len(p) < 5 else p[3]), int(p[4]) if len(p) > 4 else 0, 0, 0]
        elif op == 4:
            pp = [int(p[1]), int(p[2]), int(p[3]) if len(p) > 3 and isinstance(p[3], (int, float)) else 0, 0, 0, 0]
            if int(p[2]) == 1 and len(p) > 3: s = str(p[3])
        elif op == 5: pp = [int(p[1]), int(p[2]), int(p[3]) if len(p) > 3 else 0, 0, 0, 0]
        elif op == 8: pp = [int(p[1]), 0, 0, 0, 0, 0]
    elif code == 121 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2]), 0, 0, 0]
    elif code == 122 and len(p) >= 5:
        op = int(p[2]); pp = [int(p[0]), int(p[1]), int(p[3]), int(p[4]) if not isinstance(p[4], str) else 0, int(p[5]) if len(p) > 5 and not isinstance(p[5], str) else 0, 0, 0, 0, 0, 0]
    elif code == 230 and p: pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 402 and p: pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 102 and p and isinstance(p[0], list):
        s = '\n'.join(str(x) for x in p[0])
        pp = [len(p[0]), 0, 0, 0, 0, 0]
    elif code == 117 and p: pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 129 and len(p) >= 2: pp = [int(p[0]), int(p[1]), int(p[2]) if len(p) > 2 else 0, 0, 0, 0]
    elif code == 205 and len(p) >= 2:
        route = p[1] if isinstance(p[1], dict) else {}
        pp = [int(p[0]), len(route.get('list', [])), 1 if route.get('wait') else 0, 0, 0, 0]
    elif code == 223 and len(p) >= 2:
        tone = p[0] if isinstance(p[0], list) else [0, 0, 0, 0]
        op = 1 if (len(p) > 2 and p[2]) else 0
        pp = [int(tone[0]), int(tone[1]), int(tone[2]), int(tone[3]), int(p[1]), 0]
    elif code == 250 and p and isinstance(p[0], dict): s = str(p[0].get('name', ''))
    elif code == 313 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), 0, 0]
    elif code in (126, 127, 128) and len(p) >= 4:
        op = int(p[1]); pp = [int(p[0]), 0, int(p[2]), int(p[3]) if not isinstance(p[3], str) else 0, 0, 0]
    elif code in (311, 312) and len(p) >= 6:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]) if not isinstance(p[4], str) else 0, int(p[5])]
    elif code == 212 and len(p) >= 2:
        pp = [int(p[0]), int(p[1]), 1 if (len(p) > 2 and p[2]) else 0, frames_of(int(p[1])), 0, 0]
    elif code == 203 and len(p) >= 4:

        if p[1] == 0: pp = [int(p[0]), 0, 0, int(p[2]), int(p[3]), int(p[4]) if len(p) > 4 else 0]
        elif p[1] == 1: pp = [int(p[0]), 1, 0, int(p[2]), int(p[3]), int(p[4]) if len(p) > 4 else 0]
        else: pp = [int(p[0]), 2, int(p[2]), 0, 0, int(p[4]) if len(p) > 4 else 0]
    elif code == 211 and p: pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 216 and p: pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 322 and len(p) >= 6:
        s = '\n'.join([str(p[1]), str(p[2]), str(p[3]), str(p[4]), str(p[5])])
        pp = [int(p[0]), 0, 0, 0, 0, 0]
    elif code == 201 and len(p) >= 6:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]), int(p[5])]
    elif code == 213 and len(p) >= 2:
        pp = [int(p[0]), int(p[1]), 0, 0, 0, 0]
    elif code == 214: pp = [0, 0, 0, 0, 0, 0]
    elif code == 204 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2]), 0, 0, 0]
    elif code in (401, 405) and p: s = str(p[0])
    elif code in (355, 655) and p: s = str(p[0])
    elif code == 356 and p: s = str(p[0])
    elif code == 132 and p and isinstance(p[0], dict): s = str(p[0].get('name', ''))
    elif code == 231 and len(p) >= 10:
        pp = [int(p[0]), int(p[2]), int(p[3]), int(p[4]) if not isinstance(p[4], str) else 0,
              int(p[5]) if not isinstance(p[5], str) else 0,
              int(p[6]), int(p[7]), int(p[8]), int(p[9]), 0]
        s = str(p[1])
    elif code == 232 and len(p) >= 12:
        op = 1 if p[11] else 0
        pp = [int(p[0]), int(p[2]), int(p[3]),
              int(p[4]) if not isinstance(p[4], str) else 0,
              int(p[5]) if not isinstance(p[5], str) else 0,
              int(p[6]), int(p[7]), int(p[8]), int(p[9]), int(p[10])]
    elif code == 233 and len(p) >= 2: pp = [int(p[0]), int(p[1]), 0, 0, 0, 0, 0, 0, 0, 0]
    elif code == 234 and len(p) >= 4:
        tone = p[1] if isinstance(p[1], list) else [0, 0, 0, 0]
        op = 1 if p[3] else 0
        pp = [int(p[0]), int(tone[0]), int(tone[1]), int(tone[2]), int(tone[3]), int(p[2]), 0, 0, 0, 0]
    elif code == 235 and p: pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]
    elif code == 236 and len(p) >= 3:
        op = 1 if (len(p) > 3 and p[3]) else 0
        pp = [{'none': 0, 'rain': 1, 'storm': 2, 'snow': 3}.get(p[0], 0),
              int(p[1]), int(p[2]), 0, 0, 0, 0, 0, 0, 0]
    elif code == 123 and len(p) >= 2:
        pp = ['ABCD'.index(p[0]) if p[0] in 'ABCD' else 0, int(p[1]), 0, 0, 0, 0, 0, 0, 0, 0]
    elif code == 124 and len(p) >= 1:
        pp = [int(p[0]), int(p[1]) if len(p) > 1 else 0, 0, 0, 0, 0, 0, 0, 0, 0]
    elif code == 125 and len(p) >= 3:
        pp = [int(p[0]), int(p[1]), int(p[2]) if not isinstance(p[2], str) else 0, 0, 0, 0, 0, 0, 0, 0]
    elif code == 135 and p: pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]
    elif code in (115, 118, 119, 221, 222): pp = [0] * 10
    elif code == 301 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3])] + [0] * 6
    elif code == 302 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3])] + [0] * 6
    elif code == 605 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2])] + [0] * 7
    elif code == 303 and len(p) >= 2: pp = [int(p[0]), int(p[1])] + [0] * 8
    elif code == 314 and len(p) >= 2: pp = [int(p[0]), int(p[1])] + [0] * 8
    elif code in (315, 316) and len(p) >= 6:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]) if not isinstance(p[4], str) else 0, int(p[5])] + [0] * 4
    elif code == 317 and len(p) >= 6:

        pp = [int(p[0]), int(p[1]), int(p[3]), int(p[4]), int(p[5]) if not isinstance(p[5], str) else 0, int(p[2])] + [0] * 4
    elif code == 326 and len(p) >= 5:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]) if not isinstance(p[4], str) else 0, 0] + [0] * 4
    elif code == 318 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3])] + [0] * 6
    elif code == 319 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2])] + [0] * 7
    elif code == 320 and len(p) >= 2: pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]; s = str(p[1])
    elif code == 321 and len(p) >= 2: pp = [int(p[0]), int(p[1])] + [0] * 8
    elif code == 324 and len(p) >= 2: pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]; s = str(p[1])
    elif code == 325 and len(p) >= 2: pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]; s = str(p[1])
    elif code in (331, 332) and len(p) >= 5:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]) if not isinstance(p[3], str) else 0, int(p[4])] + [0] * 5
    elif code == 333 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2])] + [0] * 7
    elif code == 334 and p: pp = [int(p[0])] + [0] * 9
    elif code == 335 and p: pp = [int(p[0])] + [0] * 9
    elif code == 336 and len(p) >= 2: pp = [int(p[0]), int(p[1])] + [0] * 8
    elif code == 337 and len(p) >= 3: pp = [int(p[0]), int(p[1]), 1 if p[2] else 0] + [0] * 7
    elif code == 339 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3])] + [0] * 6
    elif code == 342 and len(p) >= 4:
        pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]) if not isinstance(p[3], str) else 0] + [0] * 6
    elif code in (340, 351, 352, 353, 354, 206, 217): pp = [0] * 10
    elif code == 103 and len(p) >= 3: pp = [int(p[0]), int(p[1]), int(p[2])] + [0] * 7
    elif code == 104 and len(p) >= 2: pp = [int(p[0]), int(p[1])] + [0] * 8
    elif code == 105: pp = [0] * 10
    elif code in (241, 245, 249) and p and isinstance(p[0], dict): s = str(p[0].get('name', ''))
    elif code in (242, 246) and p: pp = [int(p[0])] + [0] * 9
    elif code in (243, 244, 251): pp = [0] * 10
    elif code in (133, 139) and p and isinstance(p[0], dict): s = str(p[0].get('name', ''))
    elif code in (134, 136, 137) and p: pp = [int(p[0])] + [0] * 9
    elif code == 138 and p and isinstance(p[0], list):
        pp = [int(p[0][0]), int(p[0][1]), int(p[0][2]), int(p[0][3])] + [0] * 6
    elif code == 140 and len(p) >= 2:
        pp = [int(p[0]), 0, 0, 0, 0, 0, 0, 0, 0, 0]
        if isinstance(p[1], dict): s = str(p[1].get('name', ''))
    elif code == 281 and p: pp = [int(p[0])] + [0] * 9
    elif code == 282 and p: pp = [int(p[0])] + [0] * 9
    elif code == 283 and len(p) >= 2: s = str(p[0]) + '\n' + str(p[1])
    elif code == 284 and len(p) >= 5:
        pp = [int(p[1]), int(p[2]), int(p[3]), int(p[4])] + [0] * 6; s = str(p[0])
    elif code == 285 and len(p) >= 4: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3])] + [0] * 6
    elif code == 202 and len(p) >= 5: pp = [int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4])] + [0] * 5
    elif code == 261 and p: s = str(p[0])
    elif code == 224 and len(p) >= 2:
        col = p[0] if isinstance(p[0], list) else [0, 0, 0, 0]
        op = 1 if (len(p) > 2 and p[2]) else 0
        pp = [int(col[0]), int(col[1]), int(col[2]), int(col[3]), int(p[1]), 0, 0, 0, 0, 0]
    elif code == 225 and len(p) >= 3:
        op = 1 if (len(p) > 3 and p[3]) else 0
        pp = [int(p[0]), int(p[1]), int(p[2]), 0, 0, 0, 0, 0, 0, 0]
    pp = (list(pp) + [0] * 10)[:10]
    return {'code': code, 'indent': c.get('indent', 0), 'jump': jump,
            'op': op, 'p': pp, 's': s}

def refsim(cmds, init_sw=None, init_var=None, branch=0, sel=0, ce=None, init_party=(), init_hp=None,
           ev_id=0, on_map=0, troop_n=8, init_ehp=None):
    sw = dict(init_sw or {}); var = dict(init_var or {})
    bv = {}; text = []; trace = []; waits = 0; unknown = 0
    party = list(init_party); tint = [0, 0, 0, 0]; tint_f = 0
    rng = [1]
    last_se = ''; se_n = 0; routes = []; astate = {}
    inv = {}; hp = dict(init_hp or {}); mp = {}
    anims = []; chpos = {}
    refresh = [0]; appearance = {}; plug = []
    transparent = [0]; followers = [1]; transfer = {}; balloons = []
    erased = []; scrolls = []
    selfsw = {}; timer = [0, 0]; gold = [0]; bbgm = ['']; menu = [0]
    fade = [0]; flash = [[0, 0, 0, 0], 0]; shake = [[0, 0, 0]]
    pics = {}; weather = [[0, 0, 0]]
    exp = {}; level = {}; apram = {}; skills = {}; equip = {}
    anames = {}; aclass = {}; anick = {}; aprof = {}; tp = {}
    ehp = dict(init_ehp or {}); emp = {}; etp = {}; estate = {}; eappear = {}; etransform = {}
    battle = {}; shop = []; shop_only = [0]
    nameinput = [0, 0]; scene = [0]; numinput = [0, 0, 0]; itemchoice = [0, 0]
    audiolog = {}; audioflag = {}; sysnames = {}; sysflag = {'save': 1, 'encounter': 1, 'formation': 1, 'namedisp': 1}
    wtone = [0, 0, 0, 0]; vehbgm = [0, '']; battlebacks = []
    parallax = ['', 0, 0, 0, 0]; locinfo = [0, 0, 0, 0]; vehloc = [0] * 5
    vehin = [0]; gather = [0]; movie = ['']
    force = {}
    stack = []; pc, wait, n = 0, 0, len(cmds)
    cur, curlen = cmds, n
    steps = 0
    while steps < 100000:
        steps += 1
        while pc >= len(cur) and stack:
            cur, pc = stack.pop()
        if pc >= len(cur):
            break
        if wait > 0: wait -= 1; waits += 1; continue
        c = cur[pc]; trace.append(pc | (len(stack) << 24)); code = c['code']
        if code in (0, 108, 112, 404, 412, 604): pc += 1
        elif code == 101: text = []; pc += 1
        elif code in (401, 405):
            if c['s'] is not None: text.append(c['s'])
            pc += 1
        elif code == 102: pc += 1
        elif code == 402: pc = pc + 1 if sel == c['p'][0] else c['jump']
        elif code == 403: pc = pc + 1 if sel < 0 else c['jump']
        elif code == 111:
            r = False
            if c['op'] == 0: r = sw.get(c['p'][0], 0) == (c['p'][1] == 0)
            elif c['op'] == 8: r = inv.get((126, c['p'][0]), 0) > 0
            elif c['op'] == 4:
                a, sub, n = c['p'][0], c['p'][1], c['p'][2]
                if sub == 0: r = a in party
                elif sub == 1: r = c.get('s') is not None and anames.get(a, '') == c['s']
                elif sub == 2: r = aclass.get(a, 0) == n
                elif sub == 3: r = skills.get((a, n), 0) == 1
                elif sub in (4, 5): r = any(k[0] == a and v == n for k, v in equip.items())
                elif sub == 6: r = astate.get((a, n), 0) == 1
            elif c['op'] == 5:
                e, sub, n = c['p'][0], c['p'][1], c['p'][2]
                if e < 0 or e >= troop_n: r = False
                elif sub == 0: r = ehp.get(e, 0) > 0
                elif sub == 1: r = estate.get((e, n), 0) == 1
            elif c['op'] == 1:
                v1 = var.get(c['p'][0], 0); v2 = c['p'][2]
                if c['p'][1] == 1: v2 = var.get(v2, 0)
                elif c['p'][1] != 0: unknown += 1
                cmp = c['p'][3]
                r = [v1 == v2, v1 >= v2, v1 <= v2, v1 > v2, v1 < v2, v1 != v2][cmp] if cmp <= 5 else False
            else: unknown += 1
            bv[c['indent']] = r
            pc = pc + 1 if r else c['jump']
        elif code == 411: pc = c['jump'] if bv.get(c['indent']) else pc + 1
        elif code in (113, 413): pc = c['jump']
        elif code == 601: pc = pc + 1 if branch == 0 else c['jump']
        elif code == 602: pc = pc + 1 if branch == 1 else c['jump']
        elif code == 603: pc = pc + 1 if branch == 2 else c['jump']
        elif code == 121:
            for i in range(max(0, c['p'][0]), min(3601, c['p'][1] + 1)): sw[i] = 1 if c['p'][2] == 0 else 0
            pc += 1
        elif code == 122:
            rhs = c['p'][3]
            if c['p'][2] == 2:
                lo, span = rhs, c['p'][4] - rhs + 1
                if span < 1: span = 1
                for i in range(max(0, c['p'][0]), min(451, c['p'][1] + 1)):
                    rng[0] = (rng[0] * 1664525 + 1013904223) & 0xFFFFFFFF
                    roll = lo + ((((rng[0] >> 16) * span) >> 16) % span)
                    if c['op'] == 0: var[i] = roll
                    elif c['op'] == 1: var[i] = var.get(i, 0) + roll
                    elif c['op'] == 2: var[i] = var.get(i, 0) - roll
                    elif c['op'] == 3: var[i] = var.get(i, 0) * roll
                    elif c['op'] == 4 and roll: var[i] = int(var.get(i, 0) / roll)
                    elif c['op'] == 5 and roll: var[i] = var.get(i, 0) % roll
                    else: unknown += 1
                pc += 1
                continue
            if c['p'][2] == 1: rhs = var.get(rhs, 0)
            elif c['p'][2] != 0: unknown += 1; pc += 1; continue
            for i in range(max(0, c['p'][0]), min(451, c['p'][1] + 1)):
                if c['op'] == 0: var[i] = rhs
                elif c['op'] == 1: var[i] = var.get(i, 0) + rhs
                elif c['op'] == 2: var[i] = var.get(i, 0) - rhs
                elif c['op'] == 3: var[i] = var.get(i, 0) * rhs
                elif c['op'] == 4 and rhs: var[i] = int(var.get(i, 0) / rhs)
                elif c['op'] == 5 and rhs: var[i] = var.get(i, 0) % rhs
                else: unknown += 1
            pc += 1
        elif code == 230: wait += c['p'][0]; pc += 1
        elif code == 117:
            tgt = (ce or {}).get(c['p'][0])
            if tgt is not None and len(stack) < 8:
                stack.append((cur, pc + 1)); cur, pc = tgt, 0
            else: unknown += 1; pc += 1
        elif code == 129:
            a, add = c['p'][0], c['p'][1] == 0
            if add:
                if a not in party and len(party) < 16: party.append(a)
            elif a in party: party.remove(a)
            pc += 1
        elif code == 205:
            routes.append((c['p'][0], c['p'][1], c['p'][2])); pc += 1
        elif code == 223:
            tint = c['p'][:4]; tint_f = c['p'][4]
            if c['op']: wait += c['p'][4]
            pc += 1
        elif code == 250:
            if c['s']: last_se, se_n = c['s'], se_n + 1
            pc += 1
        elif code == 313:
            sel0, idv, add, st = c['p'][0], c['p'][1], c['p'][2] == 0, c['p'][3]
            ids = []
            if sel0 == 0: ids = list(party) if idv == 0 else [idv]
            elif var.get(idv, 0) > 0: ids = [var[idv]]
            for a in ids:
                if 0 <= a < 64 and 0 <= st < 128: astate[(a, st)] = 1 if add else 0
            pc += 1
        elif code in (126, 127, 128):
            v = c['p'][3] if c['p'][2] == 0 else (var.get(c['p'][3], 0) if c['p'][2] == 1 else None)
            if v is None: unknown += 1; pc += 1; continue
            if c['op'] == 1: v = -v
            elif c['op'] != 0: unknown += 1; pc += 1; continue
            inv[(code, c['p'][0])] = inv.get((code, c['p'][0]), 0) + v
            pc += 1
        elif code in (311, 312):
            v = c['p'][4] if c['p'][3] == 0 else (var.get(c['p'][4], 0) if c['p'][3] == 1 else None)
            if v is None: unknown += 1; pc += 1; continue
            if c['p'][2] == 1: v = -v
            elif c['p'][2] != 0: unknown += 1; pc += 1; continue
            ids = []
            if c['p'][0] == 0: ids = list(party) if c['p'][1] == 0 else [c['p'][1]]
            elif var.get(c['p'][1], 0) > 0: ids = [var[c['p'][1]]]
            for a in ids:
                if not 0 <= a < 64: continue
                if code == 311:
                    if hp.get(a, 0) > 0:
                        if not c['p'][5] and hp[a] <= -v: v = 1 - hp[a]
                        hp[a] = hp.get(a, 0) + v
                else:
                    mp[a] = max(0, mp.get(a, 0) + v)
            pc += 1
        elif code == 212:
            anims.append((c['p'][0], c['p'][1], 0))
            if c['p'][2]: wait += c['p'][3] * 4 + 1
            pc += 1
        elif code == 203:
            ci = c['p'][0] + 2
            if c['p'][1] == 0: chpos[ci] = (c['p'][3], c['p'][4], chpos.get(ci, (0, 0, 0))[2])
            elif c['p'][1] == 1:
                chpos[ci] = (var.get(c['p'][3], 0), var.get(c['p'][4], 0), chpos.get(ci, (0, 0, 0))[2])
            elif c['p'][1] == 2:
                cj = c['p'][2] + 2
                a = chpos.get(ci, (0, 0, 0)); b = chpos.get(cj, (0, 0, 0))
                chpos[ci] = (b[0], b[1], a[2]); chpos[cj] = (a[0], a[1], b[2])
            else: unknown += 1
            if c['p'][5] > 0:
                x, y, _ = chpos.get(ci, (0, 0, 0)); chpos[ci] = (x, y, c['p'][5])
            pc += 1
        elif code in (355, 655):
            import re as _re
            s = c['s'] or ''
            handled = False
            if s in ('$gamePlayer.refresh();', '$gamePlayer.refresh()'):
                refresh[0] += 1; handled = True
            else:
                for pat, kind in [
                    (r"\$gameActors\.actor\((\d+)\)\.setCharacterImage\('([^']+)',\s*(\d+)\);?", 'ch'),
                    (r"\$gameActors\.actor\((\d+)\)\.setBattlerImage\('([^']+)'\);?", 'bt'),
                    (r"\$gameActors\.actor\((\d+)\)\.setFaceImage\('([^']+)',\s*(\d+)\);?", 'fc')]:
                    mm = _re.fullmatch(pat, s)
                    if mm and 0 <= int(mm.group(1)) < 64:
                        a = int(mm.group(1))
                        if kind == 'ch': appearance[(a, 'ch')] = (mm.group(2), int(mm.group(3)))
                        elif kind == 'bt': appearance[(a, 'bt')] = (mm.group(2), 0)
                        else: appearance[(a, 'fc')] = (mm.group(2), int(mm.group(3)))
                        handled = True
                        break
            if not handled: unknown += 1
            pc += 1
        elif code == 356:
            parts = (c['s'] or '').split(' ')
            plug.append((parts[0], ' '.join(parts[1:])))
            pc += 1
        elif code == 505: pc += 1
        elif code == 211: transparent[0] = 1 if c['p'][0] == 0 else 0; pc += 1
        elif code == 216: followers[0] = 1 if c['p'][0] == 0 else 0; refresh[0] += 1; pc += 1
        elif code == 322:
            a = c['p'][0]
            lines = (c['s'] or '').split('\n')
            if 0 <= a < 64 and len(lines) == 5:
                appearance[(a, 'ch')] = (lines[0], int(lines[1]))
                appearance[(a, 'fc')] = (lines[2], int(lines[3]))
                appearance[(a, 'bt')] = (lines[4], 0)
            else: unknown += 1
            refresh[0] += 1; pc += 1
        elif code == 201:
            if c['p'][0] == 0:
                transfer.update(map=c['p'][1], x=c['p'][2], y=c['p'][3])
            else:
                transfer.update(map=var.get(c['p'][1], 0), x=var.get(c['p'][2], 0), y=var.get(c['p'][3], 0))
            transfer.update(dir=c['p'][4], fade=c['p'][5], pending=1)
            pc += 1
        elif code == 213: balloons.append((c['p'][0], c['p'][1])); pc += 1
        elif code == 214:
            if on_map and ev_id > 0: erased.append(ev_id)
            pc += 1
        elif code == 204: scrolls.append((c['p'][0], c['p'][1], c['p'][2])); pc += 1
        elif code == 115: pc = len(cur)
        elif code == 118: pc += 1
        elif code == 119: pc = c['jump']
        elif code == 123:
            if ev_id > 0 and 0 <= c['p'][0] < 4: selfsw[(ev_id, c['p'][0])] = 1 if c['p'][1] == 0 else 0
            pc += 1
        elif code == 124:
            if c['p'][0] == 0: timer[:] = [1, c['p'][1] * 60]
            else: timer[0] = 0
            pc += 1
        elif code == 125:
            v = c['p'][2] if c['p'][1] == 0 else (var.get(c['p'][2], 0) if c['p'][1] == 1 else None)
            if v is None or c['p'][0] not in (0, 1): unknown += 1; pc += 1; continue
            gold[0] += -v if c['p'][0] == 1 else v
            pc += 1
        elif code == 132: bbgm[0] = c['s'] or ''; pc += 1
        elif code == 135: menu[0] = 1 if c['p'][0] == 0 else 0; pc += 1
        elif code == 221: fade[0] = -1; wait += 24; pc += 1
        elif code == 222: fade[0] = 1; wait += 24; pc += 1
        elif code == 224:
            flash[0] = c['p'][:4]; flash[1] = c['p'][4]
            if c['op']: wait += c['p'][4]
            pc += 1
        elif code == 225:
            shake[0] = c['p'][:3]
            if c['op']: wait += c['p'][2]
            pc += 1
        elif code == 231:
            i, xmode = c['p'][0], c['p'][2]
            x = c['p'][3] if xmode == 0 else var.get(c['p'][3], 0)
            y = c['p'][4] if xmode == 0 else var.get(c['p'][4], 0)
            pics[i] = {'used': 1, 'origin': c['p'][1], 'x': x, 'y': y,
                       'sx': c['p'][5], 'sy': c['p'][6], 'op': c['p'][7],
                       'blend': c['p'][8], 'rot': 0, 'tone': [0, 0, 0, 0],
                       'name': c['s'] or ''}
            pc += 1
        elif code == 232:
            i = c['p'][0]
            if i in pics and pics[i]['used']:
                xmode = c['p'][2]
                pics[i].update(origin=c['p'][1],
                               x=c['p'][3] if xmode == 0 else var.get(c['p'][3], 0),
                               y=c['p'][4] if xmode == 0 else var.get(c['p'][4], 0),
                               sx=c['p'][5], sy=c['p'][6], op=c['p'][7], blend=c['p'][8])
            if c['op']: wait += c['p'][9]
            pc += 1
        elif code == 233:
            if c['p'][0] in pics: pics[c['p'][0]]['rot'] = c['p'][1]
            pc += 1
        elif code == 234:
            if c['p'][0] in pics and pics[c['p'][0]]['used']:
                pics[c['p'][0]]['tone'] = c['p'][1:5]
            if c['op']: wait += c['p'][5]
            pc += 1
        elif code == 235:
            if c['p'][0] in pics: pics[c['p'][0]]['used'] = 0
            pc += 1
        elif code == 236:
            weather[0] = c['p'][:3]
            if c['op']: wait += c['p'][2]
            pc += 1
        elif code in (315, 316, 317, 326):
            sel, idv = c['p'][0], c['p'][1]
            v = c['p'][4] if c['p'][3] == 0 else (var.get(c['p'][4], 0) if c['p'][3] == 1 else None)
            if v is None or c['p'][2] not in (0, 1): unknown += 1; pc += 1; continue
            if c['p'][2] == 1: v = -v
            ids = []
            if sel == 0: ids = list(party) if idv == 0 else [idv]
            elif var.get(idv, 0) > 0: ids = [var[idv]]
            for a in ids:
                if not 0 <= a < 64: continue
                if code == 315: exp[a] = exp.get(a, 0) + v
                elif code == 316: level[a] = level.get(a, 0) + v
                elif code == 326: tp[a] = tp.get(a, 0) + v
                elif 0 <= c['p'][5] < 8:
                    apram[(a, c['p'][5])] = apram.get((a, c['p'][5]), 0) + v
                else: unknown += 1
            pc += 1
        elif code == 318:
            sel, idv = c['p'][0], c['p'][1]
            ids = []
            if sel == 0: ids = list(party) if idv == 0 else [idv]
            elif var.get(idv, 0) > 0: ids = [var[idv]]
            for a in ids:
                if 0 <= a < 64 and 0 <= c['p'][3] < 384:
                    skills[(a, c['p'][3])] = 1 if c['p'][2] == 0 else 0
            pc += 1
        elif code == 319:
            if 0 <= c['p'][0] < 64 and 0 <= c['p'][1] < 8:
                equip[(c['p'][0], c['p'][1])] = c['p'][2]
            pc += 1
        elif code == 320:
            if 0 <= c['p'][0] < 64: anames[c['p'][0]] = c['s'] or ''
            pc += 1
        elif code == 321:
            if 0 <= c['p'][0] < 64: aclass[c['p'][0]] = c['p'][1]
            pc += 1
        elif code == 324:
            if 0 <= c['p'][0] < 64: anick[c['p'][0]] = c['s'] or ''
            pc += 1
        elif code == 325:
            if 0 <= c['p'][0] < 64: aprof[c['p'][0]] = c['s'] or ''
            pc += 1
        elif code == 314:
            sel, idv = c['p'][0], c['p'][1]
            ids = []
            if sel == 0: ids = list(party) if idv == 0 else [idv]
            elif var.get(idv, 0) > 0: ids = [var[idv]]
            for a in ids:
                if 0 <= a < 64:
                    for k in list(astate):
                        if k[0] == a: del astate[k]
                    tp[a] = 0
            pc += 1
        elif code in (331, 332, 342):
            idx = c['p'][0]
            v = c['p'][3] if c['p'][2] == 0 else (var.get(c['p'][3], 0) if c['p'][2] == 1 else None)
            if v is None or c['p'][1] not in (0, 1): unknown += 1; pc += 1; continue
            if c['p'][1] == 1: v = -v
            for e in range(troop_n):
                if idx >= 0 and e != idx: continue
                if code == 331:
                    if ehp.get(e, 0) > 0:
                        if not c['p'][4] and ehp[e] <= -v: v = 1 - ehp[e]
                        ehp[e] = ehp.get(e, 0) + v
                elif code == 332: emp[e] = max(0, emp.get(e, 0) + v)
                else: etp[e] = etp.get(e, 0) + v
            pc += 1
        elif code == 333:
            for e in range(troop_n):
                if c['p'][0] >= 0 and e != c['p'][0]: continue
                if 0 <= c['p'][2] < 128: estate[(e, c['p'][2])] = 1 if c['p'][1] == 0 else 0
            pc += 1
        elif code == 334:
            for e in range(troop_n):
                if c['p'][0] >= 0 and e != c['p'][0]: continue
                for k in list(estate):
                    if k[0] == e: del estate[k]
            pc += 1
        elif code == 335:
            for e in range(troop_n):
                if c['p'][0] >= 0 and e != c['p'][0]: continue
                eappear[e] = 1
            pc += 1
        elif code == 336:
            for e in range(troop_n):
                if c['p'][0] >= 0 and e != c['p'][0]: continue
                etransform[e] = c['p'][1]
            pc += 1
        elif code == 337:
            anims.append((-100 - c['p'][0], c['p'][1], 1 if c['p'][2] else 0)); pc += 1
        elif code == 339:
            force.update(side=c['p'][0], idx=c['p'][1], skill=c['p'][2], target=c['p'][3], pending=1)
            pc += 1
        elif code == 340: battle['pending'] = 2; pc += 1
        elif code == 301:
            if c['p'][0] == 0: battle.update(troop=c['p'][1])
            elif c['p'][0] == 1: battle.update(troop=var.get(c['p'][1], 0))
            else: battle.update(troop=-1)
            battle.update(esc=c['p'][2], lose=c['p'][3], pending=1)
            pc += 1
        elif code == 302:
            shop.append((c['p'][0], c['p'][1], c['p'][2])); shop_only[0] = c['p'][3]
            k = pc + 1
            while k < len(cur) and cur[k]['code'] == 605 and len(shop) < 16:
                shop.append((cur[k]['p'][0], cur[k]['p'][1], cur[k]['p'][2])); k += 1
            pc = k
        elif code == 605: pc += 1
        elif code == 303: nameinput[:] = [c['p'][0], c['p'][1]]; pc += 1
        elif code in (351, 352, 353, 354): scene[0] = {351: 1, 352: 2, 353: 3, 354: 4}[code]; pc += 1
        elif code == 103: numinput[:] = c['p'][:3]; pc += 1
        elif code == 104: itemchoice[:] = c['p'][:2]; pc += 1
        elif code == 105: text = []; pc += 1
        elif code in (241, 245, 249):
            audiolog[c['code']] = c['s'] or ''; pc += 1
        elif code == 242: audioflag['bgm_fade'] = c['p'][0]; pc += 1
        elif code == 246: audioflag['bgs_fade'] = c['p'][0]; pc += 1
        elif code == 251: audioflag['se_stop'] = 1; pc += 1
        elif code == 243: audioflag['bgm_saved'] = 1; pc += 1
        elif code == 244: audioflag['bgm_replayed'] = 1; pc += 1
        elif code == 133: sysnames['victory'] = c['s'] or ''; pc += 1
        elif code == 139: sysnames['defeat'] = c['s'] or ''; pc += 1
        elif code == 134: sysflag['save'] = 1 if c['p'][0] else 0; pc += 1
        elif code == 136: sysflag['encounter'] = 1 if c['p'][0] else 0; pc += 1
        elif code == 137: sysflag['formation'] = 1 if c['p'][0] else 0; pc += 1
        elif code == 138: wtone[:] = c['p'][:4]; pc += 1
        elif code == 140: vehbgm[:] = [c['p'][0], c['s'] or '']; pc += 1
        elif code == 281: sysflag['namedisp'] = 1 if c['p'][0] == 0 else 0; pc += 1
        elif code == 282: sysflag['tileset'] = c['p'][0]; pc += 1
        elif code == 282: sysflag['tileset'] = c['p'][0]; pc += 1
        elif code == 283: battlebacks[:] = (c['s'] or '').split('\n'); pc += 1
        elif code == 284:
            parallax[:] = [c['s'] or '', c['p'][0], c['p'][1], c['p'][2], c['p'][3]]; pc += 1
        elif code == 285: locinfo[:] = c['p'][:4]; pc += 1
        elif code == 202: vehloc[:] = c['p'][:5]; pc += 1
        elif code == 206: vehin[0] = 0 if vehin[0] else 1; pc += 1
        elif code == 217: gather[0] = 1; pc += 1
        elif code == 261: movie[0] = c['s'] or ''; pc += 1
        else: unknown += 1; pc += 1

    return {'trace': trace, 'sw': sw, 'var': var, 'text': text,
            'waits': waits, 'unknown': unknown, 'party': party, 'tint': tint,
            'tint_f': tint_f, 'last_se': last_se, 'se_n': se_n,
            'routes': routes, 'astate': astate, 'inv': inv, 'hp': hp, 'mp': mp,
            'anims': anims, 'chpos': chpos, 'refresh': refresh[0],
            'appearance': appearance, 'plug': plug, 'transparent': transparent[0],
            'followers': followers[0], 'transfer': transfer, 'balloons': balloons,
            'erased': erased, 'scrolls': scrolls, 'selfsw': selfsw, 'timer': timer,
            'gold': gold[0], 'bbgm': bbgm[0], 'menu': menu[0], 'fade': fade[0],
            'flash': flash, 'shake': shake[0], 'pics': pics, 'weather': weather[0],
            'exp': exp, 'level': level, 'apram': apram, 'skills': skills, 'equip': equip,
            'anames': anames, 'aclass': aclass, 'anick': anick, 'aprof': aprof, 'tp': tp,
            'ehp': ehp, 'emp': emp, 'etp': etp, 'estate': estate,
            'eappear': eappear, 'etransform': etransform, 'battle': battle,
            'shop': shop, 'shop_only': shop_only[0], 'nameinput': nameinput, 'scene': scene[0],
            'numinput': numinput, 'itemchoice': itemchoice, 'audiolog': audiolog,
            'audioflag': audioflag, 'sysnames': sysnames, 'sysflag': sysflag,
            'wtone': wtone, 'vehbgm': vehbgm, 'battlebacks': battlebacks,
            'parallax': parallax, 'locinfo': locinfo, 'vehloc': vehloc,
            'vehin': vehin[0], 'gather': gather[0], 'movie': movie[0], 'force': force}

def main():
    game = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else 'tests')
    out.mkdir(parents=True, exist_ok=True)
    data = game / 'data'
    def load(n): return json.loads((data / n).read_text(encoding='utf-8'))
    jumps = json.loads(pathlib.Path('converted/baked/jumps.json').read_text())['jumps']


    cands = []
    for f in sorted(data.glob('Map[0-9]*.json')):
        d = load(f.name)
        for ev in filter(None, d.get('events', []) or []):
            for pi, pg in enumerate(ev.get('pages', [])):
                cands.append((f'{f.name}/ev{ev["id"]}/pg{pi}', pg.get('list', [])))
    for ce in filter(None, load('CommonEvents.json') or []):
        cands.append((f'CommonEvents/{ce.get("id")}', ce.get('list', [])))
    for tr in filter(None, load('Troops.json') or []):
        for pi, pg in enumerate(tr.get('pages', [])):
            cands.append((f'Troops/{tr.get("id")}/pg{pi}', pg.get('list', [])))

    def codeset(lst): return {c.get('code') for c in lst if isinstance(c, dict)}
    bykey = dict(cands)

    t1 = next(k for k, l in cands if 1 <= len(l) <= 8 and codeset(l) <= {121, 0})

    t2 = next(k for k, l in cands if 5 <= len(l) <= 14 and 111 in codeset(l) and 411 in codeset(l)
              and codeset(l) <= {111, 411, 412, 121, 0})

    t3 = next(k for k, l in cands if 102 in codeset(l) and 113 in codeset(l) and len(l) < 120)

    t4 = next(k for k, l in cands if 601 in codeset(l) and 604 in codeset(l) and len(l) < 60)

    ce_data = {ce.get('id'): ce.get('list', []) for ce in filter(None, load('CommonEvents.json') or [])}
    def ce_ok(cid):
        lst = ce_data.get(cid, [])
        return bool(lst) and len(lst) < 40 and 117 not in {
            d.get('code') for d in lst if isinstance(d, dict)}
    def uses_small_ce(lst):
        return any(isinstance(c, dict) and c.get('code') == 117
                   and ce_ok(c.get('parameters', [0])[0]) for c in lst)
    t5 = next(k for k, l in cands if 117 in codeset(l) and len(l) < 40 and uses_small_ce(l))
    t5_ce = next(c.get('parameters', [0])[0] for c in bykey[t5]
                 if isinstance(c, dict) and c.get('code') == 117)

    t6 = next(k for k, l in cands if 126 in codeset(l) and (311 in codeset(l) or 312 in codeset(l))
              and len(l) < 60)

    t7 = next(k for k, l in cands if 203 in codeset(l) and 212 in codeset(l) and len(l) < 60)

    def has_esc(lst, *subs):
        for c in lst:
            if isinstance(c, dict) and c.get('code') in (401, 405) and c.get('parameters'):
                s = str(c['parameters'][0])
                if all(x in s for x in subs):
                    return True
        return False
    t8 = next(k for k, l in cands if 101 in codeset(l) and has_esc(l, '\\c[', '\\N[') and len(l) < 60)

    t9 = next(k for k, l in cands
              if any(isinstance(c, dict) and c.get('code') == 355 and 'setCharacterImage' in str(c.get('parameters', [''])[0])
                     for c in l) and len(l) < 200)

    def has_plug(lst):
        for c in lst:
            if isinstance(c, dict) and c.get('code') == 356 and c.get('parameters'):
                w = str(c['parameters'][0]).split(' ')[0]
                if w in ('GabText', 'ShowGab', 'Light'):
                    return True
        return False
    t10 = next(k for k, l in cands if has_plug(l) and len(l) < 60)

    t11 = next(k for k, l in cands if 211 in codeset(l) and 216 in codeset(l) and len(l) < 40)

    t12 = next(k for k, l in cands if 322 in codeset(l) and len(l) < 40)


    t13 = next(k for k, l in cands if 201 in codeset(l) and len(l) < 80)
    t13_ev = 0


    t14 = next(k for k, l in cands if 204 in codeset(l) and len(l) < 60)


    def terminates(key):
        lst = bykey[key]
        n = len(lst)
        jm = jumps.get(key, {})
        cmds = [to_cmd(c, int(jm.get(str(i), n))) for i, c in enumerate(lst)]
        eid = int(key.split('/ev')[1].split('/')[0]) if '/ev' in key else 0
        e = refsim(cmds, {}, {}, 0, 0, {}, ev_id=eid, on_map=1)
        return len(e['trace']) < 50000
    t15 = next(k for k, l in cands if 119 in codeset(l) and len(l) < 100 and terminates(k))

    t16 = next(k for k, l in cands if 224 in codeset(l) and len(l) < 60)
    t16b = next(k for k, l in cands if 225 in codeset(l) and len(l) < 60)
    t16c = 'Map002.json/ev1/pg2'

    t17 = next(k for k, l in cands if 231 in codeset(l) and 235 in codeset(l) and len(l) < 120)
    t17b = next(k for k, l in cands if 236 in codeset(l) and len(l) < 10)


    t18 = next(k for k, l in cands if 123 in codeset(l) and len(l) < 40)
    t18b = next(k for k, l in cands if 124 in codeset(l) and len(l) < 20)

    t19 = next(k for k, l in cands if 319 in codeset(l) and len(l) < 60)
    t20 = next(k for k, l in cands if 301 in codeset(l) and len(l) < 60)
    t21 = next(k for k, l in cands if 333 in codeset(l) and len(l) < 100)
    t21b = next(k for k, l in cands if 337 in codeset(l) and len(l) < 100)
    t22 = next(k for k, l in cands if 318 in codeset(l) and 320 in codeset(l) and len(l) < 100)
    t23 = next(k for k, l in cands if 353 in codeset(l) and len(l) < 60)
    t24 = next(k for k, l in cands if 249 in codeset(l) and len(l) < 80)
    t24b = next(k for k, l in cands if 251 in codeset(l) and len(l) < 60)
    t24c = next(k for k, l in cands if 245 in codeset(l))
    t25 = next(k for k, l in cands if 283 in codeset(l) and len(l) < 80)
    t25b = next(k for k, l in cands if 282 in codeset(l) and len(l) < 20)
    t25c = next(k for k, l in cands if 335 in codeset(l) and len(l) < 100)

    tests = [
        {'name': 'T1_switches', 'key': t1, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T2_conditional', 'key': t2, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T2b_conditional_true', 'key': t2, 'init_sw': 'AUTO_TRUE', 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T3_choices', 'key': t3, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T3b_choice_cancel', 'key': t3, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': -1},
        {'name': 'T4_battle_win', 'key': t4, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T4b_battle_lose', 'key': t4, 'init_sw': {}, 'init_var': {}, 'branch': 2, 'sel': 0},
        {'name': 'T5_common', 'key': t5, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'ce': {t5_ce: ce_data[t5_ce]}},
        {'name': 'T6_items_hp', 'key': t6, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'init_hp': 'AUTO'},
        {'name': 'T7_locate_anim', 'key': t7, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T8_escapes', 'key': t8, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T9_costume', 'key': t9, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T10_plugin', 'key': t10, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T11_presence', 'key': t11, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T12_graphic', 'key': t12, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T13_transfer', 'key': t13, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'ev_id': t13_ev, 'on_map': 1},
        {'name': 'T14_scroll', 'key': t14, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T15_flow', 'key': t15, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'ev_id': int(t15.split('/ev')[1].split('/')[0]) if '/ev' in t15 else 0, 'on_map': 1},
        {'name': 'T16_flash', 'key': t16, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T16b_shake', 'key': t16b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T16c_fade', 'key': t16c, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T17_pictures', 'key': t17, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T17b_weather', 'key': t17b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T18_economy', 'key': t18, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'ev_id': int(t18.split('/ev')[1].split('/')[0]) if '/ev' in t18 else 0, 'on_map': 1},
        {'name': 'T18b_timer', 'key': t18b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T19_equip', 'key': t19, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T20_battle', 'key': t20, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T21_enemies', 'key': t21, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'troop': 8, 'init_ehp': 'AUTO'},
        {'name': 'T21b_enemyanim', 'key': t21b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'troop': 8, 'init_ehp': 'AUTO'},
        {'name': 'T22_actoradmin', 'key': t22, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T23_gameover', 'key': t23, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T24_me', 'key': t24, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T24b_stopse', 'key': t24b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T24c_bgs', 'key': t24c, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T25_battleback', 'key': t25, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T25b_tileset', 'key': t25b, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0},
        {'name': 'T25c_appear', 'key': t25c, 'init_sw': {}, 'init_var': {}, 'branch': 0, 'sel': 0,
         'troop': 8},
        {'name': 'T26_rottenmeat', 'key': 'Map001.json/ev37/pg0', 'init_sw': {}, 'init_var': {},
         'branch': 0, 'sel': 0},
        {'name': 'T26b_leaveit', 'key': 'Map001.json/ev37/pg0', 'init_sw': {}, 'init_var': {},
         'branch': 0, 'sel': 1},
        {'name': 'T27_golemrite', 'key': 'Troops/44/pg4', 'init_sw': {215: 1, 2766: 1}, 'init_var': {14: 31}, 'branch': 0, 'sel': 0,
         'troop': 6},
    ]
    actors = load('Actors.json')
    actnames = [next((a['name'] for a in actors if a and a['id'] == i), '') for i in range(1, 41)]
    currency = str(load('System.json').get('currencyUnit', ''))
    H = ['/* AUTO-GENERATED by tools/emit_interp_test.py — do not hand-edit. */',
         '#include "../runtime/interp.h"', '']
    exp = {}
    for t in tests:
        lst = bykey[t['key']]
        n = len(lst)
        jm = jumps.get(t['key'], {})
        cmds = [to_cmd(c, int(jm.get(str(i), n))) for i, c in enumerate(lst)]

        ce_vecs = {}
        for cid, celist in (t.get('ce') or {}).items():
            cjm = jumps.get(f'CommonEvents/{cid}', {})
            ce_vecs[cid] = [to_cmd(c, int(cjm.get(str(i), len(celist))))
                            for i, c in enumerate(celist)]
        init_sw = dict(t['init_sw']) if isinstance(t['init_sw'], dict) else {}
        if t['init_sw'] == 'AUTO_TRUE':

            c111 = next(c for c in cmds if c['code'] == 111 and c['op'] == 0)
            init_sw = {c111['p'][0]: 1 if c111['p'][1] == 0 else 0}
        init_hp = {}
        if isinstance(t.get('init_hp'), dict):
            init_hp = dict(t['init_hp'])
        elif t.get('init_hp') == 'AUTO':

            c311 = next((c for c in cmds if c['code'] == 311), None)
            if c311 and c311['p'][0] == 0 and c311['p'][1] != 0:
                init_hp = {c311['p'][1]: 50}
        init_ehp = {}
        if t.get('init_ehp') == 'AUTO':
            idxs = set()
            for c in cmds:
                if c['code'] in (331, 332, 333, 334, 335, 336, 337, 342):
                    if c['p'][0] < 0:
                        idxs.update(range(8))
                    else:
                        idxs.add(c['p'][0])
            init_ehp = {e: 100 for e in idxs if 0 <= e < 16}
        e = refsim(cmds, init_sw, dict(t['init_var']), t['branch'], t['sel'],
                   {cid: v for cid, v in ce_vecs.items()}, init_hp=init_hp,
                   ev_id=t.get('ev_id', 0), on_map=t.get('on_map', 0),
                   troop_n=t.get('troop', 0), init_ehp=init_ehp)
        exp[t['name']] = e
        H.append(f'static const FhCmd {t["name"]}_list[] = {{')
        for c in cmds:
            s = 'NULL' if c['s'] is None else cstr(c['s'])
            H.append(f'  {{{c["code"]},{c["indent"]},{c["jump"]},{c["op"]},'
                     f'{{{c["p"][0]},{c["p"][1]},{c["p"][2]},{c["p"][3]},{c["p"][4]},{c["p"][5]},{c["p"][6]},{c["p"][7]},{c["p"][8]},{c["p"][9]}}},{s}}},')
        H.append('};')
        for cid, celist in ce_vecs.items():
            H.append(f'static const FhCmd {t["name"]}_ce{cid}[] = {{')
            for c in celist:
                s = 'NULL' if c['s'] is None else cstr(c['s'])
                H.append(f'  {{{c["code"]},{c["indent"]},{c["jump"]},{c["op"]},'
                         f'{{{c["p"][0]},{c["p"][1]},{c["p"][2]},{c["p"][3]},{c["p"][4]},{c["p"][5]},{c["p"][6]},{c["p"][7]},{c["p"][8]},{c["p"][9]}}},{s}}},')
            H.append('};')
        if ce_vecs:
            ids = sorted(ce_vecs)
            H.append(f'static const int {t["name"]}_ce_ids[] = {{{",".join(map(str, ids))}}};')
            H.append(f'static const FhCmd *{t["name"]}_ce_lists[] = '
                     f'{{{",".join(f"{t["name"]}_ce{cid}" for cid in ids)}}};')
            H.append(f'static const int {t["name"]}_ce_lens[] = '
                     f'{{{",".join(str(len(ce_vecs[c])) for c in ids)}}};')
            H.append(f'static const int {t["name"]}_ce_n = {len(ids)};')
        else:
            H.append(f'static const int *{t["name"]}_ce_ids = 0;')
            H.append(f'static const FhCmd **{t["name"]}_ce_lists = 0;')
            H.append(f'static const int *{t["name"]}_ce_lens = 0;')
            H.append(f'static const int {t["name"]}_ce_n = 0;')
        H.append(f'static const int {t["name"]}_trace[] = {{{",".join(map(str, e["trace"]))}}};')
        switems = sorted(e['sw'].items())
        H.append(f'static const int {t["name"]}_sw[][2] = {{{",".join(f"{{{k},{v}}}" for k, v in switems) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_swn = {len(switems)};')
        varitems = sorted(e['var'].items())
        H.append(f'static const int {t["name"]}_var[][2] = {{{",".join(f"{{{k},{v}}}" for k, v in varitems) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_varn = {len(varitems)};')
        H.append(f'static const char *{t["name"]}_text = {cstr("".join(l + chr(10) for l in e["text"]))};')
        H.append(f'static const int {t["name"]}_waits = {e["waits"]};')
        H.append(f'static const int {t["name"]}_unknown = {e["unknown"]};')
        H.append(f'static const int {t["name"]}_init_branch = {t["branch"]};')
        H.append(f'static const int {t["name"]}_init_sel = {t["sel"]};')
        H.append(f'static const int {t["name"]}_init_swn = {len(init_sw)};')
        H.append(f'static const int {t["name"]}_init_sw[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(init_sw.items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_party[] = {{{",".join(map(str, e["party"]))}}};')
        H.append(f'static const int {t["name"]}_partyn = {len(e["party"])};')
        H.append(f'static const int {t["name"]}_tint[] = '
                 f'{{{e["tint"][0]},{e["tint"][1]},{e["tint"][2]},{e["tint"][3]},{e["tint_f"]}}};')
        H.append(f'static const char *{t["name"]}_se = {cstr(e["last_se"])};')
        H.append(f'static const int {t["name"]}_sen = {e["se_n"]};')
        H.append(f'static const int {t["name"]}_routes[][3] = '
                 f'{{{",".join(f"{{{a},{b},{c}}}" for a, b, c in e["routes"]) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_routen = {len(e["routes"])};')
        H.append(f'static const int {t["name"]}_astate[][3] = '
                 f'{{{",".join(f"{{{a},{s},{v}}}" for (a, s), v in sorted(e["astate"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_astaten = {len(e["astate"])};')
        H.append(f'static const int {t["name"]}_init_hp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(init_hp.items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_init_hpn = {len(init_hp)};')
        H.append(f'static const int {t["name"]}_inv[][3] = '
                 f'{{{",".join(f"{{{c},{i},{v}}}" for (c, i), v in sorted(e["inv"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_invn = {len(e["inv"])};')
        H.append(f'static const int {t["name"]}_hp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["hp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_hpn = {len(e["hp"])};')
        H.append(f'static const int {t["name"]}_mp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["mp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_mpn = {len(e["mp"])};')
        H.append(f'static const int {t["name"]}_anims[][3] = '
                 f'{{{",".join(f"{{{a},{b},{m}}}" for a, b, m in e["anims"]) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_animsn = {len(e["anims"])};')
        H.append(f'static const int {t["name"]}_chpos[][4] = '
                 f'{{{",".join(f"{{{c},{x},{y},{d}}}" for c, (x, y, d) in sorted(e["chpos"].items())) or "{0,0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_chposn = {len(e["chpos"])};')
        H.append(f'static const int {t["name"]}_refresh = {e["refresh"]};')
        H.append(f'static const char *{t["name"]}_chname = {cstr(chr(10).join(f"{a}:{k}={v[0]},{v[1]}" for (a, k), v in sorted(e["appearance"].items())))};')
        H.append(f'static const char *{t["name"]}_plug = {cstr(chr(10).join(f"{n} {a}".rstrip() for n, a in e["plug"]))};')
        H.append(f'static const int {t["name"]}_progn = {len(e["plug"])};')

        _vars = [0] * 451
        for k, v in e['var'].items():
            if 0 <= k < 451:
                _vars[k] = v
        H.append(f'static const char *{t["name"]}_dec = '
                 f'{cstr(py_decode(chr(10).join(e["text"]) + (chr(10) if e["text"] else ""), _vars, actnames, e["party"], currency))};')
        H.append(f'static const int {t["name"]}_ev = {t.get("ev_id", 0)};')
        H.append(f'static const int {t["name"]}_onmap = {t.get("on_map", 0)};')
        H.append(f'static const int {t["name"]}_transp = {e["transparent"]};')
        H.append(f'static const int {t["name"]}_foll = {e["followers"]};')
        tr = e['transfer']
        H.append(f'static const int {t["name"]}_transfer[] = '
                 f'{{{tr.get("map", 0)},{tr.get("x", 0)},{tr.get("y", 0)},{tr.get("dir", 0)},{tr.get("fade", 0)},{tr.get("pending", 0)}}};')
        H.append(f'static const int {t["name"]}_balloons[][2] = '
                 f'{{{",".join(f"{{{a},{b}}}" for a, b in e["balloons"]) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_balloonn = {len(e["balloons"])};')
        H.append(f'static const int {t["name"]}_erased[] = {{{",".join(map(str, e["erased"])) or "-1"}}};')
        H.append(f'static const int {t["name"]}_erasedn = {len(e["erased"])};')
        H.append(f'static const int {t["name"]}_scrolls[][3] = '
                 f'{{{",".join(f"{{{a},{b},{c}}}" for a, b, c in e["scrolls"]) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_scrolln = {len(e["scrolls"])};')
        H.append(f'static const int {t["name"]}_selfsw[][2] = '
                 f'{{{",".join(f"{{{ev * 4 + l},{v}}}" for (ev, l), v in sorted(e["selfsw"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_selfswn = {len(e["selfsw"])};')
        H.append(f'static const int {t["name"]}_timer[] = {{{e["timer"][0]},{e["timer"][1]}}};')
        H.append(f'static const int {t["name"]}_gold = {e["gold"]};')
        H.append(f'static const char *{t["name"]}_bbgm = {cstr(e["bbgm"])};')
        H.append(f'static const int {t["name"]}_menu = {e["menu"]};')
        H.append(f'static const int {t["name"]}_fade = {e["fade"]};')
        H.append(f'static const int {t["name"]}_flash[] = '
                 f'{{{e["flash"][0][0]},{e["flash"][0][1]},{e["flash"][0][2]},{e["flash"][0][3]},{e["flash"][1]}}};')
        H.append(f'static const int {t["name"]}_shake[] = {{{e["shake"][0]},{e["shake"][1]},{e["shake"][2]}}};')
        piclines = []
        for pid, q in sorted(e['pics'].items()):
            piclines.append(f'{pid}:{q["used"]}:{q["origin"]}:{q["x"]}:{q["y"]}:{q["sx"]}:{q["sy"]}:'
                            f'{q["op"]}:{q["blend"]}:{q["rot"]}:'
                            f'{q["tone"][0]},{q["tone"][1]},{q["tone"][2]},{q["tone"][3]}:{q["name"]}')
        H.append(f'static const char *{t["name"]}_pics = {cstr(chr(10).join(piclines))};')
        H.append(f'static const int {t["name"]}_weather[] = {{{e["weather"][0]},{e["weather"][1]},{e["weather"][2]}}};')
        H.append(f'static const int {t["name"]}_troop = {t.get("troop", 0)};')
        H.append(f'static const int {t["name"]}_init_ehp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(init_ehp.items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_init_ehpn = {len(init_ehp)};')
        H.append(f'static const int {t["name"]}_exp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["exp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_expn = {len(e["exp"])};')
        H.append(f'static const int {t["name"]}_level[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["level"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_leveln = {len(e["level"])};')
        H.append(f'static const int {t["name"]}_apram[][3] = '
                 f'{{{",".join(f"{{{a},{p},{v}}}" for (a, p), v in sorted(e["apram"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_apramn = {len(e["apram"])};')
        H.append(f'static const int {t["name"]}_skills[][3] = '
                 f'{{{",".join(f"{{{a},{s},{v}}}" for (a, s), v in sorted(e["skills"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_skillsn = {len(e["skills"])};')
        H.append(f'static const int {t["name"]}_equip[][3] = '
                 f'{{{",".join(f"{{{a},{s},{v}}}" for (a, s), v in sorted(e["equip"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_equipn = {len(e["equip"])};')
        H.append(f'static const char *{t["name"]}_anames = {cstr(chr(10).join(f"{a}={n}" for a, n in sorted(e["anames"].items())))};')
        H.append(f'static const int {t["name"]}_aclass[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["aclass"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_aclassn = {len(e["aclass"])};')
        H.append(f'static const char *{t["name"]}_anick = {cstr(chr(10).join(f"{a}={n}" for a, n in sorted(e["anick"].items())))};')
        H.append(f'static const char *{t["name"]}_aprof = {cstr(chr(10).join(f"{a}={n}" for a, n in sorted(e["aprof"].items())))};')
        H.append(f'static const int {t["name"]}_ehp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["ehp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_ehpn = {len(e["ehp"])};')
        H.append(f'static const int {t["name"]}_emp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["emp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_empn = {len(e["emp"])};')
        H.append(f'static const int {t["name"]}_etp[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["etp"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_etpn = {len(e["etp"])};')
        H.append(f'static const int {t["name"]}_estate[][3] = '
                 f'{{{",".join(f"{{{a},{s},{v}}}" for (a, s), v in sorted(e["estate"].items())) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_estaten = {len(e["estate"])};')
        H.append(f'static const int {t["name"]}_eappear[] = {{{",".join(map(str, sorted(e["eappear"]))) or "-1"}}};')
        H.append(f'static const int {t["name"]}_eappearn = {len(e["eappear"])};')
        H.append(f'static const int {t["name"]}_etransform[][2] = '
                 f'{{{",".join(f"{{{k},{v}}}" for k, v in sorted(e["etransform"].items())) or "{0,0}"}}};')
        H.append(f'static const int {t["name"]}_etransformn = {len(e["etransform"])};')
        b = e['battle']
        H.append(f'static const int {t["name"]}_battle[] = '
                 f'{{{b.get("troop", 0)},{b.get("esc", 0)},{b.get("lose", 0)},{b.get("pending", 0)}}};')
        H.append(f'static const int {t["name"]}_shop[][3] = '
                 f'{{{",".join(f"{{{a},{b},{c}}}" for a, b, c in e["shop"]) or "{0,0,0}"}}};')
        H.append(f'static const int {t["name"]}_shopn = {len(e["shop"])};')
        H.append(f'static const int {t["name"]}_shop_only = {e["shop_only"]};')
        H.append(f'static const int {t["name"]}_nameinput[] = {{{e["nameinput"][0]},{e["nameinput"][1]}}};')
        H.append(f'static const int {t["name"]}_scene = {e["scene"]};')
        H.append(f'static const int {t["name"]}_numinput[] = {{{e["numinput"][0]},{e["numinput"][1]},{e["numinput"][2]}}};')
        H.append(f'static const int {t["name"]}_itemchoice[] = {{{e["itemchoice"][0]},{e["itemchoice"][1]}}};')
        H.append(f'static const char *{t["name"]}_audio = '
                 f'{cstr(chr(10).join(f"{k}={v}" for k, v in sorted(e["audiolog"].items())))};')
        af = e['audioflag']
        H.append(f'static const int {t["name"]}_aflag[] = '
                 f'{{{af.get("bgm_fade", 0)},{af.get("bgs_fade", 0)},{af.get("se_stop", 0)},{af.get("bgm_saved", 0)},{af.get("bgm_replayed", 0)}}};')
        H.append(f'static const char *{t["name"]}_sysnames = '
                 f'{cstr(chr(10).join(f"{k}={v}" for k, v in sorted(e["sysnames"].items())))};')
        sf = e['sysflag']
        H.append(f'static const int {t["name"]}_sysflag[] = '
                 f'{{{sf.get("save", 1)},{sf.get("encounter", 1)},{sf.get("formation", 1)},{sf.get("namedisp", 1)},{sf.get("tileset", 0)}}};')
        H.append(f'static const int {t["name"]}_wtone[] = {{{e["wtone"][0]},{e["wtone"][1]},{e["wtone"][2]},{e["wtone"][3]}}};')
        H.append(f'static const char *{t["name"]}_vehbgm = {cstr(str(e["vehbgm"][0]) + "=" + e["vehbgm"][1])};')
        H.append(f'static const char *{t["name"]}_battlebacks = {cstr(chr(10).join(e["battlebacks"]))};')
        px = e['parallax']
        H.append(f'static const char *{t["name"]}_parallax = {cstr(px[0])};')
        H.append(f'static const int {t["name"]}_parallaxn[] = {{{px[1]},{px[2]},{px[3]},{px[4]}}};')
        H.append(f'static const int {t["name"]}_locinfo[] = {{{e["locinfo"][0]},{e["locinfo"][1]},{e["locinfo"][2]},{e["locinfo"][3]}}};')
        H.append(f'static const int {t["name"]}_vehloc[] = {{{e["vehloc"][0]},{e["vehloc"][1]},{e["vehloc"][2]},{e["vehloc"][3]},{e["vehloc"][4]}}};')
        H.append(f'static const int {t["name"]}_vehin = {e["vehin"]};')
        H.append(f'static const int {t["name"]}_gather = {e["gather"]};')
        H.append(f'static const char *{t["name"]}_movie = {cstr(e["movie"])};')
        fz = e['force']
        H.append(f'static const int {t["name"]}_force[] = '
                 f'{{{fz.get("side", 0)},{fz.get("idx", 0)},{fz.get("skill", 0)},{fz.get("target", 0)},{fz.get("pending", 0)}}};')
        H.append('')
    (out / 'test_vec.h').write_text('\n'.join(H))

    with open(out / 'test_vec.h', 'a') as f:
        f.write('\nstatic const char *fh_actnames[] = {'
                + ','.join(cstr(n) for n in actnames) + '};\n')
        f.write(f'static const int fh_nactors = {len(actnames)};\n')
        f.write(f'static const char *fh_currency = {cstr(currency)};\n')
    for t in tests:
        print(f'{t["name"]}: from {t["key"]}, trace={len(exp[t["name"]]["trace"])} steps, '
              f'sw={len(exp[t["name"]]["sw"])} var={len(exp[t["name"]]["var"])} '
              f'textlines={len(exp[t["name"]]["text"])} waits={exp[t["name"]]["waits"]} '
              f'unknown={exp[t["name"]]["unknown"]} party={exp[t["name"]]["party"]} '
              f'se={exp[t["name"]]["se_n"]} routes={len(exp[t["name"]]["routes"])}')

if __name__ == '__main__':
    main()
