# Tessera card art

- **cards/** — full images, named after the card in game.
- **thumbs/** — small versions with the same card name.
- **backs/** — the Mosaic, Dragon and Celestial card backs.

Example: `cards/Enraged Mudcrab.jpg` and `thumbs/Enraged Mudcrab.jpg`.

Supported formats: **.png, .jpg, .jpeg, .webp**. Export a real image in that format; merely renaming its extension does not convert it. Preserve the card name, including spaces and punctuation. Normal and foil cards share their image.

Put replacements in a separate mod with this same folder structure, loaded after Tessera. Replace both the full image and thumbnail so all views match, then restart Skyrim.

When multiple formats exist, the first available wins:

- Faces, thumbnails and backs: **PNG → JPG → JPEG → WebP**.

All supplied faces, thumbnails and backs use WebP. This priority lets other formats override them. Replace an older PNG Card Art installation when updating, rather than merging it; otherwise the old files take priority. Avoid installing overlapping replacers with different extensions unless you hide the unwanted variants.

See the [complete guide](../../../docs/card-art-replacers.md) and [released card filename index](../../../data/card-art-filenames.json). No ESP editing or recompilation is needed.
