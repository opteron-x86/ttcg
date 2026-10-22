# Card library finishes

`tools/build-card-library.py` embeds these files in `docs/card-library.html` so the page works offline. `tools/card-library.html` owns the catalog and details dialog.

- `finishes.css`: existing experimental materials and shared card presentation.
- `TesseraUI/foil-material.js`: the shared renderer for the game’s Prismatic finish and the Satin/Engraved previews. One shared WebGL context draws procedural reflections onto each card’s canvas. Artwork stays in ordinary images. Grain and engraving use fixed card coordinates; lighting is the only moving input.
- `motion.js`: interpolated pointer response and the runtime foil’s lighting variables.
- `review.html`, `review.css`, `review.js`: synchronized comparison, sample artwork, keyboard controls and automatic lighting.


Automatic lighting stops when the comparison is closed, offscreen, covered by card details, or in a background tab. Reduced-motion preferences disable automatic and pointer movement. The new materials have CSS fallbacks for unavailable or lost WebGL contexts.

Rebuild and check:

```sh
python3 tools/build-card-library.py
python3 tools/build-card-library.py --check
node tests/library_finishes_ui_test.cjs
node tests/library_foil_review_ui_test.cjs
```

Open `docs/card-library.html#foil-review`, or the same path on the LAN workshop server. Use “Compare foil finishes” in any card’s details to add it to the comparison.

The library uses the Prismatic foil material from `TesseraUI/foil.css`.
