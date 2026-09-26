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
from stage_data import swizzle8

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
    full = Image.new('RGBA', (512, 512), (0, 0, 0, 255))
    full.paste(img, (0, 0))
    rgb = full.convert('RGB').quantize(colors=255, method=Image.MEDIANCUT)
    pal = rgb.getpalette()[:255 * 3]
    idx = rgb.tobytes()
    mask = bytes(255 if y < 272 else 0 for y in range(512)
                 for _ in range(512))
    idx = bytes(b + 1 if m else 0 for b, m in zip(idx, mask))
    import struct
    swiz = swizzle8(bytes(idx), 512, 512)
    out = pathlib.Path('psp/gu_demo/data/title')
    out.parent.mkdir(parents=True, exist_ok=True)
    (out.parent / 'title.t8').write_bytes(swiz)
    clut = struct.pack('<I', 0x00000000)
    for i in range(255):
        r, g, b = pal[i * 3:(i + 1) * 3]
        clut += struct.pack('<I', 0xFF000000 | (b << 16) | (g << 8) | r)
    (out.parent / 'title.clut').write_bytes(clut)
    print(f'wrote title.t8 ({len(swiz)} bytes)')


if __name__ == '__main__':
    main()
