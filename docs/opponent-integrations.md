# Adding Tessera opponents

You can give cards to an existing player or make a custom NPC play Tessera. Put the JSON file directly in `SKSE/Plugins/Tessera/Opponents/`, install it as a separate mod, and restart Skyrim after making changes.

## Give an NPC cards

The included `opponent-example` gives Hulda two High Elf Waifu cards and one foil Dark Elf Waifu. Install it alongside **My Cool Cards**, then meet Hulda and play. Ultimate Waifu stays available for the Masters prize pool.

```json
{
  "schemaVersion": 1,
  "id": "my-cool-cards-opponents",
  "grants": [{
    "id": "hulda-my-cool-cards",
    "actorBase": {"plugin": "Skyrim.esm", "formId": "013BA3"},
    "cards": [
      {"card": "my-cool-cards:high-elf-waifu", "quantity": 2},
      {"card": "my-cool-cards:dark-elf-waifu", "foil": true}
    ]
  }]
}
```

Tessera adds these cards once, when it next checks the NPC nearby or prepares their collection. Losing the cards does not give them another copy. Keep the grant's `id` unchanged; use a new ID if a later update should give a new reward.

Use `set-id:card-id` to name a card. Built-in cards use IDs such as `tessera:murkwater-goblin`. Quantity defaults to 1 and can be 1–99; `foil` defaults to `false`. A unique allows one copy and is skipped if already owned or reserved elsewhere. Missing card mods delay the grant until they are installed.

For NPCs already in Tessera's roster, use a grant like this to add cards. Their existing skill and profile stay in place. An eligible NPC outside the roster gets a generated profile when you give them cards.

## Make a custom opponent

This example gives a follower an Expert collection with a foil card from My Cool Cards. Replace the plugin name and FormIDs with those from your mod.

```json
{
  "schemaVersion": 1,
  "id": "my-follower-tessera",
  "opponents": [{
    "id": "companion",
    "actorBase": {"plugin": "MyFollower.esp", "formId": "000800"},
    "reference": {"plugin": "MyFollower.esp", "formId": "000801"},
    "skill": "Expert",
    "homeRegion": "Whiterun",
    "reputation": "Unknown",
    "tournaments": false,
    "collection": {
      "mode": "generated",
      "theme": "Breton",
      "cards": [{"card": "my-cool-cards:dark-elf-waifu", "foil": true}],
      "restock": true
    },
    "dialogue": {"mode": "generic"}
  }]
}
```

`actorBase` identifies the NPC record; `reference` identifies the placed NPC in the world. Use the plugin that first created the record and its local hexadecimal FormID, without the load-order prefix. This also works for ESL plugins. The reference is optional; without it, Tessera registers the NPC when you meet them.

| Setting | What it does |
|---|---|
| `skill` | Choose `Beginner`, `Learner`, `Regular`, `Experienced`, `Expert` or `Master`. This affects their play and generated cards. |
| `homeRegion` | Their hold for the Journal and local rules. Use `Elsewhere` if they have no Skyrim hold. |
| `reputation` | The player's required renown. `Unknown` lets them play without earning renown first. Other choices are `Unsung`, `Recognized`, `Respected`, `Famous` and `Legendary`. |
| `tournaments` | Set to `true` to let them enter tournaments when they meet the usual requirements. Defaults to `false`. |
| `collection.mode` | `generated` gives them a normal collection plus your listed cards. `authored` uses only the listed cards. |
| `collection.theme` | An optional card group to favor when generating their collection |
| `collection.restock` | `true` lets them replenish ordinary cards over time. `false` lets their collection run out. |
| `wager` | Maximum gold wager, from 0 to 2,000. Defaults to 100, independent of card skill. |
| `traveller` | `true` places them in the Travellers Journal section and removes the default local renown requirement |
| `requiredForLegendary` | `true` counts a local Master toward the hold's “defeat every Master” requirement. Defaults to `false`. |
| `enabled` | Starts at `true`. Your script can change it later. |

Holds are `Whiterun`, `Falkreath`, `Haafingar`, `Hjaalmarch`, `The Pale`, `The Rift`, `Winterhold`, `Eastmarch`, `The Reach` and `Solstheim`. The default is `Elsewhere`.

Each opponent has a saved betting purse, separate from their inventory and shop funds. It starts at three times their wager cap and tops up toward that amount every three game days. Winnings can take it above that target. Rematches do not refill it. Players can choose any offered stake within the cap that both sides can afford; set `wager` to 0 for card-only stakes.

If you leave out `reputation`, local Experienced opponents require Recognized renown, Experts require Respected, and Masters require Famous. A fully authored collection needs enough cards for a legal five-card hand. Use at most two copies of an ordinary card or one of a unique in that hand.

Starting collections are given once. Editing the starting list later does not reset cards the NPC has won or lost. Use a new grant to add cards to an existing save.

Opponents normally follow their hold's rules. To give them personal rules, add:

```json
"rules": ["Open", "Plus"],
"trade": "One"
```

Available rules are Open, Three Open, Same, Plus, Reverse, Affinity, Legion, Decimation, Order, Chaos, Fallen Ace, Sudden Death, Same Wall, Swap, Ascension, Descension and Dominion. Trade choices are None, One, Diff, Direct and All. Same Wall needs Same. Ascension and Descension cannot be used together; other incompatible combinations are also rejected. Tournament games always follow the tournament's announced rules.

## Decide when they can play

Named adult humanoids can qualify, including Khajiit and Argonians. Animals, children, temporary or respawning NPCs, summons and ghosts cannot join through this system. Dead, hostile or disabled NPCs cannot play, and games are blocked during combat, scenes or sleep.

Use `availability` for your mod's story requirements. For example, add this inside the opponent entry to wait until a quest stage has been completed:

```json
"availability": [{
  "quest": {"plugin": "MyFollower.esp", "formId": "000900"},
  "stageDone": 20
}]
```

You can list several conditions; all must pass. Supported conditions are:

| Condition | Example |
|---|---|
| A quest stage was completed | `{"quest": {"plugin": "MyFollower.esp", "formId": "900"}, "stageDone": 20}` |
| A quest has reached a stage number | `{"quest": {"plugin": "MyFollower.esp", "formId": "900"}, "minimumStage": 30}` |
| The player belongs to a faction | `{"faction": {"plugin": "MyFollower.esp", "formId": "902"}, "subject": "player"}` |
| A global is at least 1 | `{"global": {"plugin": "MyFollower.esp", "formId": "903"}, "minimum": 1}` |

Use `"subject": "opponent"` for an NPC faction condition. For more complicated story checks, have your quest set a global or call `TTCG_Opponents.SetEnabled`. Grants can also have `availability` conditions.

To keep specific NPCs or a whole mod out of Tessera, put this in its own opponent JSON file:

```json
{
  "schemaVersion": 1,
  "id": "story-exclusions",
  "exclude": [{"plugin": "MyQuest.esp", "formId": "800"}],
  "excludePlugins": ["MyCrowdMod.esp"]
}
```

## Use your NPC's own voice

Set `dialogue.mode` to `custom` and create your own dialogue and recordings. In the accepting dialogue's end fragment, call:

```papyrus
TTCG_Native.BeginMatch(akSpeaker)
```

Use these conditions on your Tessera dialogue:

- `TTCG_GamesEnabled` must equal 1. This global is in `Tessera TCG.esp`, local FormID `000FDA`.
- Check the NPC's faction rank in `TTCG_PlayerAvailability`, local FormID `000886` in the same plugin. Rank 1 means ready to play, 2 means they decline, and 3 means they lack enough cards. Rank 0 or no membership means unavailable.
- Add your story conditions, and also put them in JSON or apply them through `SetEnabled` so the challenge hotkey follows them.

To let the hotkey use your recorded responses too, supply the dialogue INFO records:

```json
"dialogue": {
  "mode": "custom",
  "accept": {"plugin": "MyFollower.esp", "formId": "A00"},
  "decline": {"plugin": "MyFollower.esp", "formId": "A01"},
  "noCards": {"plugin": "MyFollower.esp", "formId": "A02"}
}
```

If you omit `accept`, the hotkey opens the lobby when the NPC is ready. Missing refusal lines use a HUD message. `generic` mode uses Tessera's shared acceptance line when the voice has a recording, otherwise subtitles.

The ready-to-edit template is in `opponent-templates/custom-opponent.json.template`. Fill in your records, rename it to end in `.json`, and place it in the Opponents folder. Compile scripts against the included source declarations. Tessera supplies their compiled versions; do not ship replacements.

## Calls and events for quests

```papyrus
TTCG_Opponents.AddPlayer(MyActor)
TTCG_Opponents.SetEnabled(MyActor, True)
TTCG_Opponents.Refresh(MyActor)
TTCG_Opponents.GiveCard(MyActor, "my-cool-cards:dark-elf-waifu", "my-quest:chapter-two", 1, True)
```

`GiveCard` takes the NPC, full card ID, a reward ID, quantity and foil choice. Keep the reward ID unchanged to prevent repeats. `HasReceivedGrant` tells you whether it was processed. A unique that already exists elsewhere is skipped but still counts as processed. Calls take effect shortly afterward, so allow time before checking the result. A scripted grant attempted during unsettled match stakes needs another request later; JSON grants wait automatically.

`IsRegistered` checks whether the NPC is a Tessera player. `GetAvailability` returns the ranks described above, or -1 if unregistered. `SetEnabled` applies to added opponents.

The example `opponent-templates/MyTesseraEvents.psc` listens for completed games. The two events available to your quests are:

| Event | Data |
|---|---|
| `TTCG_MatchCompleted` | Sender is the opponent. The number is -1 for a draw, 0 for a player win, or 1 for an NPC win. The text contains JSON with `rules`, `tournament` and `forfeited`. |
| `TTCG_CardTransferred` | Sender is the opponent. The number is +1 for a card going to the player or -1 for a card going to the NPC. The text contains JSON with the full `card` ID and `foil`. |

These events arrive after the result and trades are settled. A tournament sends a match event for each game. Card transfer events describe traded copies; reward grants do not send them.

## Automatically added residents

Players can also use **Settings → Gameplay → Modded players** to allow some or all eligible residents. Tessera gives them a skill and collection and remembers them in that save. Turning this off suspends automatically added opponents without deleting their cards.

To add someone manually, face them, open the album and use **Settings → Gameplay → Add player**. They still need to meet the normal requirements.

**Only vanilla voices** limits automatically and manually added players to Skyrim and DLC voice types. Opponents defined in your JSON can use custom voices. Giving an NPC cards alone does not exempt them from the voice setting; define their opponent profile when they need that exception.

## Check your files

Keep file, opponent and grant IDs unchanged after release. Use lowercase letters, numbers, hyphens and underscores, starting with a letter. IDs have a 64-character limit. A batch of cards can contain up to 128 copies. Avoid duplicate profiles for the same NPC.

After restarting Skyrim, check the NPC's dialogue, their cards, any story conditions, and a save/reload. Problems appear in `TTCG.log`. Missing NPC or card mods leave their saved data in place until reinstalled.
