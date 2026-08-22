#!/usr/bin/env bash
# Configure (when needed) and build the Browser to-do proof with Emscripten.
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="$SCRIPT_DIR/runtime"
readonly BUILD_DIR="$SCRIPT_DIR/build"
readonly CACHE="$BUILD_DIR/CMakeCache.txt"

if ! command -v cmake >/dev/null 2>&1; then
  cat >&2 <<'EOF'
error: cmake is required to build the Browser proof.
Install CMake and ensure 'cmake' is on PATH, then rerun this script.
EOF
  exit 1
fi

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
  if ! command -v emcmake >/dev/null 2>&1; then
    cat >&2 <<'EOF'
error: emcmake is required to configure the Browser proof.
Activate the Emscripten SDK environment (for example, run emsdk_env.sh), then
rerun this script.
EOF
    exit 1
  fi
  if ! command -v ninja >/dev/null 2>&1; then
    cat >&2 <<'EOF'
error: Ninja is required to configure the Browser proof.
Install Ninja and ensure 'ninja' is on PATH, then rerun this script.
EOF
    exit 1
  fi

  echo "Configuring Emscripten Browser proof..."
  emcmake cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja
fi

echo "Building Browser proof..."
cmake --build "$BUILD_DIR"
