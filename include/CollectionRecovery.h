#pragma once
#include "Tournaments.h"
#include "SavedMatch.h"
namespace ttcg {
// A finished table has nothing left to play. It only needs the player again when the
// winner still has cards to choose, or a tournament prize screen still holds foil
// upgrades. Any other finished table can be settled without them.
inline bool finishedMatchSettles(const SavedMatch& table, const CollectionSave& s) {
    if (!table.present || !table.table.finished()) return false;
    return table.tournament ? !pendingTournamentFoils(s) : !s.contract.choicePending();
}
inline std::string_view collectionRecoveryBlocked(const CollectionSave& s, bool interrupted = false) {
    if (interrupted) return "Finish or forfeit your interrupted game first.";
    if (s.contract.pending() || s.goldCredit) return "Finish the pending card trade or payment first.";
    if (pendingTournamentFoils(s)) return "Use or leave your tournament rewards first.";
    if (tournamentEntryActive(s)) return "Finish or withdraw from your tournament first.";
    return {};
}
inline std::string_view tesseraResetBlocked(const CollectionSave& s, bool interrupted = false) {
    if (const auto blocked = collectionRecoveryBlocked(s, interrupted); !blocked.empty()) return blocked;
    for (const auto& [id, merchant] : s.merchants)
        if (merchant.goldCredit) return "Finish pending merchant payments first.";
    if (s.tournaments.packCredit || s.tournaments.standardPackCredit) return "Collect pending tournament packs first.";
    for (const auto& [id, gold] : s.tournaments.npcGold)
        if (gold) return "Finish pending tournament payments first.";
    return {};
}
struct DroppedPayments {
    unsigned residents = 0, merchants = 0, gold = 0;
    explicit operator bool() const { return residents || merchants; }
};
// Gold owed to the world that can never be delivered: a tournament purse for someone
// who no longer plays Tessera, or a payment to a merchant chest whose plugin is gone.
// Left in place, either would block Reset Tessera for good. `exists(id)` says
// whether the saved form can still be found.
template <class Exists> DroppedPayments dropUndeliverablePayments(CollectionSave& s, Exists exists) {
    DroppedPayments dropped;
    for (auto& [base, gold] : s.tournaments.npcGold)
        if (gold && (opponentIndex(base) < 0 || !exists(base))) {
            ++dropped.residents;
            dropped.gold += gold;
            gold = 0;
        }
    std::erase_if(s.tournaments.npcGold, [](const auto& entry) { return !entry.second; });
    for (auto& [chest, merchant] : s.merchants)
        if (merchant.goldCredit > 0 && !exists(chest)) {
            ++dropped.merchants;
            dropped.gold += unsigned(merchant.goldCredit);
            merchant.goldCredit = 0;
        }
    return dropped;
}
inline CollectionSave freshTesseraSave(const CollectionSave& previous) {
    CollectionSave fresh;
    // Old courier acknowledgements must never match a newly scheduled tournament.
    fresh.tournaments.sequence = previous.tournaments.sequence;
    // Quests can retain offer serials in their own saves. Keep cancelled tombstones
    // so a delayed completion cannot match a new offer after resetting Tessera.
    fresh.exchanges = previous.exchanges;
    for (auto& [owner, offer] : fresh.exchanges) {
        offer.status = ExchangeStatus::Cancelled;
        for (auto id : {offer.requested, offer.reward})
            if (auto found = previous.missingCards.find(baseCardID(id)); found != previous.missingCards.end())
                fresh.missingCards.emplace(*found);
    }
    grantStarter(fresh);
    return fresh;
}
inline bool unavailableCollectionCard(const CollectionSave& s, CardID id) {
    return id && cardIndex(id) < 0 && s.missingCards.contains(baseCardID(id));
}
inline std::set<std::string> missingCollectionSets(const CollectionSave& s) {
    std::set<std::string> result;
    for (const auto& [id, n] : s.player)
        if (n > 0 && unavailableCollectionCard(s, id)) {
            const auto& key = s.missingCards.at(baseCardID(id));
            result.insert(key.substr(0, key.find(':')));
        }
    return result;
}
inline bool removeUnavailableCards(CollectionSave& s) {
    if (!collectionRecoveryBlocked(s).empty()) return false;
    for (auto* stock : {&s.player, &s.discovered})
        std::erase_if(*stock, [&](const auto& entry) { return unavailableCollectionCard(s, entry.first); });
    retainOwnedDecks(s);
    // Other ledgers still need the identity dictionary to survive save/load.
    // Serialization drops dictionary entries once nothing references them.
    return true;
}
}
