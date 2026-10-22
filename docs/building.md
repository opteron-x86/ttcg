# Development

[Building Tessera](../BUILDING.md) covers compilers, dependencies and packages. [Testing](TESTING.md) lists automated and in-game checks.

The native interface is compiled into `TTCG.dll`. Gameplay code is in `include/`, Skyrim integration in `src/` and `native/skse/`, and interface code in `native/src/`.

## Local settings

Settings are saved to `SKSE/Plugins/TTCG.ini`. The file belongs to the installed mod. `SKSE/Plugins/TTCG.ini.example` lists the available options.

For development controls, set `[Development] Enabled=1`. The album then offers card grants, opponent access and tournament test controls. Use a separate test save.

For detailed logs, set `[Diagnostics] Enabled=1`. Normal logs cover loading, matches, rewards and errors; diagnostics add input, timing and dialogue traces.
