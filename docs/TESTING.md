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

The established gameplay test runtime is Skyrim SE 1.6.1170 under Linux/Proton. The DLL enables SE, AE and VR; the automated loader checks cover their declarations. In-game validation on other runtimes and VR remains to be recorded here.
