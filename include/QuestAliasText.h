#pragma once
#include <algorithm>
#include <cstdint>
#include <span>

namespace ttcg {
struct AliasTextName {
    std::uint32_t aliasID;
    std::uint32_t fullNameFormID;
};

// Refresh alias text for the current quest instance.
template<class Quest, class Create>
bool refreshQuestAliasText(Quest& quest, std::span<const AliasTextName> names, Create create) {
    if (names.empty()) return false;
    auto found = std::find_if(quest.instanceData.begin(), quest.instanceData.end(),
        [&](auto* instance) { return instance && instance->id == quest.currentInstanceID; });
    auto* instance = found == quest.instanceData.end() ? create(quest.currentInstanceID) : *found;
    if (!instance) return false;
    if (found == quest.instanceData.end()) quest.instanceData.push_back(instance);
    for (const auto& name : names) {
        auto entry = std::find_if(instance->stringData.begin(), instance->stringData.end(),
            [&](const auto& value) { return value.aliasID == name.aliasID; });
        if (entry == instance->stringData.end())
            instance->stringData.push_back({name.aliasID, name.fullNameFormID});
        else
            entry->fullNameFormID = name.fullNameFormID;
    }
    return true;
}
}
