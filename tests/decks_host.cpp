#include "Campaign.h"
#include "DeckPresentation.h"
#include <iostream>
#include <sstream>
using namespace ttcg;
int main(int argc,char**){
 const bool economy=argc>1;
 CollectionSave saved;grantStarter(saved);for(const auto& c:cards)saved.player[c.form]=2;
 if(economy)for(const auto& c:cards){saved.player[c.form|foilFlag]=1;saved.discovered[c.form|foilFlag]=1;}
 struct Gold {int amount=20000;int count(int,CardID){return amount;}int change(int,CardID,int n){amount+=n;return n;}} gold;
 std::vector<CardID> revealed;unsigned revealID=0;int packs=2;
 std::uint64_t session=1,revision=0;std::string screen="lobby";bool active=false;
 Match match(17,ThreeOpen|Same);
 auto output=[&]{
   auto out=snapshotJson(match,session,revision,"Kleppr",active,false,{},false,false,false);out.pop_back();
   out+=",\"screen\":"+quote(screen)+",\"matchContext\":true,\"collection\":{"+savedDeckFields(saved);
   out+=",\"deck\":[";for(int h=0;h<5;++h){if(h)out+=',';out+=std::to_string(displayIndex(saved.deck[h]));}out+="]";
   for(auto field:{"owned","known"}){out+=",\""+std::string(field)+"\":[";
     for(unsigned i=0;i<cards.size()*2;++i){if(i)out+=',';auto id=displayCardID(i);const auto f=std::string_view(field);int n=f=="owned"?count(saved.player,id):(count(saved.player,id)>0);out+=std::to_string(n);}out+="]";}
   out+=",\"fixedRules\":65,\"fixedTrade\":2,\"fixedWager\":0,\"maxWager\":10,\"gold\":100,\"rivalGold\":100,\"canStake\":true,\"canPlay\":true,\"packs\":[0,0],\"revealed\":[],\"revealID\":0}}";
   if(economy){
     auto replace=[&](std::string from,std::string to){auto pos=out.find(from);if(pos!=out.npos)out.replace(pos,from.size(),to);};
     replace("\"gold\":100,","\"gold\":"+std::to_string(gold.amount)+",");
     replace("\"packs\":[0,0]","\"packs\":["+std::to_string(packs)+",0]");
     std::string contents;for(auto id:revealed){if(!contents.empty())contents+=',';contents+=std::to_string(displayIndex(id));}
     replace("\"revealed\":[]","\"revealed\":["+contents+"]");replace("\"revealID\":0","\"revealID\":"+std::to_string(revealID));
   }
   std::cout<<out<<std::endl;
 };
 output();for(std::string line;std::getline(std::cin,line);){
   std::istringstream in(line);std::string verb;std::uint64_t s,r;in>>verb>>s>>r;
   if(s==session&&r==revision){
     if(verb=="deck-manage"&&!active){std::string action,name;unsigned id;in>>action>>id;std::getline(in>>std::ws,name);if(manageDeck(saved,action,id,name)){++session;revision=0;}}
     if(verb=="deck"&&!active){Hand deck{};for(auto& id:deck){int i=-1;in>>i;if(i>=0&&i<int(cards.size()*2))id=displayCardID(i);}if(validDeck(deck,saved.player)){saved.deck=deck;saveActiveDeck(saved);}}
     if(verb=="open-pack"&&!active&&packs){revealed=packCards(34);for(auto& id:revealed){id=mintCard(saved,id);++saved.player[id];}--packs;++revealID;}
     if(verb=="browse"&&!active){in>>screen;++session;}
     if(verb=="reload"){CollectionSave loaded;if(!decodeCollection(encodeCollection(saved),loaded))return 2;saved=std::move(loaded);++session;}
     if(verb=="start"&&!active){unsigned rules,trade,wager;in>>rules>>trade>>wager;Hand hand{};for(auto& id:hand){int i=-1;in>>i;if(i>=0&&i<int(cards.size()*2))id=displayCardID(i);}
       if(validHand(hand,saved.player)){saved.deck=hand;saveActiveDeck(saved);match=Match(indices(hand),indices(hand),17,rules);for(int p=0;p<2;++p)for(int h=0;h<5;++h)match.foilHands[p][h]=isFoil(hand[h]);active=true;++session;}}
   }output();
 }
}
