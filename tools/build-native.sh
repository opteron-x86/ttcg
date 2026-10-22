#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/build-env.sh
python3 tools/setup-dependencies.py
export XWIN_SYSROOT="${XWIN_SYSROOT:-$PWD/build/toolchain/xwin}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
export VCPKG_MAX_CONCURRENCY="${VCPKG_MAX_CONCURRENCY:-4}"
mkdir -p build/vcpkg-cache
export VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-clear;files,$PWD/build/vcpkg-cache,readwrite}"
cmake --preset linux-msvc-tessera
cmake --build --preset linux-msvc-tessera
