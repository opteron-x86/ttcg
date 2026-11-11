#pragma once
#include <array>
#include <string_view>

namespace ttcg {
struct InterfaceTheme {
    std::string_view id, name;
    int boardSurface;
    bool retired = false;
};
// The index is saved in TTCG.ini. Append themes; never reorder or remove entries. A retired theme
// keeps its place so the indices after it stay put, and a saved retired index opens the default.
inline constexpr std::array interfaceThemes{InterfaceTheme{"tessera", "Tessera", 3, true},
                                            InterfaceTheme{"charcoal", "Charcoal", 2, true},
                                            InterfaceTheme{"oxblood", "Oxblood", 5, true},
                                            InterfaceTheme{"parchment", "Parchment", 4, true},
                                            InterfaceTheme{"umber", "Umber", 6, true},
                                            InterfaceTheme{"oathvein", "Oathvein", 5},
                                            InterfaceTheme{"veldun", "Vel'dun", 5},
                                            InterfaceTheme{"norden", "Norden", 2},
                                            InterfaceTheme{"untarnished", "Untarnished", 7},
                                            InterfaceTheme{"edge", "Edge", 5},
                                            InterfaceTheme{"deardiary", "Dear Diary Dark", 2},
                                            InterfaceTheme{"nordic", "Nordic", 7},
                                            InterfaceTheme{"outlander", "Outlander", 2},
                                            InterfaceTheme{"skyrim", "Skyrim", 7},
                                            InterfaceTheme{"diarypaper", "Dear Diary", 5},
                                            InterfaceTheme{"woodpaper", "Wood and Paper", 6}};
inline constexpr int interfaceThemeCount = int(interfaceThemes.size());
inline constexpr int defaultInterfaceTheme = 13;
inline constexpr bool interfaceThemeAvailable(int value) {
    return value >= 0 && value < interfaceThemeCount && !interfaceThemes[value].retired;
}
inline constexpr int interfaceThemeIndex(int value) {
    return interfaceThemeAvailable(value) ? value : defaultInterfaceTheme;
}
static_assert(interfaceThemes[defaultInterfaceTheme].id == "skyrim");
}
