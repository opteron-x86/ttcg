# Testing

`ctest --preset native` runs gameplay, collection, save, trading, tournament, record, interface and packaging tests. The interface tests exercise production screens with generated game snapshots.

`python tools/test-windows.py --build build/windows --output build/test-results` checks DLL metadata, SKSE queries, the collection API, menu object lifetimes and DirectX state restoration. Logs and a JSON report are written to the output directory.

## In Skyrim

Test with an ordinary play save and a separate development save:

- Load a save made before installing Tessera. Die and let the game reload at least three times, then manually reload without dying. Repeat after visiting a general-goods merchant; check that packs appear and an album can be purchased. Run this on Skyrim 1.7.104 as well as 1.6.1170.
- Load an older Tessera save with merchant albums still in inventory. Confirm those albums are removed, other stock remains, and packs return at the normal merchant restock.
- Buy an album, open it, change decks and reload the save.
- Start and finish a match, inspect cards, settle the result and resume an interrupted game.
- Buy and sell cards, use buyback and open both pack types.
- Enter a tournament, complete a pairing, receive prizes and reload.
- Navigate with mouse, keyboard and a controller. Close the interface during each type of interaction.
- Check themes, text and card art at your usual resolution.

For collection recovery, use a separate development save and retain a copy before deletion:

- Grant normal cards and foils, include expansion cards in multiple decks, then remove an expansion and reload. Cancel **Settings > Gameplay > Remove unavailable cards** once, then confirm it. Installed cards remain; unavailable ownership, discovery marks and deck slots disappear. Save, reload and reinstall the expansion: removed copies stay gone.
- With `[Development] Enabled=1`, cancel **Settings > Developer > Clear collection** once, then confirm it. All card copies, discoveries and deck slots disappear. Album access, gold, unopened packs, deck names, opponents and progress remain. Save and reload; automatic grants stay disabled and starter cards do not reappear. Acquire a new card and check the album and collection API.
- Disable development mode and restart: the Developer section disappears, while missing-expansion cleanup remains available. Verify recovery is blocked during interrupted matches, unsettled trades, active tournaments and pending foil rewards. A failed INI write must leave the collection intact.
- Build up match records, reputation, changed regional rules, extra decks, tournaments, buyback stock and manually added players. Cancel **Reset Tessera** once, then confirm. The album remains with eight starter cards and one starter deck; the previous progress is gone. Carried packs and old courier invitations are removed, while Skyrim gold, unrelated items, stored packs and interface preferences remain. Card exchange quests receive cancellation. Reopen, save and reload: defaults and starter cards persist without duplicates, and new collecting, trading and tournaments work normally.
- Try a full reset while a merchant payment, tournament pack delivery or courier request is pending; the reset must wait. Verify old UI commands, queued card grants, exchange completions and courier acknowledgements cannot restore pre-reset progress.

With a physical controller (the automated tests drive the same input path, but not a real pad):

- D-pad and left stick move focus on every page; A selects; B goes back once per press and never closes Tessera while a list is open, a slider is being adjusted or a card is being dragged.
- The right stick moves the cursor; A then clicks and drags what is under it, Y inspects it and X adds it to the deck.
- In the inspector the right stick turns the card.
- LB and RB step through the album's tabs and the shop's Buy, Sell and Buy back.
- Choose a custom control colour with the swatches, and a foil upgrade from the grid, without touching the mouse.
- The prompts show pad glyphs while the pad is in use and switch back on a key press or mouse movement.
- Repeat the match, deck and shop checks above with keyboard and mouse only.

The established gameplay test runtime is Skyrim SE 1.6.1170 under Linux/Proton. The DLL enables SE, AE and VR; the automated loader checks cover their declarations. In-game validation on other runtimes and VR remains to be recorded here.

## Interface

Desktop previews, the native tests and the Wine probes do not replace these checks. Record the runtime, resolution, input device and any UI or icon mods with the results.

- Skins: switch between every skin in **Settings > Appearance**, Tessera included. Check the album, lobby, match and a result dialog at the normal play resolution. On Tessera the green ground shows no bands, and dialogs have the gold outline and corner tiles.
- First run: with `Theme` removed from `TTCG.ini` and one of Oathvein, Norden, Edge, Untarnished, Dear Diary or Dear Diary Dark Mode installed, Tessera opens with that skin; with none, Skyrim.
- Card face: in Appearance, default fonts and colours follow the skin. Switch between Medallions, Borderless and Printed: nameplate, shadow and stroke controls belong to Borderless; the other two show border controls with the control marker. A font or colour chosen by hand survives a skin change and a restart.
- Capture preview: with a card chosen, pointing at a square marks the cards it would take. Turn it off in **Settings > Gameplay** and the marks are gone.
- Blur: the scene blurs only while Tessera is open. Check repeated opening and closing, after card inspection, match setup and results, paused and unpaused, and after a forced close and a save reload.
- Complete a match, settle the trade, open packs and receive a tournament reward. Cards, board and result summaries must not obstruct selection or settlement.

## Merchant inventory crash isolation

The first `noInit` cleanup candidate still crashed during the reporter's death/reload test on 1.7.104. The new crash log's DLL fingerprint matches the CI candidate, and the stack still passes through leveled-inventory initialization and scrap-heap cleanup. The cause remains unconfirmed.

Since then the pack roll is added to chest bases by the game's own routine, not CommonLib's `TESContainer::AddObjectToContainer`. SkyPatcher changed to the same routine after crashes from leveled lists added to containers (see `docs/roadmap.md`, phase 2). `TTCG.log` reports one of:

- `Vendor pack injection: enabled`, followed by `packs added to N of M container bases`;
- `Vendor pack injection: disabled (diagnostic)` when `VendorPackInjection=0`;
- `Vendor pack injection: unavailable on this game version` when the routine isn't found or doesn't begin as expected. Merchants then stock no packs, and Settings > Gameplay says so.

Ask for this line with any crash report. The steps below still isolate the injection.

Install the diagnostic DLL as a separate mod after the Tessera Main installation. Keep Card Art enabled as its own mod. Set the following in the effective `Data/SKSE/Plugins/TTCG.ini`:

```ini
[Diagnostics]
Enabled=1
VendorPackInjection=0
```

Restart Skyrim completely and confirm `TTCG.log` says `Vendor pack injection: disabled (diagnostic)`. Load the original pre-Tessera save, repeat at least five death/reload cycles, and try manual reloads. Existing packs can remain; untouched merchant chests will not receive Tessera pack rolls in this mode. Seller dialogue and the rest of Tessera remain active.

Repeat with `VendorPackInjection=1` using the same diagnostic DLL, the same original save, and a full process restart. Preserve `TTCG.log`, `skse64.log`, and any crash log from each mode, along with the death/reload count and whether an album or shop was opened. If disabled mode passes while enabled mode crashes, that narrows the investigation to vendor pack injection; it does not establish the underlying defect.
