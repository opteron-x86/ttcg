#include "Campaign.h"
#include "Tournaments.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace ttcg;
struct Bank {
 CollectionSave& save;int gold=10000;unsigned key=123;bool fail=false;
 unsigned wagerLedger()const{return key;}
 int count(int who,CardID id){assert(!id);return who?save.bettingPurses.at(key).gold:gold;}
 int change(int who,CardID id,int n){assert(!id);if(fail&&n>0)return 0;if(who)return changeBettingPurse(save.bettingPurses.at(key),n);n=std::max(-gold,n);gold+=n;return n;}
};
int main(){
 // Fixtures were written by the unmodified 0.30.0 format-31 encoder. Money
 // already held by an old game returns to physical NPC gold, never a new purse.
 for(bool paying:{false,true}){
  std::ifstream input(paying?"tests/fixtures/collection-v31-paying-wager.txt":"tests/fixtures/collection-v31-held-wager.txt");assert(input);
  std::vector<std::uint32_t> wire;for(unsigned n;input>>n;)wire.push_back(n);
  CollectionSave old;assert(decodeCollection(wire,old,31)&&old.contract.pending()&&!old.contract.wagerLedger);
  refreshBettingPurse(old.bettingPurses[123],250,10);
  CollectionSave upgraded;assert(decodeCollection(encodeCollection(old),upgraded)&&!upgraded.contract.wagerLedger);
  struct LegacyBank {
   int physical[2]{900,900};
   int change(int who,CardID id,int n){assert(!id);physical[who]+=n;return n;}
  } bank;
  assert(upgraded.contract.settleSelection(bank,paying?1:-1,0));
  assert(bank.physical[0]==(paying?900:1000)&&bank.physical[1]==(paying?1100:1000));
  assert(upgraded.bettingPurses.at(123).gold==750);
 }
 BettingPurse p;refreshBettingPurse(p,1000,10);assert(p.gold==3000&&p.restockAt==82);
 assert(changeBettingPurse(p,-1000)==-1000);refreshBettingPurse(p,1000,81);assert(p.gold==2000);
 refreshBettingPurse(p,1000,82);assert(p.gold==3000&&p.restockAt==154);
 changeBettingPurse(p,5000);refreshBettingPurse(p,1000,10000);assert(p.gold==8000&&p.restockAt==10072);
 changeBettingPurse(p,-8000);refreshBettingPurse(p,1000,10001);assert(p.gold==0);refreshBettingPurse(p,1000,20000);assert(p.gold==3000); // One top-up, no elapsed-period windfall.
 p.gold=maximumBettingGold-10;assert(changeBettingPurse(p,100)==10&&changeBettingPurse(p,-maximumBettingGold)==-maximumBettingGold);
 Hand hand{};for(int h=0;h<5;++h)hand[h]=cards[h].form;
 for(int outcome:{-1,0,1})for(int wager:wagerAmounts){
  CollectionSave s;refreshBettingPurse(s.bettingPurses[123],2000,10);Bank b{s};
  assert(s.contract.reserve(b,{hand,hand},TradeNone,wager,456));assert(s.contract.wagerLedger==123);
  const int gold=b.gold;CollectionSave restored;assert(decodeCollection(encodeCollection(s),restored));Bank bank{restored,gold};
  if(wager){bank.fail=true;assert(!restored.contract.settleSelection(bank,outcome,0));assert(restored.contract.paying);CollectionSave retry;assert(decodeCollection(encodeCollection(restored),retry));restored=retry;bank.fail=false;}
  assert(restored.contract.settleSelection(bank,outcome,0)&&!restored.contract.pending());
  assert(bank.gold==10000+(outcome==0?wager:outcome==1?-wager:0));
  assert(bank.count(1,0)==6000+(outcome==1?wager:outcome==0?-wager:0));
  auto settled=encodeCollection(restored);assert(restored.contract.settleSelection(bank,outcome,0)&&encodeCollection(restored)==settled);
 }
 CollectionSave s;refreshBettingPurse(s.bettingPurses[123],100,0);Bank b{s};b.gold=24;
 assert(!s.contract.reserve(b,{hand,hand},TradeNone,25,456)&&b.gold==24&&b.count(1,0)==300);
 b.gold=1000;assert(!s.contract.reserve(b,{hand,hand},TradeNone,500,456));
 // Stable purses and pending credits participate in the common identity migration.
 b.gold=1000;assert(s.contract.reserve(b,{hand,hand},TradeNone,25,456));
 assert(remapWorldState(s,[](unsigned id){return id==123?789u:id;}));assert(s.contract.wagerLedger==789&&s.bettingPurses.contains(789));
 auto invalid=s;invalid.bettingPurses[789].gold=-1;CollectionSave loaded;assert(!decodeCollection(encodeCollection(invalid),loaded));
 invalid=s;invalid.contract.wagerLedger=555;assert(!decodeCollection(encodeCollection(invalid),loaded));
 const int talsgar=opponentIndex(0x83D99),maiq=opponentIndex(0x954BF);assert(talsgar>=0&&maiq>=0);
 const auto& bard=opponents[talsgar];assert(bard.spawned&&bard.traveller&&bard.reference==bard.base&&!bard.requiredMaster&&!bard.tournamentEligible);
 assert(opponentLedgerKey(bard.base,0xFF000123)==opponentLedgerKey(bard.base,0xFF000999));
 finishTravellerEncounter(s,talsgar,10);assert(encounterReady(s,talsgar,11)&&challengeReputationMet(s,talsgar)&&!tournamentEntrant(s,talsgar));
 assert(opponentCardAllowed(cards[0],talsgar)&&opponentTerms(s,talsgar).rules==Open);
 finishTravellerEncounter(s,maiq,10);assert(!encounterReady(s,maiq,11));
 assert(opponents[opponentIndex(0x13BA3)].wager==250&&opponents[opponentIndex(0x1B072)].wager==100);
 std::cout<<"Betting purses: independent budgets, timed top-ups, all stakes, settlement/retry/refund, migration, bounds and stable travelling bard identity passed\n";
}
