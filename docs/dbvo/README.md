# Tessera player voice topics

Import [Tessera TCG - Topics.txt](<Tessera TCG - Topics.txt>) into your Dragonborn Voice Over voice-pack maker. It contains all five unique player dialogue prompts from the current `Tessera TCG.esp`: challenges, album purchase, tournaments, registration, and card trading.

The file uses UTF-8 without a BOM, one prompt per line, with no headings or blank lines. Prompts retain their exact in-game spelling and punctuation. The album's `(100 gold)` suffix is display information; the [original DBVO Voice Pack Maker](https://github.com/MathiewMay/dbvo-vp-maker/blob/main/src/pages/ElevenLabsBatchTool.svelte) strips it before speech generation. If your tool separates matching text from spoken text, keep the full prompt for matching and omit the price from the spoken text.

Generate and package the audio for your chosen voice and DBVO version. This is generation input, not an installed voicepack. NPC responses and Tessera window controls are excluded.

Extracted from the plugin's `DIAL/FULL` fields on 2026-10-08. No `INFO/RNAM` prompt overrides were present. Refresh the file when player dialogue changes.
