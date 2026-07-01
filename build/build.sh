#!/usr/bin/env bash
# nvAL build script for Linux (.so) and macOS (.dylib) using GCC or Clang.
#
# Usage: ./build.sh /path/to/nvgt
#   or:  NVGT_PATH=/path/to/nvgt ./build.sh
#
# NVGT_PATH must point at a checkout of the NVGT *source* repo (github.com/samtupy/nvgt),
# not an installed copy -- it provides src/nvgt_plugin.h, ASAddon/include/scriptarray.h
# and ASAddon/plugin/scriptarray.cpp.
#
# angelscript.h is not committed to the NVGT repo; it ships in the platform dev bundle
# (lindev / macosdev), downloadable from https://nvgt.dev/lindev.zip and
# https://nvgt.dev/macosdev.zip. Extract the matching bundle in the NVGT repo root so
# you have e.g. nvgt/lindev/include/angelscript.h. To point somewhere else, set INC_AS
# to a directory that contains angelscript.h.
#
# The plugin dlopen()s OpenAL at runtime (libopenal.so.1 / libopenal.dylib), so no
# OpenAL dev package is needed to build, and it has no undefined AngelScript symbols
# (nvgt_plugin.h supplies them via a shared struct at load time) -- a plain shared
# library link is all that is required.

set -e

[ -n "$1" ] && NVGT_PATH="$1"
if [ -z "$NVGT_PATH" ]; then
    echo "Error: NVGT_PATH not set."
    echo "Usage: ./build.sh /path/to/nvgt"
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src"

# Platform-specific output name, shared-library flags and dev-bundle name.
case "$(uname -s)" in
    Darwin)
        OUT="$HERE/../nval.dylib"
        OSDEV="macosdev"
        SHFLAGS="-dynamiclib"
        LIBS=""
        ;;
    *)
        OUT="$HERE/../nval.so"
        OSDEV="lindev"
        SHFLAGS="-shared -fPIC"
        LIBS="-ldl"
        ;;
esac

INC_NVGT="$NVGT_PATH/src"
INC_ASADDON="$NVGT_PATH/ASAddon/include"
SCRIPTARRAY="$NVGT_PATH/ASAddon/plugin/scriptarray.cpp"
# angelscript.h location: honor a caller-provided INC_AS, else the dev bundle.
INC_AS="${INC_AS:-$NVGT_PATH/$OSDEV/include}"

if [ ! -f "$INC_AS/angelscript.h" ]; then
    echo "Error: angelscript.h not found in '$INC_AS'."
    echo "Extract the $OSDEV bundle (https://nvgt.dev/$OSDEV.zip) in the NVGT repo root,"
    echo "or set INC_AS to a directory containing angelscript.h."
    exit 1
fi

CXX="${CXX:-c++}"

echo "Building nvAL ($(uname -s)) with $CXX -> $(basename "$OUT")..."
"$CXX" $SHFLAGS -std=c++17 -O2 \
    -I"$INC_NVGT" -I"$INC_AS" -I"$INC_ASADDON" -I"$SRC" \
    "$SRC/nval.cpp" "$SCRIPTARRAY" \
    -o "$OUT" $LIBS

echo "Success: $(basename "$OUT") written to repo root."
