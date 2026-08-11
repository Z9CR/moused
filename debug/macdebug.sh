#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cmake -B "$ROOT_DIR/build"
cmake --build "$ROOT_DIR/build"
"$ROOT_DIR/build/moused.app/Contents/MacOS/moused"
