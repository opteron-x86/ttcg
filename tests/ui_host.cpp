#include "Presentation.h"
#include "Collection.h"
#include <iostream>
#include <sstream>
int main(int argc,char** argv) {
 if(argc>1&&std::string_view(argv[1])=="--multi-fixtures"){
  for(unsigned seed=1;seed<5000;++seed){ttcg::Match before(seed,ttcg::Open|ttcg::Same|ttcg::Plus);std::mt19937 rng(seed);
   while(!before.finished()){auto next=before;const auto moves=next.legalMoves();const auto result=next.play(moves[rng()%moves.size()]);
    if(result.stages.size()==1&&std::popcount(result.stages[0].captures)>=2){
     std::cout<<ttcg::snapshotJson(before,103,0,"Hulda",true,false,{},false,true,true)<<std::endl;
     std::cout<<ttcg::snapshotJson(next,103,1,"Hulda",true,false,result,true,true,true)<<std::endl;return 0;
    }before=next;
   }
  }throw std::runtime_error("No simultaneous capture fixture");
 }
 if(argc>1) {
   // Find a real Plus -> Combo -> Combo sequence in the current roster.
   // Fixed catalog indices stopped describing this capture after the overhaul.
   for(unsigned seed=1;seed<=100000;++seed){
     ttcg::Match fixture(seed,ttcg::Open|ttcg::Plus);fixture.turn=0;fixture.placed=7;
     fixture.used={{{false,false,true,true,true},{false,true,true,true,true}}};
     fixture.board[0]={fixture.hands[0][2],0,2};fixture.board[3]={fixture.hands[1][1],1,6};fixture.board[4]={fixture.hands[1][2],1,7};
     fixture.board[5]={fixture.hands[1][3],1,8};fixture.board[6]={fixture.hands[1][4],1,9};fixture.board[7]={fixture.hands[0][3],1,3};fixture.board[8]={fixture.hands[0][4],1,4};
     auto next=fixture;auto result=next.play({0,1});
     if(result.stages.size()!=3||result.stages[0].rule!=ttcg::Plus||result.stages[0].captures!=16||result.stages[0].highlights!=19||result.stages[1].captures!=8||result.stages[2].captures!=64)continue;
     std::cout<<ttcg::snapshotJson(fixture,102,0,"Hulda",true,false,{},false,true,true)<<std::endl;
     std::cout<<ttcg::snapshotJson(next,102,1,"Hulda",true,false,result,true,true,true)<<std::endl;return 0;
   }
   throw std::runtime_error("No two-wave Combo fixture in this roster");
 }
 struct Bank {
   std::array<ttcg::Stock,2> stock{};
   int count(int p,ttcg::CardID id) { return ttcg::count(stock[p],id); }
   int change(int p,ttcg::CardID id,int n) { if(n<0) n=-std::min(-n,count(p,id)); stock[p][id]+=n; return n; }
 } bank;
 for(int i=0;i<8;++i) bank.stock[0][ttcg::cards[i].form]=1;
 for(int i:{0,1,2,4,5,6,7,8,9,10,9,10}) ++bank.stock[1][ttcg::cards[i].form];
 bank.stock[0][0]=100; bank.stock[1][0]=50;
 ttcg::CollectionSave saved; saved.starter=true;
 saved.deck=ttcg::selectHand(bank.stock[0],17);
 auto rival=ttcg::selectHand(bank.stock[1],123);
 ttcg::Match match(17,ttcg::Open|ttcg::Same|ttcg::Plus); std::uint64_t session=1,revision=0; bool active=false,settling=false,musicEnabled=true;
 ttcg::Result lastResult;
 auto record=[&](ttcg::Result result) { if(result.legal) {
   ++revision; lastResult=std::move(result); settling=true;
   if(match.finished()&&saved.contract.held) { auto score=match.score(); saved.contract.outcome=score[0]==score[1]?-1:score[0]>score[1]?0:1; }
 } };
 auto output=[&] {
   auto data=ttcg::snapshotJson(match,session,revision,"Hulda",active,false,lastResult,settling,musicEnabled,true); data.pop_back();
   data+=",\"collection\":{\"owned\":[";
   for(std::size_t i=0;i<ttcg::cards.size();++i) { if(i) data+=','; data+=std::to_string(bank.count(0,ttcg::cards[i].form)); }
   data+="],\"deck\":["; for(int h=0;h<5;++h) { if(h) data+=','; data+=std::to_string(ttcg::cardIndex(saved.deck[h])); }
   data+="],\"rivalDeck\":["; for(int h=0;h<5;++h) { if(h) data+=','; data+=std::to_string(ttcg::cardIndex(rival[h])); }
   const auto& c=saved.contract;
   data+=std::format("],\"gold\":{},\"rivalGold\":{},\"canStake\":true,\"staked\":{},\"reward\":{},\"trade\":{},\"wager\":{},\"notice\":\"\",\"receipt\":\"\",\"prizes\":[",bank.count(0,0),bank.count(1,0),c.pending()?"true":"false",c.held&&c.rule()==ttcg::TradeOne&&c.outcome==0?"true":"false",c.rule(),c.wager);
   for(int h=0;h<5;++h) { if(h) data+=','; data+=std::to_string(ttcg::cardIndex(c.hands[1][h])); }
   data+=std::format("],\"fixedRules\":{},\"fixedTrade\":0,\"maxWager\":25,\"fixedWager\":-1",match.rules);
   std::cout<<data+"}}"<<std::endl;
 };
 output();
 for(std::string line;std::getline(std::cin,line);) {
   std::istringstream input(line); std::string verb; std::uint64_t s=0,r=0; input>>verb>>s>>r;
   if(s!=session||r!=revision) { output(); continue; }
   if(verb=="prepare"&&!settling&&match.finished()&&!saved.contract.pending()) {
     active=false; ++session; revision=0; lastResult={};
     rival=ttcg::selectHand(bank.stock[1],123);
     ttcg::retainOwnedDeck(saved.deck,bank.stock[0]);
   }
   if(verb=="start"&&!settling&&!active) {
     unsigned flags=0; int one=0,gold=0; input>>flags>>one>>gold;
     ttcg::Hand deck{}; for(auto& id:deck) { int index=-1; input>>index; if(index>=0&&index<static_cast<int>(ttcg::cards.size())) id=ttcg::cards[index].form; }
     if(ttcg::validHand(deck,bank.stock[0])&&(!(one||gold)||saved.contract.reserve(bank,{deck,rival},one!=0,gold,123))) {
       saved.deck=deck; match=ttcg::Match(ttcg::indices(deck),ttcg::indices(rival),17,flags); active=true; settling=true; ++session; revision=0; lastResult={};
     }
   }
   if(verb=="deck"&&!active) { ttcg::Hand deck{}; for(auto& id:deck) { int index=-1; input>>index; if(index>=0&&index<static_cast<int>(ttcg::cards.size())) id=ttcg::cards[index].form; } if(ttcg::validHand(deck,bank.stock[0])) saved.deck=deck; }
   if(verb=="play"&&active&&!settling&&match.turn==0) { ttcg::Move m; input>>m.hand>>m.square; record(match.play(m)); }
   if(verb=="music") { int enabled=0; input>>enabled; musicEnabled=enabled!=0; }
   if(verb=="settled") {
     settling=false; auto& c=saved.contract;
     if(match.redeal()){++revision;lastResult={};settling=true;}
     if(match.finished()&&c.held&&(c.rule()!=ttcg::TradeOne||c.outcome!=0)) c.settleSelection(bank,c.outcome,c.automaticChoice(c.outcome));
   }
   if(verb=="claim"&&!settling&&saved.contract.held&&saved.contract.outcome==0) { int h=-1; input>>h; saved.contract.settleSelection(bank,0,h>=0&&h<5?1u<<h:~0u); }
   if(verb=="forfeit"&&saved.contract.held&&!match.finished()) { saved.contract.settleSelection(bank,1,saved.contract.automaticChoice(1)); active=false; settling=false; ++session; revision=0; }
   if(verb=="close"&&!saved.contract.pending()) { active=false; settling=false; lastResult={}; ++session; revision=0; }
   if(active&&!settling&&!match.finished()&&match.turn==1) record(match.play(ttcg::chooseMove(match)));
   output();
 }
}
