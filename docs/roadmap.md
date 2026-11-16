# Tessera roadmap: plan, status and handoff

Started 2026-11-16 from a full review of `master` at `6fc37d1` (0.33.0, 90/90 native tests passing). It replaces the open lists in [ui-overhaul.md](ui-overhaul.md); those are carried into phase 5.

## Status

| Phase | Scope | Branch | State |
|---|---|---|---|
| 1 | Bugs | `phase1/bug-fixes` | Planned |
| 2 | Merchant packs through SkyPatcher (1.7.104 crash) | `phase2/skypatcher-packs` | Planned |
| 3 | AI | `phase3/ai` | Planned |
| 4 | Balance: first move, regional rules, trade rules | `phase4/balance` | Planned; decisions D1–D3 needed |
| 5 | Interface and UX | `phase5/interface` | Planned |
| 6 | Tessera green-and-gold skin | `phase6/tessera-skin` | Planned; decision D5 needed |
| 7 | Code cleanup | `phase7/cleanup` | Planned; C1 can go earlier |

## How the work runs

- One branch and one PR per phase, cut from `master` after the previous phase merges. Tick items here as they land; this file is the handoff for the next session.
- A phase is done when `ctest --preset native` passes, `tools/format.sh --check` is clean, UI changes have reviewed previews, and the player has checked a Main zip in Skyrim (see [Build](#build-test-and-package)). Then merge.
- Suggested releases: **0.33.1** after phases 1–2, **0.34.0** after phases 3–4, **0.35.0** after phases 5–6. Cleanup rides along with the phase whose files it touches; the rest ships in 0.35.x. Changelog entries are one line per change.
- **Save formats.** `master` writes collection format 33. The parked `codex/transmute-testing` branch writes 34 with a different layout, so the next bump on `master` must be **35**, and 34 stays unsupported unless transmutation lands. The saved-match record is format 1 (65 words); G1 needs format 2.
- Scratch harnesses from the review are in `build/ai-lab-scratch/` (untracked; see its README). A5 turns the useful ones into a tool.

## Decisions

Made by the player on 2026-11-16:

- Fix every bug in phase 1.
- Merchant packs move from runtime injection to a SkyPatcher container patch. SkyPatcher's source is on GitHub, which fits the source-available SKSE rule.
- Fix the Master AI, including hidden-hand guesses drawn from every card (tier 8–10 and pending ones included).
- Diff stays as it is: it is classic Triple Triad.
- The All trade rule becomes rarer.
- Fix the cost of off-screen tournament games if possible.
- Do every UI/UX item and the code cleanup, including deleting the retired web UI.
- Bring back the green-and-gold Tessera theme as a skin on the new layout.

Open:

| ID | Question | Needed by | Recommendation |
|---|---|---|---|
| D1 | First-move compensation | Phase 4 | +1 on all four sides of the first card while the first mover still owns it ("Vanguard"), and show who moves first before stakes are accepted. See the data below. |
| D2 | Keep authored hold rules, or leave rules to spread and drift | Phase 4 | Decide after the G2 simulations |
| D3 | Trade-rule weights for drift | Phase 4 | One 35, Diff 30, Direct 25, All 10 |
| D4 | Skill labels vs search settings (38 of 46 "Expert" opponents run Master's search) | Phase 3 | The search follows the level the player sees; keep explicit exceptions for named masters |
| D5 | Default skin | Phase 6 | Skyrim stays the default; Tessera returns at index 0, so players whose INI still says `Theme=0` get it back |
| D6 | Legion/Decimation: once one group is active no other can activate | Phase 1 | Keep the rule, fix the rule text ("once per board") |
| D7 | Sectioned save format (C7) | Phase 7 | Do it, after B1 has made unreadable records survive |
| D8 | Merchants added by other mods under a static SkyPatcher list | Phase 2 | Use a SkyPatcher filter if one exists (P1); otherwise vanilla and DLC only |

### First-move data (D1)

Expert self-play, 3000 deals per row, tier 4–7 decks. Score is the first mover's (win 1, draw ½); 0.5 is fair, standard error about ±0.009. Built from `build/ai-lab-scratch/vanguard`.

| Rules | Today | First card +1, always | +1 while the first mover owns it |
|---|---|---|---|
| Open | 0.355 | 0.457 | 0.459 |
| Hidden | 0.431 | 0.474 | 0.490 |
| Open, Same + Plus | 0.327 | 0.346 | 0.373 |
| Hidden, Same + Plus | 0.468 | 0.458 | 0.472 |

The bonus fixes most of the gap for ordinary captures. It barely affects Same and Plus because those rules use printed values. Same/Plus needs a second idea, tested in G1.

## Phase 1: bugs

Each fix comes with a test where the harness allows it (`tests/`, `native/tests/game_ui_test.cpp`, `tests/native_input_test.py`).

Save and economy
- [ ] **B1 Unreadable save record deletes progress.** If decoding fails or the format is unknown, `saveValid` goes false (`include/NativeCollection.h:1142-1150`) and `save()` writes nothing (`:1088`), so the next save has no Tessera record. Keep the raw `COLL`/`MATC` records (type, version, bytes) and write them back unchanged while invalid. Show an in-game message once per load. Test: an undecodable record survives a save byte-for-byte.
- [ ] **B2 A finished but unsettled staked match blocks the economy.** After an interrupt or reload between the last move and the "settled" command, packs, shops, decks, exchanges, tournaments and other opponents are blocked until the player returns to that NPC (`MatchFlow.cpp:10-22`, `Commands.cpp:78`, `NativeCollection.h:737-742,790-799`). Settle immediately when no choice is pending (a loss, a draw, None/Direct/All). When a choice is pending, offer it from the album's interrupted-game panel.
- [ ] **B3 CancelOffer on a completed offer reports "cancelled".** `Exchange.h:57`, `NativeExchange.h:157`. Answer "stale", as `docs/exchange-api.md` says.
- [ ] **B4 CompleteOffer can go silent.** If the (owner, serial) confirmation is still open, it returns with no event (`NativeExchange.h:300`). Emit a result, or expire the confirmation.
- [ ] **B5 Undeliverable payments block Reset Tessera forever.** NPC tournament gold for a removed or deselected resident (`NativeCollection.h:546-553`) and merchant credit for a chest that no longer resolves (`Shop.h:137-141`, `NativeShop.h:24-28`). After a failed resolution, drop or redirect the debt and log it.
- [ ] **B6 A circuit stalls after a failed courier invitation.** The 30 s retry has no limit and only one invitation may be outstanding (`Tournaments.h:659-667`, `NativeCollection.h:533`, `TTCG_EncountersQuest.psc:24-29`). Cap the retries, then cancel the invitation and release the circuit and its trophy.
- [ ] **B7 Exchange reward refilled by a restock is returned on top at cancel.** The NPC's ledger gains a copy (`Exchange.h:66` vs `Campaign.h:403-409`).
- [ ] **B8 Death events touch the shared form-identity map off the main thread.** `Events.cpp:97` → `WorldIdentity.h:41-53`. Compute the ID inside the queued task.
- [ ] **B9 Records injected into another plugin's FormID space resolve wrongly after reload.** `persistentID` uses `GetFile(0)` (`NativeCollection.h:97-103`). Investigate first; fix only if the change is safe for existing saves.

Match and AI
- [ ] **B10 The opponent freezes, then the match closes as "stalled".** A queued AI move is dropped when `revision` changed meanwhile (an expansion reward, for example), and `thinking` is never cleared (`src/MatchFlow.cpp:52`). The watchdog closes the match after 30 s (`World.cpp:139-148`). On a stale revision, clear `thinking` and run `runAI()` again.
- [ ] **B11 Opening Rules mid-turn changes the live match.** The rule demo shares `selectedHand` (`GameRules.cpp:17,73,81,86`, `GameMatch.cpp:523-526,666-694`). Give the demo its own selection.
- [ ] **B12 A Same Wall / Plus capture plays three sounds.** `GameMatch.cpp:831-840` breaks only on the exact label "same wall".
- [ ] **B13 Legion/Decimation text (D6).** `Match.h:125` locks to the first group; say so in `TesseraUI/rules.json` and the rules page.
- [ ] **B14 Tutorial and affinity layouts depend on the standard library.** `std::shuffle` and `uniform_int_distribution` differ between MSVC and libstdc++ (`Lesson.h:65-67`, `Match.h::seedTiles`), so the Linux-generated `rules.json` caption doesn't match the MSVC game. Use an explicit shuffle and range. Check first whether saved matches store tiles or reseed them.

Input and interface
- [ ] **B15 Esc or B closes Tessera while ending a drag or a slider.** `GameApp.cpp:716-721` ignores ImGui's active widget and drag payload. Add `backPressed()`, which skips back handling when `GetActiveID()` or `GetDragDropPayload()` is set, and use it at all 11 `Esc || FaceRight` sites.
- [ ] **B16 The right stick doesn't turn the inspected card.** `MenuInput.h:119-121` never passes it to ImGui, yet `GameInspector.cpp:112` advertises it. While the inspector is open, send the right stick to rotation.
- [ ] **B17 Q or E pressed while binding a hotkey also switches tabs.** `GameApp.cpp:734-741` doesn't check `capturing()`.
- [ ] **B18 A drag survives closing the menu and can finish on reopen.** In the reset branch at `Host.cpp:353-359`, call `ClearActiveID()` and `ClearDragDrop()`.
- [ ] **B19 Pad X adds the focused card, not the one under the cursor.** In cursor mode use the hovered item (`MenuInput.h:100-105`, `Album.cpp:43-47`).
- [ ] **B20 Card fact rows overflow.** Text is measured at a shrunken size and drawn at the 15 px floor (`GameApp.cpp:383`, `App.cpp:150`). The same happens to the match player name (`GameMatch.cpp:453-456`). Measure at the size that is drawn; wrap or ellipsize.
- [ ] **B21 B in the rule-request dialog exits Tessera.** `GameMatch.cpp:84-90`. Make B mean decline, as in other dialogs.
- [ ] **B22 Material slots can be reused within a frame.** On a very tall or portrait layout, the grid's `material++` can reach the drag and pack slots. A size change then rebuilds a render target whose view the draw list still uses (`RendererDX11.cpp:175-179`). Bound the slots and name them.
- [ ] **B23 `TTCG.ini.example` lists the retired themes.** It shows themes 0–4 with `Theme=0` and lacks themes 11–15, KeyPrompts, WindowSize and the Printed frame. `tools/update-development-settings.py` copies those comments into players' INIs.

In Skyrim: a match with an expansion reward during the AI turn; Esc during a deck drag; pad B after a slider; the inspector's right stick; interrupting a won staked match and reloading; Q/E during hotkey binding.

## Phase 2: merchant packs through SkyPatcher

Background: the 1.7.104 crash happens during death/reload while leveled merchant inventory is being set up, and runtime pack injection is the leading suspect. `VendorDistribution.h:111-124` calls CommonLib's `TESContainer::AddObjectToContainer`, which builds the container entry and its ownership data (COED) from CommonLib's own struct layouts. In Skyrim.esm only one of 19,244 container entries carries COED. See `docs/TESTING.md` for the diagnostics so far.

- [ ] **P1 Read SkyPatcher's source.** Two questions: which container filters exist (one that matches merchant chests generically would cover modded merchants, D8), and how it adds entries. If it goes through the same CommonLib call, say so: that would weaken the crash hypothesis, and the in-game check matters more.
- [ ] **P2 Generator.** `tools/build-vendor-packs.py` lists eligible merchant chests from Skyrim.esm, Update.esm, Dawnguard.esm, HearthFires.esm and Dragonborn.esm, using the same eligibility as `vendors::eligible`. It writes `SKSE/Plugins/SkyPatcher/container/Tessera TCG/merchant-packs.ini` in the form `filterByContainers=Skyrim.esm|<chest>:addToContainers=Tessera TCG.esp|<pack list>~1`, plus a `--check` mode for CI.
- [ ] **P3 Remove runtime injection.** Delete the base-container change and the `[Diagnostics] VendorPackInjection` switch. Keep what still has a job: faction tagging, the vendor-list keyword and the legacy album removal. Confirm each one.
- [ ] **P4 Stop forcing inventory setup.** `physicalCount` and `GetInventoryCounts` callers run without noInit: the courier and chest in `recoverTournamentLetters` (`NativeCollection.h:81-95,486-490`), the tournament-gold loop (`:546-553`) and `ShopBank` (`NativeShop.h:26-27`). Use noInit wherever the reference may not be set up yet. This was the second crash hypothesis.
- [ ] **P5 Packaging and requirements.** Add the ini to `tools/release_files.py` and the package tests. The README and Nexus requirements list SkyPatcher for merchant packs. Without SkyPatcher, Tessera logs that merchant packs are off and the Journal says so.
- [ ] **P6 Tests.** The generator's output, the package contents, and no remaining call to `AddObjectToContainer`.

In Skyrim: on 1.6.1170, load a pre-Tessera save and do three death/reloads, a merchant visit, then confirm packs are in stock and an album can be bought. Ask the 1.7.104 reporter to repeat their five-cycle test.

## Phase 3: AI

Measured in the review (paired, mirrored deals, tier 4–7; Master's score against Expert, ±0.015):

| Master variant | Hidden, basic | Time per move |
|---|---|---|
| Today (one guessed hand) | 0.47 | 7–10 ms |
| 8 guesses sharing the 70k-node budget | 0.65 | ~9 ms |
| 8 guesses × 70k nodes | 0.68 | 55–65 ms |
| 8 × 70k, guesses from obtainable cards only | 0.69 | ~58 ms |
| Sees the real hand (upper bound) | 0.77 | 7.5 ms |

In Open play today's Master scores 0.60 against Expert. It plays the last four or so moves exactly; before that, the node budget stops it at 3 to 6 plies, so the 7-ply cap is never reached. It is not unbeatable: even with the player's real hand it loses or draws against Expert some of the time. It also never reads hidden values; A1 keeps a test for that.

- [ ] **A1 Several guessed hands.** Sample K hands, run the search on each, add up the values per root move and play the best total. Start with K = 8 for Master and 4 for Expert, sharing the existing budgets, then tune with A5. Add a test that the chosen move doesn't change when only the player's hidden values change.
- [ ] **A2 Guess from cards the player could hold.** Cards that are available and not pending, with deck limits (one copy of a Unique, two of anything else), and without cards already visible. Weight by tier only if the player's deck strength is already public, for example in the lobby.
- [ ] **A3 Search.** Pass alpha at the root and try the previous best move first (about one more ply on the same budget). Remove the unused ply cap and name the per-skill budgets.
- [ ] **A4 One belief and budget helper.** Shared by `Opponents.h::opponentMove` and `Tournaments.h::tournamentNPCWinner`.
- [ ] **A5 `tools/ai-lab`.** A CMake target that isn't shipped, built from the scratch harness. It runs matchups, reports depth and measures first-move effects, and has a small ctest smoke run. Record the baseline tables here.
- [ ] **A6 Skill labels (D4).** Line up the authored profiles (`include/OpponentProfiles.inc`) with what the level shows. Generated residents use level → skill 0,0,1,1,2,3 (`OpponentRegistry.h:134`).
- [ ] **A7 Off-screen tournament games.** `resolveTournament(..., finish)` plays every remaining NPC pairing on the game thread, up to 7 pairings × 3 games × 3 replays per expiring event (`Tournaments.h:290-317,409-420`). Measure the worst case, then pick: one pairing per background tick, a worker thread on a snapshot with the result applied through the task queue, or a cheaper search. Results stay seeded and deterministic.

Target: Master clearly above Expert in both hidden and Open play, and Expert above Regular, with per-move time under about 60 ms on the game's worker thread.

## Phase 4: balance

- [ ] **G1 First move (D1).** If accepted:
  - Vanguard: +1 on all sides of the first card while the first mover owns it. It counts in the AI search, the tournament simulations and Sudden Death redeals.
  - Saved-match format 2 stores its square; a format-1 record loads with no bonus.
  - A marker on the card, a rules-page entry with a demo, and the lobby shows who moves first before stakes are accepted.
  - Test one more idea for the Same/Plus gap (for example, the first card can't be taken by the next play) and report numbers before adopting anything.
- [ ] **G2 Authored rules vs spread (D2).** Today every hold drifts every 7–21 days by adding or removing a random rule from all 17, with no pull back (`RegionalRules.h:90-140`). After the first year only 0.4% of hold-time still has the authored rules and 68% is hidden-hand play. Simulate with `build/ai-lab-scratch/drift.cpp` and bring numbers for:
  - A: authored rules kept, with drift weighted back toward them;
  - B: no authored rules, everything comes from player spread and drift;
  - C: authored rules as a core that only abolition can remove, drift changes the extras;
  - D: no time drift, player spread only.

  Measure the share of time at the authored set, rules per hold, hidden-hand share and how much the player's choices matter. Also review the spread chance after a loss (20% vs 35% after a win, `RuleCulture.h:152`). Existing saves move toward the new behaviour over time; no migration.
- [ ] **G3 Trade-rule weights (D3).** Today drift picks the trade rule uniformly, so All holds 25% of the time. Apply the agreed weights. Diff is unchanged.
- [ ] **G4 Exchange gold floor.** The generated floor of 50 (`NativeExchange.h:229`) is more than the shop price of tier I–II commons (25 and 45). Tie it to the requested card's price.

## Phase 5: interface and UX

New
- [ ] **U1 Capture preview.** With a card chosen, hovering or focusing a square marks every card that would flip, including Same, Plus and Combo. It replays `Match::play` on a copy of the board, which is all public. Mention it in the tutorial; add a setting to turn it off.
- [ ] **U2 Lobby facts.** Who moves first (G1) and, for rule challenges, when the next abolition challenge opens. The 7-day cooldown also applies after a successful challenge, and today the button simply disappears.

Fixes and gaps
- [ ] **U3 Settings > Controls help.** Describe Y/I to inspect, LB/RB and Q/E for tabs, X/Enter to add to a deck, and the right-stick cursor (`GameSettings.cpp:334-336`).
- [ ] **U4 Shop tabs.** Buy, Sell and Buy back step with LB/RB and Q/E (`GameShop.cpp:10-17`).
- [ ] **U5 Prompts in result and trade dialogs.** Show them, and in "Choose a card" say why B does nothing (`GameTournaments.cpp:519-597`).
- [ ] **U6 Rules modal.** Replace the × with a Close prompt (`GameRules.cpp:101`).
- [ ] **U7 Shop notice.** Stop it covering the footer rule, the prompts and the last grid row (`GameShop.cpp:201-202`).
- [ ] **U8 Delete-deck dialog.** Name the deck and use the skinned widgets (`Album.cpp:425-431`).
- [ ] **U9 Foil-upgrade picker.** Replace the one long dropdown with the album grid, search and filters, so a pad can use it (`GameTournaments.cpp:288-304`).
- [ ] **U10 Owned-count badge.** Respect the text floor; it is about 7 px at 1080p on Small density (`Album.cpp:86-97`).
- [ ] **U11 Deck slot names.** Ellipsize instead of hard clipping (`Album.cpp:289-291`).
- [ ] **U12 Custom colours on a pad.** Swatches and sliders instead of `ColorEdit3`.
- [ ] **U13 Light colours on paper skins.** Route the hard-coded ones through `lightGround()`/`rarityInk()`: gold headings (`GameTournaments.cpp:240,453`), "−N gold" (`:576`) and rank deltas (`GameAlbum.cpp:267-268`).
- [ ] **U14 Docs.** Update `docs/reference-guide.md` (deck editor, inspector, skins, a controller section) and the README. Close issue #3 after the physical pad check.

Carried over from [ui-overhaul.md](ui-overhaul.md). At the start of the phase, check each one against the code, because later checkpoints finished some.
- [ ] **U15** Physical controller pass and the keyboard/mouse fallback checklist in `docs/TESTING.md`.
- [ ] **U16** Card-face verification: the three deck densities, tournament prize cards, the drag preview, and value legibility at 720p.
- [ ] **U17** Type scale: five steps with a 16 px floor at 1080p. Bundle Jost Medium/SemiBold. Decide on FreeType rasterisation (vendored dependency).
- [ ] **U18** Match: score as pips or a clear count, a fanned or stacked hand, a "thinking" state, and a visual for hidden cards.
- [ ] **U19** Journal scouting detail; shop detail with the album's item-card treatment; a tournament-won screen and prize presentation.
- [ ] **U20** First-run detection of the installed UI mod to pick a skin (new installs only).
- [ ] **U21** Optional: real small caps and a user font folder.

## Phase 6: Tessera skin

The classic Tessera theme (index 0, retired in 0.33.0) was dark green and gold. Its palette from `6f310b4^:native/src/App.cpp`:

| Role | Colour |
|---|---|
| Ground | `#131e19` |
| Panel | `#28372e` |
| Ink | `#e7dec6` |
| Muted | `#afa991` |
| Gold accent | `#d0b474` |
| Lines | `#586044` (alpha 130) and `#4c5237` |

Its default board was Felt (3). The ground was a dithered radial gradient (`Surface.h`, removed in the same commit).

- [ ] **T1** Clear `retired` on index 0 and add `TesseraUI/skins/tessera/skin.json` with the palette, a gold hairline, corner marks and Felt as its board. The typeface is chosen from previews: Jost, or the bundled serif.
- [ ] **T2** Bring back the gradient ground, either as a baked texture from a small tool or as a `ground.gradient` option in `skin.json`. Choose whichever keeps the skin loader simpler.
- [ ] **T3** Optional: Tessera's own panel and title-band art. It's first-party, so no third-party permission is needed. Record provenance if generated.
- [ ] **T4** Settings order, `TTCG.ini.example`, `native/tests/game_ui_test.cpp` theme expectations, and `TesseraUI/skins/README.md`. Previews at 720p, 1080p and 4K for the album, lobby, match, results, shop and settings. D5 decides the default.

## Phase 7: code cleanup

- [ ] **C1 Delete the retired web UI.** First move the pieces the tools still use into `tools/shared/`: affinity icons, `cardElement`, `foil.css` and `foil-material.js`. `tools/build-card-library.py`, `tools/serve-rule-lab.py`, `tools/build-expansion-editor.py` and `tools/rule-lab/index.html` currently cut them out of `TesseraUI/app.js` between marker strings. Then delete:
  - `TesseraUI/*.js|css|html`; `rules.json`, assets, skins, fonts, icons and art stay;
  - `tests/*.cjs` (52 Playwright tests not run by CI) and `package.json`/`package-lock.json`;
  - the web theme previews in `tools/*.cjs`;
  - `TesseraUI/assets/themes/*.png` and the old test that reads them.

  Move `TesseraUI/Tessera_Card_Art_*.md` beside `docs/card-art-prompts`. Each tool's `--check` must still pass. Independent of the other phases, so it can land early.
- [ ] **C2 Split `NativeCollection.h`.** Move it into `src/*.cpp` modules, build the snapshot JSON with nlohmann instead of the 170-line string emitter (`:914-1086`), and break the circular includes. Its forward declarations at `:19-29` refer to `NativeOpponents.h`, which is included at `:1205`, and `exchangeIntegration::reset/publish` is declared non-inline in `NativeCollectionAPI.h`. Turn `NativeTournamentCommands.h` into a .cpp.
- [ ] **C3 Duplication:**
  - gold payout ×4 with different clamping (`NativeCollection.h:557-560,732-735`, `NativeExchange.h:272-274`, `Shop.h:137-139`);
  - the development-grant block in `recover()`;
  - `validHand`/`validDeck`;
  - the `resolveOpponent`/`alternateReference` fallback ×3;
  - `rgb`/`alpha` ×3 plus `fade`;
  - Roman numerals ×6;
  - rarity colours ×4 (Legendary is `e6bc65` in some and `f0c465` in others; pick one);
  - the inspect trigger ×7;
  - 14 raw `ImGui::Button`/`Checkbox`/`TextUnformatted` calls that bypass `App::button`;
  - the Same/Plus participation check written twice in `Match::play`;
  - the hash multiplier `2654435761u` in four headers.
- [ ] **C4 Dead code:**
  - `App::closing` and the close path at `Host.cpp:434`;
  - `GameApp::filtersOpen`, `App::showMetrics`, `App::autoRotate`;
  - `Assets::card/back/megabytes/decoded`;
  - `Palette::name` and `Palette::centre`.

  Replace the five empty `themes[]` rows with an explicit index map that keeps the saved indices.
- [ ] **C5 Named constants:**
  - derive `RuleMask` from `ruleFlags` with a `static_assert`;
  - printed Ace values;
  - Reverse `11 - rank`;
  - the 72-hour restock;
  - `168` at `Campaign.h:67`, which equals `cultureCooldown`;
  - the material slot numbers;
  - the convoluted font-index expressions in `applySettings` (`GameApp.cpp:163-164`).
- [ ] **C6 Split `Match::play`.** Separate rule resolution from stage recording (`Match.h:263-407`).
- [ ] **C7 Sectioned save format (D7).** Format 35, never 34.
  - Each part becomes its own section, so one bad section no longer loses the rest.
  - Parsing is separated from validation, and the code moves to a .cpp in a small core library the tests link against.
  - Golden save files for formats 20, 21 and 26–33 must load unchanged.
- [ ] **C8 Docs.** Mark `ui-overhaul.md` complete, rewrite the checkpoint section of `docs/TESTING.md`, and drop hard-coded test counts.
- [ ] **C9 Optional, with the player's OK.** Prune `dist/` (26 old test builds, 1.7 GB) and stale `build/` trees (9.2 GB).

## Build, test and package

```
bash tools/format.sh --check
cmake --preset native && cmake --build --preset native
ctest --preset native --parallel 8        # about 2 minutes; native_game_ui is the long one
tools/preview-ui.sh NAME --snapshot build/test-fixtures/<fixture>.json [--page ...] [--theme <id>]
```

In-game build for the player (MO2 downloads folder, Skyrim SE 1.6.1170 under Proton), from a committed, clean tree:

```
bash tools/build-native.sh
bash tools/test-native.sh
python3 tools/package-release.py --build-dir build/linux-clangcl \
  --validation build/test-results/windows-tests.json --output-dir dist/<phase>
```

Hand over the absolute path of `dist/<phase>/Tessera-<ver>-dev.<hash>-Main.zip`, and compare its file hashes with the previous Main zip.
