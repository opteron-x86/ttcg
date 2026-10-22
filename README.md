# Tessera Trading Card Game

Tessera adds a collectible card game to Skyrim, inspired by Triple Triad. Challenge NPCs, build decks, trade cards and enter tournaments across Skyrim and Solstheim.

## Downloads

Install **Main** and **Card Art** through your mod manager, then enable `Tessera TCG.esp`.

- [Main, source and developer downloads](https://github.com/opteron-x86/ttcg/releases)
- [Card Art and optional addons on Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/193442?tab=files)
- [Development builds](https://github.com/opteron-x86/ttcg/actions/workflows/build.yml)

Card faces and thumbnails are distributed on Nexus. Main contains the DLL, plugin, scripts, sounds, card backs and interface assets.

Tessera requires SKSE and the Address Library for your Skyrim runtime. The plugin uses Skyrim.esm and Dragonborn.esm. Runtimes before 1.6.1130 need [Backported Extended ESL Support](https://www.nexusmods.com/skyrimspecialedition/mods/106441). Gameplay testing is recorded in [Testing](docs/TESTING.md).

## Playing

Buy a Tessera Album from a general-goods merchant for 100 gold. It comes with eight starter cards and a five-card deck. Open the album with **F7**, or challenge a nearby player through dialogue or **F8**. Shortcuts can be changed in Settings.

Win cards through matches, buy packs and individual cards, and earn tournament prizes. Regional rules, opponents' collections and your reputation develop as you play. Games with children are casual.

The album includes rule demonstrations, saved decks, a journal and appearance settings. Mouse, keyboard and controller input are supported.

## Guides

[Playing Tessera](docs/reference-guide.md) · [Opponents](docs/opponents-by-hold.md) · [Tournaments](docs/tournaments.md) · [Card trading](docs/card-trading.md) · [Replacing artwork](docs/card-art-replacers.md)

[Build from source](BUILDING.md) · [Releases](docs/RELEASING.md) · [Card expansions](docs/card-expansions.md) · [Collection API](docs/collection-api.md) · [Exchange API](docs/exchange-api.md)

Source is licensed under GPL-3.0. Dependency notices are in [docs/licenses](docs/licenses).
