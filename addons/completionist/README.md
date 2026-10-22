# Tessera — Completionist

Track Tessera cards in Completionist. Open Completionist's addon menu and choose **Tessera Cards** or **Tessera Foils**. Each checklist is grouped by tier.

A card stays checked after you sell, trade or lose it. A foil checks both lists. Seeing an opponent's card does not count as collecting it.

## Install

Requires Tessera 0.32.5 or newer, Completionist NG 5.0.0.4 or newer, and their usual requirements. Install this archive as a separate mod and enable **Tessera Completionist.esp** after Tessera and Completionist. It uses a light plugin slot.

You can add it to an existing game. Your current collection is added automatically. Cards you sold or lost before installing the addon cannot be recovered from Tessera's discovery record; you can check those entries yourself in Completionist.

The included checklists cover 674 released cards and their foils. Unreleased cards are left out. Unique cards are included, and tournament foil upgrades count.

Progress is saved with each character's SKSE co-save. Loading an older save restores that save's progress. Updates happen when the collection changes or a save loads; there is no repeating background scan. Large collections are checked in small batches, so the initial update can take a moment. Reopen the checklist page to refresh its display.

This addon does not add cards to your inventory or change Tessera's collection, decks or rewards. Keep Completionist's item tracking enabled for these checklists.

## Custom expansions

Expansion authors can generate matching checklists using the tool in Tessera's source:

```bash
python3 tools/build-completionist-addon.py --expansion path/to/my-cards.json --output build/My-Cards-Completionist
```

Install that folder alongside the expansion and this addon, then enable its ESP. Keep the generated `card-ids.json` when rebuilding or distributing a checklist: it keeps the same entries linked to existing saves. Expansion checklists use a separate light plugin. They do not include the card art or replace the expansion itself.

## Translation

The two text files in `SKSE/Plugins/CompletionistData/Translations` contain the page labels and card names. Edit the text after `|`, keeping the keys and tabs intact. The addon also accepts translated page names.

## Troubleshooting

If the pages are missing, check that all three ESPs are enabled and that Completionist's item tracking is on. `Completionist.log` reports checklist loading; `TesseraCompletionist.log` reports the connection to Tessera. Closing the MCM retries any unfinished update.
