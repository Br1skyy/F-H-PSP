#!/usr/bin/env bash
# Rebuild psp/gu_demo/EBOOT.PBP after code changes.
#
# Usage: ./build_psp.sh [--zip]
#   --zip   also repack FHDEMO.zip without prompting
#
# Toolchain lookup (first match wins):
#   1. PSPDEV / PSPSDK already exported in your shell
#   2. psp-config found on PATH
#   3. $HOME/pspdev/psp  (the layout this script used to hardcode)
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

if [ -z "${PSPDEV:-}" ]; then
    if command -v psp-config >/dev/null 2>&1; then
        PSPDEV="$(psp-config --pspdev-path)"
    else
        PSPDEV="$HOME/pspdev/psp"
    fi
fi
export PSPDEV
export PATH="$PSPDEV/bin:$PATH"

if [ -z "${PSPSDK:-}" ]; then
    if command -v psp-config >/dev/null 2>&1; then
        PSPSDK="$(psp-config --pspsdk-path)"
    else
        PSPSDK="$PSPDEV/sdk"
    fi
fi
export PSPSDK

if ! command -v psp-gcc >/dev/null 2>&1; then
    echo "error: psp-gcc not found (looked in $PSPDEV/bin)." >&2
    echo "       Install the PSP toolchain or export PSPDEV to its location." >&2
    exit 1
fi

(
    cd psp/gu_demo
    make clean
    make
    echo "Build complete: psp/gu_demo/EBOOT.PBP"
    ls -lh EBOOT.PBP
)

want_zip=0
if [ "${1:-}" = "--zip" ]; then
    want_zip=1
elif [ -t 0 ]; then
    read -p "Repack FHDEMO.zip? (y/N) " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]] && want_zip=1
fi

if [ "$want_zip" = 1 ]; then
    rm -f FHDEMO.zip
    (cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP)
    echo "FHDEMO.zip created"
    ls -lh FHDEMO.zip
fi
