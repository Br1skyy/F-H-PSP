#!/usr/bin/env python3
"""Stage game audio for the PSP build.

Decrypts .rpgmvo (same key as images) and stages two layouts:
SE become 22050 Hz mono 16-bit WAV (cheap to mix, streamed per
play), while BGM/BGS/ME stay decrypted OGG and stream through
tremor. Output: psp/gu_demo/data/audio/{se,bgm,bgs,me}/<name>.*.

Usage (from the repo root):
    python3 tools/bake_audio.py "Fear & Hunger_WIN/www" [--force]
"""
import json
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from convert_assets import decrypt_blob


def main() -> None:
    game = pathlib.Path(sys.argv[1])
    force = '--force' in sys.argv
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    data = pathlib.Path('psp/gu_demo/data/audio')
    n_new, n_skip = 0, 0
    for cat in ('se', 'bgm', 'bgs', 'me'):
        src = game / 'audio' / cat
        dst = data / cat
        dst.mkdir(parents=True, exist_ok=True)
        if not src.exists():
            continue
        for f in sorted(src.iterdir()):
            if f.suffix not in ('.rpgmvo', '.ogg'):
                continue
            raw = decrypt_blob(f.read_bytes(), key) \
                if f.suffix == '.rpgmvo' else f.read_bytes()
            if cat == 'se':
                out = dst / (f.stem + '.wav')
                if out.exists() and not force:
                    n_skip += 1
                    continue
                tmp = out.with_suffix('.ogg')
                tmp.write_bytes(raw)
                r = subprocess.run([
                    'ffmpeg', '-y', '-v', 'error', '-i', str(tmp),
                    '-ar', '22050', '-ac', '1', '-c:a', 'pcm_s16le',
                    str(out),
                ])
                tmp.unlink(missing_ok=True)
                if r.returncode != 0:
                    sys.exit(f'ffmpeg failed on {f.name}')
                n_new += 1
            else:
                out = dst / (f.stem + '.ogg')
                if out.exists() and not force:
                    n_skip += 1
                    continue
                out.write_bytes(raw)
                n_new += 1
    print(f'audio staged: {n_new} new, {n_skip} unchanged')


if __name__ == '__main__':
    main()
