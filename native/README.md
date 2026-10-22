# Native interface

`TTCG.dll` renders the interface with Dear ImGui and DirectX 11. The desktop preview uses the same screens with SDL/OpenGL on Linux.

| Files | Purpose |
|---|---|
| `src/App.cpp`, `src/Game*.cpp` | Screens, controls and presentation |
| `src/Assets.cpp`, `src/Material.cpp` | Images and foil effects |
| `src/RendererDX11.cpp`, `src/DXIsolation.h` | Rendering and graphics state |
| `skse/Host.cpp`, `skse/Menu.cpp` | Skyrim input and menu ownership |
| `skse/Session.h` | Menu session state |

Game state is updated on the game thread. The interface reads JSON snapshots and submits commands with their session and revision. Runtime resources are under `TesseraUI/`.

See [Building](../BUILDING.md) and [Testing](../docs/TESTING.md).
