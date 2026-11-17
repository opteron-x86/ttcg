// Stakes and results: accepting a challenge, recording a game, settling it and resuming an interrupted one.
#include "PluginState.h"

namespace campaign {
void acceptChallenge(RE::Actor* actor) {
    if (!actor || resumingWith(actor)) return;
    const int i = profileIndex(actor);
    if (i >= 0)
        ttcg::acceptOpponentChallenge(saved, i);
    else if (isChild(actor)) {
        const auto base = persistentID(actor->GetActorBase());
        saved.players[base].known = true;
        saved.childHolds[base] = actorHold(actor);
    }
}
void recordGame(RE::Actor* actor, int winner, bool completed) {
    if (tournamentID) {
        if (completed)
            ttcg::recordTournamentMatch(saved, tournamentID, winner, development.recordResults);
        else
            ttcg::withdrawTournament(saved, tournamentID);
        if (completed) notifyNpcMatch(actor, winner, sessionRules, tournamentID, false);
        syncProgression();
        updateTournaments();
        return;
    }
    const int i = profileIndex(actor);
    if (!development.recordResults || isChild(actor) || i < 0) return;
    const auto& c = saved.contract;
    const bool staked = competitiveMatch && c.held && (c.trade != ttcg::TradeNone || c.wager > 0);
    if (!staked) {
        syncProgression();
        return;
    }
    if (completed && cultureGame)
        cultureNotice = ttcg::cultureChangeText(ttcg::finishRuleCulture(saved, i, winner, sessionRules, gameHour()));
    else
        ttcg::abandonRuleCulture(saved, gameHour());
    cultureGame = false;
    ttcg::recordCampaignResult(saved, i, winner, true, sessionRules, true);
    ttcg::finishTravellerEncounter(saved, i, gameHour());
    syncProgression();
}
void finish(int winner, unsigned selection, std::uint32_t recoveryLedger) {
    auto& c = saved.contract;
    if (!c.pending()) return;
    if (selection == ~0u) selection = c.automaticChoice(winner);
    if (c.held) {
        receipt.clear();
        const auto owners = c.transfers(winner, selection);
        int won = 0, lost = 0;
        for (int i = 0; i < 10; ++i)
            if (owners[i] != i / 5) {
                if (owners[i] == 0)
                    ++won;
                else
                    ++lost;
            }
        if (won) receipt = std::format("Won {} card{}", won, won == 1 ? "" : "s");
        if (lost) {
            if (!receipt.empty()) receipt += " · ";
            receipt += std::format("Lost {} card{}", lost, lost == 1 ? "" : "s");
        }
        if (c.wager) {
            if (!receipt.empty()) receipt += " · ";
            receipt += winner < 0 ? "Wager returned" : std::format("{}{} gold", winner == 0 ? "+" : "−", c.wager);
        }
    }
    Bank bank(c.opponent, recoveryLedger);
    if (!c.settleSelection(bank, winner, selection))
        notice = "Inventory transfer pending.";
    else {
        if (c.completed) {
            auto actor = resolveID<RE::Actor>(c.opponent);
            notifyNpcMatch(actor, winner, savedMatch.present ? savedMatch.table.rules : sessionRules, 0, c.forfeited);
            notifyNpcTransfers(actor, c);
        }
        ttcg::retainOwnedDecks(saved);
        notice.clear();
        syncProgression();
        SKSE::log::info("Match stakes settled: outcome {}, selection {}", winner, selection);
    }
}
std::string savedMatchName() {
    auto actor = resolveID<RE::Actor>(savedMatch.opponent);
    return actor ? actor->GetName() : "your opponent";
}
void resolveSavedMatch(bool forfeit) {
    if (!savedMatch.present) return;
    const auto table = savedMatch;
    savedMatch = {};
    const auto previousTournament = tournamentID;
    tournamentID = table.tournament;
    competitiveMatch = table.competitive;
    cultureGame = table.culture;
    sessionRules = table.table.rules;
    if (!table.table.finished()) {
        if (forfeit) {
            recordGame(resolveID<RE::Actor>(table.opponent), 1, false);
            saved.contract.forfeit();
        } else {
            if (table.tournament)
                if (auto* event = ttcg::tournament(saved, table.tournament)) {
                    if (!ttcg::tournamentOpen(*event, gameHour()))
                        ttcg::withdrawTournament(saved, table.tournament);
                    else {
                        event->matchNode = 0;
                        syncProgression();
                        ttcg::resolveTournament(saved, *event);
                    }
                }
            if (table.culture) ttcg::abandonRuleCulture(saved, gameHour());
        }
    }
    if (saved.contract.pending()) finish(saved.contract.completed ? saved.contract.outcome : -1, ~0u, table.ledger);
    tournamentID = previousTournament;
    cultureGame = false;
}
bool settleFinishedMatch() {
    if (!ttcg::finishedMatchSettles(savedMatch, saved)) return false;
    resolveSavedMatch(false);
    return true;
}
bool recoverSavedMatch() {
    if (settleFinishedMatch() || !savedMatch.present) return false;
    const auto* event = ttcg::tournament(saved, savedMatch.tournament);
    if (savedMatch.tournament && !savedMatch.table.finished() &&
        (!event || !ttcg::tournamentOpen(*event, gameHour()) || event->eliminated || event->awarded ||
         !event->matchNode || event->matchNode != ttcg::tournamentNode(*event))) {
        resolveSavedMatch(false);
        notice = "The tournament has ended.";
        return false;
    }
    auto actor = resolveID<RE::Actor>(savedMatch.opponent);
    if (!actor || !actor->IsHumanoid() || actor->IsDead() || actor->IsDisabled()) {
        resolveSavedMatch(false);
        notice = "Your interrupted game has been canceled.";
        return false;
    }
    return true;
}
void seedOpponent(RE::Actor* actor) {
    const ttcg::NpcSyncWorkScope timing(ttcg::NpcSyncWork::Restock);
    const int i = profileIndex(actor);
    if (i < 0) {
        if (actor && !isChild(actor) && development.allowAnyNPC)
            saved.opponents.try_emplace(ledgerKey(actor), ttcg::childStock(persistentID(actor->GetActorBase())));
        return;
    }
    const auto id = ledgerKey(actor);
    ttcg::replenishOpponent(saved, i, id, gameHour());
    applyNpcGrants(actor);
    if (!saved.contract.pending())
        ttcg::refreshBettingPurse(saved.bettingPurses[id], ttcg::opponents[i].wager, gameHour());
}
bool prepare(RE::Actor* actor, bool resetTerms) {
    notice.clear();
    receipt.clear();
    revealed.clear();
    revealKind.clear();
    if (!available()) {
        notice = saveValid ? "Card data unavailable." : "Tessera can't read this save's data. Update Tessera.";
        return false;
    }
    recover();
    if (savedMatch.present) {
        if (actor) {
            notice = savedMatch.table.finished()
                         ? "Open your album and collect your winnings from " + savedMatchName() + " first."
                         : "Finish your game with " + savedMatchName() + " first.";
            return false;
        }
        owned = inventory(RE::PlayerCharacter::GetSingleton());
        rivalDeck = {};
        canStake = false;
        return true;
    }
    if (saved.contract.pending() || saved.goldCredit) {
        notice = "Transfer pending.";
        return false;
    }
    seedOpponent(actor);
    const int i = profileIndex(actor);
    if (resetTerms) {
        ttcg::abandonRuleCulture(saved, gameHour());
        cultureGame = false;
        cultureNotice.clear();
        requestAnswered = false;
        cultureHeldHold = 0;
        updateRegionalRules();
        const auto* p = profile(actor);
        cultureHeldHold = actor && !tournamentID && !(p && (p->personalRules || p->traveller)) ? actorHold(actor) : 0;
        sessionRules = terms(actor).rules;
        ruleRequest =
            development.rules >= 0 || !isChild(actor)
                ? 0
                : ttcg::childRequestedRule(saved, persistentID(actor->GetActorBase()), gameHour() / 24, sessionRules);
    }
    owned = inventory(RE::PlayerCharacter::GetSingleton());
    auto rival = inventory(actor);
    practice = isChild(actor);
    firstMove = -1;
    if (!actor) {
        canStake = false;
        rivalDeck = {};
        if (ttcg::playableCount(owned) < 5) notice = "You need five cards.";
        ttcg::retainOwnedDecks(saved);
        return true;
    }
    if (isChild(actor)) rival = ttcg::childStock(persistentID(actor->GetActorBase()));
    canStake = !isChild(actor) && (profile(actor) || development.allowAnyNPC) && ttcg::playableCount(owned) >= 5 &&
               ttcg::playableCount(rival) >= 5;
    if (ttcg::playableCount(owned) < 5)
        notice = "You need five cards.";
    else if (ttcg::playableCount(ttcg::playableOpponentStock(rival, i)) < 5) {
        notice = "They don't have enough cards for a game.";
        return false;
    }
    const auto base = actor->GetActorBase() ? persistentID(actor->GetActorBase()) : 0;
    const auto found = saved.players.find(base);
    const ttcg::PlayerRecord record = found == saved.players.end() ? ttcg::PlayerRecord{} : found->second;
    const auto games = record.competitive.wins + record.competitive.losses + record.competitive.draws;
    const auto handSeed = practice
                              ? static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
                              : persistentID(actor) ^ (games * ttcg::seedSpread);
    if (tournamentID) {
        auto* e = ttcg::tournament(saved, tournamentID);
        if (!e) return false;
        sessionRules = e->rules;
        practice = false;
        canStake = false;
        ruleRequest = 0;
    }
    const auto* event = ttcg::tournament(saved, tournamentID);
    const auto node = event ? ttcg::tournamentNode(*event) : 0;
    if (event && !node) {
        notice = "This tournament has no game to prepare.";
        return false;
    }
    const auto seed =
        event ? ttcg::tournamentHash(event->seed ^ node ^
                                     (event->wins[node - 8][0] + event->wins[node - 8][1]) * ttcg::seedSpread ^
                                     event->replays)
              : handSeed;
    // A tournament game's seed already decides it; elsewhere it follows the same
    // count of games that fixes the opponent's hand, so reopening the lobby can't change it.
    firstMove = event ? ttcg::Match::firstMover(seed) : int(ttcg::invitationHash(handSeed ^ 0x46495253u) & 1u);
    rivalDeck =
        practice ? ttcg::childHand(rival, base, seed, sessionRules) : ttcg::opponentHand(rival, i, seed, sessionRules);
    if (!ttcg::validHand(rivalDeck, rival)) {
        notice = "They don't have enough cards for a game.";
        return false;
    }
    if (saved.culture.pending.kind && practice) {
        ttcg::abandonRuleCulture(saved, gameHour());
        sessionRules = terms(actor).rules;
        notice = "Rule challenge canceled.";
    }
    if (!tournamentID && cultureEnabled(actor)) ttcg::prepareRuleSpread(saved, i, gameHour());
    ttcg::retainOwnedDecks(saved);
    return true;
}
}
