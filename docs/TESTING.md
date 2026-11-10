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
