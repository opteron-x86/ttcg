#include "Campaign.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int main(){
 std::array<unsigned,3> levels{};
 for(const auto& p:childProfiles){
  assert(opponentIndex(p.base)==-1);++levels[p.skill];
  const auto stock=childStock(p.base),again=childStock(p.base);assert(stock==again);
  assert(cardCount(stock)==(p.skill?30:24));
  const auto [low,high]=childTiers(p.base);
  bool rare=false,epic=false;unsigned themed=0;
  for(auto [id,n]:stock){const auto& c=cards[cardIndex(id)];assert(n==1&&!c.unique&&!c.pending&&c.available&&c.tier<=7);
   assert(std::string_view(c.rarity)!="Legendary");rare|=std::string_view(c.rarity)=="Rare";epic|=std::string_view(c.rarity)=="Epic";
   themed+=std::string_view(c.groups).find(p.group)!=std::string_view::npos;
  }
  assert(rare&&themed>=8);if(!p.skill)assert(!epic);
  for(unsigned seed=0;seed<100;++seed)for(auto rules:std::array<unsigned,4>{Open,Open|Legion,Open|Reverse,Open|Chaos|Swap}){
   const auto hand=childHand(stock,p.base,seed,rules);assert(validHand(hand,stock));
   for(auto id:hand){const auto& c=cards[cardIndex(id)];assert(rules&Reverse?c.tier<=2:c.tier>=low&&c.tier<=high);}
  }
 }
 for(auto n:levels)assert(n>=5);
 const auto fallback=childStock(0xFA123456);assert(childProfile(0xFA123456).skill==0&&validHand(childHand(fallback,0xFA123456,123,Open),fallback));
 // Child profiles cannot earn hold credit or enter the authored adult roster.
 CollectionSave s;recordCampaignResult(s,opponentIndex(childProfiles[0].base),0,true,Open,true);
 assert(s.players.empty());
 std::cout<<"17 practice profiles: stable collections, three strengths, themes, Reverse reserves, legal hands, fallback and no renown passed\n";
}
