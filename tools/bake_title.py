#!/usr/bin/env python3
"""Bake the title screen card at PSP resolution.

Decrypts titles2/title2 and cover-fits it to 480x272 raw RGBA8888,
drawn unswizzled like movie frames (no palette step involved).

Usage (from the repo root):
    python3 tools/bake_title.py "Fear & Hunger_WIN/www"
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from convert_assets import decrypt_blob

try:
    from PIL import Image
except ImportError:
    sys.exit('Pillow required: pip install Pillow')


def main() -> None:
    game = pathlib.Path(sys.argv[1])
    key = json.loads((game / 'data/System.json').read_text())['encryptionKey']
    src = game / 'img/titles2/title2.rpgmvp'
    if not src.exists():
        src = game / 'img/titles2/title2.png'
    raw = src.read_bytes()
    if src.suffix == '.rpgmvp':
        raw = decrypt_blob(raw, key)
    import io
    img = Image.open(io.BytesIO(raw)).convert('RGBA')
    w, h = img.size
    s = max(512.0 / w, 272.0 / h)
    img = img.resize((round(w * s) + 1, round(h * s) + 1), Image.LANCZOS)
    w2, h2 = img.size
    img = img.crop(((w2 - 512) // 2, (h2 - 272) // 2,
                    (w2 - 512) // 2 + 512, (h2 - 272) // 2 + 272))
    out = pathlib.Path('psp/gu_demo/data/title.rgba')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img.tobytes())
    print(f'wrote {out} ({len(img.tobytes())} bytes)')


if __name__ == '__main__':
    main()
