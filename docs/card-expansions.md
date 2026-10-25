# Creating a Tessera card expansion

A card expansion is a JSON file describing your cards, plus their artwork. Install it alongside Tessera 0.30.0 or later and the Card Art download. Tessera reads it when Skyrim starts.

## Make a set in the editor

Open [Expansion Workshop](expansion-editor.html) in Chrome, Edge or Firefox. It works offline on Windows and Linux.

1. Give your set a name and a set ID, such as `my-cool-cards`.
2. Choose a group, then drop artwork onto **Add card** in the tier you want. You can also click **Add card** and choose artwork later.
3. Set the card's name, rarity, four values, group and affinity. Choose how players will get it, then adjust the artwork crop.
4. Use **Save project** to keep an editable copy with your artwork. The browser also saves your draft, but a project file lets you move it to another computer or recover it later.
5. Use **Export expansion** to make an installable ZIP. Install that ZIP as its own MO2 or Vortex mod, then fully restart Skyrim.

**Tessera tier rules** checks values against the usual card strength rules. Choose **Custom values** if you want different strengths. You can add as few cards as you like; you do not need to fill every tier or rarity.

You can reopen a saved project, expansion ZIP or mod folder. Opening a JSON file alone also works, but you will need to supply its images. Keep your original artwork separately from the editor project.

## Example cards

The [My Cool Cards example](../examples/my-cool-cards/SKSE/Plugins/Tessera/Expansions/my-cool-cards.json) defines three cards:

| Card | Tier and rarity | Where it can appear |
|---|---|---|
| High Elf Waifu | II, Rare | Standard packs and card traders |
| Dark Elf Waifu | III, Epic | Standard packs, tournament packs and specialist dealers |
| Ultimate Waifu | X, Legendary unique | Masters tournament prizes |

Supply images at the paths listed in the JSON, then zip the `SKSE` and `TesseraUI` folders together. In the album, choose **Cards → Filters → Card set → My Cool Cards**.

The sample uses custom card strengths. Choose **Custom values** in the editor to export it with those values unchanged.

For quick testing, **Settings → Development → Add all cards** includes installed expansions. Use a different save when testing Ultimate Waifu as a tournament prize: once a unique is owned, it cannot be selected as a new prize. Tournament prizes are random, and newly installed cards only enter tournaments announced afterward.

## Edit the JSON yourself

The example uses this folder layout:

```text
SKSE/Plugins/Tessera/Expansions/my-cool-cards.json
TesseraUI/art/expansions/my-cool-cards/high-elf-waifu.jpg
TesseraUI/art/expansions/my-cool-cards/high-elf-waifu-thumb.webp
```

Put the JSON file directly in `Expansions`. Keep it as a loose file, outside a BSA. Here is a complete one-card example:

```json
{
  "schemaVersion": 1,
  "setId": "my-cool-cards",
  "name": "My Cool Cards",
  "cards": [
    {
      "id": "high-elf-waifu",
      "name": "High Elf Waifu",
      "tier": 2,
      "rarity": "Rare",
      "sides": [1, 3, 5, 2],
      "groups": ["High Elf"],
      "affinities": ["Agility"],
      "unique": false,
      "art": "expansions/my-cool-cards/high-elf-waifu.jpg",
      "thumbnail": "expansions/my-cool-cards/high-elf-waifu-thumb.webp",
      "acquisition": {
        "standardPacks": true,
        "tournamentPacks": true,
        "merchants": "all"
      }
    }
  ]
}
```

The card's full ID is `my-cool-cards:high-elf-waifu`. Once you release a set, keep its set ID and card IDs unchanged so saved collections still recognize the cards. You can change display names, values and artwork later.

Use lowercase letters, numbers, hyphens or underscores for IDs. Start with a letter and stay within 64 characters. Choose your own set ID; `tessera` is reserved for the built-in cards.

| Setting | What to enter |
|---|---|
| `tier` | A number from 1 to 10 |
| `rarity` | `Common`, `Rare`, `Epic` or `Legendary` |
| `sides` | Four values from 1 to 10, ordered **top, right, bottom, left**. A value of 10 appears as A. |
| `groups` | One or more groups from the list below |
| `affinities` | `Strength`, `Intelligence`, `Willpower`, `Agility` or `Endurance`. Use `[]` for Neutral. |
| `unique` | `true` limits the card to one copy in a deck across normal and foil finishes. Other cards allow two. |
| `art`, `thumbnail` | Image paths starting from `TesseraUI/art/`; leave them out to use a card-back placeholder |
| `acquisition` | Where players can get the card. Options you leave out are disabled. |

Supported groups are Argonian, Beast, Beastfolk, Breton, Construct, Daedra, Dark Elf, Dragon, God, High Elf, Imperial, Khajiit, Nord, Orc, Redguard, Spirit, Undead and Wood Elf. Groups work with Legion and Decimation. Expansions cannot add new groups or scripted card abilities.

## Artwork

Use the illustration alone. Tessera adds the name, values, badges and foil finish. Portrait images around 3:4 work well. The editor exports 768×1024 WebP faces and 240×320 thumbnails; hand-made sets can also use PNG or JPEG.

Keep images inside `TesseraUI/art/expansions/<your-set-id>/`. In JSON, write paths like `expansions/my-cool-cards/high-elf-waifu.jpg`. Use forward slashes on Windows and Linux, and match the filename's capitalization. Filenames can contain letters, numbers, hyphens, underscores and dots, but no spaces. Use lowercase file extensions. Display names on the cards can contain spaces and accented letters.

A missing face shows the Tessera card back. A missing thumbnail falls back to the full image.

## How players get your cards

| Option | Cards it supports |
|---|---|
| `"standardPacks": true` | Common, Rare and Epic cards in tiers I–VI |
| `"tournamentPacks": true` | Common, Rare and Epic cards in tiers III–VII |
| `"merchants": "all"` | Non-unique Common, Rare and Epic cards in tiers I–VII. Use `"regular"` or `"specialist"` to limit the dealers, or `"none"` to disable sales. |
| `"tournamentPrizes": ["masters"]` | Unique Legendary cards of any tier. Use `"invitational"`, `"masters"`, or both. |
| NPC or quest rewards | Cards you assign through an opponent file or a script |

Pack and shop settings still follow the tier limits above. For example, the High Elf Waifu card has `tournamentPacks` enabled, but its Tier II keeps it out of those packs. Traders add eligible expansion stock at their next restock, normally after 48 game hours.

For a unique tournament prize, use settings like these from Ultimate Waifu:

```json
"rarity": "Legendary",
"unique": true,
"acquisition": {
  "standardPacks": false,
  "tournamentPacks": false,
  "merchants": "none",
  "tournamentPrizes": ["masters"]
}
```

Use `["invitational", "masters"]` to allow either tournament type. Regular tournaments do not award these cards. A tournament reserves its prize when it is announced. If an NPC wins it, they can use it in later games, giving you another way to win it.

Unique cards cannot come from ordinary packs or shop stock. Tessera will not create another copy while one is owned or reserved as a prize. Account for this if a quest can also award the same unique.

NPCs can play expansion cards they win from you. To give an NPC your cards from the start, follow [Adding Tessera opponents](opponent-integrations.md).

## Give a card as a quest reward

Use the supplied `Scripts/Source/TTCG_CardExpansion.psc` when compiling your script:

```papyrus
If TTCG_CardExpansion.IsCardLoaded("my-cool-cards:dark-elf-waifu")
    TTCG_CardExpansion.GiveCard("my-cool-cards:dark-elf-waifu", 1, False)
EndIf
```

The arguments are the full card ID, quantity, and whether it should be foil. Quantities can be 1–99, or 1 for a unique. Tessera processes the request shortly afterward; the function does not return a success value. Rejected rewards appear in `TTCG.log`.

Have your quest remember that it gave the reward so it does not repeat on every load. Tessera already supplies the compiled API scripts; your expansion should include only its own scripts. A set that uses only JSON and artwork does not need an ESP.

## Check and share your set

Restart Skyrim after changing the JSON. Check the artwork, card values, deck editing and a game. Save and reload to check that your cards stay in the collection.

If the set does not appear, check `TTCG.log` for its filename. Common mistakes are a misspelled setting, duplicate ID, trailing comma or missing quotation mark. JSON does not allow comments. Each file can contain 1–512 cards and must stay under 2 MiB; Tessera supports up to 128 sets and 4,096 expansion cards in total.

For release, distribute the ZIP exported by the editor, or zip your own `SKSE` and `TesseraUI` folders together. Leave the authoring guides and editor out of the installed mod.

Removing a set makes its cards unavailable but keeps their saved ownership. Reinstalling it with the same IDs restores them. A deck containing unavailable cards cannot start a game.

If you have permanently removed a test set, open **Settings > Gameplay > Remove unavailable cards** in the album. Confirming removes saved copies and discovery marks for unavailable cards and clears their deck slots. Installed cards and game progress are kept. This also covers cards removed from a newer version of an installed set. After cleanup, reinstalling the old definitions will not restore those copies; keep an earlier save if you may want them back. Development mode is not required.

To start collecting again after granting yourself every card, set `[Development] Enabled=1` in `SKSE/Plugins/TTCG.ini`, restart Skyrim, and use **Settings > Developer > Clear collection**. Confirming deletes all standard and foil copies, including unavailable cards, clears discoveries and empties every saved deck for the current character. The album remains acquired; gold, unopened packs, opponents, match records and reputation are kept. It also turns off `AddAllCards`, `AddAllFoils` and `RevealAllCards` in the INI for all characters so loading a save cannot refill the collection automatically. Set `Enabled=0` and restart to hide Developer settings when finished.

For a complete restart, use **Settings > Developer > Reset Tessera**. This replaces the current character's collection with the normal eight starter cards and one starter deck, keeping the album acquired. It erases match records, reputation, tournament history and prizes, regional rule changes, opponent and merchant collections, buyback stock and manually added players. Card exchange offers are cancelled. Carried standard and tournament packs and old tournament invitations in the player's or courier's inventories are removed. Stored packs in other containers, Skyrim gold, quests and interface preferences are kept. Automatic and authored opponents register again under the current settings and Skyrim quest state.

Reset also restores the development overrides in the INI to their defaults for all characters, including grants, roster visibility, unlocks, forced match rules and test tournaments. Development mode itself stays enabled so the tab remains available. Cancelled exchange serials and the tournament ID counter are retained internally to reject old quest and courier callbacks; they do not preserve gameplay progress.

All three actions require you to finish or forfeit interrupted matches, settle pending trades and payments, and finish or withdraw from a tournament. Full reset also waits for pending merchant/tournament deliveries and a courier request already being handed off. If an INI write fails, saved progress stays intact. Restore an earlier save to undo deletion. Addons that keep their own acquisition history, such as Completionist, retain that history.
