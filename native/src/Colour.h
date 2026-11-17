#pragma once
#include <imgui.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace preview {
constexpr ImU32 rgb(unsigned n, unsigned alpha = 255) { return IM_COL32(n >> 16, (n >> 8) & 255, n & 255, alpha); }
// The colour at an opacity of its own, replacing the one it had.
inline ImU32 alpha(ImU32 colour, float amount) {
    return (colour & 0xffffff) | (unsigned(std::clamp(amount, 0.f, 1.f) * 255) << 24);
}
// The colour with the opacity it had scaled down.
inline ImU32 fade(ImU32 colour, float opacity) {
    return (colour & 0xffffff) | (unsigned(((colour >> 24) & 255) * std::clamp(opacity, 0.f, 1.f)) << 24);
}
// A rarity's pigment as 0xrrggbb, bright enough for a dark ground.
constexpr unsigned rarityPigment(std::string_view rarity) {
    return rarity == "Rare" ? 0x56dfff : rarity == "Epic" ? 0xd684fa : rarity == "Legendary" ? 0xe6bc65 : 0xb7b89e;
}
inline constexpr int highestTier = 10;
// A card tier as it is printed, I to X; empty outside that range.
constexpr const char* tierNumeral(int tier) {
    constexpr const char* numerals[]{"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    return numerals[tier >= 1 && tier <= highestTier ? tier : 0];
}
// A tier filter's choices: the entry for every tier, then I to X.
inline std::vector<std::string> tierChoices(const char* every) {
    std::vector<std::string> choices{every};
    for (int tier = 1; tier <= highestTier; ++tier) choices.emplace_back(tierNumeral(tier));
    return choices;
}
}
