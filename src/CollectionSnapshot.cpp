// The collection, lobby and journal as the JSON snapshot the interface reads.
#include "PluginState.h"

namespace campaign {
std::string json(RE::Actor* actor, bool, bool conceal) {
    const auto* event = ttcg::tournament(saved, tournamentID);
    const auto displayHold = event ? event->hold : actorHold(actor);
    auto stock = inventory(RE::PlayerCharacter::GetSingleton());
    auto rival = inventory(actor);
    std::string out = "{\"owned\":[";
    for (std::size_t i = 0; i < ttcg::cards.size() * 2; ++i) {
        if (i) out += ',';
        out += std::to_string(ttcg::count(stock, ttcg::displayCardID(static_cast<int>(i))));
    }
    ttcg::ensureSavedDecks(saved);
    auto visibleDeck = saved.deck;
    out += "],\"deck\":[";
    for (int h = 0; h < 5; ++h) {
        if (h) out += ',';
        out += std::to_string(ttcg::displayIndex(visibleDeck[h]));
    }
    out += "]," + ttcg::savedDeckFields(saved);
    out += ",\"rivalDeck\":[";
    for (int h = 0; h < 5; ++h) {
        if (h) out += ',';
        out += conceal ? "-2" : std::to_string(ttcg::displayIndex(rivalDeck[h]));
    }
    auto& c = saved.contract;
    const auto missingSets = ttcg::missingCollectionSets(saved);
    out += "],\"missingSets\":[";
    bool firstMissing = true;
    for (const auto& set : missingSets) {
        if (!firstMissing) out += ',';
        firstMissing = false;
        out += ttcg::quote(set);
    }
    out += "],\"recoveryBlocked\":" + ttcg::quote(ttcg::collectionRecoveryBlocked(saved, savedMatch.present));
    out += ",\"resetBlocked\":" + ttcg::quote(resetBlocked());
    out += std::format(
        ",\"gold\":{},\"rivalGold\":{},\"canStake\":{},\"staked\":{},\"reward\":{},\"wager\":{},\"notice\":{},\"receipt\":{},\"prizes\":[",
        ttcg::count(stock, 0), ttcg::count(rival, 0), canStake ? "true" : "false",
        c.pending() && (c.rule() || c.wager) ? "true" : "false", c.choicePending() ? "true" : "false", c.wager,
        ttcg::quote(notice), ttcg::quote(receipt));
    for (int h = 0; h < 5; ++h) {
        if (h) out += ',';
        out += conceal ? "-2" : std::to_string(ttcg::displayIndex(c.hands[1][h]));
    }
    out += std::format("],\"outcome\":{},\"choice\":{},\"paying\":{}}}", c.outcome,
                       c.rule() == ttcg::TradeOne && c.outcome == 1 ? ttcg::prizeChoice(c.hands[0]) : -1,
                       c.paying ? "true" : "false");
    out.pop_back();
    out += ",\"trade\":" + std::to_string(c.rule()) + ",\"required\":" + std::to_string(c.required(c.outcome)) +
           ",\"selectedMask\":" + std::to_string(c.held ? c.automaticChoice(c.outcome) : c.selected) +
           ",\"transferOwners\":[";
    const auto destinations = c.transfers(c.outcome, c.held ? c.automaticChoice(c.outcome) : c.selected);
    for (unsigned n = 0; n < 10; ++n) {
        if (n) out += ',';
        out += std::to_string(destinations[n]);
    }
    out += "]";
    out += ",\"known\":[";
    for (std::size_t i = 0; i < ttcg::cards.size() * 2; ++i) {
        if (i) out += ',';
        out += ttcg::count(saved.discovered, ttcg::displayCardID(static_cast<int>(i))) ? "true" : "false";
    }
    out += std::format("],\"packs\":[{},{}],\"revealID\":{},\"revealKind\":{},\"revealed\":[",
                       physicalCount(RE::PlayerCharacter::GetSingleton(), packForm),
                       physicalCount(RE::PlayerCharacter::GetSingleton(), tournamentPackForm),
                       revealed.empty() ? 0 : revealID, ttcg::quote(revealKind));
    for (std::size_t i = 0; i < revealed.size(); ++i) {
        if (i) out += ',';
        out += std::to_string(ttcg::displayIndex(revealed[i]));
    }
    out += "],\"roster\":[";
    bool first = true;
    for (std::size_t i = 0; i < ttcg::opponents.size(); ++i) {
        if (!development.showsPlayer(saved, i)) continue;
        if (!first) out += ',';
        first = false;
        const auto& p = ttcg::opponents[i];
        const auto& r = ttcg::progress(saved, i).competitive;
        out += std::format(
            "{{\"name\":{},\"location\":{},\"skill\":{},\"wins\":{},\"losses\":{},\"draws\":{},\"unlocked\":{}}}",
            ttcg::quote(p.name), ttcg::quote(p.location), ttcg::quote(p.skillName), r.wins, r.losses, r.draws,
            unlocked(i) ? "true" : "false");
        out.pop_back();
        out += ",\"holdID\":" +
               std::to_string(p.traveller ? 11
                              : p.hold    ? p.hold
                                          : 12) +
               ",\"hold\":" + ttcg::quote(p.traveller ? "Travellers" : ttcg::holdName(p.hold)) +
               ",\"traveller\":" + (p.traveller ? "true" : "false") + "}";
        out.pop_back();
        // What a game with them is played for, as far as it is settled before the lobby.
        const auto playerTerms = ttcg::opponentTerms(saved, int(i));
        out += ",\"base\":" + std::to_string(p.base) + ",\"friendlyOnly\":" + std::string("false") +
               ",\"rules\":" + std::to_string(playerTerms.rules) + ",\"trade\":" + std::to_string(playerTerms.trade) +
               ",\"wager\":" + std::to_string(p.wager) +
               ",\"ownRules\":" + (p.personalRules || ttcg::isMaiq(p) ? "true" : "false") + "}";
    }
    // Children are revealed by accepted challenges, without recording their games.
    for (const auto& [base, record] : saved.players)
        if (ttcg::opponentIndex(base) < 0 && record.known) {
            auto npc = resolveID<RE::TESNPC>(base);
            if (!npc || !npc->GetRace() || !npc->GetRace()->IsChildRace()) continue;
            if (!first) out += ',';
            first = false;
            const auto& r = record.competitive;
            const auto hold = saved.childHolds.contains(base) ? saved.childHolds.at(base) : 0;
            out += std::format(
                "{{\"name\":{},\"location\":{},\"skill\":{},\"wins\":{},\"losses\":{},\"draws\":{},\"unlocked\":true}}",
                ttcg::quote(npc->GetName()), ttcg::quote(childPlace(base, hold)),
                ttcg::quote(ttcg::childSkillName(base)), r.wins, r.losses, r.draws);
            out.pop_back();
            out += ",\"holdID\":" + std::to_string(hold ? hold : 12) +
                   ",\"hold\":" + ttcg::quote(ttcg::holdName(hold)) + "}";
            out.pop_back();
            out += ",\"base\":" + std::to_string(base) + ",\"friendlyOnly\":true}";
        }
    const auto* p = profile(actor);
    const int i = profileIndex(actor);
    const auto playStock = isChild(actor) ? ttcg::childStock(actor ? persistentID(actor->GetActorBase()) : 0) : rival;
    const std::string playReason = ttcg::cardsPreventingPlay(stock, playStock, i);
    const bool ready = actor && canChallenge(actor, false) && playReason.empty();
    out += std::format(
        "],\"introduced\":{},\"unlocked\":{},\"skill\":{},\"fixedRules\":{},\"fixedTrade\":{},\"maxWager\":{},\"tutorial\":false}}",
        saved.starter ? "true" : "false", saved.starter ? "true" : "false",
        ttcg::quote(development.skillName(p                ? p->skillName
                                          : isChild(actor) ? ttcg::childSkillName(persistentID(actor->GetActorBase()))
                                                           : "")),
        sessionRules, !practice && !tournamentID ? terms(actor).trade : 0, maxWager(actor));
    out.pop_back();
    out += std::format(",\"canPlay\":{},\"practice\":{},\"ruleRequest\":{},\"hold\":{},\"objective\":{}}}",
                       ready ? "true" : "false", practice ? "true" : "false", requestAnswered ? 0 : ruleRequest,
                       ttcg::quote(p && p->traveller ? "" : ttcg::holdName(displayHold)),
                       ttcg::quote(ttcg::reputationObjective(saved, displayHold)));

    out.pop_back();
    out += ",\"playUnavailableReason\":" +
           ttcg::quote(playReason.empty() && !ready ? (p && p->traveller && !ttcg::encounterReady(saved, i, gameHour())
                                                           ? "M'aiq has played enough for now."
                                                           : "They are unavailable for another game.")
                                                    : playReason);
    out += ",\"fixedWager\":" + std::to_string(fixedWager());
    out += ",\"firstMove\":" + std::to_string(actor ? firstMove : -1);
    const auto cultureHold = actorHold(actor), hour = gameHour();
    const bool cultureAvailable = cultureEnabled(actor);
    out += ",\"culture\":" + ttcg::cultureJson(saved, i, cultureHold, hour, cultureAvailable, cultureNotice);
    out += ",\"holdID\":" + std::to_string(displayHold) + ",\"regions\":[";
    bool regionFirst = true;
    for (const auto& h : ttcg::holds) {
        if (!regionFirst) out += ',';
        regionFirst = false;
        const auto terms = ttcg::holdTerms(saved, h.id);
        const auto rep = ttcg::localReputation(saved, h.id);
        out += std::format("{{\"id\":{},\"name\":{},\"rules\":{},\"trade\":{},\"reputationTitle\":{},\"wins\":{}}}",
                           h.id, ttcg::quote(h.name), terms.rules, terms.trade, ttcg::quote(rep.title()), rep.wins);
        out.pop_back();
        out += ",\"ruleChanges\":" + ttcg::regionalChangesJson(saved, h.id, hour, gameDate()) + "}";
    }
    bool elsewhere = false;
    for (int n = 0; n < int(ttcg::opponents.size()); ++n)
        if (!ttcg::opponents[n].hold && !ttcg::opponents[n].traveller && development.showsPlayer(saved, n))
            elsewhere = true;
    for (auto [base, hold] : saved.childHolds)
        if (!hold && saved.players.contains(base) && saved.players.at(base).known) elsewhere = true;
    if (elsewhere) {
        out +=
            ",{\"id\":12,\"name\":\"Elsewhere\",\"rules\":16,\"trade\":1,\"wins\":0,\"reputationTitle\":\"\",\"unaffiliated\":true}";
    }
    for (int n = 0; n < int(ttcg::opponents.size()); ++n)
        if (ttcg::opponents[n].traveller && development.showsPlayer(saved, n)) {
            out += ",{\"id\":11,\"name\":\"Travellers\",\"travellers\":true}";
            break;
        }
    out += "],\"ruleLesson\":" + std::to_string(tournamentID ? 0 : development.ruleLesson) +
           ",\"tournamentID\":" + std::to_string(tournamentID) + ",\"tournaments\":" +
           ttcg::tournamentsJson(saved, gameHour(), tournamentVenue(), tournamentRegistration, gameDate(),
                                 RE::PlayerCharacter::GetSingleton()->GetName(),
                                 actorHold(RE::PlayerCharacter::GetSingleton(), 0)) +
           "}";
    return out;
}
}
