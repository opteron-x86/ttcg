# Themes and skins

Tessera, Charcoal, Oxblood, Parchment and Umber are drawn by the native renderer from a palette and a generated background.

Oathvein, Vel’dun, Norden, Untarnished, Edge and Dear Diary are skins: a `skin.json` and textures under `TesseraUI/skins/<id>`, loaded by `native/src/Skin.cpp`. A skin supplies the palette, typeface and control colours, and textures for the window panel, title band, tabs and selection bands. The panel is a nine-slice; bands keep their ends and stretch the middle. A skinned theme titles the window with a band or hairline in place of the title bar, shows key prompts along the bottom, sets card names and values in the skin’s typeface and colours each card’s nameplate by its owner. A missing texture falls back to a flat fill or a hairline. [TesseraUI/skins/README.md](../../TesseraUI/skins/README.md) describes the format.

The skin textures are artwork from Nithog’s Oathvein UI, Vel’dun UI and Norden UI, EugeneUI’s Edge UI and uranreactor’s Dear Diary Dark Mode, used with permission; Untarnished, after Vorganger’s Untarnished UI, is a palette alone. A skin also names its default game board. `tools/build-skin.py` renders them from installed copies of those mods. Tessera does not need the original UI mods installed.

Key prompts use the keycap images of the player’s ImGui Icons installation, `Data/Interface/ImGuiIcons/Icons`, and switch to controller glyphs when a pad is used. Without that folder Tessera draws plain caps.

Choosing an interface font in Appearance overrides the skin’s typeface; card names and values follow the skin until their own font settings are changed.

The theme list in `include/InterfaceTheme.h` also supplies the saved setting bounds and default board materials. New entries go at the end so existing saved choices do not change.
