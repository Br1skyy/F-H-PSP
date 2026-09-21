#!/usr/bin/env bash
# PSP build script - run this to rebuild EBOOT.PBP after code changes
# Usage: ./build_psp.sh

set -e

export PSPDEV=/home/samarie/pspdev/psp
export PSPSDK=$PSPDEV/sdk
export PATH=$PSPDEV/bin:$PATH

cd psp/gu_demo
make clean
make

echo "Build complete: psp/gu_demo/EBOOT.PBP"
ls -lh EBOOT.PBP

# Optionally repack FHDEMO.zip
read -p "Repack FHDEMO.zip? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cd ../..
    rm -f FHDEMO.zip
    cd psp/gu_demo && zip ../../FHDEMO.zip EBOOT.PBP && cd ../..
    echo "FHDEMO.zip created"
    ls -lh FHDEMO.zip
fi
