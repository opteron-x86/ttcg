#include "Campaign.h"
#include "DeckPresentation.h"
#include "ShopPresentation.h"
#include <iostream>
#include <sstream>
using namespace ttcg;
int main(){
 CollectionSave saved;grantStarter(saved);for(const auto& c:cards)if(!c.pending)saved.player[c.form]=c.unique?1:3;
 for(unsigned i=0;i<6;++i)saved.player[cards[i].form|foilFlag]=1;
 ShopVisit visit;unsigned hour=100;const unsigned key=0x123;refreshShop(saved.merchants[key],key,hour,3,true);
 struct Bank{std::array<int,2> gold{50000,20000};int count(int p){return gold[p];}int change(int p,int n){gold[p]+=n;return n;}}bank;
 std::uint64_t session=80,revision=0;std::string notice,screen="shop";
 auto output=[&]{
  auto out=snapshotJson(Match(1,0),session,revision,"Sayma",false,false,{},false,false,false);out.pop_back();
  out+=",\"screen\":"+quote(screen)+",\"matchContext\":false,\"shop\":"+shopJson(saved,saved.merchants[key],visit,"Sayma",true,bank.gold[0],bank.gold[1],notice)+",\"collection\":{"+savedDeckFields(saved);
  for(auto field:{"owned","known"}){out+=",\""+std::string(field)+"\":[";for(unsigned i=0;i<cards.size()*2;++i){if(i)out+=',';out+=std::to_string(count(saved.player,displayCardID(i)));}out+="]";}
  out+=",\"deck\":[";for(unsigned i=0;i<5;++i){if(i)out+=',';out+=std::to_string(displayIndex(saved.deck[i]));}out+="]";
  out+=",\"gold\":"+std::to_string(bank.gold[0])+",\"fixedRules\":0,\"fixedTrade\":0,\"maxWager\":0,\"canPlay\":true,\"packs\":[0,0],\"revealed\":[],\"revealID\":0}}";
  std::cout<<out<<std::endl;
 };
 output();for(std::string line;std::getline(std::cin,line);){
  std::istringstream input(line);std::string verb,action,extra;std::uint64_t s=0,r=0;input>>verb>>s>>r;
  if(s==session&&r==revision){
   if(verb=="shop"){
    int index=-1,n=0,confirm=0;
    if((input>>action>>index>>n>>confirm)&&!(input>>extra)&&index>=0&&index<int(cards.size()*2)&&(action=="buy"||action=="sell"||action=="buyback")&&(confirm==0||confirm==1)){
     auto result=tradeCard(saved,saved.merchants[key],visit,bank,action=="sell"?ShopAction::Sell:action=="buy"?ShopAction::Buy:ShopAction::Buyback,displayCardID(index),n,confirm);
     notice=result==ShopResult::Done?"Trade completed.":shopMessage(result);++revision;
    }
   }
   if(verb=="test-restock"){hour+=48;visit={};refreshShop(saved.merchants[key],key,hour,3,true);++session;}
   if(verb=="test-poor"){bank.gold={0,0};++revision;}
   if(verb=="test-rich"){bank.gold={50000,20000};++revision;}
   if(verb=="test-album"){screen="album";++session;}
   if(verb=="test-shop"){screen="shop";++session;}
   if(verb=="reload"){CollectionSave loaded;if(!decodeCollection(encodeCollection(saved),loaded))return 2;saved=std::move(loaded);visit={};++session;}
  }
  output();
 }
}
