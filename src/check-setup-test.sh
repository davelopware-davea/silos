#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly TEMP_DIR="$(mktemp -d)"
readonly BIN_DIR="$TEMP_DIR/bin"
mkdir "$BIN_DIR"
trap 'rm -rf "$TEMP_DIR"' EXIT

ln -s "$(command -v sed)" "$BIN_DIR/sed"
ln -s /bin/true "$BIN_DIR/apt-get"
printf '#!/bin/bash\nprintf "cmake version 3.28.3\\n"\n' >"$BIN_DIR/cmake"
chmod +x "$BIN_DIR/cmake"

set +e
output="$(PATH="$BIN_DIR" "$BASH" "$SCRIPT_DIR/check-setup.sh" 2>&1)"
status=$?
set -e
if (( status == 0 )) || [[ "$output" != *"Ninja is not on PATH"* ]] ||
   [[ "$output" != *"sudo apt-get install -y ninja-build"* ]] ||
   [[ "$output" == *"Emscripten is not activated"* ]]; then
  printf 'unexpected first-failure diagnostic:\n%s\n' "$output" >&2
  exit 1
fi

for command_name in ninja python3; do
  ln -s /bin/true "$BIN_DIR/$command_name"
done
mkdir -p "$TEMP_DIR/home/emsdk"
printf 'export EMSDK_TEST_ACTIVATED=1\n' >"$TEMP_DIR/home/emsdk/emsdk_env.sh"

set +e
output="$(HOME="$TEMP_DIR/home" PATH="$BIN_DIR" \
  "$BASH" "$SCRIPT_DIR/check-setup.sh" 2>&1)"
status=$?
set -e
if (( status == 0 )) ||
   [[ "$output" != *"cannot activate the SDK in your current shell"* ]] ||
   [[ "$output" != *'source "$HOME/emsdk/emsdk_env.sh"'* ]] ||
   [[ "$output" == *"Run this fix now?"* ]]; then
  printf 'unexpected direct-execution activation diagnostic:\n%s\n' "$output" >&2
  exit 1
fi

for command_name in emcmake emcc em++ ctest; do
  ln -s /bin/true "$BIN_DIR/$command_name"
done

output="$(PATH="$BIN_DIR" "$BASH" "$SCRIPT_DIR/check-setup.sh")"
if [[ "$output" != "SilOS setup check passed." ]]; then
  printf 'unexpected success diagnostic:\n%s\n' "$output" >&2
  exit 1
fi

printf 'check-setup diagnostics pass.\n'
