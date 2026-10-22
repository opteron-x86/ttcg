#include "FreePlay.h"
#include "Tournaments.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;

struct WinningsBank {
 CollectionSave& save;
 std::uint32_t rival;
 Stock& stock(int p){return p?save.opponents.at(rival):save.player;}
 int count(int p,CardID id){return ttcg::count(stock(p),id);}
 int change(int p,CardID id,int amount){amount=std::clamp(amount,-count(p,id),1000000-count(p,id));stock(p)[id]+=amount;return amount;}
};

// A completed 9–1 table, with the losing player's fifth card still in hand.
Match wonTable(const Hand& player,const Hand& rival,int winner){
 Match match(indices(player),indices(rival),17,Open);match.placed=9;
 for(auto& used:match.used)used.fill(true);
 const int unplayed=(1-winner)*5+4;match.used[1-winner][4]=false;
 int square=0;
 for(int origin=0;origin<10;++origin)if(origin!=unplayed)
  match.board[square++]={match.hands[origin/5][origin%5],winner,origin};
 assert(match.finished()&&match.score()[winner]==9);return match;
}

int main(){
 const int beginner=opponentIndex(0x13BA3);assert(beginner>=0&&opponents[beginner].level==0);
 const auto key=opponentLedgerKey(opponents[beginner].base,opponents[beginner].reference);
 Hand valuable{};unsigned slot=0;
 for(int tier:{3,5,7,8,10}){
  const auto found=std::find_if(cards.begin(),cards.end(),[&](const auto& card){
   return card.tier==tier&&!card.pending&&(tier<=7?std::string_view(card.rarity)=="Epic":card.unique);
  });
  assert(found!=cards.end());valuable[slot++]=found->form|(tier==7?foilFlag:0);
 }
 Stock high;for(auto id:valuable)++high[id];

 for(unsigned trade:{TradeOne,TradeDiff,TradeDirect,TradeAll}){
  CollectionSave s;grantStarter(s);s.opponents[key]=personalPool(beginner);
  for(auto id:valuable)++s.player[id];
  const auto initial=s.opponents.at(key);const auto rival=opponentHand(initial,beginner,11);
  WinningsBank bank{s,key};auto& contract=s.contract;
  assert(contract.reserve(bank,{valuable,rival},trade,0,key));
  assert(contract.capture(wonTable(valuable,rival,1))&&contract.outcome==1);
  const auto choice=contract.automaticChoice(1);const auto owners=contract.transfers(1,choice);
  std::set<CardID> won;
  for(unsigned h=0;h<5;++h)if(owners[h]==1)won.insert(valuable[h]);
  assert(!won.empty());
  assert(contract.settleSelection(bank,1,choice));
  for(auto id:won)assert(count(s.player,id)==0&&count(s.opponents.at(key),id)==1);

  CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));
  auto& stock=loaded.opponents.at(key);
  // Won cards enter the next draw pool while the original collection is full.
  auto unseen=won;
  for(unsigned seed=0;seed<256;++seed){
   const auto hand=opponentHand(stock,beginner,seed);assert(validHand(hand,stock));
   for(auto id:hand)unseen.erase(id);
  }
  if(!unseen.empty())std::cerr<<"Trade "<<trade<<": "<<unseen.size()<<" owned winnings never entered a hand\n";
  assert(unseen.empty());

  // Deplete original cards until the NPC needs their winnings to field five.
  unsigned kept=0;
  for(auto [id,n]:initial)stock[id]=kept++<4?1:0;
  assert(opponentHasCards(loaded,beginner));
  assert(std::string_view(cardsPreventingPlay(loaded.player,stock,beginner)).empty());
  auto next=opponentHand(stock,beginner,29);assert(validHand(next,stock));
  auto prize=std::find_if(next.begin(),next.end(),[&](auto id){return won.contains(id);});assert(prize!=next.end());

  // The exact won copy, including its foil flag, can be staked and recovered.
  WinningsBank recovery{loaded,key};const auto player=selectHand(loaded.player,31);
  assert(loaded.contract.reserve(recovery,{player,next},TradeOne,0,key));
  assert(loaded.contract.capture(wonTable(player,next,0)));
  assert(loaded.contract.settleSelection(recovery,0,1u<<std::distance(next.begin(),prize)));
  assert(count(loaded.player,*prize)==1&&count(stock,*prize)==0);
 }

 // A card outside the authored theme is also eligible within the original
 // tier range. Both finishes and additional owned copies may be drawn.
 auto original=personalPool(beginner);
 const auto extra=std::find_if(cards.begin(),cards.end(),[&](const auto& card){return card.tier==1&&card.available&&!card.pending&&!count(original,card.form);})->form;
 original[extra]=2;original[extra|foilFlag]=1;
 std::set<CardID> seen;bool duplicate=false;
 for(unsigned seed=0;seed<256;++seed){
  const auto hand=opponentHand(original,beginner,seed);assert(validHand(hand,original));unsigned copies=0;
  for(auto id:hand)if(baseCardID(id)==extra){seen.insert(id);++copies;}
  duplicate|=copies==2;
 }
 assert(seen.contains(extra)&&seen.contains(extra|foilFlag)&&duplicate);

 // Every ordinary skill level can use high-tier cards already owned, including
 // existing saved collections. Dialogue readiness and tournaments agree.
 CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;
 for(int i=0;i<int(opponents.size());++i)if(!opponents[i].traveller){
  const auto& profile=opponents[i];const auto ledger=opponentLedgerKey(profile.base,profile.reference);
  s.opponents[ledger]=high;
  assert(opponentHasCards(s,i));assert(validHand(opponentHand(high,i,43),high));
  if(profile.level>=2&&unlockedOpponent(s,i))assert(tournamentEntrant(s,i));
 }
 // Copy limits still apply across normal and foil versions; empty/invalid
 // stock cannot make up the fifth card or strand an otherwise legal hand.
 auto stock=high;stock[0]=1000;stock[0x7FFFFFFFu]=50;
 for(auto [id,n]:high)stock[id|foilFlag]=2;
 for(unsigned seed=0;seed<128;++seed)assert(validHand(opponentHand(stock,beginner,seed),stock));
 stock={{valuable[3],1},{valuable[3]|foilFlag,1},{valuable[4],1},{valuable[4]|foilFlag,1}};
 s.opponents[key]=stock;assert(!opponentHasCards(s,beginner));assert(opponentHand(stock,beginner,17)==Hand{});

 // Reverse still selects acquired low cards ahead of high-tier winnings.
 stock=high;for(const auto& card:cards)if(card.starter)stock[card.form]=1;
 for(unsigned seed=0;seed<32;++seed){const auto hand=opponentHand(stock,beginner,seed,Open|Reverse);assert(validHand(hand,stock));for(auto id:hand)assert(cards[cardIndex(id)].tier<=2);}
 std::cout<<"NPC winnings: all trades, immediate selection, depleted collections, exact-copy recovery, reload, skill levels, tournaments, copy limits and Reverse passed\n";
}
