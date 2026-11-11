# Board source art

Painted field textures for the game boards, one per material. `tools/build-board-textures.py` turns each into the shipped `surface.webp` and the rail of `frame.png`; a material without a file here falls back to the procedural texture in that script (`--procedural` forces it for all).

The images were generated on 10 November 2026 with the image generation tool of OpenAI's Codex CLI 0.166.0. Each prompt was the material description below followed by the shared suffix.

Shared suffix: "flat, orthographic, straight top-down material texture, even soft lighting with no cast shadows, no objects, no borders, no text, no vignette, edge-to-edge material only, portrait 3:4, painterly realism in the style of a dark fantasy RPG interface"

| File | Material description |
|---|---|
| `slate.webp` | a dark blue-grey slate stone slab for a game table, honed surface with subtle natural cleft, faint mineral veins and light wear |
| `felt.webp` | deep forest-green wool gaming felt, slightly worn baize with fine fibre nap and a few faint lighter scuffs |
| `parchment.webp` | aged vellum parchment, warm tan, with soft foxing stains, faint fibres and gentle creases |
| `leather.webp` | old dark brown tooled saddle leather, fine pebbled grain, subtle creases and oiled sheen |
| `walnut.webp` | an aged dark walnut game-table surface, hand-oiled, fine grain running vertically, subtle wear and a few faint scratches |
| `slab.webp` | a near-black basalt slab, charcoal grey, honed, with very fine pale scratches and faint chisel marks |
