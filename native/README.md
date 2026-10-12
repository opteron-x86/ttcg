# Tessera native UI

The production interface is compiled into `TTCG.dll`. It uses Dear ImGui and DirectX 11 inside Skyrim’s `TesseraMenu`; it has no Meridian, Chromium or browser dependency. The game backend, Papyrus APIs and co-save formats are unchanged.

- `src/App.cpp`: shared cards, palettes, fonts and controls.
- `src/Game*.cpp`: all game screens, settings, tutorials, inspection and results.
- `src/Assets.cpp`, `Material.cpp`, `RendererDX11.cpp`: artwork cache and Prismatic foil.
- `skse/Host.cpp`, `Menu.cpp`, `Session.h`: engine input, menu/pause ownership and session lifetime.
- `src/DXIsolation.h`: graphics-state restoration around rendering.

Commands retain the existing session/revision checks. Skyrim objects are accessed on the game thread; the renderer receives immutable JSON snapshots. Frame clocks drive placements, simultaneous capture waves, Swap, opening and result callouts. An acknowledged revision cannot replay. Engine interruption and timeout recovery still use the existing saved-match system.

Runtime resources are generated under `TesseraUI/`. Card Art, expansions, replacers and custom backs use `TesseraUI/art/`. When upgrading from 0.29.4 or earlier, update both Main and Card Art and relocate addon artwork as described in [the migration guide](../docs/asset-path-migration.md). Browser sources are retained for authoring tools and tutorial generation, outside release packages.

## Build and verify

From the repository root:

```sh
bash tools/build-native.sh
bash tools/test-core.sh
bash tools/test-native.sh
bash tools/test-native-ui.sh
```

The native UI test exercises production screens, input, deck dragging, trade selection and inspection, interrupted matches, starter/pack receipts, all interactive rules examples, tournament victory/reward paths, five themes and several resolutions. Session tests check stale input, focus transitions and cleanup. The Windows loader probe does not call engine functions.

`build/native-ui/TesseraNativeUI` is a desktop verification host for the **same frontend**, not a separate practice game. For example:

```sh
build/native-ui/TesseraNativeUI --snapshot build/native-migration/fixtures/album.json --page decks
```

On Windows, CMake builds this host with DirectX 11 and `preview_dx_state_test.exe`. The latter checks context restoration, including targets, viewport, scissor, raster/blend states, samplers, constant buffers, resources and compute UAVs. The legacy test executable name is not a shipped plugin. Linux uses SDL/OpenGL for screenshots and interaction tests. No desktop executable or test fixture ships with the mod.

Runtime ownership is a single DLL. Disable the old **Tessera Native UI Preview** MO2 entry before testing production. It overrides `TTCG.dll` and must not remain enabled.
