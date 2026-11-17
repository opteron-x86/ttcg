#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace vendors {
// Merchant chests get their card packs from one entry added to the chest's base
// container. The entry is added by the game's own routine, found through the
// Address Library: 14377 in Skyrim SE, 14517 in AE, and a fixed offset in VR.
// CommonLib's TESContainer::AddObjectToContainer rebuilds the entry list itself, and
// a leveled list added that way crashed while merchant stock was being rolled.
// SkyPatcher saw the same crash and moved to this routine (its commit 0612fff).
inline constexpr std::uint64_t addObjectSE = 14377, addObjectAE = 14517;
inline constexpr std::uintptr_t addObjectVR = 0x19F320;
// How that routine begins in every runtime it has been checked on. Where the bytes
// differ the game is one Tessera hasn't seen: packs stay off there, not guessed at.
inline constexpr std::array<std::uint8_t, 16> addObjectPrologue{0x48, 0x8B, 0xC4, 0x57, 0x41, 0x54, 0x41, 0x55,
                                                                0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x40};
inline bool addObjectRecognised(const std::uint8_t* code) {
    return code && std::memcmp(code, addObjectPrologue.data(), addObjectPrologue.size()) == 0;
}
enum class PackStock { On, SwitchedOff, UnknownGame };
inline constexpr PackStock packStockFor(bool enabled, bool routineFound) {
    return !enabled ? PackStock::SwitchedOff : routineFound ? PackStock::On : PackStock::UnknownGame;
}
// What the player is told in Settings when merchants have no packs to sell.
inline constexpr std::string_view packNotice(PackStock stock) {
    switch (stock) {
        case PackStock::SwitchedOff: return "Merchants don't stock card packs: VendorPackInjection is 0 in TTCG.ini.";
        case PackStock::UnknownGame:
            return "Merchants don't stock card packs on this version of Skyrim. Tessera needs an update for it.";
        default: return {};
    }
}
}
