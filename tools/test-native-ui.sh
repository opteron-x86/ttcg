#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --preset native
cmake --build --preset native
ctest --preset native -R '^native_(fixtures|session|game_ui)$'
