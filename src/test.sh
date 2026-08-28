#!/usr/bin/env bash
# Build and run the SilOS Browser tests.
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="$SCRIPT_DIR/build"

source "$SCRIPT_DIR/check-setup.sh"
"$BASH" "$SCRIPT_DIR/build.sh"

echo "Testing SilOS Browser target..."
ctest --test-dir "$BUILD_DIR" --output-on-failure
