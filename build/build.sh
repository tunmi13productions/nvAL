#!/usr/bin/env bash
# nvAL build script for Linux / macOS (GCC or Clang)
# Usage: ./build.sh /path/to/nvgt
# Or set NVGT_PATH before running.

set -e

[ -n "$1" ] && NVGT_PATH="$1"
if [ -z "$NVGT_PATH" ]; then
    echo "Error: NVGT_PATH not set."
    echo "Usage: ./build.sh /path/to/nvgt"
    exit 1
fi

SRC="$(cd "$(dirname "$0")/../src" && pwd)"
OUT="$(cd "$(dirname "$0")/.." && pwd)/nval.so"
INC_NVGT="$NVGT_PATH/src"
INC_AS="$NVGT_PATH/windev/include"  # adjust for your platform

CXX="${CXX:-g++}"

echo "Building nvAL with $CXX..."
"$CXX" -shared -fPIC -std=c++17 \
    -I"$INC_NVGT" -I"$INC_AS" -I"$SRC" \
    "$SRC/nval.cpp" -o "$OUT" -ldl

echo "Success: nval.so written to repo root."
