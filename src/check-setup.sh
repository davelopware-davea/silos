#!/usr/bin/env bash
# Report and optionally fix the first missing SilOS Browser prerequisite.
set -euo pipefail

setup_is_sourced=false
if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  setup_is_sourced=true
fi

setup_fail() {
  printf 'SilOS setup check failed: %s\n\n%b\n' "$1" "$2" >&2
  exit 1
}

run_fix() {
  local action="$1"
  local argument="${2:-}"

  case "$action" in
    apt)
      sudo apt-get update && sudo apt-get install -y "$argument"
      ;;
    brew)
      brew install "$argument"
      ;;
    pacman)
      sudo pacman -S --needed --noconfirm "$argument"
      ;;
    activate-emsdk)
      # This script is sourced by the build entry points, so activation remains
      # in effect for the configure and build commands that follow.
      source "$HOME/emsdk/emsdk_env.sh"
      ;;
    install-emsdk)
      git clone https://github.com/emscripten-core/emsdk.git "$HOME/emsdk" &&
        "$HOME/emsdk/emsdk" install latest &&
        "$HOME/emsdk/emsdk" activate latest &&
        source "$HOME/emsdk/emsdk_env.sh"
      ;;
    *)
      return 1
      ;;
  esac
}

offer_fix() {
  local problem="$1"
  local command_text="$2"
  local action="$3"
  local argument="${4:-}"
  local reply

  printf 'SilOS setup check failed: %s\n\nFix it with:\n%b\n' \
    "$problem" "$command_text" >&2

  if [[ ! -t 0 ]]; then
    printf '\nRun that command, or rerun this check in an interactive terminal to be offered the fix.\n' >&2
    exit 1
  fi

  printf '\nRun this fix now? [y/N] ' >&2
  read -r reply
  if [[ ! "$reply" =~ ^[Yy]([Ee][Ss])?$ ]]; then
    printf 'Setup was not changed.\n' >&2
    exit 1
  fi

  printf 'Running setup fix...\n' >&2
  if ! run_fix "$action" "$argument"; then
    setup_fail "The setup command did not complete successfully." \
      "Run it manually to see its full diagnostics:\n$command_text"
  fi

  printf '\nSetup fix completed.\nContinuing to check the SilOS setup...\n\n' >&2
}

offer_package_fix() {
  local problem="$1"
  local apt_package="$2"
  local brew_package="$3"
  local pacman_package="${4:-$brew_package}"

  if command -v apt-get >/dev/null 2>&1; then
    offer_fix "$problem" \
      "  sudo apt-get update && sudo apt-get install -y $apt_package" \
      apt "$apt_package"
  elif command -v brew >/dev/null 2>&1; then
    offer_fix "$problem" "  brew install $brew_package" brew "$brew_package"
  elif command -v pacman >/dev/null 2>&1; then
    offer_fix "$problem" \
      "  sudo pacman -S --needed --noconfirm $pacman_package" \
      pacman "$pacman_package"
  else
    setup_fail "$problem" "Install $brew_package and ensure it is on PATH."
  fi
}

activate_emsdk_or_fail() {
  local problem="$1"

  if [[ ! -f "$HOME/emsdk/emsdk_env.sh" ]]; then
    setup_fail "$problem" \
      'Repair the Emscripten SDK so that "$HOME/emsdk/emsdk_env.sh" exists.'
  fi

  if [[ "$setup_is_sourced" == true ]]; then
    offer_fix "$problem" '  source "$HOME/emsdk/emsdk_env.sh"' activate-emsdk
  else
    setup_fail "$problem" \
      'This checker was executed as a child process, so it cannot activate the SDK in your current shell.

Activate it in the current shell with:
  source "$HOME/emsdk/emsdk_env.sh"

Then rerun the check or workflow.'
  fi
}

if ! command -v cmake >/dev/null 2>&1; then
  offer_package_fix "CMake is not on PATH." cmake cmake
  command -v cmake >/dev/null 2>&1 ||
    setup_fail "CMake is still not on PATH after the setup command." \
      "Install CMake and ensure its bin directory is on PATH."
fi

cmake_version="$(cmake --version 2>/dev/null | sed -n '1s/^[^0-9]*//p')"
cmake_major="${cmake_version%%.*}"
cmake_rest="${cmake_version#*.}"
cmake_minor="${cmake_rest%%.*}"
if [[ ! "$cmake_major" =~ ^[0-9]+$ || ! "$cmake_minor" =~ ^[0-9]+$ ]] ||
   (( cmake_major < 3 || (cmake_major == 3 && cmake_minor < 20) )); then
  offer_package_fix \
    "CMake 3.20 or newer is required (found ${cmake_version:-an unknown version})." \
    cmake cmake
  cmake_version="$(cmake --version 2>/dev/null | sed -n '1s/^[^0-9]*//p')"
  cmake_major="${cmake_version%%.*}"
  cmake_rest="${cmake_version#*.}"
  cmake_minor="${cmake_rest%%.*}"
  if [[ ! "$cmake_major" =~ ^[0-9]+$ || ! "$cmake_minor" =~ ^[0-9]+$ ]] ||
     (( cmake_major < 3 || (cmake_major == 3 && cmake_minor < 20) )); then
    setup_fail "The installed CMake is still older than 3.20." \
      "Install a newer CMake release and ensure it is first on PATH."
  fi
fi

if ! command -v ninja >/dev/null 2>&1; then
  offer_package_fix "Ninja is not on PATH." ninja-build ninja
  command -v ninja >/dev/null 2>&1 ||
    setup_fail "Ninja is still not on PATH after the setup command." \
      "Install Ninja and ensure its bin directory is on PATH."
fi

if ! command -v python3 >/dev/null 2>&1; then
  offer_package_fix "Python 3 is not on PATH." python3 python
  command -v python3 >/dev/null 2>&1 ||
    setup_fail "Python 3 is still not on PATH after the setup command." \
      "Install Python 3 and ensure its bin directory is on PATH."
fi
if ! python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 0))' >/dev/null 2>&1; then
  offer_package_fix "The 'python3' command does not run Python 3." python3 python
  if ! python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 0))' >/dev/null 2>&1; then
    setup_fail "The installed 'python3' command still does not run Python 3." \
      "Install Python 3 and ensure its 'python3' command is first on PATH."
  fi
fi

if ! command -v emcmake >/dev/null 2>&1; then
  if [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
    activate_emsdk_or_fail "Emscripten is installed but not activated."
  else
    offer_fix "Emscripten is not installed or activated." \
      '  git clone https://github.com/emscripten-core/emsdk.git "$HOME/emsdk"
  "$HOME/emsdk/emsdk" install latest
  "$HOME/emsdk/emsdk" activate latest
  source "$HOME/emsdk/emsdk_env.sh"' install-emsdk
    if [[ "$setup_is_sourced" != true ]]; then
      setup_fail "Emscripten was installed, but it is not active in your current shell." \
        'Activate it in the current shell with:
  source "$HOME/emsdk/emsdk_env.sh"

Then rerun the check or workflow.'
    fi
  fi
  command -v emcmake >/dev/null 2>&1 ||
    setup_fail "'emcmake' is still not on PATH after the setup command." \
      'Activate it manually with:\n  source "$HOME/emsdk/emsdk_env.sh"'
fi

if ! command -v emcc >/dev/null 2>&1 || ! emcc --version >/dev/null 2>&1; then
  activate_emsdk_or_fail \
    "The Emscripten C compiler ('emcc') is unavailable or unusable."
  command -v emcc >/dev/null 2>&1 && emcc --version >/dev/null 2>&1 ||
    setup_fail "'emcc' is still unavailable or unusable after activation." \
      "Repair the Emscripten SDK, then activate it again."
fi

if ! command -v em++ >/dev/null 2>&1 || ! em++ --version >/dev/null 2>&1; then
  activate_emsdk_or_fail \
    "The Emscripten C++ compiler ('em++') is unavailable or unusable."
  command -v em++ >/dev/null 2>&1 && em++ --version >/dev/null 2>&1 ||
    setup_fail "'em++' is still unavailable or unusable after activation." \
      "Repair the Emscripten SDK, then activate it again."
fi

if ! command -v ctest >/dev/null 2>&1; then
  offer_package_fix "CTest is not on PATH." cmake cmake
  command -v ctest >/dev/null 2>&1 ||
    setup_fail "CTest is still not on PATH after the setup command." \
      "Install CMake with its CTest component and ensure it is on PATH."
fi

printf 'SilOS setup check passed.\n'
