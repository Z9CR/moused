#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo $XDG_SESSION_TYPE
echo $WAYLAND_DISPLAY
cmake -B "$ROOT_DIR/build"
cmake --build "$ROOT_DIR/build"
"$ROOT_DIR/build/moused"
echo $XDG_SESSION_TYPE
echo $WAYLAND_DISPLAY
