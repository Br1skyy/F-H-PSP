#!/usr/bin/env python3.12
"""Reference map renderer — md build order: map renderer (after interpreter).

Ports Tilemap._drawNormalTile/_drawAutotile (rpg_core.js:5010/5038) to resolve
every map tile into source rects on the CONVERTED sheets, then blits a PNG.
This render is the golden master for the later C port (§3.5 loop):
C must produce identical quad lists.

Autotile recap (shapes are pre-baked in data, kind*48+shape):
  normal: set = A5->4 else 5+tid//256; sx=(tid//128%2*8+tid%8)*48; sy=(tid%256//8%16)*48
  auto: kind=(tid-2048)//48, shape=(tid-2048)%48; (set,bx,by) per A1-A4 rules
        (anim frame 0 = static golden); 4 quadrants from FLOOR/WALL/WATERFALL
        tables: src=((bx*2+qsx)*24,(by*2+qsy)*24,24x24) at 48px scale.

Usage:
    python3.12 tools/render_map.py "Fear & Hunger_WIN/www" --map Map030 --tile 24 --out render_out
"""
import json, sys, pathlib, struct
from PIL import Image

A1, A2, A3, A4, A5, TMAX = 2048, 2816, 4352, 5888, 1536, 8192
FULL = 48  # MV tile px

def kind_of(t): return (t - A1) // 48
def shape_of(t): return (t - A1) % 48
def is_a1(t): return A1 <= t < A2
def is_a2(t): return A2 <= t < A3
def is_a3(t): return A3 <= t < A4
def is_a4(t): return A4 <= t < TMAX
def is_a5(t): return A5 <= t < A1

def auto_cell(t):
    """Return (setNumber, bx, by, tableName) at animation frame 0."""
    kind = kind_of(t)
    tx, ty = kind % 8, kind // 8
    if is_a1(t):
        if kind == 0: return 0, 0, 0, 'FLOOR'
        if kind == 1: return 0, 0, 3, 'FLOOR'
        if kind == 2: return 0, 6, 0, 'FLOOR'
        if kind == 3: return 0, 6, 3, 'FLOOR'
        bx = (tx // 4) * 8
        by = ty * 6 + (tx // 2) % 2 * 3
        if kind % 2 == 0:
            return 0, bx, by, 'FLOOR'
        return 0, bx + 6, by, 'WATERFALL'
    if is_a2(t): return 1, tx * 2, (ty - 2) * 3, 'FLOOR'
    if is_a3(t): return 2, tx * 2, (ty - 6) * 2, 'WALL'
    # A4
    bx = tx * 2
    by = int((ty - 10) * 2.5 + (0.5 if ty % 2 == 1 else 0))
    table = 'WALL' if ty % 2 == 1 else 'FLOOR'
    return 3, bx, by, table

def load_sheet(conv_dir, name):
    """Deswizzle .t8 + apply .clut -> PIL RGBA (cropped to meta w/h)."""
    base = pathlib.Path(conv_dir) / name
    meta = json.loads((pathlib.Path(str(base) + '.meta.json')).read_text())
    tw, th, w, h = meta['tex_w'], meta['tex_h'], meta['w'], meta['h']
    raw = pathlib.Path(str(base) + '.t8').read_bytes()
    out = bytearray(tw * th)
    src = 0
    for by in range(0, th, 8):
        for bx in range(0, tw, 16):
            for row in range(8):
                dst = (by + row) * tw + bx
                out[dst:dst + 16] = raw[src:src + 16]
                src += 16
    clut = struct.unpack('<256I', pathlib.Path(str(base) + '.clut').read_bytes())
    px = Image.new('RGBA', (tw, th))
    px.putdata([((c) & 255, (c >> 8) & 255, (c >> 16) & 255, (c >> 24) & 255) for c in out])
    pal = Image.new('RGBA', (16, 16))
    pal.putdata([((c) & 255, (c >> 8) & 255, (c >> 16) & 255, (c >> 24) & 255) for c in clut])
    # map index->color via palette image lookup
    lut = pal.tobytes()
    rgb = bytearray()
    for i in out:
        rgb += lut[i * 4:(i + 1) * 4]
    im = Image.frombytes('RGBA', (tw, th), bytes(rgb))
    return im.crop((0, 0, w, h)), meta

def main():
    args = sys.argv[1:]
    game = pathlib.Path(args[0])
    mname = args[args.index('--map') + 1] if '--map' in args else 'Map030'
    tile = int(args[args.index('--tile') + 1] if '--tile' in args else 24)
    out = pathlib.Path(args[args.index('--out') + 1] if '--out' in args else 'render_out')
    out.mkdir(parents=True, exist_ok=True)
    data = game / 'data'
    conv = pathlib.Path('converted')
    scale = tile / FULL

    m = json.loads((data / (mname + '.json')).read_text())
    w, h = m['width'], m['height']
    raw = m['data']
    ts = next(t for t in json.loads((data / 'Tilesets.json').read_text()) if t and t['id'] == m['tilesetId'])
    auto = json.loads(pathlib.Path('converted/baked/autotiles.json').read_text())
    tables = {'FLOOR': auto['FLOOR'], 'WALL': auto['WALL'], 'WATERFALL': auto['WATERFALL']}

    # load the 9 sheets (skip empties)
    sheets = {}
    for slot, sname in enumerate(ts['tilesetNames']):
        if not sname:
            continue
        cat = 'tilesets'
        p = conv / cat / sname
        if not pathlib.Path(str(p) + '.meta.json').exists():
            print(f'MISSING converted sheet {sname} — convert it first')
            continue
        sheets[slot], _ = load_sheet(conv / cat, sname)
    print(f'sheets loaded: {sorted(sheets)} of {[i for i, s in enumerate(ts["tilesetNames"]) if s]}')

    def blit(dst, sheet, sx, sy, sw, sh, dx, dy, dw, dh):
        # source rect is in FULL-px units; sheet is downscaled: convert
        src = sheet.crop((round(sx * scale), round(sy * scale),
                          round((sx + sw) * scale), round((sy + sh) * scale)))
        if src.size != (dw, dh):  # only resample on real scale change (keeps golden bit-exact)
            src = src.resize((dw, dh), Image.LANCZOS)
        dst.alpha_composite(src, (dx, dy))

    canvas = Image.new('RGBA', (w * tile, h * tile), (0, 0, 0, 255))
    stats = {'normal': 0, 'auto': 0, 'quads': 0, 'empty': 0, 'missing_sheet': 0}
    dump = open(out / f'{mname}_quads.txt', 'w') if '--dump-quads' in args else None
    HW = FULL // 2
    for z in range(4):
        for y in range(h):
            for x in range(w):
                t = raw[(z * h + y) * w + x] or 0
                if t == 0 or t >= TMAX:
                    if t == 0: stats['empty'] += 1
                    continue
                dx, dy = x * tile, y * tile
                if t >= A1:  # autotile
                    stats['auto'] += 1
                    aset, bx, by, tname = auto_cell(t)
                    shape = shape_of(t)
                    # engine skips out-of-range shapes (undefined table) — replicate
                    if (tname == 'WALL' and shape >= 16) or (tname == 'WATERFALL' and shape >= 4):
                        continue
                    table = tables[tname][shape]
                    if aset not in sheets:
                        stats['missing_sheet'] += 1
                        continue
                    for i, (qsx, qsy) in enumerate(table):
                        sx = (bx * 2 + qsx) * HW
                        sy = (by * 2 + qsy) * HW
                        qdx = dx + (i % 2) * (tile // 2)
                        qdy = dy + (i // 2) * (tile // 2)
                        blit(canvas, sheets[aset], sx, sy, HW, HW, qdx, qdy, tile // 2, tile // 2)
                        stats['quads'] += 1
                        if dump: dump.write(f'{z} {x} {y} {i} {aset} {sx} {sy} 24 24\n')
                else:  # normal
                    stats['normal'] += 1
                    aset = 4 if is_a5(t) else 5 + t // 256
                    if aset not in sheets:
                        stats['missing_sheet'] += 1
                        continue
                    sx = (t // 128 % 2 * 8 + t % 8) * FULL
                    sy = (t % 256 // 8 % 16) * FULL
                    blit(canvas, sheets[aset], sx, sy, FULL, FULL, dx, dy, tile, tile)
                    stats['quads'] += 1
                    if dump: dump.write(f'{z} {x} {y} -1 {aset} {sx} {sy} 48 48\n')
    if dump: dump.close()
    canvas.save(out / f'{mname}.png')
    print(f'{mname} {w}x{h}: {stats} -> {out / (mname + ".png")}')
    # nonzero check vs map data
    nz = sum(1 for z in range(4) for y in range(h) for x in range(w)
             if (raw[(z * h + y) * w + x] or 0) not in (0,) and raw[(z * h + y) * w + x] < TMAX)
    print(f'nonzero in-data tiles (layers 0-3, id<TMAX): {nz}')

if __name__ == '__main__':
    main()
