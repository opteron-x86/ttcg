# Tessera rule table

From the Tessera source folder, run:

```sh
python3 tools/serve-rule-lab.py --open
```

Open [the table](http://127.0.0.1:8137/tools/rule-lab/) while that command is running.

Choose rules and decks, then **Deal cards**. Play against the computer or control both hands. Hover over a square to preview the move, right-click a card to inspect it, and use **Undo** to try another placement. **Replay deal** restores the same hands, rules and first player.

The proposed rules are:

- **Ascension:** each placed card adds 1 to its affinity’s modifier, up to +3.
- **Descension:** the same count gives a penalty, down to −3. Neutral cards are unaffected.
- **Dominion:** a card loses 1 from all sides when at least two adjacent cards share a group it does not belong to. Only touching sides count. In a Nord–Orc–Nord line, only the Orc gets −1. One Nord and one Khajiit do not weaken it.

Dominion works for every group from the start, regardless of who owns the cards. There is no first-group requirement, and the penalty never stacks beyond −1. A card with multiple groups is immune to Dominion from its own groups, but another group can still weaken it.

Affinity counts are shared by both players and affect cards in hand too. The newly placed card counts before captures. Modifiers can take values outside 1–10. Same and Plus use printed values; ordinary captures and Combo use effective values.

This is a browser playtest, separate from the Skyrim mod. It uses the current roster, artwork, card renderer and theme styles. It does not read or change saves, settings or collections from the game.

To repeat the full rule comparison, run `node tools/rule-lab/simulate.mjs` after starting the server once to prepare its card data. Results are saved under `build/rule-lab/simulations`.

For the Construct comparison, use a separate output folder:

```sh
node tools/rule-lab/simulate.mjs --construct-study --affinity-cap 2 --affinity-start 2 --out build/rule-lab/construct-study/cap2-second
```

`--affinity-start 2` gives no modifier for the first matching card, ±1 for the second, and ±2 for the third. Change the cap and start to compare alternatives. These options affect the simulations; the browser table keeps its original rules.
