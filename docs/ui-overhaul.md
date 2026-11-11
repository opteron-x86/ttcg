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
| Image generation | Optional generated board art or a shared card-frame asset, using the built-in image tool. Preserve the procedural board set. See [Image generation](#image-generation). |
| Commits | Committing on this branch is fine; package builds require a clean tree. |

## Status

The player confirmed scene blur in Skyrim on 10 November 2026. The controller/Edge follow-up fixes right-stick glyph switching, cursor activation and cross-panel navigation, and replaces Edge’s sparse treatment with its inventory backdrop, selection art and a separate detail panel. Physical-controller verification of this follow-up remains open.

| Phase | Scope | State |
|---|---|---|
| 1 Skins as data | `skin.json` + textures; Oathvein, Vel'dun, Norden; converter; controller glyph prompts | **Done** (`2d1bf12`); Settings/prompt follow-ups complete |
| 2 Card face | One framed card design | **Done** (`1a18721`); verification items open |
| 3 Game board | Material boards with rail and recessed squares | **Done** (`84f231b`); refinements open |
| 4 Layouts and type | Match, lobby, results, journal, shop, album, type scale, fonts | **Partly done** (results summary, trade-rule label); most open |
| 5 In-game integration | Blur, packaging, neutral default skin, theme migration | **Partly done** (blur, skin packaging); rest open |
| Extra skins | Untarnished, Edge, Dear Diary | **Committed** (`04c0924`), desktop previews reviewed |

Last verified: **89/89 native tests pass**, including right-stick cursor glyphs, cursor A/Y activation, held-button mode changes, disconnect cleanup, dropdown dismissal, cross-panel navigation and framed/borderless Settings visibility. Native build, formatting, and preview-script failure checks pass. **Rebuild and re-probe before handing a zip to the player** (see [Build](#build-test-and-package)); the checkpoint output directory is `dist/controller-edge-followups/`, whose `build-info.json` records the exact tested source and DLL. `dist/skins-phase1/` is stale.

### First actions for the next agent

1. Check `git status` and the checkpoint's `dist/controller-edge-followups/build-info.json`; if source has changed, rebuild/probe/package rather than handing over an older DLL.
2. Have the player run the native UI checkpoint checklist in `docs/TESTING.md` with the new Main zip. Blur is confirmed in normal Skyrim use. Desktop previews and synthetic controller input do not establish physical-controller behaviour or unexpected-close blur cleanup.
3. Continue with the phase 2 card-face verification list below, then the ordered board, layout and typography work. Keep existing theme indices and the agreed shared card design.

### Phase 1 follow-up checkpoint

- Initial album, match, lobby and result previews are under `build/previews/phase1-checkpoint/before/`. Edge’s revised inventory backdrop, gold selection bands, tabs and detail panel use artwork from the installed Edge UI. The message-box frame remains on dialogs. Follow-up previews are under `build/previews/controller-edge/`: album, lobby, match and results at 1080p, plus the album at 720p and 4K.
- Settings now names skin fonts and control colours, identifies inherited lettering/stroke/border defaults, and preserves saved setting values. Nameplate and numeral-finish controls appear only for the borderless face; additional-border controls appear on framed cards only when the control marker enables that border.
- Footer prompts follow the active page. Inspection is advertised for card/deck views, shops, lobbies and matches; modal dialogs suppress background prompts. Lobby cards now accept focused `I`/pad Y inspection alongside the existing mouse action. Runtime icon names `360_A`, `360_B`, `360_Y`, `Esc`, `Mouse1`, `Mouse2` were confirmed in the installed icon pack; no icons are bundled.
- `tools/preview-ui.sh` renders into a temporary directory, propagates host/conversion failure, and replaces the output PNG only after success. An invalid-fixture check confirmed that an older PNG is preserved and stale BMPs cannot produce a false success.
- Updated Settings previews, including runtime keycaps, are under `build/previews/phase1-checkpoint/after/`. Normal Skyrim blur is player-confirmed. The revised controller behavior and remaining phases still need in-game verification.

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
| Controller cursor/navigation and input device | `native/skse/Host.cpp`, `native/skse/MenuInput.h` |
| Menu object, scene blur | `native/skse/Menu.cpp` |
| Preview host (desktop SDL/OpenGL, same screens) | `native/src/HostGL.cpp`, `native/src/main.cpp`, `tools/preview-ui.sh` |
| Skin textures | `TesseraUI/skins/<id>/` (format: `TesseraUI/skins/README.md`) |
| Board textures | `TesseraUI/assets/boards/` (`cell.png`, `<material>/surface.webp`, `<material>/frame.png`) |
| Generators | `tools/build-skin.py` + `tools/skin/recipes.json`, `tools/skin/swf_*.py`, `tools/build-board-textures.py` |
| Packaging file list | `tools/release_files.py` (skins globbed; boards listed) |
| Credits | `docs/licenses/Skin-credits.txt` (ships in Main) |

### Skins

A skin is `TesseraUI/skins/<theme id>/skin.json` plus textures. The folder name must equal a theme id in `InterfaceTheme.h`. `App::loadSkins()` reads them at startup; a skinned theme has `FrameStyle::Skinned` in its built-in palette row and the skin file overrides the palette, typeface (`jost|lato|serif|sans`), control colours, default board (`board`) and textures. Every texture is optional; missing ones fall back to flat fills or hairlines. Textures are authored at 3x Skyrim's 720p stage (1 texture pixel = 1 screen pixel at 2160p; `skinUnit()` = display height / 2160).

Pieces: `panel` (nine-slice; `margin`, `inset`, `tile`), optional `window`/`detail` panels, `title`, `band`, `plate`, `selector`, `tab` (bands with fixed ends given by `cap`), `sigil` (watermark, `opacity`). Full table in `TesseraUI/skins/README.md`.

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

- **Done:** Edge uses its own inventory backdrop and selection art, with a gold-framed detail panel and message-box dialogs. Untarnished remains a clean white-on-black palette.
- **Done:** Settings displays effective skin defaults and hides framed-card controls that do not apply. Existing persisted meanings are unchanged; selecting a skin default still follows the skin.
- **Done in automation:** page-specific Back/Close/Select/Play/Inspect prompts, keyboard/pad switching and focused pad Y inspection in the lobby. Icon files exist in the installed pack.
- **Implemented and tested:** original SKSE input events determine glyphs; right-stick cursor movement preserves pad prompts. A/Y operate the cursor after right-stick use; left stick/D-pad resume focus navigation across panel boundaries. Mode changes and disconnect release controller clicks. Focus outlines remain visible on lobby cards, hands, board cells and shop cards. B closes a dropdown without closing the underlying screen.
- **Open:** repeat these checks with a real pad and confirm keyboard/mouse fallback in Skyrim.

### Phase 2: card face

- Verify the framed face everywhere it appears: deck slots and decks grid at all three densities, shop tiles, pack reveal, tournament prize cards, rule demonstrations (`rules.json` demos use `valueAnchor`; check the highlight circles line up), lobby setup cards, drag preview, the 3D inspector (`GameInspector.cpp`, unchanged and may not match), `card-faces` in tests.
- Foil: confirm the foil shader (`Renderer::material`) still reads well under the bezel, and that the bezel doesn't tint foil art. Consider a foil sheen on the bezel itself.
- Value legibility at small sizes (Small deck density, 720p). The 25-unit digit and plate sizes were tuned at 1080p only.
- Names: plates cover the artwork edges; check long expansion names. The framed face has a fixed nameplate; the "Printed on art" option (`face.nameplate == 2`) is now offered only for the borderless face. Any later expansion of framed nameplate styles needs its own visual verification.
- Classic value layout (`rankLayout == 1`) is drawn as a diamond plaque; check it, and that the count badge and affinity icons don't collide.
- Compact legend: a gem for tier is unlabelled; confirm the inspector or tooltip explains it.

### Phase 3: board

- Affinity markers: set them into the surface (inlay) instead of a translucent icon.
- Corner ornaments on the rail; optional skin-specific rail art.
- Turn indication on the board (rail glow on the side of whoever moves).
- Last-move highlight on the square just played.
- Check every material at 720p, 1080p and 4K, and with Board texture at 0, 10 and 100.
- Optional: painterly boards generated with the image tool (see below) to compare against the procedural ones; the procedural set is the shipping baseline and must remain.
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

- Scene blur: implemented (`UIBlurManager` Increment/Decrement in `Menu.cpp`, unit-stubbed in `tests/native_input_test.py`) and player-confirmed in Skyrim. Unexpected-close cleanup and paused/unpaused coverage remain on the checklist.
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

### Skins, boards and foil checkpoint (2026-11-11)

- Edge now lays its lists on a translucent ground with no title band; Dear Diary uses the mod's own bronze-cornered panel and red plaques with a serif face. Both were checked against the mods' published screenshots.
- Untarnished, Nordic (theme 11) and Outlander (theme 12) are flat-ground skins: `"ground"` in `skin.json` sets the translucent window fill, the dialog fill, whether a hairline border is drawn, underline-style selection and optional corner marks. They carry no textures. They were matched to player-supplied screenshots; their display fonts are not bundled, so Nordic uses Jost and Outlander the serif face.
- Boards use painted fields from `tools/board-art/` (generated with the Codex CLI image tool, prompts in its README); rails, shadows and recesses are still drawn by `tools/build-board-textures.py`, and `--procedural` rebuilds the earlier set.
- The foil shader in `native/src/Material.cpp` was rewritten: the film tints the printed art at constant brightness, with a moving sheen and flakes, so shadows stay dark. Not yet seen in game.
- Card names never drop below nine real pixels. Shop tiles, pack reveals, the rule demo and the classic layout were checked on the framed face; tournament prize cards, the drag preview and the three deck densities were not.
- The player decided to keep the drawn card frame for now.

Hand over the absolute path of `dist/<name>/Tessera-<ver>-dev.<hash>-Main.zip`. Compare with the previous Main zip's file hashes to confirm only intended files changed. Skyrim UI mods' assets and the player's mod folder are at `/mnt/mods/Amaranth/mods` (profile `KINMUNE CS`); the player runs Oathvein UI, Dear Diary Dark Mode, Natural Typefaces and Quickslot, so Oathvein is the look to be judged against in game.

## Image generation

Generated art is optional. Use the Codex CLI image tool (`codex exec --skip-git-repo-check --sandbox workspace-write -C DIR "Use your image generation tool to create ONE image and save it as NAME.png …"`) for painterly boards or a shared card-frame asset; keep the procedural board set available. Record the final prompt, tool and provenance beside any accepted asset. Implement code directly, as requested by the player.

## Licensing rules

- Skin textures are third-party art used with permission, not GPL; credited in `docs/licenses/Skin-credits.txt` and `TesseraUI/skins/README.md`. Keep the credits current when adding a skin. The player's note that the Oathvein panels derive partly from other authors' work was answered: permission was confirmed by DM.
- No fonts exported from game or UI-mod files are bundled. Futura Book (the Untarnished font) has no established redistribution grant: do not bundle it.
- Keycap images are never bundled.
- The repo is public (GPL-3.0). Tell the owner when asset licensing might matter for the Nexus page.

## Origin

The first assessment found: a well-engineered mod (703 cards, 320 opponents, 17 rules, reputation, tournaments, trading, collection API, 89 tests); an interface that read as a desktop window with thumbnails; weak ownership readability and legibility on the card face; flat result moments; a Times-like default font with synthesised small caps; and a Master AI that doesn't scale when hands are hidden. The overhaul targets the interface first; gameplay ideas (capture preview, opponent personalities, scouting journal, Speech-linked prices) are discussion items for later.
