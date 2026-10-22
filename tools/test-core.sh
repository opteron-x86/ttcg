#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/setup-dependencies.py --native
python3 tools/test-art-fixtures.py
cmake --preset native
cmake --build --preset native
ctest --preset native
