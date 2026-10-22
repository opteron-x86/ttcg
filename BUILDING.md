# Building Tessera

## Windows DLL

Install Git, Python 3.11+, CMake 3.25+, Ninja, LLVM and Visual Studio 2022 C++ Build Tools with a Windows SDK. Run these commands in an x64 developer shell with `clang-cl` on PATH:

```powershell
python tools/setup-dependencies.py
cmake --preset windows-release
cmake --build --preset windows-release
python tools/test-windows.py --build build/windows --output build/test-results
```

The DLL and symbols are `build/windows/TTCG.dll` and `TTCG.pdb`. Dependencies are pinned in `cmake/dependencies.json`; the setup script fetches them into `build/deps`. The build enables SE, AE and VR in alandtse's CommonLibSSE-NG.

The ESP, SEQ and compiled Papyrus scripts are included in the repository. `cmake/runtime-assets.json` checks those files and their script sources. The DLL build uses them as supplied.

## Linux tests and preview

Use GCC 14+, CMake, Ninja, Python 3.11+, Pillow, SDL2, OpenGL and libwebp development packages.

```sh
python3 tools/setup-dependencies.py --native
python3 tools/test-art-fixtures.py
cmake --preset native
cmake --build --preset native
ctest --preset native
```

The fixture command creates geometric card images in the ignored card-art folders. It fills missing files and preserves locally installed artwork. To preview the album after testing:

```sh
build/native/native/TesseraNativeUI --snapshot build/test-fixtures/album.json
```

For a Windows DLL from Linux, install clang-cl, lld-link, llvm-rc, xwin and Wine, then run `bash tools/build-native.sh`. The script downloads the pinned SDK and build dependencies. Run `bash tools/test-native.sh` to test that DLL through Wine. `XWIN_SYSROOT` can select an existing Windows SDK.

## Packages

Commit the source, build it and run the Windows probes, then:

```sh
python tools/package-release.py --build-dir build/windows --validation build/test-results/windows-tests.json --output-dir dist/github
```

This produces Main, source, symbols and API ZIPs with checksums. GitHub runs the same commands on each push. See [Releasing](docs/RELEASING.md).

## Papyrus and plugin records

To edit scripts, set `TESSERA_GAME_DATA`, `TESSERA_PAPYRUS_COMPILER` and `TESSERA_SKSE_SOURCES`, then run `bash tools/build-papyrus.sh` through Wine. `TESSERA_PAPYRUS_FLAGS` can select a flags file. Update the corresponding hashes in `cmake/runtime-assets.json` after compiling.

Plugin authoring uses `python3 tools/build-plugin.py`. The `--check` option verifies generated records. Run the native tests and test dialogue, quests and save loading in Skyrim after changing records or scripts.

Install a local build with `python3 tools/install-mod.py --target /path/to/mod`. The installer requires the complete local runtime, including the Nexus card art. It backs up files it replaces under `build/backups`.
