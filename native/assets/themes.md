# Quickslot-inspired themes

Oathvein, Vel’dun and Norden are Tessera adaptations of the matching widget designs in Quickslot. The native renderer draws their frames, tabs and controls; their backgrounds are generated once and reused. Tessera does not need Quickslot or the original UI mods installed.

Oathvein keeps clean corners, as requested for Tessera. Vel’dun keeps the widget’s broad top and right edges with an open lower-left corner. Norden keeps its clipped corners and fine silver lines. No converted SWF frames or exported game fonts are included.

The theme defaults use Jost for Oathvein and Norden, and Lato for Vel’dun. Both fonts are bundled with their Open Font License notices. Choosing an interface font in Appearance overrides the theme default; card names and values retain their own font settings.

The theme list in `include/InterfaceTheme.h` also supplies the saved setting bounds and default board materials. New entries go at the end so existing saved choices do not change.
