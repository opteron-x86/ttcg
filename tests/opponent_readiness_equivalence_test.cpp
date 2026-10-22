#include "FreePlay.h"
#include <cassert>
#include <iostream>

using namespace ttcg;

// Keep the previous full-stock calculation as an independent reference for the
// bounded hand check. No timing assertions: correctness must not depend on CPU.
bool referenceReadiness(const CollectionSave& s,int i){
 if(i<0||i>=int(opponents.size()))return false;
 const auto& p=opponents[i];const auto found=s.opponents.find(opponentLedgerKey(p.base,p.reference));
 return playableCount(playableOpponentStock(found==s.opponents.end()?unclaimedPersonalPool(s,i):found->second,i))>=5;
}
void compare(const CollectionSave& s,int i){assert(opponentHasCards(s,i)==referenceReadiness(s,i));}
int linearIndex(CardID id){
 for(unsigned i=0;i<cards.size();++i)if(cards[i].form==baseCardID(id))return int(i);
 return -1;
}
void checkIndices(){
 for(const auto& c:cards)for(auto id:{c.form,c.form|foilFlag})assert(cardIndex(id)==linearIndex(id));
 for(CardID id:{0u,1u,2083u,0xFFFFFFu,missingCardBase,missingCardBase|foilFlag,expansionCardBase+maximumExpansionCards,0x7FFFFFFFu,0xFFFFFFFFu})assert(cardIndex(id)==linearIndex(id));
}
int main(){
 checkIndices();
 // Expansion IDs remain correct after appending, reallocation and test-only
 // catalog resets. A previous lookup must not retain an absent expansion.
 std::vector<ExpansionCard> extra;
 for(unsigned n=0;n<65;++n){ExpansionCard c;c.key="readiness:card-"+std::to_string(n);c.name="Extra";c.rarity="Common";c.sides={3,3,3,3};c.tier=4;c.unique=n==64;c.tournamentPrizes=c.unique?1:0;extra.push_back(c);}
 registerExpansion("readiness","Readiness",std::move(extra));checkIndices();
 const auto extraID=expansionCardBase;
 CollectionSave s;grantStarter(s);compare(s,-1);compare(s,int(opponents.size()));
 std::mt19937 rng(41071);
 for(int i=0;i<int(opponents.size());++i){
  compare(s,i);assert(s.opponents.empty());
  auto& stock=s.opponents[opponents[i].reference];
  // Empty/depleted, foil duplicates, invalid cards, zero/negative counts, and
  // acquired high-tier/unique/expansion cards all use actual playable limits.
  for(unsigned sample=0;sample<40;++sample){
   stock.clear();
   for(unsigned c=0;c<sample%12;++c){auto id=cards[rng()%cards.size()].form;stock[id]=int(rng()%6)-1;stock[id|foilFlag]=int(rng()%5)-1;}
   stock[missingCardBase]=8;stock[0]=8;compare(s,i);
  }
  for(auto id:{baseCards[0].form,extraID}){stock={{id,3},{id|foilFlag,2}};compare(s,i);assert(!opponentHasCards(s,i));}
  s.opponents.clear();
 }
 // Authored pool overrides share identity limits and exclude uniques already
 // in the world, including reserved tournament prizes and held stakes.
 const int i=0;const auto unique=extraID+64;
 auto& pool=opponentPools()[opponents[i].base];pool={{extraID,1},{extraID|foilFlag,1},{extraID+1,2},{unique,1},{unique|foilFlag,1}};
 compare(s,i);assert(opponentHasCards(s,i));
 s.player[unique|foilFlag]=1;compare(s,i);assert(!opponentHasCards(s,i));s.player.erase(unique|foilFlag);
 s.tournaments.trophies.insert(unique);compare(s,i);assert(!opponentHasCards(s,i));s.tournaments.trophies.clear();
 s.contract.escrow[1][unique]=1;compare(s,i);assert(!opponentHasCards(s,i));s.contract.escrow[1].clear();
 // Once this NPC owns the unique, ownership elsewhere does not filter their
 // real stock; only unclaimed starting pools need the reservation check.
 s.opponents[opponents[i].reference]=pool;compare(s,i);assert(opponentHasCards(s,i));
 cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());
 expansionCards.clear();expansionKeys.clear();cardSets.clear();opponentPools().clear();
 checkIndices();assert(cardIndex(extraID)<0);compare(s,i);assert(!opponentHasCards(s,i));
 std::cout<<"Readiness equivalence: full roster, acquired cards, foil identity limits, expansion lifetime, unique reservations and depleted collections passed\n";
}
