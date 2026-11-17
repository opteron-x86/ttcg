// Keeps the collection in step with the world: forms, opponent stock, tournaments and their couriers.
#include "PluginState.h"

namespace campaign {
void initForms(bool distributeVendorPacks) {
    initNpcForms();
    childChallengeInfo =
        RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESTopicInfo>(ttcg::encounter_childAccept, plugin);
    gamesEnabledGlobal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xFDA, plugin);
    syncGameAvailability();
    erandurGlobal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xD20, plugin);
    guildGlobal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xB40, plugin);
    brandSheiArrest = RE::TESForm::LookupByID<RE::TESQuest>(0x4EF25);
    collegeGlobal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xF20, plugin);
    valdrGlobal = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xC20, plugin);
    albumForm = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectBOOK>(0x870, plugin);
    for (std::size_t i = 0; i < ttcg::baseOpponents.size(); ++i) {
        opponentGlobals[i] =
            RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(ttcg::opponents[i].knownGlobal, plugin);
    }
    progression = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x850, plugin);
    goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    packForm = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0x840, plugin);
    tournamentPackForm = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0xFD6, plugin);
    // Only look the routine up when it will be used.
    const auto addObject = distributeVendorPacks ? vendors::engineAddObject() : nullptr;
    vendors::initialize(plugin,
                        addObject ? vendors::AddPackRoll([addObject](RE::TESObjectCONT* base, RE::TESLevItem* roll) {
                            return addObject(static_cast<RE::TESContainer*>(base), roll, 1, nullptr) != nullptr;
                        })
                                  : vendors::AddPackRoll{},
                        distributeVendorPacks);
    SKSE::log::info("Album forms available: {}", available());
}
void syncProgression(ttcg::NpcSyncTiming* timing) {
    ttcg::NpcSyncCapture capture(timing);
    {
        const ttcg::NpcSyncPhaseScope phase(ttcg::NpcSyncPhase::World);
        syncGameAvailability();
        saved.brandSheiJailed =
            brandSheiArrest && brandSheiArrest->IsRunning() && brandSheiArrest->GetCurrentStageID() == 20;
        if (guildGlobal) guildGlobal->value = 1.0f;
        if (collegeGlobal) collegeGlobal->value = 1.0f;
        auto stage = [](std::uint32_t id) {
            const auto q = resolveID<RE::TESQuest>(id);
            return q ? unsigned(q->GetCurrentStageID()) : 0u;
        };
        auto running = [](std::uint32_t id) {
            const auto q = resolveID<RE::TESQuest>(id);
            return q && q->IsRunning();
        };
        auto completed = [](std::uint32_t id) {
            const auto q = resolveID<RE::TESQuest>(id);
            return q && q->IsCompleted();
        };
        ttcg::refreshWorldAccess(saved, {stage(0x23B6C), stage(0x76B4A), running(0xD2CAF) && stage(0x1F7A3) < 200,
                                         running(0x21550), stage(0xDB017F90), stage(0xDB01CAF1), stage(0xDB01AAB0),
                                         completed(0x25F3E), completed(0xDB018B15)});
    }
    {
        const ttcg::NpcSyncPhaseScope phase(ttcg::NpcSyncPhase::Access);
        const auto builders = resolveID<RE::TESFaction>(0xDB01C4E5);
        saved.tournamentBlocked.clear();
        for (int i = 0; i < static_cast<int>(ttcg::opponents.size()); ++i) {
            const auto& p = ttcg::opponents[i];
            bool present = false, dead = false;
            for (auto id : {p.reference, p.alternateReference})
                if (id) {
                    auto actor = id == p.reference ? resolveOpponent(p) : resolveID<RE::Actor>(id);
                    {
                        const ttcg::NpcSyncWorkScope stateTiming(ttcg::NpcSyncWork::ActorState);
                        present |= actor && !actor->IsDead() && !actor->IsDisabled();
                        dead |= actor && actor->IsDead();
                    }
                    if (actor && p.dialogueFaction) {
                        auto faction = ttcg::measureNpcSync(ttcg::NpcSyncWork::Forms, [&] {
                            return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(p.dialogueFaction,
                                                                                                  plugin);
                        });
                        const ttcg::NpcSyncWorkScope factionTiming(ttcg::NpcSyncWork::Factions);
                        if (faction && !actor->IsInFaction(faction)) actor->AddToFaction(faction, 0);
                    }
                    if (actor && !npcIntegrationReady(actor)) saved.accessBlocked.insert(p.base);
                    if (actor && builders &&
                        ttcg::measureNpcSync(ttcg::NpcSyncWork::Factions, [&] { return actor->IsInFaction(builders); }))
                        saved.accessBlocked.insert(p.base);
                    if (actor && (p.base == 0x13653 || p.base == 0x13657) &&
                        locationHold(actor, 0) == ttcg::haafingarHold)
                        saved.tournamentBlocked.insert(p.base);
                }
            ttcg::progress(saved, i).unavailable = !present;
            if (dead && !present)
                saved.deadPlayers.insert(p.base);
            else
                saved.deadPlayers.erase(p.base);
        }
    }
    {
        const ttcg::NpcSyncPhaseScope phase(ttcg::NpcSyncPhase::Globals);
        const ttcg::NpcSyncWorkScope readinessTiming(ttcg::NpcSyncWork::Readiness);
        if (valdrGlobal && valdrGlobal->value > 0) saved.valdrRescued = true;
        if (erandurGlobal) saved.erandurReady = erandurGlobal->value > 0;
        if (progression) progression->value = hasAlbum() ? 2.0f : 0.0f;
        const auto hour = gameHour();
        for (int i = 0; i < static_cast<int>(ttcg::opponents.size()); ++i) {
            const auto& p = ttcg::opponents[i];
            const auto restock = ttcg::progress(saved, i).restockAt;
            // Refresh due stock before dialogue conditions are evaluated; never seed an
            // unseen collection just to decide whether its owner is willing to play.
            if (hasAlbum() && unlocked(i) && !saved.contract.pending() && restock && hour >= restock) {
                if (auto actor = resolveActiveOpponent(p)) seedOpponent(actor);
            }
            const bool freeToTalk =
                !saved.accessBlocked.contains(p.base) &&
                (actorReadyForCards(resolveOpponent(p)) ||
                 (p.alternateReference && actorReadyForCards(resolveID<RE::Actor>(p.alternateReference))));
            const bool resuming = savedMatch.present && !savedMatch.tournament &&
                                  (savedMatch.ledger == p.reference || savedMatch.opponent == p.reference ||
                                   savedMatch.opponent == p.alternateReference);
            if (i < int(opponentGlobals.size()) && opponentGlobals[i])
                opponentGlobals[i]->value =
                    hasAlbum() && (resuming || unlocked(i)) && freeToTalk
                        ? (resuming || (ttcg::opponentHasCards(saved, i) && ttcg::encounterReady(saved, i, hour) &&
                                        (development.unlockAllPlayers || ttcg::willingToPlay(saved, i, hour)))
                               ? 1.0f
                               : 2.0f)
                        : 0.0f;
        }
    }
    syncNpcDialogue();
}
unsigned tournamentLetterID(RE::TESObjectBOOK* book) {
    if (!book) return 0;
    const auto data = RE::TESDataHandler::GetSingleton();
    if (!data) return 0;
    // Match the loaded base forms directly, including ESL load indices and book
    // overrides. Receipt recognition must not depend on source-file metadata.
    for (unsigned id = 0xF80; id < 0xF80 + 30; ++id)
        if (data->LookupForm<RE::TESObjectBOOK>(id, plugin) == book) return id;
    return 0;
}
std::string_view resetBlocked() {
    if (tournamentLetterInFlight.load()) return "Wait for the pending courier request to finish first.";
    return ttcg::tesseraResetBlocked(saved, savedMatch.present);
}
bool removeResetItems() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    bool removed = true;
    const auto remove = [&](RE::TESObjectREFR* owner, RE::TESBoundObject* item) {
        if (!owner || !item) return;
        const int copies = physicalCount(owner, item);
        if (copies && physicalChange(owner, item, -copies) != -copies) removed = false;
    };
    remove(player, packForm);
    remove(player, tournamentPackForm);
    auto* data = RE::TESDataHandler::GetSingleton();
    const std::array<RE::TESObjectREFR*, 3> owners{player, resolveID<RE::TESObjectREFR>(0x39FB9),
                                                   resolveID<RE::TESObjectREFR>(0x39FB7)};
    for (unsigned id = 0xF80; id < 0xF80 + 30; ++id)
        if (auto* book = data->LookupForm<RE::TESObjectBOOK>(id, plugin))
            for (auto* owner : owners) remove(owner, book);
    return removed;
}
void acknowledgeTournamentCourier(std::uint64_t request) {
    auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(unsigned(request));
    const auto letter = tournamentLetterID(book);
    if (!letter) return;
    if (auto id = ttcg::recordTournamentInvitation(saved, letter, false, gameHour(), unsigned(request >> 32)))
        SKSE::log::info("Tournament {} invitation queued with courier", id);
    tournamentLetterInFlight.compare_exchange_strong(request, 0);
}
void recoverTournamentLetters() {
    auto player = RE::PlayerCharacter::GetSingleton();
    auto container = resolveID<RE::TESObjectREFR>(0x39FB9);
    auto courier = resolveID<RE::TESObjectREFR>(0x39FB7);
    if (!player || !container || !courier) return;
    auto data = RE::TESDataHandler::GetSingleton();
    std::set<unsigned> letters;
    for (const auto& e : saved.tournaments.events)
        if (e.invitation == 1 || e.invitation == 2) letters.insert(ttcg::tournamentLetter(e));
    for (auto id : letters)
        if (auto book = data->LookupForm<RE::TESObjectBOOK>(id, plugin)) {
            const auto carried = physicalCount(player, book),
                       copies = physicalCount(container, book) + physicalCount(courier, book);
            for (auto event : ttcg::reconcileTournamentInvitations(saved, id, carried, copies, gameHour()))
                SKSE::log::info("Tournament {} invitation recovered from player inventory", event);
        }
}
void updateRegionalRules() {
    if (available()) ttcg::tickRegionalRules(saved, gameHour(), cultureHeldHold);
}
void updateTournaments(bool background) {
    if (!available()) return;
    updateRegionalRules();
    if (saved.contract.pending()) return;
    recoverTournamentLetters();
    ttcg::refreshTournamentRewards(saved);
    const auto hour = gameHour();
    const bool hadAlbum = hasAlbum();
    for (auto id : ttcg::updateDevelopmentTournaments(saved, development, hour, tournamentVenue()))
        if (const auto* e = ttcg::tournament(saved, id))
            SKSE::log::info("Development {} {} in {}: {}", ttcg::tournamentCircuitName(e->circuit), id,
                            ttcg::holdName(e->hold), e->circuit ? "awaiting courier" : "ready at inn");
    if (!hadAlbum && hasAlbum()) ensureAlbum();
    if (progression && hasAlbum()) progression->value = 2.0f;
    ttcg::tickTournaments(saved, hour, actorHold(RE::PlayerCharacter::GetSingleton(), 0),
                          development.tournamentSwitches(), background ? ttcg::backgroundTournamentGames : 1000000);
    // Use the vanilla courier holding container; never add the letter to the player.
    auto data = RE::TESDataHandler::GetSingleton();
    for (unsigned hold = 1; hold <= 10; ++hold)
        if (auto global = data->LookupForm<RE::TESGlobal>(0xFCC + hold - 1, plugin)) {
            global->value = std::any_of(saved.tournaments.events.begin(), saved.tournaments.events.end(),
                                        [&](const auto& e) {
                                            return e.hold == hold && ttcg::tournamentEntryAvailable(e, hour) &&
                                                   !ttcg::tournamentEntryActive(saved);
                                        })
                                ? 1.0f
                                : 0.0f;
        }
    if (auto ready = tournamentLetterReady.load()) {
        const auto* e = ttcg::tournament(saved, unsigned(ready >> 32));
        if (!e || !e->circuit || e->invitation != 1 || e->awarded) tournamentLetterReady = 0;
    }
    if (auto request = tournamentLetterInFlight.load()) {
        const auto* e = ttcg::tournament(saved, unsigned(request >> 32));
        if (!e || e->invitation != 1 || e->awarded)
            tournamentLetterInFlight.compare_exchange_strong(request, 0);
        else if (tournamentCourierClock() - tournamentLetterSentAt.load() > 30000) {
            // A letter that never reaches the courier would hold its circuit for good.
            const auto id = e->id;
            if (++tournamentCourierFailures[id] < tournamentCourierAttempts)
                SKSE::log::warn("Tournament {} courier request was not acknowledged; retrying", id);
            else if (ttcg::withdrawTournamentInvitation(saved, id, hour)) {
                tournamentCourierFailures.erase(id);
                SKSE::log::error(
                    "Tournament {} invitation could not be handed to the courier after {} attempts; the event runs without the player",
                    id, tournamentCourierAttempts);
            }
            tournamentLetterInFlight.compare_exchange_strong(request, 0);
        }
    }
    for (auto& e : saved.tournaments.events)
        if (e.circuit && e.invitation == 1 && !e.awarded) {
            auto book = data->LookupForm<RE::TESObjectBOOK>(ttcg::tournamentLetter(e), plugin);
            if (!book) continue;
            if (tournamentLetterInFlight.load()) continue;
            if (!tournamentLetterReady.load())
                tournamentLetterReady.store((std::uint64_t(e.id) << 32) | book->GetFormID());
        }
    for (auto& [base, amount] : saved.tournaments.npcGold)
        if (amount) {
            int i = ttcg::opponentIndex(base);
            if (i >= 0) {
                auto actor = resolveOpponent(ttcg::opponents[i]);
                if (actor) amount -= std::clamp(physicalChange(actor, goldForm, int(amount)), 0, int(amount));
            }
        }
    ttcg::deliverTournamentPacks(saved, [](unsigned kind, int amount) {
        return physicalChange(RE::PlayerCharacter::GetSingleton(), kind ? tournamentPackForm : packForm, amount);
    });
    payPlayerGold();
}
}
