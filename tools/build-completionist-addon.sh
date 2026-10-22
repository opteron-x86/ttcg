#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/build-env.sh
python3 tools/build-completionist-addon.py
TESSERA_PAPYRUS_SOURCES="$PWD/addons/completionist/Scripts/Source" \
TTCG_PAPYRUS_OUTPUT="$PWD/addons/completionist/Scripts" \
TESSERA_PAPYRUS_EXTRA_IMPORTS="$PWD/addons/completionist/headers" bash tools/build-papyrus.sh
export PATH="$PWD/build/toolchain/host/usr/bin:$PATH"
export XWIN_SYSROOT="${XWIN_SYSROOT:-$PWD/build/toolchain/xwin}"
completionist_mingw_dir=$(python3 -c 'import json; print(json.load(open("cmake/dependencies.json"))["llvm_mingw"]["directory"])')
export LLVM_MINGW_BIN="${LLVM_MINGW_BIN:-$PWD/build/toolchain/$completionist_mingw_dir/bin}"
cmake --preset linux-msvc-tessera
cmake --build build/linux-clangcl --target TesseraCompletionist -j 4
