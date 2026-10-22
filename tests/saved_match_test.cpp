#include "SavedMatch.h"
#include "Tournaments.h"
#include "Opponents.h"
#include <cassert>
#include <iostream>
using namespace ttcg;

struct Bank {
 std::array<Stock,2> stock;
 int count(int p,CardID id){return ttcg::count(stock[p],id);}
 int change(int p,CardID id,int n){n=std::max(n,-count(p,id));stock[p][id]+=n;return n;}
};
int main(){
 unsigned positions=0,redeals=0;
 for(unsigned seed=1;seed<=80;++seed){
  std::mt19937 rng(seed);const unsigned rules=randomTournamentRules(rng)|SuddenDeath;
  SavedMatch saved;saved.present=true;saved.opponent=0xA2C94;saved.ledger=0x13BB9;saved.competitive=true;
  saved.table=Match(seed,rules);
  const auto original=saved.originals();
  Bank bank;for(int p=0;p<2;++p){bank.stock[p][0]=100;for(auto id:original[p])++bank.stock[p][id];}
  Contract contract;assert(contract.reserve(bank,original,TradeDirect,25,saved.opponent));
  while(true){
   assert(matchContractValid(saved,contract));
   auto bytes=encodeMatch(saved);SavedMatch loaded;assert(decodeMatch(bytes,loaded));assert(encodeMatch(loaded)==bytes);
   assert(loaded.originals()==original);assert(matchContractValid(loaded,contract));
   assert(loaded.table.score()==saved.table.score());assert(loaded.table.known(1,0,0)==saved.table.known(1,0,0));
   assert(loaded.table.legalMoves().size()==saved.table.legalMoves().size());++positions;
   assert(matchDecisionSeed(loaded.table)==matchDecisionSeed(saved.table));
   if(!saved.table.boardComplete()&&positions%31==0){
    const auto a=opponentMove(saved.table,0,matchDecisionSeed(saved.table)),b=opponentMove(loaded.table,0,matchDecisionSeed(loaded.table));
    assert(a.hand==b.hand&&a.square==b.square&&a.group==b.group);
   }
   // Bad records cannot replace a valid destination or manufacture an origin.
   auto broken=bytes;broken[37]=broken[38];assert(!decodeMatch(broken,loaded));assert(encodeMatch(loaded)==bytes);
   broken=bytes;broken[5]=Order|Chaos;assert(!decodeMatch(broken,loaded));
   broken=bytes;broken[27]=0;assert(!decodeMatch(broken,loaded));
   broken.pop_back();assert(!decodeMatch(broken,loaded));
   if(saved.table.finished()){
    auto tampered=contract;tampered.outcome=contract.outcome==0?1:0;assert(!matchContractValid(saved,tampered));
    tampered=contract;tampered.finalOwners[0]=1-tampered.finalOwners[0];assert(!matchContractValid(saved,tampered));
    break;
   }
   if(saved.table.suddenDeathPending()){assert(saved.table.redeal());++redeals;continue;}
   const auto moves=saved.table.legalMoves();const auto move=moves[rng()%moves.size()];
   assert(saved.table.play(move).legal);assert(loaded.table.play(move).legal);
   assert(encodeMatch(saved)==encodeMatch(loaded));
   if(saved.table.finished())assert(contract.capture(saved.table));
  }
  const auto result=contract.outcome;const auto bytes=encodeMatch(saved);
  SavedMatch loaded;assert(decodeMatch(bytes,loaded));
  assert(!contract.capture(loaded.table)); // The final result was already recorded before the save.
  assert(contract.settleSelection(bank,result,contract.automaticChoice(result)));
  const auto once=bank.stock;assert(contract.settleSelection(bank,result,contract.automaticChoice(result)));assert(bank.stock==once);
  assert(matchContractValid(loaded,contract));
 }
 assert(positions>700&&redeals>0);
 // Exact copies and an unfinished escrow must agree, even when the names match.
 SavedMatch saved;saved.present=true;saved.opponent=123;saved.competitive=true;saved.table=Match(7,Open|Swap|Chaos);
 saved.table.foilHands[0][0]=true;Bank bank;auto originals=saved.originals();
 for(int p=0;p<2;++p){bank.stock[p][0]=100;for(auto id:originals[p])++bank.stock[p][id];}
 Contract contract;assert(contract.reserve(bank,originals,TradeAll,10,123));assert(matchContractValid(saved,contract));
 auto bad=contract;bad.hands[0][0]^=foilFlag;assert(!matchContractValid(saved,bad));
 bad=contract;bad.opponent++;assert(!matchContractValid(saved,bad));
 bad=contract;bad.forfeit();assert(!matchContractValid(saved,bad));
 const auto before=bank.stock;assert(contract.settleSelection(bank,-1,0));
 assert(bank.stock[0][0]==100&&bank.stock[1][0]==100);
 for(int p=0;p<2;++p)for(auto id:originals[p])assert(bank.count(p,id)>0);
 // A tournament's clock does not stop for an open or interrupted table.
 CollectionSave collection;grantStarter(collection);collection.collegeMember=collection.guildMember=collection.erandurReady=collection.valdrRescued=true;
 for(const auto& card:cards)if(card.starter)collection.player[card.form]=2;
 collection.deck=selectHand(collection.player,4);const auto id=newTournament(collection,1,100,true,0);assert(id);
 discoverTournaments(collection,1,100);Bank entry;entry.stock[0][0]=10000;assert(enterTournament(collection,id,100,entry));
 assert(startTournamentRound(collection,id,100));auto* event=tournament(collection,id);assert(event->matchNode);
 const auto ends=event->ends;tickTournaments(collection,ends-1);assert(!tournament(collection,id)->eliminated);
 tickTournaments(collection,ends);event=tournament(collection,id);assert(event->eliminated&&event->awarded&&!event->matchNode);
 std::cout<<"Saved matches: "<<positions<<" positions, "<<redeals<<" redeals, exact copies, RNG/visibility, corrupt records, escrow and tournament expiry passed\n";
}
