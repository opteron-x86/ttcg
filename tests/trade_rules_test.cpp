#include "Collection.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank {
 std::array<Stock,2> stock;int remaining=-1;
 int count(int p,CardID id){return ttcg::count(stock[p],id);}
 int change(int p,CardID id,int n){if(remaining==0)return 0;if(remaining>0)--remaining;stock[p][id]+=n;return n;}
};
int main(){
 Hand a{ttcg::cards[0].form,ttcg::cards[0].form,ttcg::cards[1].form,ttcg::cards[2].form,ttcg::cards[3].form},b{ttcg::cards[0].form,ttcg::cards[5].form,ttcg::cards[6].form,ttcg::cards[9].form,ttcg::cards[10].form};
 unsigned directDraws=0,swaps=0;
 for(unsigned flags:std::array<unsigned,7>{Same|Plus,ThreeOpen|Plus,Legion,Decimation,ThreeOpen|Legion|Same|Plus,ThreeOpen|Decimation|Same|Plus,Affinity|Same|Plus}){
 for(unsigned seed=0;seed<150;++seed){
   Match m(indices(a),indices(b),seed,flags);std::mt19937 rng(seed);
   while(!m.finished()){auto moves=m.legalMoves();assert(m.play(moves[rng()%moves.size()]).legal);}
   auto score=m.score();int winner=score[0]==score[1]?-1:score[0]>score[1]?0:1;
   for(unsigned rule=0;rule<=4;++rule){
     CollectionSave s;Bank bank;for(auto id:a)++bank.stock[0][id];for(auto id:b)++bank.stock[1][id];bank.stock[0][0]=bank.stock[1][0]=50;
     const auto before=bank.stock;auto& c=s.contract;
     assert(c.reserve(bank,{a,b},rule,5,123));assert(c.capture(m));assert(!c.capture(m));
     assert(c.outcome==winner&&c.finalScore==score);
     auto mask=c.automaticChoice(winner);auto owners=c.transfers(winner,mask);
     if(rule==TradeDiff)assert(c.required(winner)==(winner<0?0u:std::min(5u,static_cast<unsigned>(std::abs(score[0]-score[1])))));
     if(rule==TradeDirect){for(auto cell:m.board)assert(owners[cell.origin]==cell.owner);if(winner==-1)++directDraws;bool in=false,out=false;for(int i=0;i<10;++i){in|=i>=5&&owners[i]==0;out|=i<5&&owners[i]==1;}swaps+=in&&out;}
     assert(!c.settleSelection(bank,winner,32));if(c.required(winner)>0)assert(!c.settleSelection(bank,winner,0));
     CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(loaded.contract.finalOwners==c.finalOwners);
     bank.remaining=rule?2:0;assert(!loaded.contract.settleSelection(bank,winner,mask));assert(loaded.contract.paying);
     CollectionSave retry;assert(decodeCollection(encodeCollection(loaded),retry));bank.remaining=-1;assert(retry.contract.flush(bank));auto settled=bank.stock;assert(retry.contract.settleSelection(bank,winner,mask));assert(settled==bank.stock);
     std::array<Stock,2> expected=before;
     if(rule)for(int i=0;i<10;++i)if(owners[i]!=i/5){auto id=(i<5?a:b)[i%5];--expected[i/5][id];++expected[owners[i]][id];}
     if(winner>=0){expected[winner][0]+=5;expected[1-winner][0]-=5;}
     for(int p=0;p<2;++p)for(auto [id,n]:expected[p])assert(bank.count(p,id)==n);
     assert(cardCount(bank.stock[0])+cardCount(bank.stock[1])==10);
   }
 }
 }
 assert(directDraws&&swaps);
 for(unsigned rule=1;rule<=4;++rule){
   Bank bank;for(auto id:a)++bank.stock[0][id];for(auto id:b)++bank.stock[1][id];bank.stock[0][0]=bank.stock[1][0]=50;
   Contract c;assert(c.reserve(bank,{a,b},rule,5,123));c.forfeit();assert(c.settleSelection(bank,1,c.automaticChoice(1)));assert(cardCount(bank.stock[0])==(rule==TradeOne?4:0));
   Bank refund;for(auto id:a)++refund.stock[0][id];for(auto id:b)++refund.stock[1][id];Contract cancel;assert(cancel.reserve(refund,{a,b},rule,0,123));assert(cancel.settleSelection(refund,-1,0));assert(cardCount(refund.stock[0])==5&&cardCount(refund.stock[1])==5);
 }
 CollectionSave corrupt;corrupt.contract.trade=5;assert(!decodeCollection(encodeCollection(corrupt),corrupt));
 std::cout<<"Trade: 5250 full-match settlements across all implemented rule families, duplicate provenance, Direct draws and two-way transfers, Diff counts, forfeit, partial payout and v9 recovery passed\n";
}
