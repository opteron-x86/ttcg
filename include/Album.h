#pragma once
#include "Collection.h"
#include <string_view>
namespace ttcg {
// Development grants enter the same ownership ledger as purchases and prizes.
// Check every limit first so an all-card grant cannot be partially applied.
inline bool grantDevelopmentCards(CollectionSave& s,bool development,int index) {
 if(!development||s.contract.pending()||s.goldCredit||index< -1||index>=static_cast<int>(cards.size()))return false;
 for(int i=0;i<static_cast<int>(cards.size());++i)if((index<0||index==i)&&count(s.player,cards[i].form)>=1000000)return false;
 for(int i=0;i<static_cast<int>(cards.size());++i)if(index<0||index==i){++s.player[cards[i].form];s.discovered[cards[i].form]=1;}
 return true;
}
inline constexpr int packPrice=200;
// Independent finish roll for newly created copies. Transfers preserve the ID.
inline CardID mintCard(CollectionSave& s,CardID id){
 if(!s.economyRandom)s.economyRandom=0x7F4A7C15u;
 auto& x=s.economyRandom;x^=x<<13;x^=x>>17;x^=x<<5;
 return id|(x%1000==0?foilFlag:0);
}
inline std::vector<CardID> packCards(std::uint32_t seed) {
 std::vector<CardID> result;
 std::mt19937 rng(seed);
 const int upgrade=rng()%100<2?int(rng()%5):-1;
 for(int slot=0;slot<5;++slot){
   const unsigned roll=rng()%100;int tier=roll<45?1:roll<80?2:3;
   if(slot==upgrade){auto high=rng()%100;tier=high<75?4:high<95?5:6;}
   const unsigned premium=rng()%1000;
   const char* rarity=slot<4?"Common":premium<965?"Rare":"Epic";
   std::vector<CardID> pool;
   for(const auto& c:cards)if(c.available&&c.tier==tier&&!c.unique&&std::string_view(c.rarity)==rarity&&std::find(result.begin(),result.end(),c.form)==result.end())pool.push_back(c.form);
   if(pool.empty())return {}; // No partial packs or silent rarity downgrades.
   result.push_back(pool[rng()%pool.size()]);
 }
 return result;
}
}
