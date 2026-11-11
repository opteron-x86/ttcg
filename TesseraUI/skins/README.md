# Skins

A skin is a folder holding `skin.json` and the textures it names. The folder name matches a theme id in `include/InterfaceTheme.h`.

```json
{
  "schema": 1,
  "id": "oathvein",
  "name": "Oathvein",
  "font": "jost",
  "palette": {"background": "#161616", "text": "#e9e6dd", "line": "#e9e6dd2e", "selection": "#8f3229"},
  "control": {"player": "#e3dfd4", "opponent": "#c8483a"},
  "art": {
    "panel": {"file": "panel.png", "margin": 170, "inset": 22, "tile": true},
    "selector": {"file": "selector.png", "cap": 0.08}
  }
}
```

Colours are `#rrggbb` or `#rrggbbaa`. `font` is `jost`, `lato`, `serif` or `sans`. `control` colours apply while the player keeps the default control palette.

| Art | Drawn as | Used for |
|---|---|---|
| `panel` | Nine-slice. `margin` is the corner size and `inset` the distance from the texture edge to its frame line, both in texture pixels. `tile` repeats the edges mirrored; otherwise they stretch. | Window and dialogs |
| `window`, `detail` | Nine-slice, with the same fields as `panel` | Optional main-window backdrop and album detail panel; dialogs keep `panel` |
| `title` | Band with fixed ends (`cap`, a fraction of the width) and a stretched middle | Top of the window |
| `selector` | Band | Hovered and primary buttons, the player to move |
| `tab` | Band | Current tab |
| `band`, `plate` | Bands | Card title and values in the album |
| `sigil` | Image at `opacity` | Watermark in the title |

Every piece is optional; a missing one falls back to a flat fill or a hairline. Textures are authored at three times Skyrim's 720p stage, so one texture pixel is one screen pixel at 2160p. `passes` draws a translucent band more than once to deepen it.

`board` names the skin's default game board: `plain`, `slate`, `felt`, `parchment`, `leather`, `walnut` or `slab`.

The textures here are artwork from Oathvein UI, Vel'dun UI and Norden UI by Nithog, Edge UI by EugeneUI and Dear Diary Dark Mode by uranreactor, used with permission and not covered by Tessera's GPL-3.0 licence. Untarnished follows Vorganger's Untarnished UI with a palette alone. `tools/build-skin.py` regenerates them from installed copies of those mods using `tools/skin/recipes.json`.
