#include "Development.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank {
 std::array<Stock,2> stock;int remaining=-1;
 int count(int p,CardID id){return ttcg::count(stock[p],id);}
 int change(int p,CardID id,int n){if(remaining==0)return 0;if(remaining>0)--remaining;stock[p][id]+=n;return n;}
};
int main(){
 const CardID id=cards[0].form,foil=id|foilFlag,other=cards[1].form;
 assert(cardIndex(id)==cardIndex(foil)&&displayCardID(displayIndex(foil))==foil);
 Stock stock{{id,2},{foil,2},{other,2},{cards[2].form,2}};
 assert(playableCount(stock)==6);
 Hand bad{id,id,foil,other,cards[2].form},good{id,foil,other,other,cards[2].form};
 assert(!validHand(bad,stock)&&!validDeck(bad,stock)&&validHand(good,stock));
 retainOwnedDeck(bad,stock);assert(bad[2]==0);
 for(unsigned seed=0;seed<500;++seed)assert(validHand(selectHand(stock,seed),stock));
 CollectionSave s;s.player=stock;s.deck=good;s.economyRandom=912734;saveActiveDeck(s);s.developmentFoilsGranted=true;
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(encodeCollection(s)==encodeCollection(loaded));
 assert(mintCard(s,id)==mintCard(loaded,id)&&s.economyRandom==loaded.economyRandom);
 const auto unique=std::find_if(cards.begin(),cards.end(),[](const auto& c){return c.unique;})->form;Stock uniques{{unique,1},{unique|foilFlag,1}};assert(playableCount(uniques)==1);assert(!validDeck({unique,unique|foilFlag,0,0,0},uniques));
 s={};grantStarter(s);recordCampaignResult(s,7,0,true,0,true);assert(progress(s,7).competitive.wins==1);
 unsigned upgrades=0,epics=0,legends=0,foils=0;long long value=0;constexpr unsigned trials=100000;
 for(unsigned seed=1;seed<=trials;++seed){
  auto pack=packCards(seed);assert(pack.size()==5);std::set<CardID> seen;unsigned high=0;
  for(unsigned slot=0;slot<5;++slot){auto base=pack[slot];assert(seen.insert(base).second);auto& c=cards[cardIndex(base)];assert(!c.unique&&c.tier<=6);high+=c.tier>3;
   if(slot<4)assert(std::string_view(c.rarity)=="Common");else{assert(std::string_view(c.rarity)!="Common");epics+=std::string_view(c.rarity)=="Epic";legends+=std::string_view(c.rarity)=="Legendary";}
   auto copy=mintCard(s,base);foils+=isFoil(copy);value+=cardValue(copy);
  }assert(high<=1);upgrades+=high;
 }
 assert(upgrades>1800&&upgrades<2200&&epics>3300&&epics<3700&&legends==0&&foils>400&&foils<600);
 assert(double(value)/trials<200); // Physical pack base price (also checked in the plugin records).
 std::cout<<"100,000 packs: "<<upgrades<<" upgrades, "<<epics<<" Epics, "<<legends<<" Legendaries, "<<foils<<" foils; mean intrinsic value "<<double(value)/trials<<" gold\n";
 for(int i=0;i<int(opponents.size());++i){
  auto pool=personalPool(i);assert(cardCount(pool)>=18&&cardCount(pool)<=32);CollectionSave npc;npc.economyRandom=8127;replenishOpponent(npc,i,123,100);assert(cardCount(npc.opponents[123])==cardCount(pool));const auto seeded=npc.opponents[123];
  replenishOpponent(npc,i,123,101);assert(npc.opponents[123]==seeded);
  for(unsigned seed=0;seed<100;++seed)assert(validHand(opponentHand(seeded,i,seed),seeded));
  for(auto& [card,n]:npc.opponents[123])n=0;replenishOpponent(npc,i,123,172);assert(cardCount(npc.opponents[123])==5);assert(validHand(opponentHand(npc.opponents[123],i,19),npc.opponents[123]));
 }
 // Exact copies and gold are conserved through all five trades, including retry/reload.
 Hand valued{id,foil,other,cards[2].form,cards[3].form};Contract choice;choice.hands[0]=valued;choice.trade=TradeOne;assert(choice.automaticChoice(1)==(1u<<prizeChoice(valued)));
 Hand a{id,foil,other,cards[2].form,cards[3].form},b{cards[4].form|foilFlag,cards[5].form,cards[6].form,cards[7].form,cards[8].form};
 for(unsigned seed=0;seed<50;++seed){Match match(indices(a),indices(b),seed,Same|Plus);std::mt19937 rng(seed);while(!match.finished()){auto legal=match.legalMoves();match.play(legal[rng()%legal.size()]);}
  for(unsigned trade=0;trade<=4;++trade){Bank bank;for(auto id:a)++bank.stock[0][id];for(auto id:b)++bank.stock[1][id];bank.stock[0][0]=bank.stock[1][0]=400;auto before=bank.stock;
   CollectionSave s;auto& c=s.contract;assert(c.reserve(bank,{a,b},trade,200,123));assert(c.capture(match));auto winner=c.outcome,mask=int(c.automaticChoice(winner));auto owners=c.transfers(winner,mask);
   assert(decodeCollection(encodeCollection(s),loaded));bank.remaining=trade?1:0;assert(!loaded.contract.settleSelection(bank,winner,mask));assert(decodeCollection(encodeCollection(loaded),s));bank.remaining=-1;assert(s.contract.flush(bank));auto once=bank.stock;assert(s.contract.flush(bank)&&bank.stock==once);
   for(int p=0;p<2;++p)for(auto [id,n]:before[p])assert(count(bank.stock[0],id)+count(bank.stock[1],id)==count(before[0],id)+count(before[1],id));
   if(trade)for(int origin=0;origin<10;++origin){auto id=origin<5?a[origin]:b[origin-5];assert(count(bank.stock[owners[origin]],id)>0);}
  }
 }
 s={};s.player[foil]=3;s.discovered[id]=s.discovered[foil]=1;saveActiveDeck(s);
 DevelopmentSettings dev;dev.enabled=dev.addAllFoils=true;assert(dev.grantFoils(s));auto granted=s.player;assert(!dev.grantFoils(s)&&s.player==granted);
 std::cout<<"Economy: expanded pools, replenishment, combined limits, finish ownership, five trade rules, 200-gold retry/reload, current-save recovery passed\n";
}
