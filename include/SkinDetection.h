#pragma once
#include "InterfaceTheme.h"
#include <string>
#include <vector>

namespace ttcg {
// On a first run Tessera takes the skin of the interface mod that is installed.
// A skin's `skin.json` may list `detect` paths under Data that only its mod ships;
// `markers(id)` returns that list and `exists(path)` says whether a file is there.
// Mods built on another are asked first, so their base is not mistaken for them.
// Returns the theme's index, or -1 when nothing is recognised.
template <class Markers, class Exists> int detectInterfaceTheme(Markers markers, Exists exists) {
    for (std::string_view id : {"oathvein", "veldun", "norden", "edge", "untarnished", "nordic", "outlander",
                                "woodpaper", "deardiary", "diarypaper"})
        for (int index = 0; index < interfaceThemeCount; ++index)
            if (interfaceThemes[index].id == id && !interfaceThemes[index].retired)
                for (const std::string& path : markers(id))
                    if (!path.empty() && exists(path)) return index;
    return -1;
}
}
