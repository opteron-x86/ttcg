# Tessera roadmap: plan, status and handoff

Started 2026-11-16 from a full review of `master` at `6fc37d1` (0.33.0, 90/90 native tests passing). It replaces the open lists in [ui-overhaul.md](ui-overhaul.md); those are carried into phase 5.

## Status

| Phase | Scope | Branch | State |
|---|---|---|---|
| 1 | Bugs | `phase1/bug-fixes` | Done; in-game check pending |
| 2 | Merchant packs through the game's own function (1.7.104 crash) | `phase2/merchant-packs` | Done; in-game check pending |
| 3 | AI | `phase3/ai` | Done; in-game check pending |
| 4 | Balance: first move, regional rules, trade rules | `phase4/balance` | Done; in-game check pending. Ships after phases 5–7, with the save-format work |
| 5 | Interface and UX | `phase5/interface` | Done; in-game check pending |
| 6 | Tessera green-and-gold skin | `phase6/tessera-skin` | Done; in-game check pending |
| 7 | Code cleanup | `phase7/cleanup` | Done except C7 (second release) and the snapshot emitter; in-game check pending |

## How the work runs

- One branch and one PR per phase, cut from `master` after the previous phase merges. Tick items here as they land; this file is the handoff for the next session.
- A phase is done when `ctest --preset native` passes, `tools/format.sh --check` is clean, UI changes have reviewed previews, and the player has checked a Main zip in Skyrim (see [Build](#build-test-and-package)). Then merge.
- **Releases (player, 2026-11-17).** Work that leaves the save format alone ships first: phases 1–3 and 5–7, without C7. It is branched from `phase3/ai`, so it can merge without phase 4. Phase 4 (collection format 35, saved-match format 2) and C7 (sectioned saves) follow as a second release.
  - The second release does not need a new game. Saves from the first release load in it: holds keep the rules they have, and an interrupted match plays on without a Vanguard. What it can't do is go back: a save written by it turns Tessera off on an older version (the data is kept, since phase 1). The rolled starting rules of option B only appear in a new game; an existing game keeps its holds' rules and drifts from there.
  - Changelog entries are one line per change.
- **Save formats.** Collection format is **35** since phase 4 (33 before; same layout, every hold's terms stored). The parked `codex/transmute-testing` branch writes 34 with a different layout, which stays unsupported unless transmutation lands. The saved-match record is format 2 (66 words) since phase 4; format 1 (65 words) still loads.
- `tools/ai-lab` (built as `build/native/tests/ai-lab`) measures opponents, first-move scores, regional rules and off-screen tournaments. The review's scratch harnesses in `build/ai-lab-scratch/` (untracked) are superseded by it.

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
- The opponent's search follows the skill the player is shown, with no per-opponent exceptions (was D4, accepted with the plan).
- 2026-11-17: Skyrim stays the default skin and Tessera returns at index 0 (D5, accepted with the plan).
- 2026-11-17: Vanguard for the first move (D1), option B for regional rules (D2: no authored rules, everything from spread and drift), the proposed trade-rule weights (D3) and the exchange gold floor (G4).

Open:

| ID | Question | Needed by | Recommendation |
|---|---|---|---|
| D7 | Sectioned save format (C7) | Second release | Do it, after B1 has made unreadable records survive |

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
- [x] **B13 Legion/Decimation text (D6).** The rules page says the first play to bring three of a group onto the board chooses it, that the change applies on the board and in both hands, and that only one group is chosen per board. The text lives in `tools/rules/rule-pages.js` since C1.
- [x] **B14 Layouts depend on the standard library.** `Match::seedTiles` and `Match::seedVisibility` use an explicit shuffle and plain modulo, with values pinned in `tests/match_test.cpp`. Saved matches store tiles and reveals, so nothing in a save changes. `rule-demos.js` and `rules.json` are regenerated; only the opponent cards shown face up in the Three Open demo and the rift tour moved, and no caption changed. The other `std::shuffle` calls (opponent hands, packs, brackets) are consistent inside the game and were left alone. To regenerate: build `tests/rule_demo_host.cpp`, run it to `tools/rules/rule-demos.js` (`TesseraUI/rule-demos.js` before C1), then `node tools/build-native-ui-assets.cjs`.

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

Done on `phase3/ai` (2026-11-17), stacked on phase 2, and waiting for the in-game check.

Every figure below comes from `tools/ai-lab` (built as `build/native/tests/ai-lab`): paired deals played twice with the seats exchanged, tier 4–7 cards, 500 deals, so about ±0.012. A score of 0.5 is level.

The six skills a player is shown are now six strengths (decision D4: the search follows the label). Before, they were four search settings, and most Experts thought as hard as a Master.

| Skill | Positions per move | Looks ahead | Hands imagined | Careless move |
|---|---|---|---|---|
| Beginner | 1,000 | 1 play | 1 | 1 in 3 |
| Learner | 2,000 | 1 play | 1 | 1 in 8 |
| Regular | 5,000 | 2 plays | 1 | never |
| Experienced | 10,000 | 3 plays | 1 | never |
| Expert | 20,000 | 4 plays | 3 | never |
| Master | 280,000 | to the end | 16 | never |

Each skill's score against the one below it:

| Step | Hidden | Open | Hidden, Same + Plus | Open, Same + Plus | Three Open | Open, Chaos |
|---|---|---|---|---|---|---|
| Learner over Beginner | 0.59 | 0.59 | 0.53 | 0.53 | 0.59 | 0.53 |
| Regular over Learner | 0.62 | 0.75 | 0.60 | 0.83 | 0.71 | 0.72 |
| Experienced over Regular | 0.59 | 0.65 | 0.64 | 0.74 | 0.62 | 0.61 |
| Expert over Experienced | 0.59 | 0.62 | 0.56 | 0.66 | 0.56 | 0.55 |
| Master over Expert | 0.61 | 0.67 | 0.62 | 0.71 | 0.60 | 0.55 |

Against the 0.33.0 opponents:

| Match-up | Hidden | Open |
|---|---|---|
| 0.33.0 Master over 0.33.0 Expert | 0.48 | 0.58 |
| Master over 0.33.0 Master | 0.68 | 0.60 |
| Master over 0.33.0 Expert | 0.68 | 0.69 |
| Expert over 0.33.0 Expert | 0.58 | 0.55 |
| Master over Regular | 0.74 | 0.81 |

What the numbers say:

- **Master is not unbeatable, and with hidden hands it cannot be.** A Master shown the player's real hand scores only 0.74 against the Master that has to guess. Hidden cards cap how much thinking can win, which is why the steps are smaller in hidden play and why more positions per move stopped helping: at eight times the cost Master gained 0.02 on Expert.
- **Imagining several hands is the gain.** One guessed hand was the old weakness. Master now takes about 24 ms a move with hidden hands and 14 ms with open ones on the development machine, on a worker thread.
- **Master plays the last seven plays of an Open game exactly**, and looks 4.6 and 5.5 plays ahead on the first two. The 0.33.0 Master never looked seven ahead before the endgame.

- [x] **A1 Several guessed hands.** `opponentMove` imagines the hidden hand as many times as the skill allows, searches each on its share of the budget, adds the values up per move and plays the best total. `tests/opponents_test.cpp` checks that no skill's move changes when only hidden cards change, from either seat.
- [x] **A2 Guess from cards the player could hold.** `imagineHiddenHand` draws from cards that are available and not pending, keeps to the deck limits with what that hand has already shown, and never replaces a card in view. Tier 8–10 and unreleased cards are no longer guessed. No tier weighting: nothing public says how strong the player's deck is.
- [x] **A3 Search.** `searchRoot` orders the moves best first after each pass. With one known position it also cuts off moves that can't beat the best, worth about a play of depth; with several guesses every move keeps its exact value so they can be added up. Master's unused seven-play cap is gone and the budgets are named in `searchStyles`.
- [x] **A4 One helper.** The live opponent and the off-screen tournament games both call `opponentMove`; it plays for whoever is to move, so neither seat is special.
- [x] **A5 `tools/ai-lab`.** Commands: `match`, `ladder`, `depth`, `first`, `tournament`, `smoke`. It can also play the 0.33.0 opponents (`v033:0`–`3`), a search of your own (`nodes:plies:guesses`) and an opponent shown the hidden hand (`sees:master`). `ai_lab_smoke` runs in ctest.
- [x] **A6 Skill labels (D4).** `OpponentProfile::skill` is gone from the code and from `data/opponents.json`; `level` is the strength. Children's three grades are Beginner, Regular and Expert. `[Development] OpponentSkill` now takes 0–5 for the six names, and the Developer page lists them.
  - Players will notice: Experienced opponents (54 of 72 ran the old third setting) and Experts (38 of 46 ran Master's) are easier than in 0.33.0, Masters are harder with hidden hands, and Learners are a little harder than Beginners.
- [x] **A7 Off-screen tournament games.** An event that ends with nobody watching averaged 12 ms on the game thread and reached 47 ms, more when several ended together. The periodic background update now plays one off-screen game per call (`backgroundTournamentGames`): typically under 1 ms, 9 ms at worst. Anything left is finished at once when Tessera opens. A sliced event ends exactly as it would have at once (tested; results are seeded per game). Off-screen games use `offscreenStyle`: each skill's carelessness and one or two plays of look-ahead.

Not done: the plan's target of "Master clearly above Expert in hidden play" is met at 0.61, which is as far as the hidden hand allows without making Expert weaker.

In Skyrim:
- Play a Beginner, a Regular, an Expert and a Master with hidden hands and with Open. The opponent should answer within the usual pause, and the difficulty should rise with the label.
- With `[Development] OpponentSkill` set from the Developer page, check each of the six names.
- Leave a tournament unentered until it ends, keep playing for ten minutes without opening Tessera, then open the Tournaments tab: the bracket is complete.

## Phase 4: balance

Done on `phase4/balance` (2026-11-17), stacked on phase 3, and waiting for the in-game check. One finding needs the player's eye: see "The Vanguard after phase 3" below.

- [x] **G1 First move: the Vanguard (D1).** The first card of a board has +1 on every side while the player who placed it holds it, until the fifth card is played (`Match::vanguardModifier`, `Match::vanguardSpan`).
  - Captured, it is an ordinary card; retaken by the first mover, it has the bonus again. Under Reverse it has −1, because lower ranks win there. Same and Plus compare printed ranks and ignore it. A Sudden Death redeal names a new Vanguard.
  - It is part of every rank the search compares, so opponents, tournament games and off-screen games all play with it.
  - Saved-match format 2 adds one word for its square and owner. A format 1 table loads with none and is finished without it.
  - The card shows the usual +1 marker. The Rules tab has a Vanguard page with an example (`vanguardExample`). Rule lessons leave the Vanguard out (`Lesson::table`), so each still shows one rule.
  - The lobby says who moves first before the stakes are agreed. It follows the same count of games that fixes the opponent's hand, so reopening the lobby can't change it; a tournament game's seed decides it as before.
- [x] **G2 Regional rules: option B (D2).** No hold has authored rules.
  - A new game rolls each hold's terms: the plain game (Open, trade One) after two changes of rules, and a weighted trade rule (`regionalHistory`). Every save has its own map from the first day.
  - Collection format 35 stores every hold's terms. A save from format 33 or earlier gets the terms its holds began with then, so nothing changes under a game in progress; from there drift and spread carry on as before. Format 34 stays unsupported.
  - Not changed: the spread chances (35% after a win, 20% otherwise) and the pace of drift.
- [x] **G3 Trade-rule weights (D3).** Drift picks by weight. A hold never drifts to the rule it has, so weights of 42, 29, 22 and 7 give the agreed shares of time. Diff is unchanged.
- [x] **G4 Exchange gold floor.** `exchangeGold`: three times the shop's buying price, at least 50 where the card is worth more than that, and four fifths of its shop price for cheaper cards. A tier I common pays 20 gold instead of 50 against a shop price of 25.

### Regional rules with nobody playing

`ai-lab drift`, 300 saves, three years, daily. Drift alone; the player's spreading and abolishing come on top.

| | Day 1 | Day 30 | Day 90 | Day 365 |
|---|---|---|---|---|
| Different rule sets among the 10 holds | 8.8 | 9.6 | 9.9 | 9.9 |
| Rules in force per hold | 2.0 | 2.4 | 2.8 | 2.9 |
| Holds with hands hidden | 30% | 38% | 57% | 69% |
| Holds with Open | 61% | 52% | 29% | 16% |

- Trade rules over years two and three: One 35.6%, Diff 30.1%, Direct 25.0%, All 9.3%.
- A rule stays about 96 days once it is in force. Each of the 17 rules is in force 15–19% of the time, except Same (32%, because Same Wall brings it) and Same Wall (10%).
- With no history rolled, all ten holds would start identical and it would take a month to reach 7.6 different sets. Two changes was chosen over three because it starts with fewer hidden-hand holds (30% against 38%).
- **Hidden hands take over in the long run** (69% of holds after a year), because Open and Three Open are two rules among seventeen. That was already true of 0.33.0 after its first year. If more open play is wanted, weighting drift toward Open and Three Open is a small change.

### The Vanguard after phase 3

The Vanguard was first agreed as a bonus for the whole board, on the review's numbers: measured with the 0.33.0 Expert, the first mover scored 0.36 in Open play. Phase 3 changed the opponents. With them the first mover's handicap is smaller, depends on how far the opponent looks ahead, and a whole-board bonus made the first mover the favourite. The player chose the shorter bonus on 2026-11-17: it ends when the fifth card is played.

First mover's score in self-play (`ai-lab first`, 1,200–2,000 deals, ±0.013; 0.5 is fair):

| | Regular, Open | Regular, hidden | Expert, Open | Expert, hidden | Master, Open | Master, hidden |
|---|---|---|---|---|---|---|
| No Vanguard | 0.47 | 0.44 | 0.44 | 0.46 | 0.52 | 0.41 |
| **Until the fifth card (shipped)** | 0.45 | 0.44 | 0.52 | 0.52 | 0.54 | 0.49 |
| For the whole board | 0.51 | 0.47 | 0.56 | 0.53 | 0.61 | 0.53 |
| Whole board, and immune to the next play | | | 0.59 | 0.64 | | |

- Without the Vanguard the first mover averages 0.46 across the rule sets and skills measured. The shipped rule averages 0.49 and stays within 0.06 of fair; the whole-board bonus averaged 0.52 and reached 0.61 against Master in Open play.
- Regular opponents look two plays ahead and gain nothing from the shorter bonus. Against them the first mover keeps a small handicap.
- The Same/Plus gap in the review (0.33) belongs to one search depth: without the Vanguard only Expert shows it (0.36 in Open, Same + Plus), where Regular scores 0.49 and Master 0.51. No second rule was adopted for it.
- These are opponents playing each other. How it feels with a person in one seat is for the in-game check.

In Skyrim:
- The lobby names who moves first, and that player does. Leave the lobby and return: the same player still moves first.
- The first card shows +1. Let it be captured and retake it before the fifth card: the +1 returns. From the fifth card on it shows none.
- Interrupt a match after the first card, save, reload and resume: the +1 is still there. Resume a match interrupted on 0.33.0: it plays on without one.
- Start a new game: the Journal shows different rules across the holds. Load a 0.33.0 save: every hold has the rules it had.
- Rules tab: the Vanguard page and its example.
- Post a local exchange for a tier I common: the gold offered is below its shop price.

## Phase 5: interface and UX

Done on `phase5/interface` (2026-11-17), branched from `phase3/ai`, and waiting for the in-game check. Two items are for the player (U15) or were left out (U21); two parts moved to the second release because they need saved data or phase 4.

New
- [x] **U1 Capture preview.** With a card chosen, pointing at a square (or dragging the card over it) marks every card that play would capture. The plugin works it out by replaying the move on a copy of the table (`capturePreviews` in `Presentation.h`), so Same, Plus, Combo and every other rule count, and nothing hidden is read. `[Gameplay] CapturePreview` and Settings > Gameplay turn it off; How to play mentions it.
- [x] **U2 Lobby facts.** The lobby says when rule challenges open again once the seven-day wait is running.

Fixes and gaps
- [x] **U3 Settings > Controls help** covers inspect, tab switching, adding to a deck and the right-stick cursor, for keyboard and for a pad.
- [x] **U4 Shop tabs** step with LB/RB and Q/E.
- [x] **U5 Prompts in dialogs.** The result dialog shows B/Esc Leave. The trade dialog shows Select and Inspect, and says to choose the winnings to go on. The rule request shows "Leave without answering" for B (see B21).
- [x] **U6 Rules modal** closes from a Close prompt instead of an ×.
- [x] **U7 Shop notice** sits under the gold totals, clear of the grid, the footer rule and the prompts.
- [x] **U8 Deck dialogs** name the deck and use the skin's type.
- [x] **U9 Foil-upgrade picker** is a searchable grid of cards with the chosen card beside it.
- [x] **U10 Owned-count badge** stops shrinking at the text floor.
- [x] **U11 Deck slot names** end with an ellipsis instead of being clipped.
- [x] **U12 Custom colours** have twelve swatches, one press each; the hex field stays for exact values.
- [x] **U13 Colours on paper skins.** Win gold, losses and rank changes use `goldInk()`, `lossInk()` and `gainInk()`.
- [x] **U14 Docs.** `docs/reference-guide.md` covers the deck editor, inspector, capture preview, Journal detail, skins and both input tables. Issue #3 stays open until the physical pad check.

Carried over from [ui-overhaul.md](ui-overhaul.md), each checked against the code first
- [ ] **U15 Physical controller pass.** For the player: the checklist is in `docs/TESTING.md`. The automated tests drive the same input path without a real pad.
- [x] **U16 Card-face verification.** Reviewed in previews at 720p and 1080p on a dark and a paper skin: the deck editor grid, count badges, prize cards and the drag preview. No changes were needed beyond U10.
- [x] **U17 Type scale.** Fixed text sizes use five steps: 15, 18, 21, 26 and 32 (`AlbumLayout` in `App.h`). There were fifteen sizes between 13 and 35. `tests/type_scale_test.py` keeps the sources on the scale. Sizes worked out to fit a space, card faces and letter-spaced capitals are outside it. Jost Bold was already bundled, so nothing is faux-bolded. FreeType rasterisation was not adopted: it adds a vendored dependency for a gain that is hard to see at these sizes.
- [x] **U18 Match screen.** Already done by the later overhaul checkpoints and confirmed: score counts in each player's colour, hands at board size, the rail lit on the side to move, "is thinking…", last-move marks and card backs for hidden cards. Nothing added.
- [x] **U19 Detail panels.** The Journal's player detail adds the rules and trade rule a game with them uses, and their wager cap. The shop already had the album's item-card treatment. The tournament-won dialog has the prize cards and the new picker.
  - Moved to the second release: "cards seen" and "cards won from them" in the Journal. Neither is recorded today, so both need saved data.
- [x] **U20 First-run skin.** With no saved skin, Tessera takes the skin whose `skin.json` lists a `detect` file that exists under Data (`SkinDetection.h`). Marker files are set for Oathvein, Norden, Edge, Untarnished, Dear Diary and Dear Diary Dark Mode. Vel'dun, Nordic, Outlander and Wood and Paper ship no file that tells them apart from the mods they are built on, so they are not detected.
- [ ] **U21 Optional: real small caps and a user font folder.** Not done.

In Skyrim:
- Choose a card and point at squares: the cards that would flip are marked, including by Same and Plus. Turn the setting off.
- Tournament foil prize: pick cards from the grid with a pad.
- Settings > Appearance > Match: custom colours by swatch.
- A rule challenge, then the lobby: it says when the next one opens.
- A fresh `TTCG.ini` (rename the old one): the skin matches the installed interface mod, if it is one of the six.
- The pad checklist in `docs/TESTING.md`.

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

Done on `phase6/tessera-skin` (2026-11-17), stacked on phase 5, and waiting for the in-game check.

- [x] **T1** Index 0 is no longer retired and `TesseraUI/skins/tessera/skin.json` carries the palette, Felt as its board, a gold outline (`ground.edge`), tile corners (`corner: "tile"`) and the diamond on the title rule. The player's cards are gold and the opponent's red. Typeface: Jost. The serif the old theme used was previewed too; it reads worse at card-name size, and Settings > Appearance > Fonts still offers serifs.
- [x] **T2** `ground.gradient` in `skin.json`: three colours from a centre outwards. `App::skinPanel` dithers it once into a 1024×576 texture (`gradientPixels` in `Surface.h`, the generator the boards already use) and stretches that across the main window. No baked file, and any flat skin can use it.
- [ ] **T3** Not done: own panel and title-band art. The drawn outline and corners carry the look; painted art can follow if the skin feels plain in game.
- [x] **T4** The theme menu lists Skyrim first and Tessera second. `TTCG.ini.example`, `native/tests/game_ui_test.cpp`, `tests/interface_settings_test.cpp` and `TesseraUI/skins/README.md` follow. Previews at 720p, 1080p and 4K for the album, lobby, match, results, shop and settings are in `build/previews/tessera`.
- Also fixed: the result, rule-request and inspector panels clipped the outline and corner marks of every flat skin (`App::surface`).

D5: Skyrim stays the default, as recommended. A `Theme=0` left in an INI from before 0.33.0 opens Tessera again.

In Skyrim:
- Settings > Appearance > Interface: choose Tessera. The ground is green with no visible bands, also at 1440p and 4K.
- Play a game: gold for your cards, red for the opponent's, on the felt board.
- Open a result dialog and a pack: popups have the gold outline and corner tiles.

## Phase 7: code cleanup

Done on `phase7/cleanup` (2026-11-17), stacked on phase 6, except C7 and one part of C2. Nothing here changes what the player sees, apart from two small things: Legendary text is the same gold as the card's gem, and a few buttons in Settings and dialogs now follow the skin.

- [x] **C1 The retired web UI is deleted.** What tools still use moved first:
  - `tools/shared/`: the card renderer and affinity artwork (`card-renderer.js`), the foil material, and the styles and theme grains the rule lab uses;
  - `tools/rules/`: the Rules tab's sources (`rule-pages.js`, `lesson-prompts.js`, `rule-demos.js`). `TesseraUI/rules.json` regenerates byte-identical; `BUILDING.md` describes how.

  Deleted: `TesseraUI/*.js|css|html`, the web theme previews, `tools/theme-preview`, the stale `docs/build-manifest.json` and 45 browser tests of the web UI. `TesseraUI/Tessera_Card_Art_*.md` are in `docs/card-art-prompts`.

  Changed from the plan: seven browser tests exercise developer tools that stay (card library, rule lab, expansion editor, catalog audit, workshop server). They and `package.json` are kept; `tools/test-web-tools.sh` runs them. Playwright isn't installed in this checkout, so they were not run; the rule lab's file references were checked by script and its engine test passes.
- [x] **C2 `NativeCollection.h` is split**, except the snapshot emitter.
  - The header keeps the state, the small accessors that read the world, and declarations. Its large functions moved, bodies unchanged, to `src/CollectionSync.cpp`, `CollectionMatch.cpp`, `CollectionAlbum.cpp`, `CollectionSnapshot.cpp` and `CollectionSave.cpp`. `NativeOpponents.h`, `NativeExchange.h` and `NativeShop.h` became declaration headers over `src/NpcIntegration.cpp`, `Exchange.cpp` and `ShopCommands.cpp`. `NativeTournamentCommands.h` is `src/TournamentCommands.cpp`.
  - The forward declarations that pointed at a header included later are gone, and `exchangeIntegration::reset/publish` are declared and defined alike.
  - The move was scripted: only `inline` and default arguments were removed from each definition. State stays `inline` in the headers, so the order in which it is initialised is as before.
  - **Not done: building the snapshot JSON with nlohmann.** `campaign::json` feeds every screen and can only be exercised in the running game. It also stitches in four JSON strings from core headers, which a rewrite would have to parse back. That is the wrong risk for a release of fixes and interface work. Do it with C7, where the save and snapshot code gets an in-game pass anyway, or behind a test that compares the old and new output.
- [x] **C3 Duplication.**
  - `payGoldCredit` and `campaign::payPlayerGold` replace three clamped payouts. The one in `recover()` stays apart: it also collects a negative credit.
  - `grantDevelopmentCards`, `resolveActiveOpponent`, and `validHand` as `validDeck` with every slot filled.
  - `native/src/Colour.h`: `rgb`/`alpha`/`fade`, tier numerals and rarity pigments. Legendary is `e6bc65` everywhere.
  - `App::inspectRequested` for the seven inspect triggers; `App::action` for ten raw `ImGui::Button` calls. Two `ImGui::Checkbox` calls remain on the Developer page.
  - The Same/Plus check is `Match::Contact::same/plus`; the hash multiplier is `seedSpread`.
- [x] **C4 Dead code** removed: `App::closing` and its close path, `autoRotate`, `showMetrics`, `GameApp::filtersOpen`, `Assets::card/back/megabytes/decoded`, `Palette::name` and `Palette::centre` (and the `centre` colour in every `skin.json`). The palette table is keyed by theme id, with a `static_assert` that every theme a player can choose has a row.
- [x] **C5 Named constants.** `RuleMask` is derived from `ruleFlags`; `lowestRank`, `aceRank`, `reversedRank`; `restockHours`; `travellerReturnHours` (the 168 in `Campaign.h` was M'aiq's week, not the rule-culture cooldown); `MaterialSlot` names every slot range; font choices go through one `chosenFont`.
- [x] **C6 `Match::play` is split.** `touch`, `resolveCaptures` and `resolveCombo` are the rules; `groupStage`, `modifiersBefore`, `recordModifierStages` and `recordCaptureStages` are what the interface plays back. Compared with the previous engine over 2.67 million random plays across random rule sets (61,000 Same/Plus stages, 37,000 Combo stages, 24,000 group choices): identical results, flags, stages and boards, at the same speed.
- [ ] **C7 Sectioned save format (D7).** Moved to the second release with phase 4. Format 35, never 34.
  - Each part becomes its own section, so one bad section no longer loses the rest.
  - Parsing is separated from validation, and the code moves to a .cpp in a small core library the tests link against.
  - Golden save files for formats 20, 21 and 26–33 must load unchanged.
- [x] **C8 Docs.** `ui-overhaul.md` is marked complete, the interface checklist in `docs/TESTING.md` is current, and test counts are gone from the docs.
- [ ] **C9 Optional, with the player's OK.** Prune `dist/` (old test builds) and stale `build/` trees. Not done: it deletes files outside git.

In Skyrim (a regression pass; nothing new to see):
- Open the album, start and finish a staked game, settle it, and reload the save. This crosses every moved source file.
- Enter a tournament game from the board and collect a prize.
- Buy and sell a card with a merchant; open a pack.
- With a mod that adds opponents or uses the exchange API installed, check its NPCs still offer games and its offers still complete.
- Settings > Developer and the album's filter popup: the buttons look like the rest of the skin and still work.

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
