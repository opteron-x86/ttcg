#include "Shop.h"
#include "Tournaments.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace ttcg;
struct Bank {
 std::array<int,2> gold{100000,100000};int debitLimit=-1;bool denyCredit=false;
 int count(int p){return gold[p];}
 int change(int p,int amount){
  if(amount>0&&denyCredit)return 0;
  if(amount<0){amount=std::max(amount,-gold[p]);if(debitLimit>=0)amount=std::max(amount,-debitLimit);}
  gold[p]+=amount;return amount;
 }
};
int main(){
 const CardID id=cards[0].form,foil=id|foilFlag;
 CardID unique=0,epic=0,pending=0;
 for(const auto& c:cards){if(!c.pending&&c.unique&&c.tier==10)unique=c.form;if(!c.unique&&c.tier==5&&std::string_view(c.rarity)=="Epic")epic=c.form;if(c.pending)pending=c.form;}
 assert(unique&&epic&&pending);assert(shopPrice(id)==25&&shopSalePrice(id)==5&&shopPrice(epic)==1800&&shopPrice(epic|foilFlag)==5400);
 assert(shopPrice(unique)==36000&&shopSalePrice(unique)==7200);
 assert(specialistDealer(0x1413A)&&specialistDealer(0x1329A)&&!specialistDealer(0x1C1AA));
 // Stock is small, stable and distinct. Dealers have a higher-tier selection,
 // but neither pool can mint a unique, pending card or legendary.
 long regularTiers=0,specialTiers=0,regularPremium=0,specialPremium=0;std::set<CardID> seen;
 for(unsigned key=1;key<=2000;++key)for(bool specialist:{false,true}){
  MerchantStock m;assert(refreshShop(m,key,100,1+key%10,specialist));assert(m.stock.size()==(specialist?14:10));
  const auto original=m.stock;assert(!refreshShop(m,key,147,1,specialist)&&m.stock==original);seen.clear();
  for(auto [id,n]:m.stock){const auto& c=cards[cardIndex(id)];assert(n==1&&c.available&&!c.pending&&!c.unique&&c.tier<=7);assert(seen.insert(baseCardID(id)).second);
   (specialist?specialTiers:regularTiers)+=c.tier;(specialist?specialPremium:regularPremium)+=std::string_view(c.rarity)!="Common";
  }
 }
 assert(specialTiers/28000.>regularTiers/20000.+2&&specialPremium/28000.>regularPremium/20000.+.2);
 CollectionSave s;grantStarter(s);s.player[id]=4;s.player[foil]=2;s.player[unique]=1;
 auto& m=s.merchants[123];refreshShop(m,123,0,3,true);m.stock={{epic,2},{foil,1}};ShopVisit visit;Bank bank;
 const auto original=s.player;const auto totalGold=bank.gold[0]+bank.gold[1];
 assert(shopSurplus(s,id)==2&&shopSurplus(s,foil)==0&&shopSurplus(s,unique)==0);
 assert(tradeCard(s,m,visit,bank,ShopAction::Buy,epic,1)==ShopResult::Done);
 assert(count(s.player,epic)==count(original,epic)+1&&count(m.stock,epic)==1&&bank.gold[0]==98200&&bank.gold[1]==101800);
 assert(s.discovered[epic]==1);
 assert(tradeCard(s,m,visit,bank,ShopAction::Sell,id,2)==ShopResult::Done&&shopSurplus(s,id)==0);
 assert(count(m.resale,id)==2&&count(visit.buyback,id)==2);
 const int balance=bank.gold[0];assert(tradeCard(s,m,visit,bank,ShopAction::Buyback,id,2)==ShopResult::Done&&bank.gold[0]==balance-10&&count(s.player,id)==4);
 assert(tradeCard(s,m,visit,bank,ShopAction::Buyback,id,1)==ShopResult::Unavailable);
 assert(tradeCard(s,m,visit,bank,ShopAction::Sell,foil,1)==ShopResult::Confirm);
 assert(tradeCard(s,m,visit,bank,ShopAction::Sell,foil,1,true)==ShopResult::Done&&count(s.player,foil)==1&&count(s.player,id)==4);
 assert(tradeCard(s,m,visit,bank,ShopAction::Sell,unique,1)==ShopResult::Confirm);
 assert(tradeCard(s,m,visit,bank,ShopAction::Sell,unique,1,true)==ShopResult::Done&&merchantOwnsCard(s,unique));
 assert(bank.gold[0]+bank.gold[1]==totalGold);
 // A save retains stock, treasury credits, finish and uniques; a new visit
 // drops only the temporary buyback price. Restocking retains the exact unique.
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(encodeCollection(loaded)==encodeCollection(s));
 ShopVisit newVisit;auto& restored=loaded.merchants.at(123);const auto before=restored.stock;
 assert(!refreshShop(restored,123,47,3,true)&&restored.stock==before);
 assert(refreshShop(restored,123,48,3,true)&&count(restored.resale,unique)==1&&count(restored.resale,foil)==0);
 assert(tradeCard(loaded,restored,newVisit,bank,ShopAction::Buyback,unique,1)==ShopResult::Unavailable);
 assert(tradeCard(loaded,restored,newVisit,bank,ShopAction::Buy,unique,1)==ShopResult::Done&&!merchantOwnsCard(loaded,unique)&&count(loaded.player,unique)==1);
 // An owned merchant trophy cannot be selected for another tournament.
 CollectionSave trophy;for(const auto& c:cards)if(tournamentTrophy(c,2)&&c.form!=unique)trophy.tournaments.trophies.insert(c.form);
 trophy.merchants[123].resale[unique]=1;
 for(unsigned seed=0;seed<100;++seed){Tournament event;event.circuit=2;std::mt19937 rng(seed);selectTournamentReward(trophy,event,rng);assert(event.trophy==0);}
 // Deck and last-hand protection is enforced natively, including quantities.
 CollectionSave small;small.player={{id,2},{cards[1].form,2},{epic,1}};small.deck={id,id,cards[1].form,cards[1].form,epic};saveActiveDeck(small);
 MerchantStock shop;ShopVisit v;assert(shopWarnings(small,id,1)==(ShopDeck|ShopLastHand));
 assert(tradeCard(small,shop,v,bank,ShopAction::Sell,id,1)==ShopResult::Confirm);
 assert(tradeCard(small,shop,v,bank,ShopAction::Sell,id,1,true)==ShopResult::Done&&small.deck[1]==0&&small.decks[0].cards[1]==0);
 auto unchanged=encodeCollection(small);bank.gold[1]=0;
 assert(tradeCard(small,shop,v,bank,ShopAction::Sell,id,1,true)==ShopResult::MerchantGold&&encodeCollection(small)==unchanged);
 bank.gold[0]=0;assert(tradeCard(small,shop,v,bank,ShopAction::Buyback,id,1)==ShopResult::PlayerGold);
 for(auto quantity:{-1,0,100,1000000})assert(tradeCard(small,shop,v,bank,ShopAction::Sell,id,quantity,true)==ShopResult::Invalid);
 assert(tradeCard(small,shop,v,bank,ShopAction::Sell,pending,1,true)==ShopResult::Invalid);
 // Partial physical gold operations cannot create a card or lose owed gold.
 for(bool selling:{false,true})for(bool partial:{false,true}){
  CollectionSave s;s.player[epic]=1;auto& m=s.merchants[123];refreshShop(m,123,0,3,true);m.stock={{epic,1}};ShopVisit visit;Bank bank;
  bank.denyCredit=true;bank.debitLimit=partial?3:-1;
  const auto action=selling?ShopAction::Sell:ShopAction::Buy;const auto n=count(s.player,epic);
  assert(tradeCard(s,m,visit,bank,action,epic,1)==(partial?ShopResult::Pending:ShopResult::Done));
  assert(count(s.player,epic)==n+(partial?0:selling?-1:1));assert(bank.gold[0]+bank.gold[1]+s.goldCredit+m.goldCredit==200000);
  const auto wire=encodeCollection(s);assert(tradeCard(s,m,visit,bank,action,epic,1)==ShopResult::Pending&&wire==encodeCollection(s));
  CollectionSave loaded;assert(decodeCollection(wire,loaded));bank.denyCredit=false;bank.debitLimit=-1;
  assert(recoverShop(loaded,loaded.merchants.at(123),bank)&&bank.gold[0]+bank.gold[1]==200000);
  const auto gold=bank.gold;assert(recoverShop(loaded,loaded.merchants.at(123),bank)&&bank.gold==gold);
 }
 // Format 27 has no identity dictionary or merchant trailer. Current malformed trailers fail
 // without partially replacing the collection.
 CollectionSave old;grantStarter(old);std::ifstream fixture("tests/fixtures/collection-v27-starter.txt");assert(fixture);
 std::vector<std::uint32_t> wire;for(std::uint32_t n;fixture>>n;)wire.push_back(n);
 assert(decodeCollection(wire,loaded,27)&&loaded.merchants.empty()&&loaded.starter&&loaded.player==old.player&&loaded.deck==old.deck);
 old.merchants[123].generation=1;auto valid=encodeCollection(old);const auto stable=encodeCollection(loaded);
 for(unsigned n=1;n<=4;++n){auto damaged=valid;damaged.resize(damaged.size()-n);assert(!decodeCollection(damaged,loaded)&&encodeCollection(loaded)==stable);}
 auto damaged=valid;damaged.back()=1000001;assert(!decodeCollection(damaged,loaded));
 std::cout<<"Card shop: stock distributions, persistent uniques, exact finishes, price/treasury conservation, buyback, deck guards, partial payments and save migration passed\n";
}
