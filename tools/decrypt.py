#!/usr/bin/env python3.12
"""MV asset decryption helper — implements §4.3 of the plan.

Encrypted MV assets = 16-byte fake header, then body whose first 16 bytes
are XORed with the key in System.json (`encryptionKey`).
.rpgmvp -> png, .rpgmvo -> ogg, .rpgmvm -> m4a (after decrypt).

Usage:
    python3.12 tools/decrypt.py "Fear & Hunger_WIN/www" --check
    python3.12 tools/decrypt.py "Fear & Hunger_WIN/www" --file img/foo.rpgmvp -o /tmp/foo.png
"""
import sys, pathlib, json

def decrypt_bytes(blob: bytes, key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    body = bytearray(blob[16:])
    for i in range(16):
        body[i] ^= key[i]
    return bytes(body)

def main():
    root = pathlib.Path(sys.argv[1])
    sysj = json.loads((root / 'data/System.json').read_text(encoding='utf-8'))
    key = sysj.get('encryptionKey', '')
    print(f"hasEncryptedImages={sysj.get('hasEncryptedImages')} "
          f"hasEncryptedAudio={sysj.get('hasEncryptedAudio')} key_present={bool(key)}")
    if '--check' in sys.argv:
        for ext in ('*.rpgmvp', '*.rpgmvo', '*.rpgmvm'):
            n = len(list((root / 'img').rglob(ext))) if ext == '*.rpgmvp' else len(list((root / 'audio').rglob(ext)))
            print(f'  {ext}: {n} files')
        return
    if '--file' in sys.argv:
        src = pathlib.Path(sys.argv[sys.argv.index('--file') + 1])
        dst = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])
        dst.write_bytes(decrypt_bytes(src.read_bytes(), key))
        print(f'decrypted {src} -> {dst} ({dst.stat().st_size} bytes)')

if __name__ == '__main__':
    main()
