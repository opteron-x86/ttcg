#pragma once
#include "Collection.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <cctype>

namespace preview {
using Json=nlohmann::json;
struct CardView {
    ttcg::CardID id=0;
    std::string key,name,art,thumbnail,rarity,set;
    std::array<int,4> sides{};
    int tier=1;
    std::uint64_t groups=0;
    unsigned affinityMask=0;
    bool unique=false,available=true;
    std::array<int,2> owned{};
    std::array<bool,2> known{};
    std::string setKey;
};
inline std::vector<CardView> baseCatalog() {
    std::vector<CardView> result;
    for(std::size_t i=0;i<ttcg::cards.size();++i) {
        const auto& c=ttcg::cards[i];std::string thumb=c.art;
        if(thumb.starts_with("cards/"))thumb.replace(0,6,"thumbs/");
        result.push_back({c.form,c.id,c.name,c.art,thumb,c.rarity,"Tessera",c.sides,c.tier,ttcg::cardGroups[i],c.affinityMask,c.unique,c.available&&!c.pending,{},{}});
    }
    return result;
}
struct AlbumView {
    bool connected=false,ready=false,writable=false,busy=false;
    std::uint64_t revision=0;
    unsigned activeDeck=1,maxDecks=ttcg::maxSavedDecks;
    ttcg::Hand deck{};
    std::array<int,2> packs{};
    std::uint64_t revealID=0;
    unsigned revealKind=0;
    std::vector<ttcg::CardID> revealed;
    std::vector<ttcg::SavedDeck> decks;
    std::vector<CardView> cards;
    std::string reason,notice;
    int index(ttcg::CardID id) const {
        id=ttcg::baseCardID(id);for(int i=0;i<int(cards.size());++i)if(cards[i].id==id)return i;return -1;
    }
    bool canUse(ttcg::CardID id,int slot) const {
        if(!ready||!writable||busy||slot<0||slot>=5)return false;
        const int i=index(id);if(i<0)return false;
        const auto& c=cards[i];int copies=1,identities=1;
        for(int h=0;h<5;++h)if(h!=slot) {
            if(deck[h]==id)++copies;
            if(ttcg::baseCardID(deck[h])==c.id)++identities;
        }
        return copies<=c.owned[ttcg::isFoil(id)?1:0] && identities<=(c.unique?1:2);
    }
};
}
