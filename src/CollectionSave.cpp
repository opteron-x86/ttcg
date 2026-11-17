// Loading, saving and repairing the collection in the co-save.
#include "PluginState.h"

namespace campaign {
void grantDevelopmentCards() {
    development.grantFoils(saved);
    if (development.grantCards(saved)) {
        ensureAlbum();
        SKSE::log::info("Development: granted all {} playable cards", ttcg::cards.size());
    }
}
void recover() {
    if (!available()) return;
    removeLegacyAlbumStock();
    if (!saved.economyRandom)
        saved.economyRandom =
            static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
    claimAlbum();
    // Unlike payPlayerGold, this also collects gold the player still owes (a negative credit).
    if (saved.goldCredit) {
        Bank bank(0);
        saved.goldCredit -= bank.change(0, 0, saved.goldCredit);
    }
    if (const auto dropped = ttcg::dropUndeliverablePayments(
            saved, [](std::uint32_t id) { return resolveID<RE::TESForm>(id) != nullptr; }))
        SKSE::log::warn(
            "Dropped {} gold owed to {} former player(s) and {} missing merchant chest(s); they can no longer be paid",
            dropped.gold, dropped.residents, dropped.merchants);
    syncProgression();
    if (recoverSavedMatch()) {
        // A committed choice may still have outstanding native inventory writes.
        // Retry those credits without replacing a held, unchosen card reward.
        if (saved.contract.paying) finish(saved.contract.outcome);
        return;
    }
    if (!saved.contract.pending()) {
        grantDevelopmentCards();
        ensureAlbum();
        ttcg::retainOwnedDecks(saved);
        updateTournaments();
        return;
    }
    auto& c = saved.contract;
    int winner = c.outcome >= -1 ? c.outcome : -1;
    finish(winner);
    grantDevelopmentCards();
    claimAlbum();
    ensureAlbum();
    updateTournaments();
}
void save(SKSE::SerializationInterface* serial) {
    if (!saveValid) {
        // Tessera could not read this save's records. Write them back as they were.
        if (!ttcg::writeSaveRecords(*serial, preservedRecords))
            SKSE::log::error("Could not write back the unreadable Tessera records");
        return;
    }
    auto data = ttcg::encodeCollection(saved);
    if (!serial->WriteRecord(ttcg::collectionRecord, ttcg::collectionFormat, data.data(),
                             static_cast<std::uint32_t>(data.size() * 4)))
        SKSE::log::error("Could not save collection state");
    if (savedMatch.present) {
        const auto table = ttcg::encodeMatch(savedMatch);
        if (!serial->WriteRecord(ttcg::matchRecord, ttcg::savedMatchFormat, table.data(),
                                 static_cast<std::uint32_t>(table.size() * 4)))
            SKSE::log::error("Could not save interrupted match");
    }
}
void revert(SKSE::SerializationInterface*) {
    clearNpcCache();
    saved = {};
    savedMatch = {};
    tournamentID = 0;
    cultureHeldHold = 0;
    tournamentRegistration = 0;
    tournamentLetterReady = 0;
    tournamentLetterInFlight = 0;
    tournamentCourierFailures.clear();
    cultureGame = false;
    cultureNotice.clear();
    saveValid = true;
    preservedRecords.clear();
    notice.clear();
    receipt.clear();
    revealed.clear();
    revealKind.clear();
    revealID = 0;
    ttcg::resetRuntimeOpponents();
    practice = false;
    competitiveMatch = false;
    requestAnswered = false;
    ruleRequest = 0;
    sessionRules = 0;
}
void load(SKSE::SerializationInterface* serial) {
    revert(serial);
    auto records = ttcg::readSaveRecords(*serial);
    for (const auto& record : records.records)
        if (record.type == ttcg::matchRecord &&
            (record.version != ttcg::savedMatchFormat || record.bytes.size() != 65 * 4))
            SKSE::log::warn("Unsupported saved match; returning its stakes");
    auto loaded = ttcg::decodeSaveRecords(std::move(records), [&](ttcg::CollectionSave& collection) {
        return ttcg::remapWorldState(collection, [&](std::uint32_t old) {
            if (authoredKey(old) || ttcg::opponentIndex(old) >= 0) return old;
            RE::FormID id = 0;
            if (serial->ResolveFormID(old, id)) {
                auto form = RE::TESForm::LookupByID(id);
                return form ? persistentID(form) : id;
            }
            return ttcg::worldFormID({"missing-legacy-" + std::to_string(old) + ".esp", 1});
        });
    });
    saveValid = loaded.valid;
    saved = std::move(loaded.collection);
    preservedRecords = std::move(loaded.preserved);
    auto matchData = std::move(loaded.match);
    if (!saveValid)
        SKSE::log::error("Tessera's data in this save is unreadable; keeping {} record(s) unchanged and staying off",
                         preservedRecords.size());
    if (!matchData.empty()) {
        for (unsigned i = 27; i < 37; ++i) matchData[i] = ttcg::remapSavedCard(saved, matchData[i]);
        if (!ttcg::decodeMatch(matchData, savedMatch)) {
            savedMatch = {};
            SKSE::log::warn("Interrupted match has unavailable cards; recovering its stakes");
        }
    }
    for (auto* id : {&savedMatch.opponent, &savedMatch.ledger, &savedMatch.waitPackage})
        if (*id) {
            if (auto it = saved.loadedWorldRemap.find(*id); it != saved.loadedWorldRemap.end())
                *id = it->second;
            else if (!authoredKey(*id)) {
                RE::FormID resolved = 0;
                if (serial->ResolveFormID(*id, resolved)) {
                    auto form = RE::TESForm::LookupByID(resolved);
                    *id = form ? persistentID(form) : resolved;
                } else
                    *id = 0;
            }
        }
    restoreNpcProfiles();
    if (savedMatch.present && !resolveID<RE::Actor>(savedMatch.opponent)) {
        savedMatch = {};
        SKSE::log::warn("Interrupted opponent is unavailable; returning stakes");
    }
    if (savedMatch.present && (!savedMatch.opponent || !ttcg::matchContractValid(savedMatch, saved.contract))) {
        savedMatch = {};
        SKSE::log::warn("Saved match does not match its stakes; recovering collection");
    }
    for (auto& e : saved.tournaments.events)
        if (!savedMatch.present || savedMatch.tournament != e.id || savedMatch.table.finished()) e.matchNode = 0;
    // An interrupted final keeps its prize screen. Acquisition stays locked until
    // it is resolved; deliberately leaving still forfeits unused foil upgrades.
    if (!savedMatch.present || !savedMatch.tournament) ttcg::forfeitTournamentFoils(saved);
    if (!savedMatch.present || !savedMatch.culture) ttcg::abandonRuleCulture(saved, 0);
    syncProgression();
    SKSE::log::info("Collection loaded: valid {}, starter {}, pending {}", saveValid, saved.starter,
                    saved.contract.pending());
}
}
