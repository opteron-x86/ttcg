#pragma once
#include "Collection.h"
namespace ttcg {
// Both finishes and held stakes count as ownership. Estate recovery may
// exclude its source NPC while checking every other possible owner.
inline bool uniqueCardOwned(const CollectionSave& s,CardID id,unsigned exceptOpponent=0){
 id=baseCardID(id);
 auto has=[&](const Stock& stock){return count(stock,id)>0||count(stock,id|foilFlag)>0;};
 if(has(s.player)||merchantOwnsCard(s,id))return true;
 for(const auto& [key,stock]:s.opponents)if(key!=exceptOpponent&&has(stock))return true;
 for(const auto* stocks:{&s.contract.escrow,&s.contract.credits})for(const auto& stock:*stocks)if(has(stock))return true;
 return false;
}
// Include reserved and undelivered prizes: a unique belongs to the world,
// not just the player's currently visible album.
inline bool uniqueCardClaimed(const CollectionSave& s,CardID id){
 id=baseCardID(id);
 if(uniqueCardOwned(s,id)||s.tournaments.trophies.contains(id))return true;
 for(const auto& e:s.tournaments.events)if(baseCardID(e.trophy)==id)return true;
 return false;
}
inline bool grantExpansionCard(CollectionSave& s,std::string_view key,int quantity,bool foil){
 auto id=expansionCardID(key);const auto index=cardIndex(id);
 if(index<0||quantity<1||quantity>99)return false;
 if(cards[index].unique&&(quantity!=1||uniqueCardClaimed(s,id)))return false;
 if(foil)id|=foilFlag;
 int reserved=0;for(const auto* stocks:{&s.contract.escrow,&s.contract.credits})for(const auto& stock:*stocks)reserved+=count(stock,id);
 if(count(s.player,id)>1000000-quantity-reserved)return false;
 s.player[id]+=quantity;s.discovered[id]=s.discovered[baseCardID(id)]=1;return true;
}
}
