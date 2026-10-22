#include "reputation_support.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank {
 std::array<Stock,2> stock;
 int count(int p,CardID id){return ttcg::count(stock[p],id);}
 int change(int p,CardID id,int n){stock[p][id]+=n;return n;}
};
int main(){
 const int ysolda=opponentIndex(0x13BAB),nazeem=opponentIndex(0x13BBF),kodlak=opponentIndex(0x1A68E);
 CollectionSave s;
 assert(unlockedOpponent(s,ysolda));
 assert(!challengeReputationMet(s,ysolda)&&challengeAcceptance(s,ysolda)==0);
 // Even a saved positive invitation cannot bypass the reputation requirement.
 s.invitations[opponents[ysolda].base]={24,true};
 assert(!willingToPlay(s,ysolda,1));
 assert(!challengeReputationMet(s,kodlak)&&!challengeReputationMet(s,nazeem));
 grantStarter(s);
 earnReputation(s,1);
 assert(localReputation(s,1).famous()&&challengeReputationMet(s,ysolda)&&willingToPlay(s,ysolda,1));
 assert(challengeAcceptance(s,nazeem)>0&&challengeAcceptance(s,kodlak)>0);
 const std::array<int,3> owners{ysolda,nazeem,kodlak};
 const std::array<CardID,3> signatures{1339365,1302469,2982026};
 const std::array<int,3> wagers{250,1000,500};
 for(int n=0;n<3;++n){
  const int owner=owners[n];const CardID signature=signatures[n];const auto& p=opponents[owner];
  assert(p.level==5&&p.skill==3&&std::string_view(p.skillName)=="Master"&&p.wager==wagers[n]);
  const auto& c=cards[cardIndex(signature)];assert(c.available&&c.unique&&c.tier==(n==2?9:8));
  assert(count(personalPool(owner),signature)==1);
  for(int i=0;i<int(opponents.size());++i)if(i!=owner)assert(!count(personalPool(i),signature));
  CollectionSave game;replenishOpponent(game,owner,p.reference,1);
  const auto stock=game.opponents.at(p.reference);CardID copy=count(stock,signature)?signature:signature|foilFlag;
  assert(count(stock,copy)==1);
  Hand dealt{};int slot=-1;bool absent=false;
  for(unsigned seed=0;seed<256;++seed){auto hand=opponentHand(stock,owner,seed);assert(validHand(hand,stock));
   auto it=std::find(hand.begin(),hand.end(),copy);if(it==hand.end())absent=true;else{dealt=hand;slot=it-hand.begin();}}
  assert(slot>=0&&absent); // Normal draw, not a guaranteed first-win prize.
  Bank bank;bank.stock[0]=practiceStock();bank.stock[1]=stock;
  Contract contract;assert(contract.reserve(bank,{selectHand(bank.stock[0],17),dealt},TradeOne,0,p.reference));
  assert(contract.settleSelection(bank,0,1u<<slot));
  assert(count(bank.stock[0],copy)==1&&count(bank.stock[1],copy)==0);
  game.player=bank.stock[0];game.opponents[p.reference]=bank.stock[1];
  CollectionSave loaded;assert(decodeCollection(encodeCollection(game),loaded));
  for(unsigned hour:{73u,145u,217u})replenishOpponent(loaded,owner,p.reference,hour);
  assert(!count(loaded.opponents.at(p.reference),signature)&&!count(loaded.opponents.at(p.reference),signature|foilFlag));
  for(unsigned seed=0;seed<32;++seed){auto hand=opponentHand(loaded.opponents.at(p.reference),owner,seed);for(auto id:hand)assert(baseCardID(id)!=signature);}
 }
 std::cout<<"Whiterun legends: Famous-only Master challenges, signature draws, normal trades and persistent loss passed\n";
}
