#include "Collection.h"
#include "Match.h"
#include <cassert>
#include <iostream>
struct Bank {
 std::array<ttcg::Stock,2> stock{};
 int calls=0,failAt=-1,partialAt=-1;
 int count(int p,ttcg::CardID id) { return ttcg::count(stock[p],id); }
 int change(int p,ttcg::CardID id,int amount) {
   int call=calls++; if(call==failAt) return 0;
   if(call==partialAt) amount=amount<0?-1:1;
   if(amount<0) amount=-std::min(-amount,count(p,id));
   stock[p][id]+=amount; return amount;
 }
};
int main() {
 ttcg::Hand a{ttcg::cards[0].form,ttcg::cards[0].form,ttcg::cards[1].form,ttcg::cards[2].form,ttcg::cards[3].form},b{ttcg::cards[4].form,ttcg::cards[5].form,ttcg::cards[6].form,ttcg::cards[9].form,ttcg::cards[10].form};
 Bank initial;
 for(auto id:a) ++initial.stock[0][id]; for(auto id:b) ++initial.stock[1][id];
 initial.stock[0][0]=100; initial.stock[1][0]=50;
 auto same=[](const auto& x,const auto& y) { for(int p=0;p<2;++p) { for(auto [id,n]:x.stock[p]) assert(n==ttcg::count(y.stock[p],id)); for(auto [id,n]:y.stock[p]) assert(n==ttcg::count(x.stock[p],id)); } };
 assert(ttcg::validHand(a,initial.stock[0])); auto excessive=a; excessive[2]=ttcg::cards[0].form; assert(!ttcg::validHand(excessive,initial.stock[0]));
 auto invalid=a; invalid[0]=123; assert(!ttcg::validHand(invalid,initial.stock[0]));
 for(bool one:{false,true}) for(int winner:{-1,0,1}) for(int wager:{0,5,25}) {
   Bank bank=initial; ttcg::Contract c;
   assert(c.reserve(bank,{a,b},one,wager,123));
   assert(bank.count(0,0)==100-wager&&bank.count(1,0)==50-wager);
   assert(!c.reserve(bank,{a,b},one,wager,123));
   assert(c.settleSelection(bank,winner,winner>=0&&one?8u:0u)); assert(!c.pending());
   if(winner<0) same(bank,initial);
   else {
     assert(bank.count(winner,0)==initial.count(winner,0)+wager);
     assert(bank.count(1-winner,0)==initial.count(1-winner,0)-wager);
     auto card=winner==0?b[3]:a[3];
     assert(bank.count(winner,card)==initial.count(winner,card)+(one?1:0));
     assert(bank.count(1-winner,card)==initial.count(1-winner,card)-(one?1:0));
   }
   Bank settled=bank; assert(c.settleSelection(bank,winner,winner>=0&&one?8u:0u)); same(bank,settled);
   assert(ttcg::cardCount(bank.stock[0])+ttcg::cardCount(bank.stock[1])==10);
 }
 for(int failure=0;failure<11;++failure) {
   Bank bank=initial; bank.failAt=failure; ttcg::Contract c;
   assert(!c.reserve(bank,{a,b},true,25,123)); assert(!c.pending()); same(bank,initial);
 }
 {
   Bank bank=initial; bank.partialAt=1; ttcg::Contract c;
   assert(!c.reserve(bank,{a,b},true,25,123)); same(bank,initial);
 }
 {
   Bank bank=initial; bank.stock[1][0]=4; auto before=bank; ttcg::Contract c;
   assert(!c.reserve(bank,{a,b},true,5,123)); assert(!c.pending()); same(bank,before);
 }
 // Save with a held wager, load-order remapping of the opponent is native;
 // the portable record preserves permanent card IDs, copies and pending credits.
 for(int winner:{-2,-1,0,1}) {
   Bank bank=initial; ttcg::CollectionSave saved; saved.starter=true;  saved.deck=a;
   assert(saved.contract.reserve(bank,{a,b},true,25,123)); saved.contract.outcome=winner;
   auto bytes=ttcg::encodeCollection(saved); ttcg::CollectionSave loaded;
   assert(ttcg::decodeCollection(bytes,loaded)); assert(loaded.starter&&loaded.deck==a);
   assert(loaded.contract.hands[0]==a&&loaded.contract.outcome==winner);
   assert(loaded.contract.settleSelection(bank,winner== -2?-1:winner,winner>=0?1u:0u));
   if(winner<0) same(bank,initial);
   else assert(bank.count(winner,0)==initial.count(winner,0)+25);
   for(std::size_t n=0;n<bytes.size();++n) { auto shortRecord=bytes; shortRecord.resize(n); ttcg::CollectionSave untouched; untouched.activeDeck=999; assert(!ttcg::decodeCollection(shortRecord,untouched)); assert(untouched.activeDeck==999); }
 }
 {
   Bank bank=initial; ttcg::CollectionSave s; s.deck=a;
   assert(s.contract.reserve(bank,{a,b},true,25,123));
   bank.failAt=bank.calls+2; assert(!s.contract.settleSelection(bank,0,16u)); assert(s.contract.paying);
   ttcg::CollectionSave loaded; assert(ttcg::decodeCollection(ttcg::encodeCollection(s),loaded));
   assert(loaded.contract.flush(bank)); assert(!loaded.contract.pending());
   assert(bank.count(0,0)==125&&bank.count(1,0)==25&&bank.count(0,b[4])==1&&bank.count(1,b[4])==0);
   assert(ttcg::cardCount(bank.stock[0])+ttcg::cardCount(bank.stock[1])==10);
 }
 for(int seed=0;seed<40;++seed) {
   auto picked=ttcg::selectHand(initial.stock[0],seed); assert(ttcg::validHand(picked,initial.stock[0]));
   ttcg::Match match(ttcg::indices(a),ttcg::indices(b),seed,3);
   assert(match.hands[0][0]==match.hands[0][1]);
   while(!match.finished()) assert(match.play(ttcg::chooseMove(match)).legal);
   auto scores=match.score(); assert(scores[0]+scores[1]==10);
 }
 assert(ttcg::selectHand({},1)==ttcg::Hand{});
 std::cout<<"Collection: ownership, duplicate decks, gold/card conservation, draws, wins/losses, reserve rollback, partial settlement and save recovery passed\n";
}
