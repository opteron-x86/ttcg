#pragma once
#include "ShopPresentation.h"
#include <sstream>
namespace campaign {
struct ShopContext {
    unsigned key = 0;
    bool specialist = false;
    ttcg::ShopVisit visit;
    std::string notice;
};
inline ShopContext shop;
inline RE::TESObjectREFR* merchantContainer(RE::Actor* actor) {
    if (!actor || !vendors::albumEligible(actor)) return nullptr;
    for (auto faction : vendors::factions)
        if (actor->IsInFaction(faction) && faction->vendorData.merchantContainer)
            return faction->vendorData.merchantContainer;
    // The vanilla general-goods role also covers service factions whose stock
    // categories have been changed by another mod.
    for (auto faction : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESFaction>())
        if (faction && faction->IsVendor() && actor->IsInFaction(faction) && faction->vendorData.merchantContainer)
            return faction->vendorData.merchantContainer;
    return nullptr;
}
struct ShopBank {
    std::array<RE::TESObjectREFR*, 2> actors;
    explicit ShopBank(unsigned key) : actors{RE::PlayerCharacter::GetSingleton(), resolveID<RE::TESObjectREFR>(key)} {}
    // The player is trading with this merchant: its chest is set up, as barter would.
    int count(int side) { return physicalCount(actors[side], goldForm, true); }
    int change(int side, int amount) { return physicalChange(actors[side], goldForm, amount, true); }
};
bool openShop(RE::Actor* actor);
bool shopAvailable(RE::Actor* actor);
std::string shopJson(RE::Actor* actor);
void closeShop();
bool shopCommand(RE::Actor* actor, std::istringstream& input);
}
