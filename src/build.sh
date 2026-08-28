#!/usr/bin/env bash
# Configure (when needed) and build SilOS for the Browser with Emscripten.
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="$SCRIPT_DIR"
readonly BUILD_DIR="$SCRIPT_DIR/build"
readonly CACHE="$BUILD_DIR/CMakeCache.txt"

source "$SCRIPT_DIR/check-setup.sh"

if [[ -f "$CACHE" ]]; then
  if ! grep -qx 'EMSCRIPTEN:INTERNAL=1' "$CACHE"; then
    cat >&2 <<EOF
error: the existing build at:
  $BUILD_DIR
is not configured as an Emscripten Browser build.

Move or remove that build directory, then rerun this script. It will configure
a fresh Emscripten build without overwriting the existing non-Browser build.
EOF
    exit 1
  fi
else
  echo "Configuring SilOS Browser target..."
  emcmake cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja
fi

echo "Building SilOS Browser target..."
cmake --build "$BUILD_DIR"
