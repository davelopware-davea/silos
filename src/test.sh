#!/usr/bin/env bash
# Build and run the SilOS Browser tests.
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="$SCRIPT_DIR/build"

"$BASH" "$SCRIPT_DIR/build.sh"

if ! command -v ctest >/dev/null 2>&1; then
  cat >&2 <<'EOF'
error: ctest is required to test the SilOS Browser target.
Install CMake and ensure 'ctest' is on PATH, then rerun this script.
EOF
  exit 1
fi

echo "Testing SilOS Browser target..."
ctest --test-dir "$BUILD_DIR" --output-on-failure
