#!/usr/bin/env python3
"""Transcode game movies to PSP PSMF files.

ffmpeg cannot write the PSMF header the ME firmware (and PPSSPP)
requires, so this script muxes MPEG-PS (H.264 baseline + MP3) and
prepends a 2048-byte PSMF header itself. Runtime feeds the result
straight into the sceMpeg ringbuffer; see psp/gu_demo/movie.c.

Usage (from the repo root):
    python3 tools/mux_movie.py "Fear & Hunger_WIN/www/movies/intro.webm"
"""
import json
import pathlib
import struct
import subprocess
import sys


def build_psmf(size, duration):
    h = bytearray(2048)
    h[0:4] = b'PSMF'
    h[4:8] = b'0015'
    struct.pack_into('>I', h, 8, 2048)
    struct.pack_into('>I', h, 12, size)
    struct.pack_into('>I', h, 0x54, 90000)
    struct.pack_into('>I', h, 0x5A, 90000 + round(duration * 90000))
    struct.pack_into('>H', h, 0x80, 1)
    h[0x82] = 0xE0
    return bytes(h)


def main() -> None:
    src = pathlib.Path(sys.argv[1])
    out = pathlib.Path('psp/gu_demo/data/movies') / (src.stem + '.mp4')
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.ps')
    r = subprocess.run([
        'ffmpeg', '-y', '-v', 'error', '-i', str(src),
        '-f', 'lavfi', '-i', 'anullsrc=r=44100:cl=stereo', '-shortest',
        '-vf', 'scale=480:272', '-c:v', 'libx264', '-profile:v',
        'baseline', '-level', '3.0', '-pix_fmt', 'yuv420p', '-r', '25',
        '-b:v', '768k', '-c:a', 'libmp3lame', '-b:a', '96k',
        '-f', 'mpeg', str(tmp),
    ])
    if r.returncode != 0:
        sys.exit('ffmpeg failed')
    probe = subprocess.run(
        ['ffprobe', '-v', 'error', '-show_entries', 'format=duration',
         '-of', 'default=noprint_wrappers=1', str(src)],
        capture_output=True, text=True)
    duration = 33.0
    for line in probe.stdout.splitlines():
        if line.startswith('duration='):
            try:
                duration = float(line.split('=', 1)[1])
            except ValueError:
                pass
    ps = tmp.read_bytes()
    tmp.unlink()
    out.write_bytes(build_psmf(len(ps), duration) + ps)
    print(f'wrote {out} ({len(ps) + 2048} bytes, {duration:.1f}s)')


if __name__ == '__main__':
    main()
