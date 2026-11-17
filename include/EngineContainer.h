#pragma once
#include "PackStock.h"

namespace vendors {
using EngineAddObject = RE::ContainerObject* (*)(RE::TESContainer*, RE::TESBoundObject*, std::int32_t, void*);
// The game's routine for adding an entry to a base container, or null on a game
// version where it can't be found or doesn't begin as expected.
inline EngineAddObject engineAddObject() {
    const auto base = REL::Module::get().base();
    std::uintptr_t address = 0;
    if (REL::Module::IsVR())
        address = base + addObjectVR;
    else {
        // A lookup by ID stops the game when the ID is missing, so read the table.
        const auto wanted = REL::Module::IsAE() ? addObjectAE : addObjectSE;
        for (const auto& entry : REL::Offset2ID())
            if (entry.id == wanted) {
                address = base + static_cast<std::uintptr_t>(entry.offset);
                break;
            }
    }
    if (!address || !addObjectRecognised(reinterpret_cast<const std::uint8_t*>(address))) return nullptr;
    return reinterpret_cast<EngineAddObject>(address);
}
}
