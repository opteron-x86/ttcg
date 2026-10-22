#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/validation
python3 tests/completionist_records_test.py
clang++ -std=c++23 -O1 -g -fsanitize=address,undefined \
  -Itests/completionist_stubs -Iinclude -Ibuild/vcpkg_installed/x64-windows-clangcl/include \
  tests/completionist_bridge_test.cpp -o build/validation/completionist-bridge-test
ASAN_OPTIONS=detect_leaks=1 build/validation/completionist-bridge-test
completionist_dotnet="${TESSERA_DOTNET:-$HOME/.dotnet/dotnet}"
"$completionist_dotnet" run --project tests/completionist_records_validator/Validate.csproj -- \
  addons/completionist/'Tessera Completionist.esp'
completionist_mingw=$(python3 -c 'import json; print(json.load(open("cmake/dependencies.json"))["llvm_mingw"]["directory"])')
"$PWD/build/toolchain/$completionist_mingw/bin/x86_64-w64-mingw32-clang" \
  -O2 -static tests/completionist_plugin_probe.c -o build/validation/completionist-plugin-probe.exe
export WINEPREFIX="$PWD/build/toolchain/wine"
export WINEDEBUG=-all
wine build/validation/completionist-plugin-probe.exe "Z:${PWD//\//\\}\\addons\\completionist\\SKSE\\Plugins\\TesseraCompletionist.dll"
