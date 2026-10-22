#pragma once
#include "Collection.h"
#include "Tessera/CollectionAPI.h"
#include <atomic>
#include <memory>
#include <mutex>

namespace ttcg::collectionAPI {
struct Row {
    std::string key, name, setID, setName, rarity, groups, affinities, art, thumbnail;
    TesseraCollectionCard value{};
    void bind() {
        value.size=sizeof(value);
        value.key=key.c_str();value.name=name.c_str();value.setID=setID.c_str();value.setName=setName.c_str();
        value.rarity=rarity.c_str();value.groupsJSON=groups.c_str();value.affinitiesJSON=affinities.c_str();
        value.art=art.c_str();value.thumbnail=thumbnail.c_str();
    }
};
struct Snapshot {
    TesseraCollectionSummary summary{};
    std::vector<Row> rows;
    std::map<std::string,std::size_t,std::less<>> indices;
    const TesseraCollectionCard* find(std::string_view key) const {
        const auto it=indices.find(key);return it==indices.end()?nullptr:&rows[it->second].value;
    }
};
using SnapshotPtr=std::shared_ptr<const Snapshot>;

// Stakes remain owned until settlement. Outstanding committed credits are also
// owned; never count an opponent's escrow or an unchosen prize as the player's.
inline Stock playerCards(const CollectionSave& save) {
    Stock result;
    auto add=[&](const Stock& stock){for(auto [id,n]:stock)if(n>0&&cardIndex(id)>=0)result[id]+=n;};
    add(save.player);
    if(save.contract.held)add(save.contract.escrow[0]);
    if(save.contract.paying)add(save.contract.credits[0]);
    return result;
}
inline SnapshotPtr makeSnapshot(const Stock& owned,const Stock& discovered,unsigned revision) {
    auto result=std::make_shared<Snapshot>();
    auto& summary=result->summary;summary.size=sizeof(summary);summary.revision=revision;
    result->rows.reserve(cards.size());
    for(const auto& card:cards) {
        // Include the full album catalog, including special rewards.
        auto& row=result->rows.emplace_back();auto& value=row.value;
        row.key=namedCardKey(card.form);row.name=card.name;row.rarity=card.rarity;
        row.groups=card.groups;row.affinities=card.affinities;
        const auto* expansion=expansionCard(card);
        row.setID=expansion?expansion->setId:"tessera";row.setName=expansion?expansion->setName:"Tessera";
        row.art="TesseraUI/art/"+std::string(card.art);
        row.thumbnail="TesseraUI/art/"+(expansion?expansion->thumbnail:"thumbs/"+row.name+".webp");
        value.tier=card.tier;std::copy(card.sides.begin(),card.sides.end(),value.sides);
        value.standardCount=std::max(0,count(owned,card.form));value.foilCount=std::max(0,count(owned,card.form|foilFlag));
        const bool foilKnown=value.foilCount||count(discovered,card.form|foilFlag)>0;
        const bool known=value.standardCount||foilKnown||count(discovered,card.form)>0;
        value.flags=(card.unique?TESSERA_CARD_UNIQUE:0u)|(known?TESSERA_CARD_DISCOVERED:0u)|(foilKnown?TESSERA_CARD_FOIL_DISCOVERED:0u)|(card.pending?TESSERA_CARD_PENDING:0u);
        ++summary.cardCount;summary.ownedCards+=bool(value.standardCount||value.foilCount);summary.discoveredCards+=known;
        summary.foilOwnedCards+=bool(value.foilCount);summary.foilDiscoveredCards+=foilKnown;
        summary.standardCopies+=value.standardCount;summary.foilCopies+=value.foilCount;
        result->indices.emplace(row.key,result->rows.size()-1);
    }
    // Bind string pointers after the snapshot reaches its final size.
    for(auto& row:result->rows)row.bind();
    return result;
}
class Store {
    std::atomic<SnapshotPtr> current;
    std::atomic_uint32_t revision{0};
    std::mutex writer;
    Stock previousOwned,previousDiscovered,previousPlayer,previousEscrow,previousCredits;
    bool previousHeld=false,previousPaying=false;
    std::size_t previousCatalogSize=0;
    unsigned nextRevision() {const auto next=revision.load()==INT32_MAX?1u:revision.load()+1u;revision.store(next);return next;}
public:
    SnapshotPtr acquire() const {return current.load();}
    unsigned getRevision() const {return revision.load();}
    bool reset() {
        std::scoped_lock lock(writer);
        if(!current.exchange({}))return false;
        previousOwned.clear();previousDiscovered.clear();previousPlayer.clear();previousEscrow.clear();previousCredits.clear();
        previousHeld=false;previousPaying=false;previousCatalogSize=0;nextRevision();return true;
    }
    // Publish a snapshot after a completed collection operation.
    bool refresh(const CollectionSave& save) {
        std::scoped_lock lock(writer);
        const bool hasSnapshot=bool(current.load());
        const bool sameDiscovery=save.discovered==previousDiscovered&&cards.size()==previousCatalogSize;
        if(hasSnapshot&&sameDiscovery&&save.player==previousPlayer&&save.contract.held==previousHeld&&save.contract.paying==previousPaying&&
           save.contract.escrow[0]==previousEscrow&&save.contract.credits[0]==previousCredits)return false;
        auto owned=playerCards(save);
        previousPlayer=save.player;previousEscrow=save.contract.escrow[0];previousCredits=save.contract.credits[0];
        previousHeld=save.contract.held;previousPaying=save.contract.paying;
        if(hasSnapshot&&sameDiscovery&&owned==previousOwned)return false;
        const auto next=revision.load()==INT32_MAX?1u:revision.load()+1u;
        auto snapshot=makeSnapshot(owned,save.discovered,next);
        previousOwned=std::move(owned);previousDiscovered=save.discovered;previousCatalogSize=cards.size();
        current.store(std::move(snapshot));revision.store(next);return true;
    }
};
inline Store store;
inline const Snapshot* heldSnapshot(const void* handle) {return handle?static_cast<const SnapshotPtr*>(handle)->get():nullptr;}
inline const TesseraCollectionAPI api{
    sizeof(TesseraCollectionAPI),TESSERA_COLLECTION_API_VERSION,
    []()->uint32_t{return store.getRevision();},
    []()->const void*{try{auto s=store.acquire();return s?new SnapshotPtr(std::move(s)):nullptr;}catch(...){return nullptr;}},
    [](const void* handle){delete static_cast<const SnapshotPtr*>(handle);},
    [](const void* handle)->const TesseraCollectionSummary*{const auto* s=heldSnapshot(handle);return s?&s->summary:nullptr;},
    [](const void* handle,uint32_t index)->const TesseraCollectionCard*{const auto* s=heldSnapshot(handle);return s&&index<s->rows.size()?&s->rows[index].value:nullptr;},
    [](const void* handle,const char* key)->const TesseraCollectionCard*{const auto* s=heldSnapshot(handle);return s&&key?s->find(key):nullptr;}
};
}
