# Tessera source

Source for Tessera, a Skyrim trading card game using SKSE and Meridian UI.

- `src/Plugin.cpp`: SKSE entry point, game hooks, commands, and serialization.
- `include/`: card rules, collections, opponents, tournaments, trading, and the Meridian bridge. Card/opponent definitions are included.
- `MeridianUI/ttcg/`: the HTML, CSS, JavaScript, and foil grain SVG used by the interface.
- `Scripts/Source/`: Papyrus dialogue, album, courier, and encounter glue.
- `SKSE/Plugins/TTCG.ini.example`: configuration reference.

## Native build

The supplied preset cross-compiles a Windows x64 DLL on Linux. Install Git, Python, CMake, Ninja, clang-cl/LLD, Wine, xwin, curl, and archive utilities. Dependency versions are pinned in `cmake/dependencies.json`.

```bash
python3 tools/setup-dependencies.py
export LLVM_MINGW_BIN="$PWD/build/toolchain/llvm-mingw-20260908-ucrt-ubuntu-22.04-x86_64/bin"
export WINEPREFIX="$PWD/build/toolchain/wine"
cmake --preset linux-msvc-meridian
cmake --build --preset linux-msvc-meridian
```

Output: `build/runtime-meridian/SKSE/Plugins/TTCG.dll`. Papyrus compilation separately requires Bethesda's compiler and the game/SKSE script sources.
