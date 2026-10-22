# Replacing card art

Create a separate mod with these paths, relative to Skyrim’s Data directory:

```text
TesseraUI/art/cards/Enraged Mudcrab.webp
TesseraUI/art/thumbs/Enraged Mudcrab.webp
TesseraUI/art/backs/custom/My card back.png
```

Card faces and thumbnails use the card’s displayed name. The [filename index](../data/card-art-filenames.json) lists every name. Normal and foil copies share an image; Tessera adds the foil effect.

Use PNG, JPG, JPEG or WebP. Put a full image in `cards/` and a smaller copy in `thumbs/`; a width of 320 pixels works well for thumbnails. Preserve the image’s aspect ratio. Tessera crops faces to the portrait area and draws the names, values and frame.

Install the replacer after Card Art in your mod manager. For identical paths, the mod manager’s priority decides which image is used. If a name has several extensions, Tessera tries PNG, JPG, JPEG, then WebP. Keep one format for each replacement.

## Card backs

Choose a back in Settings → Appearance. Main includes Mosaic, Dragon and Celestial.

To add a back, place an image in `TesseraUI/art/backs/custom/` and select **Refresh** beside Card back. The filename becomes its label. Portrait images around 3:4 work well, such as 768 × 1024. Tessera fits the entire back inside the card.

To replace a supplied back, use `backs/mosaic`, `backs/dragon` or `backs/celestial` with a supported image extension. Reopen the menu or restart Skyrim after replacing files to refresh the images.

Card faces and thumbnails are a separate [Nexus download](https://www.nexusmods.com/skyrimspecialedition/mods/193442?tab=files). Replacers, custom backs and [expansions](card-expansions.md) can each be installed as their own mod.
