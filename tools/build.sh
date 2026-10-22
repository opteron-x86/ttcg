#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/test-core.sh
bash tools/build-native.sh
bash tools/test-native.sh
python3 tools/package-release.py --build-dir build/linux-clangcl --output-dir dist/github --validation build/test-results/windows-tests.json
