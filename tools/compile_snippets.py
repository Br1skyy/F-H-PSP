#!/usr/bin/env python3.12
"""Compile damage formulas to VM bytecode. Op numbers must match the VM in runtime/battle.c:
  Ins = u8 op, u8 arg, f64 imm (10 bytes)
  0 PUSH 1 ADD 2 SUB 3 MUL 4 DIV 5 MOD 6 LT 7 LE 8 GT 9 GE 10 EQ 11 NE
  12 AND 13 OR 14 A_STAT 15 B_STAT 16 VAR 17 SWITCH 18 MAX 19 MIN 20 FLOOR 21 RET
  Stat arg ids: atk0 def1 mat2 mdf3 agi4 luk5 hp6 mp7 mhp8 mmp9 level10"""
import json, sys, pathlib, struct, re

OPS = {n: i for i, n in enumerate(
    'PUSH ADD SUB MUL DIV MOD LT LE GT GE EQ NE AND OR A_STAT B_STAT VAR SWITCH MAX MIN FLOOR RET'.split())}
STATS = {n: i for i, n in enumerate(
    'atk def mat mdf agi luk hp mp mhp mmp level'.split())}

TOKEN = re.compile(r"\s*(?:(\d+\.?\d*)|(\$gameVariables|\$gameSwitches|Math|[ab](?![\w]))|(\.value|\.max|\.min|\.floor)|([A-Za-z_]\w*)|(\S))")

def tokenize(src):
    toks = []
    for m in TOKEN.finditer(src):
        num, base, meth, word, other = m.groups()
        if num is not None:
            toks.append(('NUM', float(num)))
        elif base in ('a', 'b'):
            toks.append(('AB', base))
        elif base in ('$gameVariables', '$gameSwitches', 'Math'):
            toks.append(('BASE', base))
        elif meth:
            toks.append(('METH', meth[1:]))
        elif word:
            toks.append(('WORD', word))
        elif other in '+-*/%()<>,.':
            toks.append(('OP', other))
        elif other in ('<', '>', '=', '!', '&', '|'):
            toks.append(('OP', other))
        else:
            raise ValueError(f'char {other!r} in {src!r}')

    out = []
    i = 0
    while i < len(toks):
        if toks[i] == ('OP', '<') and i + 1 < len(toks) and toks[i + 1] == ('OP', '='):
            out.append(('BINOP', '<=')); i += 2
        elif toks[i] == ('OP', '>') and i + 1 < len(toks) and toks[i + 1] == ('OP', '='):
            out.append(('BINOP', '>=')); i += 2
        elif toks[i] == ('OP', '=') and i + 2 < len(toks) and toks[i+1] == ('OP', '=') and toks[i+2][0] in ('OP', 'BINOP') and False:
            i += 1
        else:
            t = toks[i]
            if t[0] == 'OP' and t[1] in '+-*/%<>':
                out.append(('BINOP', t[1]))
            else:
                out.append(t)
            i += 1
    return out

BINOP = {'+': 'ADD', '-': 'SUB', '*': 'MUL', '/': 'DIV', '%': 'MOD',
         '<': 'LT', '<=': 'LE', '>': 'GT', '>=': 'GE'}
PREC = {'OR': 1, 'AND': 2, 'EQ': 3, 'NE': 3, 'LT': 4, 'LE': 4, 'GT': 4, 'GE': 4,
        'ADD': 5, 'SUB': 5, 'MUL': 6, 'DIV': 6, 'MOD': 6}

class Ctx:
    def __init__(self, src):
        self.toks = tokenize(src)
        self.pos = 0
        self.out = []
        self.src = src

    def peek(self):
        return self.toks[self.pos] if self.pos < len(self.toks) else (None, None)

    def next(self):
        t = self.peek()
        self.pos += 1
        return t

    def parse(self):
        self.expr(0)
        if self.pos != len(self.toks):
            raise ValueError(f'trailing tokens in {self.src!r}')
        self.out.append((OPS['RET'], 0, 0.0))

    def expr(self, minp):
        self.unary()
        while True:
            t, v = self.peek()
            if t == 'BINOP' and PREC.get(BINOP.get(v, ''), 0) >= minp and v in BINOP:
                op = BINOP[v]
                self.next()
                self.expr(PREC[op] + 1)
                self.out.append((OPS[op], 0, 0.0))
            else:
                break

    def unary(self):
        t, v = self.peek()
        if t == 'OP' and v == '-':
            self.next()
            self.unary()

            self.out.append((OPS['PUSH'], 0, -1.0))
            self.out.append((OPS['MUL'], 0, 0.0))
        elif t == 'OP' and v == '(':
            self.next()
            self.expr(0)
            assert self.next() == ('OP', ')'), f') expected in {self.src!r}'
        elif t == 'AB' and v in ('a', 'b'):
            who = v
            self.next()
            assert self.next() == ('OP', '.'), f'.stat expected in {self.src!r}'
            tt, ww = self.next()
            assert tt == 'WORD' and ww in STATS, f'stat {ww!r} in {self.src!r}'
            self.out.append((OPS['A_STAT' if who == 'a' else 'B_STAT'], STATS[ww], 0.0))
        elif t == 'NUM':
            self.next()
            self.out.append((OPS['PUSH'], 0, v))
        elif t == 'BASE':
            base = v
            self.next()
            assert self.next() == ('OP', '.'), f'.method expected in {self.src!r}'
            tt, ww = self.next()
            if base in ('$gameVariables', '$gameSwitches') and tt == 'METH' and ww == 'value':
                assert self.next() == ('OP', '(')

                arg = self.collect_call_arg()
                self.out.extend(arg)
                self.out.append((OPS['VAR' if base == '$gameVariables' else 'SWITCH'], 0, 0.0))
            elif base == 'Math' and tt == 'METH' and ww in ('max', 'min', 'floor'):
                assert self.next() == ('OP', '(')
                self.expr(0)
                if self.peek() == ('OP', ','):
                    self.next()
                    self.expr(0)
                    assert self.next() == ('OP', ')')
                    self.out.append((OPS[ww.upper()], 0, 0.0))
                else:
                    assert self.next() == ('OP', ')')
                    if ww == 'floor':
                        self.out.append((OPS['FLOOR'], 0, 0.0))
                    else:
                        raise ValueError(f'Math.{ww} needs 2 args in {self.src!r}')
            else:
                raise ValueError(f'call {base}.{ww} in {self.src!r}')
        else:
            raise ValueError(f'unexpected {t, v} in {self.src!r}')

    def collect_call_arg(self):
        save = len(self.out)
        self.expr(0)
        assert self.next() == ('OP', ')')
        return []

def host_eval(insns, a, b, var=lambda i: 0.0, sw=lambda i: 0.0):
    import math
    st = []
    opname = {v: k for k, v in OPS.items()}
    for op, arg, imm in insns:
        n = opname[op]
        if n == 'PUSH': st.append(imm)
        elif n == 'ADD': st.append(st.pop() + st.pop())
        elif n == 'SUB':
            r = st.pop(); st.append(st.pop() - r)
        elif n == 'MUL': st.append(st.pop() * st.pop())
        elif n == 'DIV':
            r = st.pop(); st.append(st.pop() / r)
        elif n == 'A_STAT': st.append(a[arg])
        elif n == 'B_STAT': st.append(b[arg])
        elif n == 'VAR': st.append(var(int(st.pop())))
        elif n == 'SWITCH': st.append(sw(int(st.pop())))
        elif n == 'MAX':
            r = st.pop(); st.append(max(st.pop(), r))
        elif n == 'MIN':
            r = st.pop(); st.append(min(st.pop(), r))
        elif n == 'FLOOR': st.append(math.floor(st.pop()))
        elif n == 'RET': return st[-1]
    raise ValueError('no RET')

def main():
    game = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else 'converted/code')
    out.mkdir(parents=True, exist_ok=True)
    data = game / 'data'
    kinds = [('skill', 'Skills.json'), ('item', 'Items.json'), ('weapon', 'Weapons.json'),
             ('armor', 'Armors.json'), ('state', 'States.json'), ('enemy', 'Enemies.json')]
    compiled, fallback, table = [], [], []
    for kind, fn in kinds:
        arr = json.loads((data / fn).read_text(encoding='utf-8'))
        for o in filter(None, arr or []):
            f = o.get('damage', {}).get('formula', '') if isinstance(o.get('damage'), dict) else ''
            if not f or f == '0':
                continue
            try:
                c = Ctx(f)
                c.parse()
                compiled.append((kind, o.get('id'), f, c.out))
            except Exception as e:
                fallback.append({'kind': kind, 'id': o.get('id'), 'formula': f, 'reason': str(e)[:120]})

    a = [10.0, 8.0, 6.0, 6.0, 12.0, 9.0, 200.0, 50.0, 200.0, 50.0, 5.0]
    b = [8.0, 10.0, 5.0, 5.0, 8.0, 7.0, 150.0, 30.0, 150.0, 30.0, 3.0]
    bad = 0
    for kind, i, f, ins in compiled:

        ref_src = re.sub(r'([ab])\.(\w+)', lambda m: f"{m.group(1)}_{m.group(2)}", f)
        ns = {f'{who}_{st}': (a if who == 'a' else b)[STATS[st]]
              for who in 'ab' for st in STATS}
        ref = eval(ref_src, {'__builtins__': {}}, ns)
        got = host_eval(ins, a, b)
        if abs(got - ref) > 1e-6:
            print(f'EVAL-DIFF {kind}{i} {f!r}: ref={ref} got={got}'); bad += 1

    blob = bytearray(struct.pack('<I', len(compiled)))
    for kind, i, f, ins in compiled:
        blob += struct.pack('<BHH', {'skill': 0, 'item': 1, 'weapon': 2, 'armor': 3, 'state': 4, 'enemy': 5}[kind], i or 0, len(ins))
        for op, arg, imm in ins:
            blob += struct.pack('<BBd', op, arg, imm)
        table.append({'kind': kind, 'id': i, 'formula': f, 'nins': len(ins)})
    (out / 'formulas.bin').write_bytes(bytes(blob))
    (out / 'formulas.json').write_text(json.dumps(table, indent=1))
    (out / 'fallback.json').write_text(json.dumps(fallback, indent=1))
    print(f'{len(compiled)} formulas compiled, {len(fallback)} fallback, eval diffs: {bad}, bin {len(blob)} bytes')
    for fb in fallback[:10]:
        print('  FALLBACK:', fb)
    if bad:
        sys.exit(1)

if __name__ == '__main__':
    main()
