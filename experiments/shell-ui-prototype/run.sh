#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
port="${1:-8765}"

echo "SilOS Shell UI prototype: http://localhost:${port}/?profile=browser"
python3 -m http.server "${port}" --directory "${script_dir}"
