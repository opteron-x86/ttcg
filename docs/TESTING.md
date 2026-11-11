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

The established gameplay test runtime is Skyrim SE 1.6.1170 under Linux/Proton. The DLL enables SE, AE and VR; the automated loader checks cover their declarations. In-game validation on other runtimes and VR remains to be recorded here.

## Native UI overhaul checkpoint

In-game results for `feat/oathvein-skin` remain unrecorded. Use the Main package under `dist/ui-phase1-followups/`; its `build-info.json` identifies the source and tested DLL. Keep the separate Card Art installation enabled.

- Switch between Oathvein, Vel'dun, Norden, Untarnished, Edge and Dear Diary. Check the album, lobby, match and result dialog at the normal play resolution.
- In Appearance, check that default fonts and colours identify the skin. Switch between Framed and Borderless: nameplate/shadow/stroke controls belong to Borderless; framed border controls appear with the control marker. Explicit font/colour choices should survive theme changes and restarts.
- Move between keyboard/mouse and a physical controller. Prompts should switch to the installed icon pack's A/B/Y glyphs, or readable plain caps without that pack. Verify focused pad Y inspection in the lobby, album, decks and shop. Settings and Journal should not advertise inspection; modal dialogs should not leave active-looking prompts behind them.
- Repeatedly open and close Tessera, including after card inspection, match setup and results. Test both paused and unpaused play. The scene should blur only while Tessera is open and recover after closing, forced closure and save reload.
- Complete a match, settle the trade, open packs and receive a tournament reward. Confirm the framed cards, new board materials and result summaries do not obstruct selection or settlement.

Record runtime, resolution, input device and any UI/icon mods with results. Desktop previews, the 89 native tests and Wine probes do not replace these Skyrim checks.

## Merchant inventory crash isolation

The first `noInit` cleanup candidate still crashed during the reporter's death/reload test on 1.7.104. The new crash log's DLL fingerprint matches the CI candidate, and the stack still passes through leveled-inventory initialization and scrap-heap cleanup. The cause remains unconfirmed.

Install the diagnostic DLL as a separate mod after the Tessera Main installation. Keep Card Art enabled as its own mod. Set the following in the effective `Data/SKSE/Plugins/TTCG.ini`:

```ini
[Diagnostics]
Enabled=1
VendorPackInjection=0
```

Restart Skyrim completely and confirm `TTCG.log` says `Vendor pack injection: disabled (diagnostic)`. Load the original pre-Tessera save, repeat at least five death/reload cycles, and try manual reloads. Existing packs can remain; untouched merchant chests will not receive Tessera pack rolls in this mode. Seller dialogue and the rest of Tessera remain active.

Repeat with `VendorPackInjection=1` using the same diagnostic DLL, the same original save, and a full process restart. Preserve `TTCG.log`, `skse64.log`, and any crash log from each mode, along with the death/reload count and whether an album or shop was opened. If disabled mode passes while enabled mode crashes, that narrows the investigation to vendor pack injection; it does not establish the underlying defect.
