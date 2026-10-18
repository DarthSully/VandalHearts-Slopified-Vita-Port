#!/usr/bin/env bash
# Build Vandal Hearts for PS Vita -> platform/pc/build_vita/vandal-vita-<tag>.vpk
#
#   platform/vita/build.sh <disc.bin> <KROMDAT.BIN or 512 KB PS1 BIOS> [build tag, default r01]
#
# Needs VitaSDK at $VITASDK (with SDL2 for Vita installed), cmake, python3.
# The disc image is only read at build time to extract SLUS_004.47, which the data generators slice
# game data from; the game itself reads the .bin on the Vita at runtime.
set -euo pipefail

: "${VITASDK:?Set VITASDK to your VitaSDK install (e.g. /usr/local/vitasdk)}"
export PATH="$VITASDK/bin:$PATH"

if [ $# -lt 2 ]; then
  sed -n '2,8p' "$0"
  exit 1
fi
DISC="$1"
KROM="$2"
TAG="${3:-r01}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PC="$ROOT/platform/pc"
BUILD="$PC/build_vita"
mkdir -p "$BUILD"

EXE="$BUILD/SLUS_004.47"
if [ ! -f "$EXE" ]; then
  python3 "$ROOT/platform/vita/tools/extract_exe.py" "$DISC" "$EXE"
fi

cmake -S "$PC" -B "$BUILD" \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DVH_PSX_EXE="$EXE" \
  -DVH_KROM_SOURCE="$KROM" \
  -DVH_VITA_BUILD="$TAG"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"
ls -la "$BUILD"/vandal-vita-"$TAG".vpk
