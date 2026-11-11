# Tessera UI overhaul: plan, status and handoff

Branch: `feat/oathvein-skin` (off `master` at `fac3141`). Nothing is pushed. Owner of the repo: opteron-x86.

The goal is a complete overhaul of the native (ImGui/DirectX 11) interface so it matches Skyrim and the player's UI mods by default, with a redesigned card, board and screen layouts. The original assessment that motivated this is summarised under [Origin](#origin). The player (the repo owner) approved the direction and the decisions below; they have **permission from every UI-mod author named here** (obtained by DM) to use the artwork.

## Decisions already made (do not re-litigate)

| Question | Decision |
|---|---|
| Default look | Match Skyrim's UI. A neutral Skyrim skin ships as the default (phase 5); mod-matching skins are optional. |
| Card frame | **One design across every skin.** A skin supplies font and accent only. |
| Game board | **Selectable materials in Settings**, plus a default board named by each skin. |
| Skin source art | Convert real artwork from the author's mods with `tools/build-skin.py`. Permission granted for Oathvein UI, Vel'dun UI, Norden UI (Nithog), Edge UI (EugeneUI), Dear Diary Dark Mode (uranreactor), Untarnished UI (Vorganger). |
| Keycaps | Read at runtime from the player's `Data/Interface/ImGuiIcons/Icons` (as Quickslot does); never bundled. |
| Image generation | OpenRouter through `opencode` is available (key already updated by the player). Optional, for painterly board art only. See [Image generation](#image-generation). |
| Commits | Committing on this branch is fine; package builds require a clean tree. |

## Status

All work is committed except the last step (three new skins, below), which must be committed first.

| Phase | Scope | State |
|---|---|---|
| 1 Skins as data | `skin.json` + textures; Oathvein, Vel'dun, Norden; converter; controller glyph prompts | **Done** (`2d1bf12`) |
| 2 Card face | One framed card design | **Done** (`1a18721`); verification items open |
| 3 Game board | Material boards with rail and recessed squares | **Done** (`84f231b`); refinements open |
| 4 Layouts and type | Match, lobby, results, journal, shop, album, type scale, fonts | **Partly done** (results summary, trade-rule label); most open |
| 5 In-game integration | Blur, packaging, neutral default skin, theme migration | **Partly done** (blur, skin packaging); rest open |
| Extra skins | Untarnished, Edge, Dear Diary | **Added, uncommitted, not yet previewed** |

Last verified: 89/89 native tests pass with the extra skins (before the final docs/tooling commit). The Windows DLL was last cross-built and probed at `2d1bf12` only; **rebuild and re-probe before handing a zip to the player** (see [Build](#build-test-and-package)). `dist/skins-phase1/` is stale.

### First actions for the next agent

1. `git status`; commit the pending work: `Add Untarnished, Edge and Dear Diary skins` plus `Add UI overhaul plan and preview tooling`. Run `bash tools/format.sh` and `ctest --preset native --parallel 8` first.
2. Preview the three new skins (they have never been rendered): `THEME=edge tools/preview-ui.sh edge-album --snapshot build/test-fixtures/album.json --page cards` and likewise for `untarnished`, `deardiary`. Fix anything broken (see [Skin checks](#phase-1-follow-ups)).
3. Rebuild the Windows DLL, run the Wine probes, package, and give the player the zip path (the player tests in Skyrim through MO2; nothing in this branch has been seen in-game yet).

## Architecture

### Where things live

| Area | Files |
|---|---|
| Theme list (saved index; append only, never reorder) | `include/InterfaceTheme.h` |
| Saved settings (INI fields) | `include/InterfaceSettings.h`, `SKSE/Plugins/TTCG.ini.example` |
| Built-in palettes, frame/tab/button/control drawing | `native/src/App.cpp` |
| Skin loader and helpers (nine-slice, bands, rules, key prompts, tracked caps) | `native/src/Skin.cpp`, declarations in `native/src/App.h` |
| Card face (framed) | `native/src/CardFace.cpp` (the borderless face stays in `App::card`) |
| Match, board, hands, result dialog | `native/src/GameMatch.cpp`, `native/src/GameTournaments.cpp` (`resultModal`, `tournamentOutcome`) |
| Window frame, header, prompt bar, settings sync | `native/src/GameApp.cpp` |
| Album detail panel | `native/src/GameAlbum.cpp` |
| Settings screens | `native/src/GameSettings.cpp` |
| Menu object, scene blur | `native/skse/Menu.cpp` |
| Preview host (desktop SDL/OpenGL, same screens) | `native/src/HostGL.cpp`, `native/src/main.cpp`, `tools/preview-ui.sh` |
| Skin textures | `TesseraUI/skins/<id>/` (format: `TesseraUI/skins/README.md`) |
| Board textures | `TesseraUI/assets/boards/` (`cell.png`, `<material>/surface.webp`, `<material>/frame.png`) |
| Generators | `tools/build-skin.py` + `tools/skin/recipes.json`, `tools/skin/swf_*.py`, `tools/build-board-textures.py` |
| Packaging file list | `tools/release_files.py` (skins globbed; boards listed) |
| Credits | `docs/licenses/Skin-credits.txt` (ships in Main) |

### Skins

A skin is `TesseraUI/skins/<theme id>/skin.json` plus textures. The folder name must equal a theme id in `InterfaceTheme.h`. `App::loadSkins()` reads them at startup; a skinned theme has `FrameStyle::Skinned` in its built-in palette row and the skin file overrides the palette, typeface (`jost|lato|serif|sans`), control colours, default board (`board`) and textures. Every texture is optional; missing ones fall back to flat fills or hairlines. Textures are authored at 3x Skyrim's 720p stage (1 texture pixel = 1 screen pixel at 2160p; `skinUnit()` = display height / 2160).

Pieces: `panel` (nine-slice; `margin`, `inset`, `tile`), `title`, `band`, `plate`, `selector`, `tab` (bands with fixed ends given by `cap`), `sigil` (watermark, `opacity`). Full table in `TesseraUI/skins/README.md`.

Behaviour of a skinned theme: no title bar or Size dropdown (a title band instead); a key-prompt bar at the bottom; card names and values in the skin's typeface; the player's own font, colour or border settings still win when changed from their defaults.

Adding a skin:
1. Append a row to `interfaceThemes` (`InterfaceTheme.h`) and a matching `themes[]` row (`App.cpp`, `FrameStyle::Skinned`) and a gradient row in `Surface.h` (static_assert enforces counts).
2. Add `TesseraUI/skins/<id>/skin.json`.
3. For textures, add a recipe to `tools/skin/recipes.json` and run `python3 -I tools/build-skin.py <id> --mods <MO2 mods folder>`. To find shapes, run `python3 -I tools/skin/survey.py "<mod folder>" <out dir>` and read `sheet.png`.
4. Update `native/tests/game_ui_test.cpp` expectations if needed, `docs/licenses/Skin-credits.txt`, `TesseraUI/skins/README.md`, `SKSE/Plugins/TTCG.ini.example`.

### The converter

UI mods keep panels as vector shapes inside SWF files. `tools/skin/swf_shapes.py` parses SWF shape tags (solid, gradient and bitmap fills, curved edges) into SVG; `swf_bitmaps.py` extracts lossless bitmaps; `build-skin.py` renders with `rsvg-convert` and crops with ImageMagick. Requirements: `rsvg-convert`, ImageMagick (`magick`). Python must run with `-I` and no PIL/numpy is needed.

### Board

A material is a field (`surface.webp`, stretched), a rail (`frame.png`, 512x512 nine-slice: 96 px corners, 64 px rail, inner 32 px is the rail's shadow on the field) and the shared `cell.png` (recess for each empty square). The Board texture setting (`face.grain`) fades the material into the plain board; at 10% or more it shows in full. Board ids: 1 Plain, 2 Slate, 3 Felt, 4 Parchment, 5 Leather, 6 Walnut, 7 Dark slab. `boardSurface` setting 0 means skin default, then theme default.

### Card face

`App::card` routes to `framedCard` unless `face.frame == 1` (Settings > Card face > Frame > Borderless). Bezel colour is rarity metal in the album and the controlling player's colour in a match; footer holds the tier gem, bottom value and affinities; each value sits on a plate on its edge. `App::valueAnchor(side, size)` is the single source of value positions (used by the face and by the rule-demo highlight circles).

## Remaining work

Ordered. Each item should end with preview screenshots reviewed by looking at them, then the test suite, then a commit.

### Phase 1 follow-ups

- Preview Untarnished, Edge, Dear Diary (album, match, lobby, dialogs). Known risks: Edge's `band.png` (1300x118) is used for both the title and the item-card band and may look wrong as a title; Dear Diary has no tab/selector art so buttons and tabs fall back to hairlines/flat fills; Untarnished has no art at all (palette only). Tune `skin.json` metrics, or add recipe entries, until each looks intentional. Untarnished should be a clean, flat white-on-black look after the mod.
- Settings labels still read "Default"/"Tessera" while a skin overrides card fonts, name case, value stroke, control palette and border width. Make the Settings screen show what is actually in effect, or add a "Skin default" choice.
- Controller glyph switching (`trackInputDevice`) is untested with a real pad. Verify in game; confirm button names exist in the player's ImGui Icons (`360_A`, `360_B`, `360_Y`).
- Add controller prompts for Back/Select consistent with what the screens actually bind (Esc/pad B closes, M1 selects/plays, M2/Y inspects). Don't list a control that doesn't exist.

### Phase 2: card face

- Verify the framed face everywhere it appears: deck slots and decks grid at all three densities, shop tiles, pack reveal, tournament prize cards, rule demonstrations (`rules.json` demos use `valueAnchor`; check the highlight circles line up), lobby setup cards, drag preview, the 3D inspector (`GameInspector.cpp`, unchanged and may not match), `card-faces` in tests.
- Foil: confirm the foil shader (`Renderer::material`) still reads well under the bezel, and that the bezel doesn't tint foil art. Consider a foil sheen on the bezel itself.
- Value legibility at small sizes (Small deck density, 720p). The 25-unit digit and plate sizes were tuned at 1080p only.
- Names: plates cover the artwork edges; check long expansion names and the "Printed on art" nameplate option (`face.nameplate == 2`) behave on the framed face. Currently the framed face ignores `nameplate`; decide whether to honour it.
- Classic value layout (`rankLayout == 1`) is drawn as a diamond plaque; check it, and that the count badge and affinity icons don't collide.
- Compact legend: a gem for tier is unlabelled; confirm the inspector or tooltip explains it.

### Phase 3: board

- Affinity markers: set them into the surface (inlay) instead of a translucent icon.
- Corner ornaments on the rail; optional skin-specific rail art.
- Turn indication on the board (rail glow on the side of whoever moves).
- Last-move highlight on the square just played.
- Check every material at 720p, 1080p and 4K, and with Board texture at 0, 10 and 100.
- Optional: painterly boards generated through OpenRouter (see below) to compare against the procedural ones; the procedural set is the shipping baseline and must remain.
- `TesseraUI/assets/themes/*.png` are no longer shipped but are still referenced by the web tools and an old test; leave them or delete with the web UI (the web UI under `TesseraUI/*.js` is not shipped or tested by CI and is a deletion candidate; confirm with the owner first).

### Phase 4: layouts and typography

Match screen
- Larger board; the side hands are small relative to the window. Fan or stack hands; show remaining cards clearly.
- Score as pips or a clear count with the leading side emphasised.
- Rule strip: play rules in one group; the trade rule already reads `Trade: One` etc. Consider a distinct chip style.
- Status line ("Your turn") should be a proper turn banner; show "thinking" state.
- Unplayed opposing cards: clear visual for hidden cards (Three Open etc.).

Lobby (`GameMatch.cpp::lobby`)
- Show the opponent's record against the player, rank/skill, regional rules in effect, wager and exactly what the player can win or lose under the trade rule.
- Deck picker and Edit deck stay.

Results (`GameTournaments.cpp`)
- Done: closing dialog states Victory/Defeat/Draw, score, cards and gold changed. Open: cards-won display (thumbnails), tournament-won screen layout (it is text only and the dialog is mostly empty), prize presentation, and the card-trade animation polish.

Journal (`GameJournal.cpp`)
- Scouting detail: location, record, favoured rules, cards seen, what the player has won from them. The right-hand detail panel is sparse.

Shop (`GameShop.cpp`)
- Apply the item-card treatment to the detail panel (as done in the album); consistent prompts.

Album grid
- Denser option; the count badge is now top-right on framed cards.

Typography
- Five-step type scale with a 16 px floor at 1080p; today there are ~17 sizes between 13 and 35 px and 11-12 px labels exist ("Expert", slot numbers). Audit `text(...)` call sizes in `native/src/*.cpp`.
- Bundle **Jost Medium/SemiBold** (OFL) so numerals stop being faux-bolded by overdrawing (`CardFace.cpp`, `App.cpp`); add to `tools/release_files.py`, `native/assets/fonts`, `native/licenses`, `docs/licenses/native`, `native/assets/fonts/SOURCES.md`.
- Enable FreeType rasterisation for ImGui 1.92.6 (currently stb_truetype) to improve small text; vendored in `native/third_party`, so this needs a dependency decision and an update to `native/third_party/manifest.json` and the pin checks.
- Real small caps for card names (currently synthesised in `CardName.h`); optional.
- A user font folder so players can match their font mod (optional).

### Phase 5: in-game integration

- Scene blur: implemented (`UIBlurManager` Increment/Decrement in `Menu.cpp`, unit-stubbed in `tests/native_input_test.py`). Not verified in Skyrim; check it works unpaused and that the count is released when the menu closes unexpectedly.
- Neutral **Skyrim** skin as the shipped default (vanilla/SkyUI idiom: dark translucent ground, hairlines, Jost, caps labels, prompt bar). Changing the default theme index needs care: `Theme=0` in saved INIs currently means Tessera. Decide: keep index 0 as the old theme and change only the default for new installs (INI default), or migrate. Existing players must not have their look silently changed.
- First-run detection of the installed UI mod (Oathvein, Vel'dun, Norden, Edge, Dear Diary, Untarnished, SkyUI) to pick a skin; only for new installs, and overridable.
- Fold Charcoal, Oxblood and Umber into colour options of the neutral skin without breaking saved theme indices (indices are append-only; hide them from the picker but still honour them).
- Packaging: skins are globbed by `tools/release_files.py`, boards are listed. Keep `tests/release_package_test.py` and `tests/github_package_test.py` current. The Main zip size is dominated by art; the new assets add ~1.5 MB.
- Update `docs/reference-guide.md`, `README.md`, `CHANGELOG.md` and `docs/TESTING.md` (add an in-game checklist for the new UI) when the work ships. `docs/TESTING.md` should record Skyrim 1.6.1170 and 1.7.104 coverage.

### Also noted during the assessment (not part of this overhaul)

- Master AI is no stronger than Expert when the player's hand is hidden: `include/Opponents.h::opponentMove` samples one random hand from all 703 cards and searches 7 plies against it. Sampling several hands near the shown tiers should fix it. A simulation harness reproduced it (Master vs Expert 220W/274L/306D over 800 games, tier III decks, no Open). The harness was a throwaway; rebuild it from `include/Match.h` and `Opponents.h`.
- The 1.7.104 merchant-inventory death/reload crash is unexplained (see `docs/TESTING.md`).
- Card art provenance: `docs/card-art-prompts` and `TesseraUI/Tessera_Card_Art_*.md` suggest generated art; verify Nexus AI-content rules.
- Transmutation is parked on `codex/transmute-testing` and may be redone; it bumps the save format to 34. Do not build on it without asking.

## Build, test and package

```
bash tools/format.sh                       # CI runs --check with clang-format 23.1.4
cmake --preset native && cmake --build --preset native
ctest --preset native --parallel 8         # 89 tests, ~90 s; native_game_ui is the long one
tools/preview-ui.sh NAME --snapshot build/test-fixtures/<fixture>.json [--page ...]   # review a screen
```

Fixtures: `build/test-fixtures/*.json` (album, match, opening, lobby, shop, pack, trade, tournaments, `final-c*-r*`, `combo-*`). `tools/test-art-fixtures.py` creates placeholder card art; the real art lives in `TesseraUI/art/` (not tracked). Useful preview options: `--theme <id>`, `--page cards|decks|journal|rules|tournaments|appearance|match-appearance|controls|gameplay|developer|buy|sell|buyback`, `--topic`, `--demo-step`, `--inspect`, `--backdrop IMAGE` (draws a game screenshot behind the UI), `--key-icons DIR` (an ImGui Icons folder). Render with `FR=300+` for result screens so animations settle. Screenshots are BMP converted by ImageMagick; always look at the images.

Settings values can be previewed by editing the snapshot JSON (`settings.boardSurface`, `settings.cardFrame`, `settings.rankLayout`, `settings.theme`, ...).

In-game build (the player drops the zip into MO2 downloads; Skyrim SE 1.6.1170 under Proton). Build from a **committed, clean** tree:

```
bash tools/build-native.sh          # clang-cl cross build -> build/linux-clangcl/TTCG.dll
bash tools/test-native.sh           # Wine probes -> build/test-results/windows-tests.json
python3 tools/package-release.py --build-dir build/linux-clangcl \
  --validation build/test-results/windows-tests.json --output-dir dist/<name>
```

Hand over the absolute path of `dist/<name>/Tessera-<ver>-dev.<hash>-Main.zip`. Compare with the previous Main zip's file hashes to confirm only intended files changed. Skyrim UI mods' assets and the player's mod folder are at `/mnt/mods/Amaranth/mods` (profile `KINMUNE CS`); the player runs Oathvein UI, Dear Diary Dark Mode, Natural Typefaces and Quickslot, so Oathvein is the look to be judged against in game.

## Image generation

`opencode` 1.19.5 has an OpenRouter credential. At the time of writing `opencode models` listed only text models (deepseek, kimi, glm) under OpenRouter, so no image model was confirmed callable. Before relying on it, check `opencode models` and OpenRouter's image-capable models, then `opencode run -m openrouter/<model> "<prompt>"`. Generated art is optional; do not make shipping assets depend on it. Never print or store the API key. Any generated asset needs a recorded prompt and licence note beside it.

## Licensing rules

- Skin textures are third-party art used with permission, not GPL; credited in `docs/licenses/Skin-credits.txt` and `TesseraUI/skins/README.md`. Keep the credits current when adding a skin. The player's note that the Oathvein panels derive partly from other authors' work was answered: permission was confirmed by DM.
- No fonts exported from game or UI-mod files are bundled. Futura Book (the Untarnished font) has no established redistribution grant: do not bundle it.
- Keycap images are never bundled.
- The repo is public (GPL-3.0). Tell the owner when asset licensing might matter for the Nexus page.

## Origin

The first assessment found: a well-engineered mod (703 cards, 320 opponents, 17 rules, reputation, tournaments, trading, collection API, 89 tests); an interface that read as a desktop window with thumbnails; weak ownership readability and legibility on the card face; flat result moments; a Times-like default font with synthesised small caps; and a Master AI that doesn't scale when hands are hidden. The overhaul targets the interface first; gameplay ideas (capture preview, opponent personalities, scouting journal, Speech-linked prices) are discussion items for later.
