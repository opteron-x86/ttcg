#pragma once
#include "Campaign.h"
#include "TournamentPresentation.h"
#include "VendorDistribution.h"
#include "EngineContainer.h"
#include "RuleCulture.h"
#include "WorldAccess.h"
#include "WorldIdentity.h"
#include "Development.h"
#include "DevelopmentTournaments.h"
#include "Opponents.h"
#include "OpponentLoader.h"
#include "Presentation.h"
#include "DeckPresentation.h"
#include "SavedMatch.h"
#include "SaveRecords.h"
#include "InterfaceSettings.h"
#include "EncounterRecords.h"
#include "NpcSyncTiming.h"
#include "NativeOpponents.h"
// The collection as the plugin holds it: its state, the small accessors that read the
// world, and the declarations of the work done in src/Collection*.cpp.
namespace campaign {
inline constexpr const char* plugin = "Tessera TCG.esp";
inline ttcg::CollectionSave saved;
inline ttcg::SavedMatch savedMatch;
inline RE::TESBoundObject* goldForm{};
inline RE::TESObjectBOOK* albumForm{};
inline RE::TESBoundObject* packForm{};
inline RE::TESBoundObject* tournamentPackForm{};
inline bool saveValid = true;
// The save's own records, kept for writing back while `saveValid` is false.
inline std::vector<ttcg::SaveRecord> preservedRecords;
inline ttcg::DevelopmentSettings development;
inline ttcg::InterfaceSettings interfaceSettings;
inline RE::TESGlobal* gamesEnabledGlobal{};
inline RE::TESTopicInfo* childChallengeInfo{};
inline void syncGameAvailability() {
    if (gamesEnabledGlobal) gamesEnabledGlobal->value = interfaceSettings.gamesEnabled ? 1.f : 0.f;
}
inline RE::TESGlobal* progression{};
inline RE::TESGlobal *erandurGlobal{}, *collegeGlobal{};
inline RE::TESQuest* brandSheiArrest{};
inline RE::TESGlobal* guildGlobal{};
inline RE::TESGlobal* valdrGlobal{};
inline std::array<RE::TESGlobal*, ttcg::baseOpponents.size()> opponentGlobals{};
inline bool practice = false, competitiveMatch = false, requestAnswered = false;
inline unsigned ruleRequest = 0, sessionRules = 0, tournamentID = 0, cultureHeldHold = 0;
inline unsigned tournamentRegistration = 0;
// High word: tournament ID; low word: letter form. An acknowledgement must apply
// to the event actually queued, even when another event uses the same note.
inline std::atomic<std::uint64_t> tournamentLetterReady{0}, tournamentLetterInFlight{0};
inline std::atomic<std::uint64_t> tournamentLetterSentAt{0};
// Unacknowledged courier requests per event since the save was loaded.
inline std::map<unsigned, unsigned> tournamentCourierFailures;
inline constexpr unsigned tournamentCourierAttempts = 6;
inline std::uint64_t tournamentCourierClock() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
inline ttcg::GameDate gameDate() {
    auto c = RE::Calendar::GetSingleton();
    return c ? ttcg::GameDate{int(c->GetYear()), int(c->GetMonth()), int(c->GetDay()), int(c->GetHour())}
             : ttcg::GameDate{};
}
inline unsigned gameHour() {
    auto c = RE::Calendar::GetSingleton();
    return c ? static_cast<unsigned>(std::clamp(c->GetDaysPassed() * 24.0f, 0.0f, 99999900.0f)) : 0;
}
inline ttcg::Hand rivalDeck{};
// Who moves first in the game being set up: 0 the player, 1 the opponent, -1 none.
// It is settled in the lobby, before the stakes, and holds until that game is played.
inline int firstMove = -1;
inline bool canStake = false;
inline std::string notice, receipt, cultureNotice;
inline bool cultureGame = false;
inline std::vector<ttcg::CardID> revealed;
inline std::string revealKind;
// Monotonic within the loaded game; an empty receipt publishes ID zero.
inline std::uint64_t revealID = 0;
inline std::map<std::uint32_t, RE::ActorHandle> authoredEncounterActors;
inline ttcg::Stock owned;
// Counting must not set a reference's inventory up as a side effect: that rolls its
// leveled lists, at whatever moment the count happened to be asked for. A chest
// that hasn't been set up holds nothing Tessera put there. `prepare` is for a
// merchant the player is trading with, whose gold has to be real.
inline int physicalCount(RE::TESObjectREFR* actor, RE::TESBoundObject* form, bool prepare = false) {
    if (!actor || !form) return 0;
    auto items = actor->GetInventoryCounts([&](RE::TESBoundObject& object) { return &object == form; }, !prepare);
    auto it = items.find(form);
    return it == items.end() ? 0 : std::max(0, it->second);
}
inline int physicalChange(RE::TESObjectREFR* actor, RE::TESBoundObject* form, int amount, bool prepare = false) {
    if (!actor || !form || !amount) return 0;
    const int before = physicalCount(actor, form, prepare);
    if (amount > 0)
        actor->AddObjectToContainer(form, nullptr, amount, nullptr);
    else
        actor->RemoveItem(form, std::min(before, -amount), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    return physicalCount(actor, form, prepare) - before;
}
inline bool authoredKey(std::uint32_t id) { return ttcg::identityMaster(id) != nullptr; }
// The plugin whose FormID space a form lives in. For an ordinary record that is the
// file that defines it, so its identity is the same as it has always been. A record
// another plugin injected into that space is found by load-order index instead:
// the defining plugin's name would save an identity that resolves to a different
// form, or to none, and that changes with the load order.
inline const RE::TESFile* identityFile(RE::TESForm* form) {
    const auto first = form->GetFile(0);
    const auto id = form->GetFormID();
    const bool light = id >> 24 == 0xFE;
    const std::uint16_t lightIndex = (id >> 12) & 0xFFF;
    if (!first || (light ? first->compileIndex == 0xFE && first->smallFileCompileIndex == lightIndex
                         : first->compileIndex == id >> 24))
        return first;
    auto* data = RE::TESDataHandler::GetSingleton();
    return !data   ? nullptr
           : light ? data->LookupLoadedLightModByIndex(lightIndex)
                   : data->LookupLoadedModByIndex(static_cast<std::uint8_t>(id >> 24));
}
inline std::uint32_t persistentID(RE::TESForm* form) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Identity);
    if (!form) return 0;
    const auto id = form->GetFormID();
    // A form created at run time has no file; its ID is all there is.
    if (!form->GetFile(0)) return id;
    const auto file = identityFile(form);
    return file ? ttcg::authoredIdentity(file->GetFilename(), id & (id >> 24 == 0xFE ? 0xFFFu : 0xFFFFFFu), id) : 0;
}
template <class T> T* resolveID(std::uint32_t id) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Forms);
    if (!id) return nullptr;
    const auto master = ttcg::identityMaster(id);
    // LookupForm<T> requires an exact record type: an Actor is not tagged REFR.
    // Resolve the form first, then allow the same base-type conversions as the
    // loaded-ID path (used by Skyrim actors, match banks and merchant references).
    auto form = master ? RE::TESDataHandler::GetSingleton()->LookupForm(ttcg::identityLocal(id), master)
                       : RE::TESForm::LookupByID(id);
    return form ? form->As<T>() : nullptr;
}
inline unsigned tournamentVenue() {
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return 0;
    constexpr unsigned cells[]{0x1605E, 0x3A184, 0x16A0E, 0x138CE, 0x13A7F,
                               0x16BDF, 0x13814, 0x16789, 0x16DFE, 0xDB017EC0};
    const auto cell = persistentID(player->GetParentCell());
    for (unsigned n = 0; n < 10; ++n)
        if (cell == cells[n]) return n + 1;
    return 0;
}
inline int profileIndex(RE::Actor* actor) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Identity);
    return actor ? ttcg::opponentIndex(persistentID(actor->GetActorBase())) : -1;
}
inline const ttcg::OpponentProfile* profile(RE::Actor* actor) {
    const int i = profileIndex(actor);
    return i < 0 ? nullptr : &ttcg::opponents[i];
}
inline RE::Actor* resolveOpponent(const ttcg::OpponentProfile& p) {
    if (!p.spawned) return resolveID<RE::Actor>(p.reference);
    auto it = authoredEncounterActors.find(p.base);
    if (it == authoredEncounterActors.end()) return nullptr;
    auto actor = it->second.get();
    return actor && persistentID(actor->GetActorBase()) == p.base ? actor.get() : nullptr;
}
// The opponent as they are in the world now: their alternate reference stands in while
// the usual one is missing or disabled, as it is for NPCs a quest moves.
inline RE::Actor* resolveActiveOpponent(const ttcg::OpponentProfile& p) {
    auto actor = resolveOpponent(p);
    if ((!actor || actor->IsDisabled()) && p.alternateReference) actor = resolveID<RE::Actor>(p.alternateReference);
    return actor;
}
inline std::uint32_t ledgerKey(RE::TESObjectREFR* actor) {
    if (!actor) return 0;
    const auto* p = profile(actor->As<RE::Actor>());
    return ttcg::opponentLedgerKey(p ? p->base : 0, persistentID(actor));
}
inline std::uint32_t locationHold(RE::Actor* actor, unsigned fallback = 0) {
    auto ref = actor ? actor : RE::PlayerCharacter::GetSingleton();
    constexpr std::array<RE::FormID, 9> locations{0x16772, 0x1676F, 0x16770, 0x1676E, 0x1676D,
                                                  0x1676C, 0x1676B, 0x1676A, 0x16769};
    auto location = ref ? ref->GetCurrentLocation() : nullptr;
    for (unsigned depth = 0; location && depth < 32; ++depth, location = location->parentLoc) {
        if (persistentID(location) == 0xDB016E2A) return ttcg::solstheimRegion;
        for (unsigned n = 0; n < locations.size(); ++n)
            if (location->GetFormID() == locations[n]) return n + 1;
    }
    if (ref && persistentID(ref->GetWorldspace()) == 0xDB000800) return ttcg::solstheimRegion;
    return fallback;
}
inline std::uint32_t actorHold(RE::Actor* actor, unsigned fallback = 0) {
    if (const auto* p = profile(actor)) return p->hold;
    return locationHold(actor, fallback);
}
inline const char* namedPlace(RE::BGSLocation* location) {
    for (unsigned depth = 0; location && depth < 32; ++depth, location = location->parentLoc) {
        const char* name = location->GetFullName();
        if (name && *name) return name;
    }
    return nullptr;
}
inline const char* childPlace(std::uint32_t base, unsigned hold) {
    const char* found = nullptr;
    if (auto* lists = RE::ProcessLists::GetSingleton())
        lists->ForAllActors([&](RE::Actor* actor) {
            if (found || !actor || persistentID(actor->GetActorBase()) != base)
                return RE::BSContainer::ForEachResult::kContinue;
            if (auto* name = namedPlace(actor->GetEditorLocation()))
                found = name;
            else if (auto* name = namedPlace(actor->GetCurrentLocation()))
                found = name;
            else if (auto* cell = actor->GetParentCell()) {
                const char* name = cell->GetFullName();
                if (name && *name) found = name;
            }
            return found ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
        });
    return found ? found : ttcg::holdName(hold);
}
inline ttcg::HoldTerms terms(RE::Actor* actor) {
    const auto i = profileIndex(actor);
    const auto terms = i >= 0 ? ttcg::opponentTerms(saved, i) : ttcg::holdTerms(saved, actorHold(actor));
    auto selected = development.terms(terms);
    if (i >= 0 && ttcg::isMaiq(ttcg::opponents[i])) selected.rules &= ~ttcg::Reverse;
    return selected;
}
inline bool unlocked(int i) { return development.unlockedPlayer(saved, i); }
inline bool isChild(RE::Actor* actor) { return actor && actor->IsChild(); }
inline bool cultureEnabled(RE::Actor* actor) {
    return !tournamentID && development.recordResults && development.rules < 0 && !practice && !isChild(actor) &&
           ttcg::cultureOpponent(saved, profileIndex(actor));
}
inline bool hasAlbum() { return development.albumAccess(saved); }
inline bool childDialogueAvailable(RE::Actor* actor) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::ChildConditions);
    // Use the loaded INFO conditions for both dialogue and native entry, including
    // quest/scene gates and any compatibility patch, instead of duplicating them.
    return childChallengeInfo && childChallengeInfo->objConditions.IsTrue(actor, RE::PlayerCharacter::GetSingleton());
}
enum class ActorCardBlock { None, Missing, NonHumanoid, Dead, Disabled, Combat, Hostile, Scene, ChildDialogue };
inline ActorCardBlock actorCardBlock(RE::Actor* actor) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::ActorState);
    if (!actor) return ActorCardBlock::Missing;
    if (!ttcg::measureNpcSync(ttcg::NpcSyncWork::Humanoid, [&] { return actor->IsHumanoid(); }))
        return ActorCardBlock::NonHumanoid;
    if (actor->IsDead()) return ActorCardBlock::Dead;
    if (actor->IsDisabled()) return ActorCardBlock::Disabled;
    if (actor->IsInCombat()) return ActorCardBlock::Combat;
    if (ttcg::measureNpcSync(ttcg::NpcSyncWork::Hostility,
                             [&] { return actor->IsHostileToActor(RE::PlayerCharacter::GetSingleton()); }))
        return ActorCardBlock::Hostile;
    if (ttcg::measureNpcSync(ttcg::NpcSyncWork::Scene, [&] { return actor->GetCurrentScene(); }))
        return ActorCardBlock::Scene;
    if (isChild(actor) && !childDialogueAvailable(actor)) return ActorCardBlock::ChildDialogue;
    return ActorCardBlock::None;
}
inline bool actorReadyForCards(RE::Actor* actor) { return actorCardBlock(actor) == ActorCardBlock::None; }
inline bool resumingWith(RE::Actor* actor) {
    return savedMatch.present && !savedMatch.tournament && actor && persistentID(actor) == savedMatch.opponent;
}
inline bool canChallenge(RE::Actor* actor, bool allowResume = true) {
    if (!interfaceSettings.gamesEnabled || !hasAlbum() || !npcIntegrationReady(actor)) return false;
    if (allowResume && resumingWith(actor)) return actorReadyForCards(actor);
    const int i = profileIndex(actor);
    if (tournamentID) {
        const auto* e = ttcg::tournament(saved, tournamentID);
        return e && i >= 0 && ttcg::tournamentRival(*e) == ttcg::opponents[i].base;
    }
    if (!actorReadyForCards(actor)) return false;
    if (i >= 0 && ttcg::opponents[i].dynamic) return npcAvailability(actor) == 1;
    return isChild(actor) ||
           (i >= 0 ? unlocked(i) && ttcg::opponentHasCards(saved, i) && ttcg::encounterReady(saved, i, gameHour()) &&
                         (development.unlockAllPlayers || ttcg::challengeReputationMet(saved, i))
                   : actor && development.allowAnyNPC);
}
inline int fixedWager() {
    if (tournamentID) return 0;
    return development.fixedWager(practice, canStake);
}
inline int maxWager(RE::Actor* actor) {
    if (tournamentID) return 0;
    const auto* p = profile(actor);
    return development.maxWager(practice, canStake, p ? p->wager : 0);
}
inline ttcg::Stock inventory(RE::TESObjectREFR* actor) {
    if (!actor) return {};
    ttcg::Stock result;
    if (actor == RE::PlayerCharacter::GetSingleton())
        result = saved.player;
    else if (auto it = saved.opponents.find(ledgerKey(actor)); it != saved.opponents.end())
        result = it->second;
    if (actor == RE::PlayerCharacter::GetSingleton())
        result[0] = physicalCount(actor, goldForm);
    else if (auto it = saved.bettingPurses.find(ledgerKey(actor)); it != saved.bettingPurses.end())
        result[0] = it->second.gold;
    return result;
}
struct Bank {
    std::array<RE::TESObjectREFR*, 2> actors{};
    std::uint32_t rival = 0, goldLedger = 0;
    bool removedOpponent = false;
    explicit Bank(std::uint32_t opponent, std::uint32_t recoveryLedger = 0)
        : actors{RE::PlayerCharacter::GetSingleton(), resolveID<RE::TESObjectREFR>(opponent)},
          removedOpponent(!actors[1] && recoveryLedger) {
        const bool recovering = saved.contract.pending() && saved.contract.opponent == opponent;
        rival = actors[1]                                  ? ledgerKey(actors[1])
                : recoveryLedger                           ? recoveryLedger
                : recovering && saved.contract.wagerLedger ? saved.contract.wagerLedger
                                                           : opponent;
        goldLedger = recovering ? saved.contract.wagerLedger : rival;
    }
    std::uint32_t wagerLedger() const { return goldLedger; }
    int count(int p, ttcg::CardID id) {
        if (!id) {
            if (p == 1 && goldLedger) {
                auto it = saved.bettingPurses.find(goldLedger);
                return it == saved.bettingPurses.end() ? 0 : it->second.gold;
            }
            return physicalCount(actors[p], goldForm);
        }
        if (p == 0) return ttcg::count(saved.player, id);
        auto it = saved.opponents.find(rival);
        return it == saved.opponents.end() ? 0 : ttcg::count(it->second, id);
    }
    int change(int p, ttcg::CardID id, int amount) {
        if (p == 1 && !id && goldLedger) {
            auto it = saved.bettingPurses.find(goldLedger);
            return it == saved.bettingPurses.end() ? 0 : ttcg::changeBettingPurse(it->second, amount);
        }
        if (p == 1 && !id && removedOpponent && amount > 0) return amount;
        if (!id) return physicalChange(actors[p], goldForm, amount);
        if (!ttcg::storedCard(saved, id) || (p == 1 && !rival)) return 0;
        auto& stock = p == 0 ? saved.player : saved.opponents[rival];
        const int moved = std::clamp(amount, -ttcg::count(stock, id), 1000000 - ttcg::count(stock, id));
        stock[id] += moved;
        if (p == 0 && moved > 0) {
            saved.discovered[id] = 1;
            saved.discovered[ttcg::baseCardID(id)] = 1;
        }
        return moved;
    }
};
inline bool available() { return saveValid && goldForm && packForm && tournamentPackForm && progression && albumForm; }
// Gives the player the gold Tessera still owes them, as far as their inventory takes it.
inline void payPlayerGold() {
    Bank bank(0);
    ttcg::payGoldCredit(saved.goldCredit, [&](int owed) { return bank.change(0, 0, owed); });
}
void initForms(bool distributeVendorPacks = true);
void seedOpponent(RE::Actor* actor);
void syncProgression(ttcg::NpcSyncTiming* timing = nullptr);
unsigned tournamentLetterID(RE::TESObjectBOOK* book);
std::string_view resetBlocked();
bool removeResetItems();
void acknowledgeTournamentCourier(std::uint64_t request);
void recoverTournamentLetters();
void ensureAlbum();
void seedEconomy();
void updateRegionalRules();
// `background` is the periodic update during play, which must not stall a frame.
void updateTournaments(bool background = false);
void acceptChallenge(RE::Actor* actor);
void recordGame(RE::Actor* actor, int winner, bool completed = true);
void finish(int winner, unsigned selection = ~0u, std::uint32_t recoveryLedger = 0);
bool claimAlbum();
bool buyAlbum(RE::Actor* actor);
void removeLegacyAlbumStock();
std::string savedMatchName();
void resolveSavedMatch(bool forfeit);
// Settle a finished table at once when nothing is left for the player to choose.
// Otherwise packs, shops, decks and every other opponent stay locked until the
// player goes back to that table.
bool settleFinishedMatch();
bool recoverSavedMatch();
// The development switches that hand out every card and foil.
void grantDevelopmentCards();
void recover();
bool prepare(RE::Actor* actor, bool resetTerms = false);
bool openPack(std::uint32_t seed);
bool openTournamentPack();
std::string json(RE::Actor* actor, bool = true, bool conceal = false);
void save(SKSE::SerializationInterface* serial);
void revert(SKSE::SerializationInterface*);
void load(SKSE::SerializationInterface* serial);
}
