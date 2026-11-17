# Tessera roadmap: plan, status and handoff

Started 2026-11-16 from a full review of `master` at `6fc37d1` (0.33.0, 90/90 native tests passing). It replaces the open lists in [ui-overhaul.md](ui-overhaul.md); those are carried into phase 5.

## Status

| Phase | Scope | Branch | State |
|---|---|---|---|
| 1 | Bugs | `phase1/bug-fixes` | Done; in-game check pending |
| 2 | Merchant packs through the game's own function (1.7.104 crash) | `phase2/merchant-packs` | Done; in-game check pending |
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
- Merchant packs: first decided as a SkyPatcher container patch, then changed on the same day to Tessera calling the game's own routine, once SkyPatcher's source showed a fixed list would drop mod-added merchants (see phase 2).
- Fix the Master AI, including hidden-hand guesses drawn from every card (tier 8–10 and pending ones included).
- Diff stays as it is: it is classic Triple Triad.
- The All trade rule becomes rarer.
- Fix the cost of off-screen tournament games if possible.
- Do every UI/UX item and the code cleanup, including deleting the retired web UI.
- Bring back the green-and-gold Tessera theme as a skin on the new layout.
- Legion and Decimation keep their rule (one group per board); the rule text now says so (was D6, accepted with the plan).

Open:

| ID | Question | Needed by | Recommendation |
|---|---|---|---|
| D1 | First-move compensation | Phase 4 | +1 on all four sides of the first card while the first mover still owns it ("Vanguard"), and show who moves first before stakes are accepted. See the data below. |
| D2 | Keep authored hold rules, or leave rules to spread and drift | Phase 4 | Decide after the G2 simulations |
| D3 | Trade-rule weights for drift | Phase 4 | One 35, Diff 30, Direct 25, All 10 |
| D4 | Skill labels vs search settings (38 of 46 "Expert" opponents run Master's search) | Phase 3 | The search follows the level the player sees; keep explicit exceptions for named masters |
| D5 | Default skin | Phase 6 | Skyrim stays the default; Tessera returns at index 0, so players whose INI still says `Theme=0` get it back |
| D7 | Sectioned save format (C7) | Phase 7 | Do it, after B1 has made unreadable records survive |

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

Done on `phase1/bug-fixes` (2026-11-16) and waiting for the in-game check. Every item has a test unless noted. Where the fix differs from the plan, the item says how.

Save and economy
- [x] **B1 Unreadable save record deletes progress.** `include/SaveRecords.h` reads the `COLL` and `MATC` records as bytes. When the collection can't be used (unknown format, damaged payload, failed migration), `load()` keeps every record and `save()` writes them back unchanged; Tessera stays off for that save and shows a message box after loading. A record the file itself cuts short can't be kept.
- [x] **B2 A finished but unsettled game blocks the economy.** `settleFinishedMatch()` settles a finished table as soon as it is interrupted, and again on load and whenever Tessera opens. It applies to tournament games too: a finished one could lock packs, decks and shops for good once its event was pruned. A table is kept only while the winner has cards to choose (One, Diff) or a final still holds foil upgrades. Winnings can then be collected from the album's interrupted-game bar anywhere, without returning to the opponent (`resumeMatch()`, `checkLiveWorld()`).
- [x] **B3 CancelOffer on a completed offer** answers `stale`. Cancelling twice still answers `cancelled`.
- [x] **B4 CompleteOffer while its confirmation is open** answers `busy`. No test: the path needs Skyrim's message box.
- [x] **B5 Undeliverable payments block Reset Tessera.** `dropUndeliverablePayments()` runs in `recover()`. It drops a tournament purse owed to someone who is no longer a Tessera player or whose plugin is gone, and a merchant payment whose chest no longer exists, and logs the amount. A debt whose owner still exists is kept and still blocks the reset.
- [x] **B6 A circuit stalls after a failed courier invitation.** After six unacknowledged handoffs in a session the invitation is withdrawn (`withdrawTournamentInvitation()`): the event runs without the player, which frees the circuit and the trophy. Not covered: a courier script that never asks for the letter at all, because nothing is then in flight to time out.
- [x] **B7 Exchange reward refilled by a restock.** `replenishOpponent()` counts copies set aside for active offers.
- [x] **B8 Death events off the main thread.** The handler passes the FormID; the identity is computed in the queued task. No test.
- [x] **B9 Injected records.** Fixed, not only investigated. `persistentID()` names the plugin that owns the form's ID space, found by load-order index when the defining plugin is a different one. Ordinary records get exactly the identity they had (tested against CommonLib's old formula), so existing saves are unaffected. An NPC or chest from an injected record gets a new identity: its old ledger is orphaned and it starts with a fresh collection. The old identity never resolved back to the form, so interrupted games and stakes with such NPCs were already being dropped.

Match and AI
- [x] **B10 The opponent freezes.** `applyAIMove()` lets go of `thinking` and searches again when the table changed during the search.
- [x] **B11 Opening Rules mid-turn changes the live match.** The demo uses `demoSelected` only; `selectedHand` belongs to the live table.
- [x] **B12 Same Wall / Plus sounds.** Smaller than reported: the interface sent a third, redundant `rule same` command, which the plugin already ignored, so two sounds played as intended. The redundant command is gone. No test.
- [x] **B13 Legion/Decimation text (D6).** The rules page says the first play to bring three of a group onto the board chooses it, that the change applies on the board and in both hands, and that only one group is chosen per board. The text lives in `TesseraUI/album.js` until C1 moves it.
- [x] **B14 Layouts depend on the standard library.** `Match::seedTiles` and `Match::seedVisibility` use an explicit shuffle and plain modulo, with values pinned in `tests/match_test.cpp`. Saved matches store tiles and reveals, so nothing in a save changes. `rule-demos.js` and `rules.json` are regenerated; only the opponent cards shown face up in the Three Open demo and the rift tour moved, and no caption changed. The other `std::shuffle` calls (opponent hands, packs, brackets) are consistent inside the game and were left alone. To regenerate: build `tests/rule_demo_host.cpp`, run it to `TesseraUI/rule-demos.js`, then `node tools/build-native-ui-assets.cjs`.

Input and interface
- [x] **B15 Esc or B closes Tessera while ending a drag or a slider.** `App::backPressed()` ignores the press when, before the frame, a control was held, a drag was live or a dropdown was open above a dialog; ImGui ends those in `NewFrame`, so the state is noted in `prepareFrame()`. The press also cancels the drag, which ImGui would otherwise still deliver on release. Used at ten sites. The deck-rename field keeps its own check, because Esc there must cancel the rename while the field is active.
- [x] **B16 The right stick turns the inspected card.** `MenuInput::turning()`. Skyrim's own cursor still moves with the stick while the card turns.
- [x] **B17 Q or E while binding a hotkey** no longer switches tabs.
- [x] **B18 A drag survives closing the menu.** `App::resume()` clears the active control and the drag.
- [x] **B19 Pad X adds the card under the cursor** in cursor mode.
- [x] **B20 Card fact rows and player names overflow.** Text shrinks to the 15 px floor and is then shortened with an ellipsis (`App::fitted()`, also wanted by U11).
- [x] **B21 Withdrawn: not a bug.** B in the rule-request dialog leaves the lobby without answering, on purpose: declining is a real answer that ends the visiting rule's offer, and `game_ui_test.cpp` checks that Back does not give it. What is missing is a prompt saying so; that is part of U5.
- [x] **B22 Material slots can be reused within a frame.** Scrolling grids have their own range of 160 slots (`MaterialSlot::gridSlot()`), clear of the drag, pack and settings cards; past the range a card is drawn without a finish. The remaining literal slot numbers are named in C5.
- [x] **B23 `TTCG.ini.example`** lists the current skins, `Theme=13`, WindowSize, KeyPrompts and the Printed frame. `interface_settings_test.cpp` checks it against the settings table.

In Skyrim:
- A match where an expansion reward arrives during the opponent's turn: the opponent still moves.
- Esc during a deck drag, and pad B after adjusting a slider: Tessera stays open and the drag is cancelled.
- In the inspector, the right stick turns the card.
- Q and E while binding a hotkey.
- Win a staked game under One or Diff, press the album key before choosing a card, walk away, open the album and collect the winnings there.
- Lose or draw a staked game and interrupt it during the result: the stakes settle at once and packs and shops work.
- Reload a save with an interrupted finished game.

## Phase 2: merchant packs through the game's own function

Done on `phase2/merchant-packs` (2026-11-16), stacked on phase 1, and waiting for the in-game check.

Background: the 1.7.104 crash happens during death/reload while leveled merchant inventory is being set up. Tessera added its pack roll (a leveled list) to each merchant chest base with CommonLib's `TESContainer::AddObjectToContainer`, which creates the entry and replaces the base's entry array with one it allocates itself. See `docs/TESTING.md` for the diagnostics so far.

The plan was a SkyPatcher container patch. Reading SkyPatcher's source changed it:

- **SkyPatcher had the same crash and the same cause.** Until its commit `0612fff` (2026-10-06) it called the same CommonLib routine, and kept leveled lists out of containers behind a switch because they crashed. That commit moved it to the game's own routine (Address Library 14377 in SE, 14517 in AE), and its settings file says the crashes are fixed since 7.1.3. This is the strongest evidence so far for the cause, though the 1.7.104 crash itself is still unconfirmed until the reporter retests.
- **SkyPatcher can't find merchants.** Its container filters are an explicit list, EditorID substrings and plugin names. A list from the five official masters covers 35 chest bases. In the player's own load order Tessera's runtime rule finds 181, so 146 mod-added merchants would have lost their packs.

Decision (player, 2026-11-16): Tessera keeps finding merchants at runtime and makes the same call SkyPatcher now makes. SkyPatcher is not a requirement. This settles D8.

- [x] **P1 Read SkyPatcher's source.** Findings above, from `github.com/Zzyxz/SkyPatcher` at `48d1779`.
- [x] **P2 Add the roll through the game's routine.** `include/EngineContainer.h` finds the routine and `include/PackStock.h` holds its IDs and first bytes. The ID is searched for in the Address Library table, because CommonLib's lookup stops the game when an ID is missing. If the routine isn't found or doesn't begin as expected, packs stay off on that game version and Settings > Gameplay says so; nothing is guessed.
- [x] **P3 No CommonLib container edit.** `vendors::initialize` is handed the function that adds the roll; the test's stand-in container has no `AddObjectToContainer` at all, so the old call can't come back unnoticed. Faction tagging, the vendor-list keyword and legacy album removal are unchanged. `[Diagnostics] VendorPackInjection` stays, for the reporter's A/B test.
- [x] **P4 Counting no longer sets an inventory up.** `physicalCount` and `physicalChange` read with noInit, so polling the courier's container for letters, paying a tournament purse and the reset cleanup don't roll anyone's leveled lists. The card shop is the exception: the player is trading with that merchant, whose gold has to be real, as in barter.
- [x] **P5 Notice.** Settings > Gameplay shows why merchants have no packs when the switch is off or the game version is unknown. No new requirement, so the README and Nexus page are unchanged.
- [x] **P6 Tests.** `vendor_distribution_test` in three modes (on, switched off, unknown game), the first-bytes check, and `inventory_count_test.py`.

Not verifiable outside the game: that the routine is found and behaves on each runtime. On 1.6.1170 SkyPatcher's log in the player's game already reports the same routine as active. SE 1.5.97, VR and 1.7.104 rest on SkyPatcher's table and the first-bytes check.

In Skyrim, on 1.6.1170:
- `TTCG.log` says `Vendor pack injection: enabled` and `packs added to N of N container bases` (181 of 181 in the current load order).
- Visit a general-goods merchant from the base game and one from a mod: packs are in stock, and an album can be bought.
- Load a pre-Tessera save and do three death/reloads, then a manual reload.
- Sell a card in a merchant's Tessera shop before opening their barter menu: the merchant has gold.

Then ask the 1.7.104 reporter to repeat their five-cycle test with this build and send `TTCG.log`, which now says whether the routine was found there.

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
- [ ] **U5 Prompts in result and trade dialogs.** Show them, and in "Choose a card" say why B does nothing (`GameTournaments.cpp:519-597`). The rule-request dialog needs a "Leave" prompt for B, which leaves without answering (see B21).
- [ ] **U6 Rules modal.** Replace the × with a Close prompt (`GameRules.cpp:101`).
- [ ] **U7 Shop notice.** Stop it covering the footer rule, the prompts and the last grid row (`GameShop.cpp:201-202`).
- [ ] **U8 Delete-deck dialog.** Name the deck and use the skinned widgets (`Album.cpp:425-431`).
- [ ] **U9 Foil-upgrade picker.** Replace the one long dropdown with the album grid, search and filters, so a pad can use it (`GameTournaments.cpp:288-304`).
- [ ] **U10 Owned-count badge.** Respect the text floor; it is about 7 px at 1080p on Small density (`Album.cpp:86-97`).
- [ ] **U11 Deck slot names.** Ellipsize instead of hard clipping (`Album.cpp:289-291`), with `App::fitted()` from B20.
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
  - the material slot numbers still written as literals (B22 named the grid's);
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
