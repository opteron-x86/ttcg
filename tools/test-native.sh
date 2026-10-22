#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export WINEPREFIX="${WINEPREFIX:-$PWD/build/toolchain/wine}"
export WINEDEBUG="${WINEDEBUG:--all}"
python3 tools/test-windows.py --build build/linux-clangcl --output build/test-results
