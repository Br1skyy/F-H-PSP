#!/usr/bin/env python3.12
"""Convert BGM to MP3 and sound effects to PCM WAV. Requires ffmpeg on PATH.

Usage:
    python3.12 tools/convert_audio.py "Fear & Hunger_WIN/www" --out converted/audio [--only bgm/...]
    --only takes repeatable prefixes like bgm/fear, se/door. Default converts all.

Output: converted/audio/<cat>/<name>.mp3|.wav + manifest.json with sizes."""
import json, sys, pathlib, subprocess, shutil

def decrypt_blob(blob: bytes, key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    body = bytearray(blob[16:])
    for i in range(16):
        body[i] ^= key[i]
    return bytes(body)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'{" ".join(cmd)}\n{r.stderr[-800:]}')
    return r

def main():
    args = sys.argv[1:]
    game = pathlib.Path(args[0])
    out = pathlib.Path(args[args.index('--out') + 1] if '--out' in args else 'converted/audio')
    only = [args[i + 1] for i, a in enumerate(args) if a == '--only' and i + 1 < len(args)]
    if not shutil.which('ffmpeg'):
        sys.exit('ffmpeg not found on PATH')
    key = json.loads((game / 'data/System.json').read_text(encoding='utf-8')).get('encryptionKey', '')
    tmp = out / '_tmp'
    tmp.mkdir(parents=True, exist_ok=True)

    entries = []
    for cat in ('bgm', 'bgs', 'me', 'se'):
        d = game / 'audio' / cat
        if not d.exists():
            continue
        for f in sorted(d.glob('*.rpgmvo')) + sorted(d.glob('*.ogg')):
            rel = f'{cat}/{f.stem}'
            if only and not any(rel.startswith(o) for o in only):
                continue
            if f.suffix == '.ogg' and (f.parent / (f.stem + '.rpgmvo')).exists():
                continue
            raw = f.read_bytes()
            if f.suffix == '.rpgmvo':
                raw = decrypt_blob(raw, key)
            src = tmp / (f.stem + '.ogg')
            src.write_bytes(raw)
            cdir = out / cat
            cdir.mkdir(parents=True, exist_ok=True)
            if cat == 'se':
                dst = cdir / (f.stem + '.wav')
                run(['ffmpeg', '-y', '-v', 'error', '-i', str(src),
                     '-ac', '1', '-ar', '22050', '-sample_fmt', 's16', str(dst)])
            else:
                dst = cdir / (f.stem + '.mp3')
                run(['ffmpeg', '-y', '-v', 'error', '-i', str(src),
                     '-codec:a', 'libmp3lame', '-b:a', '96k', str(dst)])
            entries.append({'name': f.stem, 'cat': cat,
                            'src_bytes': len(raw), 'dst_bytes': dst.stat().st_size,
                            'file': str(dst.relative_to(out))})
            print(f"OK {rel} {len(raw)//1024}KB -> {dst.stat().st_size//1024}KB")

    for f in tmp.glob('*.ogg'):
        f.unlink()
    try:
        tmp.rmdir()
    except OSError:
        pass

    merged = {(e['cat'], e['name']): e for e in entries}
    mpath = out / 'manifest.json'
    if mpath.exists():
        for e in json.loads(mpath.read_text(encoding='utf-8')).get('entries', []):
            merged.setdefault((e['cat'], e['name']), e)
    entries = sorted(merged.values(), key=lambda e: (e['cat'], e['name']))
    (out / 'manifest.json').write_text(json.dumps(
        {'n': len(entries), 'entries': entries,
         'note': 'BGM=MP3 96k HW-decodable; SE=WAV 22k mono PCM (plan §2.3/§7).'}, indent=1))
    print(f'\n{len(entries)} files -> {out}')

if __name__ == '__main__':
    main()
