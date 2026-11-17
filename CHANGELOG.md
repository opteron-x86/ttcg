# Changelog

## Unreleased

- Fixed losing Tessera progress when a save's Tessera data can't be read. The data is kept unchanged and a message is shown.
- Fixed the opponent freezing mid-game until the match closed as stalled.
- Fixed a finished game left interrupted blocking packs, shops, decks and other opponents. Stakes settle at once, and winnings that need a choice can be collected from the album.
- Fixed Esc or B closing Tessera while adjusting a slider, dragging a card or closing a dropdown in a dialog.
- Fixed the right stick not turning the inspected card.
- Fixed Q and E switching tabs while binding a hotkey.
- Fixed a card drag carrying over after the menu closed.
- Fixed X adding the focused card, not the one under the cursor.
- Fixed long card facts and player names overlapping their labels.
- Fixed a card chosen in the Rules demo replacing the card chosen in the match.
- Fixed foil cards sharing a finish with a dragged card on very large layouts.
- Fixed a tournament circuit stalling when the courier can't take its invitation.
- Fixed Reset Tessera being blocked by payments that can never be delivered.
- Fixed an opponent gaining a card when an exchange offer is cancelled after a restock.
- Fixed players and merchant chests from injected records not being recognised after a reload.
- Fixed a thread-safety problem when an NPC dies.
- Card packs are added to merchant chests with the game's own function. This is a likely fix for the crash during death and reload on 1.7.104; not yet confirmed.
- Checking a container for Tessera items no longer sets up its inventory.
- Settings > Gameplay says when merchants can't stock packs on a game version.
- Vanguard: the first card played has +1 on every side while the player who placed it holds it, until the fifth card is played (−1 under Reverse). The Rules tab has a page for it.
- The lobby shows who moves first before you agree to the stakes.
- Holds no longer have fixed starting rules. Each new game rolls its own rules for every hold; existing saves keep theirs.
- The All trade rule is rarer in regional drift; One is the commonest.
- A collector's gold reward for a card is never more than the card's shop price.
- Opponents imagine several possible hands when yours is hidden, drawn only from cards you could own. Master is stronger with hidden hands.
- Each of the six skill labels is its own strength. Experienced and Expert opponents are easier than before; Learners are slightly harder than Beginners.
- Master plays the end of an Open game exactly from the third card.
- Tournaments that end unattended no longer stall the game while their remaining games are played.
- `[Development] OpponentSkill` takes 0–5 for the six skill names.
- Capture preview: with a card chosen, the board marks the cards it would capture on the square you point at. Can be turned off in Settings > Gameplay.
- The foil-upgrade prize is chosen from a searchable grid instead of a dropdown.
- Custom control colours can be picked from swatches.
- The Journal shows each player's rules, trade rule and wager cap.
- The lobby says when rule challenges open again.
- A first run picks the skin for Oathvein, Norden, Edge, Untarnished, Dear Diary or Dear Diary Dark Mode when that mod is installed.
- Tessera's green-and-gold skin is back as a choice in Settings > Appearance. A saved `Theme=0` opens it again.
- Result and rule-request dialogs show their outline on skins that draw one.
- The shop's Buy, Sell and Buy back step with LB/RB and Q/E.
- Result, trade and rule-request dialogs show their own key prompts.
- Text uses five sizes throughout.
- Buttons in Settings, the album's filters and the group choice follow the skin.
- Fixed gold and gain/loss colours being unreadable on paper skins, clipped deck slot names, a tiny owned-count badge on small cards and the shop notice covering the prompts.
- Legion and Decimation rule text says one group is chosen per board.
- The Three Open demo shows the same cards as its lesson.
- Exchange API: `CancelOffer` on a completed offer answers `stale`; `CompleteOffer` with its confirmation open answers `busy`.
- `TTCG.ini.example` lists the current skins and settings.

## 0.33.0

- Redesigned interface. Skins follow Skyrim UI mods: Skyrim (the new default), Oathvein, Vel’dun, Norden, Edge, Untarnished, Nordic, Outlander, Dear Diary, Dear Diary Dark and Wood and Paper. The five older themes are retired; a saved older theme opens the Skyrim skin.
- New card faces: Medallions (default) and Printed, with a bold value typeface and a new foil.
- Rebuilt deck editor: your deck beside a large card picker, add with a double click, Enter or X, and see each rank's gain or loss before replacing a card.
- Clearer matches: hand cards at board size, last move and whose turn it is marked, capture effects, and a result screen that shows the cards won and lost.
- Packs reveal card by card. The card inspector turns the card in 3D without warping the art.
- Q and E, or the bumpers, switch album tabs. Keycap prompts can follow the skin or use your installed ImGui Icons.
- Card trades are shorter: pick and confirm, and the result follows.

## 0.32.6

- Added Oathvein, Vel’dun and Norden themes across the album, game board and dialogs.
- Added Jost and Lato font choices.
- Added GitHub builds with Main, source, symbols and API downloads.
