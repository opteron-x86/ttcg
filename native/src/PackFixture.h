#pragma once
#include "Album.h"
#include "Tournaments.h"

namespace preview {
// Disposable desktop inventory. Production uses campaign::openPack and
// campaign::openTournamentPack directly; no test inventory reaches Skyrim.
struct PackFixture {
    std::array<int,2> inventory{1,1};
    unsigned consumed=0,seed=621;
    bool rejectRemoval=false;
    std::vector<ttcg::CardID> open(ttcg::CollectionSave& saved,unsigned kind) {
        const auto consume=[&]{if(rejectRemoval||inventory[kind]<1)return false;--inventory[kind];++consumed;return true;};
        if(kind)return ttcg::openTournamentPack(saved,consume);
        const auto random=saved.economyRandom;
        auto cards=ttcg::packCards(seed++);if(cards.size()!=5)return {};
        for(auto& id:cards)id=ttcg::mintCard(saved,id);
        if(!ttcg::canAddTournamentCards(saved,cards)||!consume()){saved.economyRandom=random;return {};}
        ttcg::addTournamentCards(saved,cards);ttcg::saveActiveDeck(saved);return cards;
    }
};
}
