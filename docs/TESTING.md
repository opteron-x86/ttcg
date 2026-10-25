# Testing

`ctest --preset native` runs gameplay, collection, save, trading, tournament, record, interface and packaging tests. The interface tests exercise production screens with generated game snapshots.

`python tools/test-windows.py --build build/windows --output build/test-results` checks DLL metadata, SKSE queries, the collection API, menu object lifetimes and DirectX state restoration. Logs and a JSON report are written to the output directory.

## In Skyrim

Test with an ordinary play save and a separate development save:

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

The established gameplay test runtime is Skyrim SE 1.6.1170 under Linux/Proton. The DLL enables SE, AE and VR; the automated loader checks cover their declarations. In-game validation on other runtimes and VR remains to be recorded here.
