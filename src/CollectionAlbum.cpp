// The album and card packs as items the player carries.
#include "PluginState.h"

namespace campaign {
void ensureAlbum() {
    auto player = RE::PlayerCharacter::GetSingleton();
    if ((saved.starter || saved.developmentCardsGranted || saved.developmentFoilsGranted) && albumForm && player &&
        physicalCount(player, albumForm) == 0)
        physicalChange(player, albumForm, 1);
}
bool claimAlbum() {
    if (!available() || !ttcg::claimAlbum(saved, physicalCount(RE::PlayerCharacter::GetSingleton(), albumForm) > 0))
        return false;
    syncProgression();
    SKSE::log::info("Album acquired: starter collection granted");
    return true;
}
bool buyAlbum(RE::Actor* actor) {
    if (!interfaceSettings.gamesEnabled || !available() || !vendors::albumEligible(actor) || hasAlbum()) return false;
    claimAlbum();
    if (hasAlbum()) return false;
    Bank bank(0);
    if (!ttcg::buyAlbum(saved, bank)) return false;
    ensureAlbum();
    syncProgression();
    SKSE::log::info("Album purchased from {} ({:08X}); starter collection granted", actor->GetName(),
                    actor->GetFormID());
    return true;
}
void removeLegacyAlbumStock() {
    if (!available()) return;
    const auto changed = vendors::removeLegacyAlbums(
        albumForm, [](RE::TESObjectREFR* ref, int amount) { return physicalChange(ref, albumForm, amount); });
    if (changed) SKSE::log::info("Removed legacy album stock from {} merchant inventories", changed);
}
bool openPack(std::uint32_t seed) {
    if (!available() || saved.contract.pending() || saved.goldCredit || ttcg::pendingTournamentFoils(saved))
        return false;
    auto cards = ttcg::packCards(seed);
    if (cards.size() != 5u) return false;
    const auto randomBefore = saved.economyRandom;
    for (auto& id : cards) id = ttcg::mintCard(saved, id);
    for (auto id : cards)
        if (ttcg::count(saved.player, id) >= 1000000) {
            saved.economyRandom = randomBefore;
            return false;
        }
    if (physicalChange(RE::PlayerCharacter::GetSingleton(), packForm, -1) != -1) {
        saved.economyRandom = randomBefore;
        return false;
    }
    Bank bank(0);
    for (auto id : cards) bank.change(0, id, 1);
    revealed = std::move(cards);
    revealKind = "standard";
    ++revealID;
    ttcg::saveActiveDeck(saved);
    return true;
}
bool openTournamentPack() {
    if (!available() || saved.goldCredit) return false;
    auto cards = ttcg::openTournamentPack(
        saved, [] { return physicalChange(RE::PlayerCharacter::GetSingleton(), tournamentPackForm, -1) == -1; });
    if (cards.empty()) return false;
    revealed = std::move(cards);
    revealKind = "tournament";
    ++revealID;
    return true;
}
}
