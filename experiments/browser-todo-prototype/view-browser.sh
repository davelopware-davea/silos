#!/usr/bin/env bash
# Build and serve the Browser to-do proof.
set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="$SCRIPT_DIR/build"
readonly SURFACE="$BUILD_DIR/browser-surface.html"
readonly PORT="${1:-8765}"

if [[ ! "$PORT" =~ ^[0-9]+$ ]] || (( PORT < 1 || PORT > 65535 )); then
  echo "error: port must be an integer from 1 through 65535 (got '$PORT')." >&2
  exit 2
fi

"$BASH" "$SCRIPT_DIR/build.sh"

if [[ ! -f "$SURFACE" ]]; then
  echo "error: build completed but did not stage $SURFACE." >&2
  exit 1
fi

declare -a PYTHON
if command -v python3 >/dev/null 2>&1 && python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 0))'; then
  PYTHON=(python3)
elif command -v python >/dev/null 2>&1 && python -c 'import sys; raise SystemExit(sys.version_info < (3, 0))'; then
  PYTHON=(python)
elif command -v py >/dev/null 2>&1 && py -3 -c 'import sys; raise SystemExit(sys.version_info < (3, 0))'; then
  PYTHON=(py -3)
else
  cat >&2 <<'EOF'
error: Python 3 is required to serve the Browser proof.
Install Python 3, then rerun this script.
EOF
  exit 1
fi

echo "Serving the Browser proof at http://127.0.0.1:$PORT/browser-surface.html"
echo "Open that URL in a browser. Press Ctrl-C here when you are finished."
"${PYTHON[@]}" -m http.server "$PORT" --bind 127.0.0.1 --directory "$BUILD_DIR" &
SERVER_PID=$!

cleanup() {
  if kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

# Surface a common port/conflicting-launcher failure before asking the user to
# visit the URL. The server continues to run until Ctrl-C after this check.
sleep 0.2
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "error: the local server did not start (is port $PORT already in use?)." >&2
  wait "$SERVER_PID" || true
  exit 1
fi

wait "$SERVER_PID"
