#pragma once
#include <array>
#include <string_view>

namespace ttcg {
struct InterfaceTheme {
    std::string_view id, name;
    int boardSurface;
};
// The index is saved in TTCG.ini. Append themes; never reorder existing entries.
inline constexpr std::array interfaceThemes{
    InterfaceTheme{"tessera", "Tessera", 3},
    InterfaceTheme{"charcoal", "Charcoal", 2},
    InterfaceTheme{"oxblood", "Oxblood", 5},
    InterfaceTheme{"parchment", "Parchment", 4},
    InterfaceTheme{"umber", "Umber", 6},
    InterfaceTheme{"oathvein", "Oathvein", 5},
    InterfaceTheme{"veldun", "Vel'dun", 5},
    InterfaceTheme{"norden", "Norden", 2}
};
inline constexpr int interfaceThemeCount = int(interfaceThemes.size());
inline constexpr int interfaceThemeIndex(int value) {
    return value >= 0 && value < interfaceThemeCount ? value : 0;
}
}
