# Playing Tessera

Tessera is a card game played on a 3×3 board. Build a five-card deck, choose where to place each card, and finish with more cards under your control than your opponent.

## Getting started

Buy a Tessera Album from a general-goods merchant for 100 gold. It includes eight starter cards and a prepared deck. Open the album through Inventory → Books or press F7.

Challenge a nearby player through dialogue or F8. The match setup shows the rules, your deck and any wager. **Edit deck** opens your saved decks and returns you to setup when you finish.

Games with children are casual. Adult players have their own skill, cards and terms. The [opponent guide](opponents-by-hold.md) includes locations and progression spoilers.

## Cards and decks

The album shows cards you own or have discovered. Search by name and filter by tier, group, affinity or rarity. Click a card to inspect it; click the inspection to enlarge the artwork.

A deck contains five cards. You can use up to two copies of an ordinary card and one of a Unique card. Normal and foil copies share this limit. Foil is an appearance finish with the same strength as the normal card.

Double-click a card or drag it into a deck slot. Drag deck cards to reorder them. You can keep up to 20 named decks; changes save automatically. Several decks can use the same owned copies.

Losing or selling a required copy clears the affected deck slots. Complete the deck before starting another match.

## Playing a match

Select a card in your hand, then an empty square, or drag the card onto it. Its four values face up, right, down and left. A stronger touching edge captures the adjacent opponent card. Captured cards count toward your score.

Right-click a card or use **Inspect** to read it. The **Rules** page explains every special rule with a demonstration. Rule names above the board also open their explanation.

The match's trade rule determines which cards change hands afterward. Review the terms before accepting a game. Gold wagers are shown separately.

## Collection and progression

Win cards in matches, buy packs, trade with merchants or enter tournaments. Opponents keep cards they win and can use them in later matches. Your Journal records known players, regional rules and your results.

Local victories build reputation. Stronger opponents and tournament success open further opportunities. Regional rules can spread through play; Famous players can challenge an Expert or Master to abolish a rule.

[Card trading](card-trading.md) covers buying, selling and buyback. [Tournaments](tournaments.md) covers entry, deadlines and prizes. [Albums and packs](pack-distribution.md) explains merchant availability.

## Settings and controls

Settings includes appearance, shortcuts and gameplay options. Choose a theme, fonts, card layout, board texture and card back. Changes apply immediately.

The album, shop, match setup and match have separate pause settings. Leaving a game unpaused lets Skyrim continue while the interface is open. An interruption can close the interface; return to the opponent or tournament host to resume a saved match.

Collections and progress belong to the character's save. Interface settings are stored in `SKSE/Plugins/TTCG.ini`.

If removed expansions leave unavailable cards in the album, use **Settings > Gameplay > Remove unavailable cards**, or reinstall the expansions to restore them. Cleanup removes only unavailable copies, their discovery marks and deck slots after confirmation. With `[Development] Enabled=1`, **Settings > Developer > Clear collection** empties the collection while keeping progress. **Reset Tessera** also erases Tessera progress and restores the album's eight starter cards and starter deck. See [collection recovery](card-expansions.md#check-and-share-your-set) for their effects and save precautions.

To change the illustrations, see [Replacing card art](card-art-replacers.md). Addon authors can create [card expansions](card-expansions.md) or use the [collection API](collection-api.md).
