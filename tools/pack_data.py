#!/usr/bin/env python3.12
"""Data packer — implements md §2 item 4 + §7 "one packed archive with an index".

Reads data/*.json, pools ALL strings into one global table, encodes values
with typed tags, and writes a single game.pak (header + index + blobs) for
sequential reads on PSP. Verifies with an exact roundtrip check.

Format (little-endian):
  header: magic 'FHPK' u32 version u32 nfiles u32 strcount u32
  strings: [u32 len + utf8 bytes] * strcount
  index: per file [u16 namelen + name + u32 offset + u32 size] * nfiles
  blobs: per file typed values:
    tag u8: 0=null 1=false 2=true 3=i32 4=f64 5=strref(u32) 6=array(u32 n + items) 7=object(u32 n + keyref(u32)+value...)

Usage:
    python3.12 tools/pack_data.py "Fear & Hunger_WIN/www" --out converted/data
"""
import json, sys, pathlib, struct

TAG_NULL, TAG_FALSE, TAG_TRUE, TAG_I32, TAG_F64 = 0, 1, 2, 3, 4
TAG_STR, TAG_ARR, TAG_OBJ = 5, 6, 7

class Packer:
    def __init__(self):
        self.strings, self.sindex = [], {}
        self.buf = bytearray()

    def intern(self, s):
        i = self.sindex.get(s)
        if i is None:
            i = len(self.strings)
            self.sindex[s] = i
            self.strings.append(s)
        return i

    def enc(self, v):
        b = self.buf
        if v is None:
            b.append(TAG_NULL)
        elif v is False:
            b.append(TAG_FALSE)
        elif v is True:
            b.append(TAG_TRUE)
        elif isinstance(v, int) and -(2**31) <= v < 2**31:
            b.append(TAG_I32); b += struct.pack('<i', v)
        elif isinstance(v, float):
            b.append(TAG_F64); b += struct.pack('<d', v)
        elif isinstance(v, str):
            b.append(TAG_STR); b += struct.pack('<I', self.intern(v))
        elif isinstance(v, list):
            b.append(TAG_ARR); b += struct.pack('<I', len(v))
            for x in v:
                self.enc(x)
        elif isinstance(v, dict):
            b.append(TAG_OBJ); b += struct.pack('<I', len(v))
            for k, x in v.items():
                b += struct.pack('<I', self.intern(k)); self.enc(x)
        else:
            raise TypeError(f'unpackable {type(v)}')

class Unpacker:
    def __init__(self, strings, buf):
        self.strings, self.buf, self.pos = strings, buf, 0

    def dec(self):
        b = self.buf
        tag = b[self.pos]; self.pos += 1
        if tag == TAG_NULL:
            return None
        if tag == TAG_FALSE:
            return False
        if tag == TAG_TRUE:
            return True
        if tag == TAG_I32:
            v = struct.unpack_from('<i', b, self.pos)[0]; self.pos += 4; return v
        if tag == TAG_F64:
            v = struct.unpack_from('<d', b, self.pos)[0]; self.pos += 8; return v
        if tag == TAG_STR:
            i = struct.unpack_from('<I', b, self.pos)[0]; self.pos += 4; return self.strings[i]
        if tag == TAG_ARR:
            n = struct.unpack_from('<I', b, self.pos)[0]; self.pos += 4
            return [self.dec() for _ in range(n)]
        if tag == TAG_OBJ:
            n = struct.unpack_from('<I', b, self.pos)[0]; self.pos += 4
            o = {}
            for _ in range(n):
                k = struct.unpack_from('<I', b, self.pos)[0]; self.pos += 4
                o[self.strings[k]] = self.dec()
            return o
        raise ValueError(f'bad tag {tag}')

def main():
    args = sys.argv[1:]
    game = pathlib.Path(args[0])
    out = pathlib.Path(args[args.index('--out') + 1] if '--out' in args else 'converted/data')
    out.mkdir(parents=True, exist_ok=True)
    data = game / 'data'
    files = sorted(p for p in data.glob('*.json') if p.name != 'MapInfos.json')
    files += [data / 'MapInfos.json']

    pk = Packer()
    blobs, originals = {}, {}
    for f in files:
        v = json.loads(f.read_text(encoding='utf-8'))
        originals[f.name] = v
        start = len(pk.buf)
        pk.enc(v)
        blobs[f.name] = bytes(pk.buf[start:])


    hdr = struct.pack('<4sIII', b'FHPK', 1, len(blobs), len(pk.strings))
    stb = bytearray()
    for s in pk.strings:
        e = s.encode('utf-8')
        stb += struct.pack('<I', len(e)) + e

    names = sorted(blobs)
    idx = bytearray()
    off = 0
    index = []
    for n in names:
        nb = n.encode('utf-8')
        idx += struct.pack('<H', len(nb)) + nb + struct.pack('<II', off, len(blobs[n]))
        index.append((n, off, len(blobs[n])))
        off += len(blobs[n])
    pak = hdr + bytes(stb) + bytes(idx) + b''.join(blobs[n] for n in names)
    (out / 'game.pak').write_text('', encoding='utf-8')
    (out / 'game.pak').write_bytes(pak)


    blob_base = len(hdr) + len(stb) + len(idx)
    assert pak[:4] == b'FHPK'
    bad = 0
    for n, o, s in index:
        u = Unpacker(pk.strings, pak[blob_base + o:blob_base + o + s])
        if u.dec() != originals[n]:
            print(f'MISMATCH {n}'); bad += 1
    total_json = sum((data / n).stat().st_size for n in names)
    meta = {'files': len(names), 'strings_pooled': len(pk.strings),
            'json_bytes': total_json, 'pak_bytes': len(pak),
            'ratio': len(pak) / total_json, 'roundtrip_errors': bad}
    (out / 'pack_manifest.json').write_text(json.dumps(meta, indent=1))
    print(f"{len(names)} files, {len(pk.strings)} pooled strings, "
          f"JSON {total_json/1e6:.1f}MB -> PAK {len(pak)/1e6:.1f}MB "
          f"({100*len(pak)/total_json:.0f}%), roundtrip errors: {bad}")
    if bad:
        sys.exit(1)

if __name__ == '__main__':
    main()
