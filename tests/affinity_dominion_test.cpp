#include "Campaign.h"
#include "Lesson.h"
#include "SavedMatch.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ttcg;
int sample(int value,unsigned affinity,std::uint64_t groups){
 auto c=cards[0];c.sides.fill(value);c.affinityMask=affinity;c.form=expansionCardBase+cards.size()-baseCards.size();c.unique=false;
 cards.push_back(c);cardGroups.push_back(groups);return int(cards.size())-1;
}
Match table(unsigned rules,int card){std::array<int,5> hand;hand.fill(card);Match m(hand,hand,9,rules|Open);m.turn=0;m.tiles={};return m;}
int main(){
 const int nord=sample(5,1,1),orc=sample(5,0,2),khajiit=sample(5,2,4),hybrid=sample(5,3,3),low=sample(1,1,1),high=sample(10,1,1);
 for(unsigned rule:{Ascension,Descension}){
  auto m=table(rule,nord);int sign=rule==Ascension?1:-1;
  assert(m.affinityModifier(nord)==0&&m.play({0,0}).legal&&m.affinityModifier(nord)==0);
  auto second=m.play({0,8});assert(m.affinityModifier(nord)==sign&&second.stages.front().rule==rule);
  assert(m.modifier(orc,-1)==0&&m.modifier(khajiit,-1)==0&&m.modifier(hybrid,-1)==sign);
  assert(m.play({1,2}).legal&&m.modifier(nord,-1)==2*sign);
  assert(m.play({1,6}).legal&&m.modifier(nord,-1)==2*sign);
  m.board[4]={khajiit,1};m.board[1]={khajiit,0};m.board[3]={khajiit,1};
  assert(m.modifier(hybrid,-1)==2*sign); // Multiple affinities never stack.
  assert(m.modifier(low,-1)==2*sign&&m.modifier(high,-1)==2*sign);
  assert(m.rank(rule==Ascension?high:low,-1,0)==(rule==Ascension?12:-1));
  m.rules|=Affinity|Legion;m.activeGroup=0;m.tiles[0]=1;
  assert(m.modifier(nord,0)==2*sign+2); // Existing sources remain additive.
 }
 // Incoming card contributes before captures. In Reverse the sign reverses the advantage.
 for(unsigned flags:{unsigned(Ascension),unsigned(Descension|Reverse)}){
  auto m=table(flags,nord);m.board[0]={nord,1};m.board[5]={orc,1};m.placed=2;
  auto no=m;no.rules=Open|(flags&Reverse);assert(!no.play({0,4}).flipped[5]);
  assert(m.play({0,4}).flipped[5]);
 }
 auto d=table(Dominion,nord);d.board[4]={orc,1};d.board[3]={nord,1};d.placed=2;
 auto plain=d;plain.rules=Open;assert(!plain.play({0,5}).flipped[4]);
 auto result=d.play({0,5});assert(result.flipped[4]&&d.dominionModifier(orc,4)==-1&&result.stages.front().rule==Dominion);
 d.board[1]={nord,0};d.board[7]={nord,1};assert(d.dominionModifier(orc,4)==-1&&d.dominionModifier(hybrid,4)==0);
 assert(d.modifier(orc,-1)==0); // Adjacency never reaches into a hand.
 d.board={};d.board[4]={orc,0};d.board[3]={nord,1};d.board[1]={khajiit,1};assert(d.dominionModifier(orc,4)==0);
 d.board[0]={nord,1};assert(d.dominionModifier(orc,4)==0); // Diagonal does not count.
 // Modifier changes alone do not capture a distant existing card.
 auto stable=table(Ascension,nord);stable.board[0]={nord,1};stable.board[1]={orc,0};stable.placed=2;
 assert(stable.play({0,8}).legal&&stable.board[0].owner==1&&stable.board[1].owner==0);
 // Printed comparisons survive every combination of the new modifiers.
 for(unsigned flags:{unsigned(Ascension),unsigned(Descension),unsigned(Dominion),unsigned(Ascension|Dominion),unsigned(Descension|Dominion|Reverse)}){
  auto same=Lesson(true);same.match.rules|=flags;assert(same.advance(3,0)&&same.last.same);
  auto plus=Lesson(true,true);plus.match.rules|=flags;assert(plus.advance(3,0)&&plus.last.plus);
 }
 for(unsigned flags:{unsigned(Ascension|Dominion|Same|Swap),unsigned(Descension|Dominion|Reverse|Plus|SuddenDeath)}){
  SavedMatch s;s.present=true;s.opponent=123;s.ledger=456;s.table=Match(217,flags|Open);
  for(int n=0;n<5;++n){auto moves=s.table.legalMoves();assert(s.table.play(moves[n%moves.size()]).legal);}
  SavedMatch copy;assert(decodeMatch(encodeMatch(s),copy));assert(encodeMatch(s)==encodeMatch(copy));
  auto move=s.table.legalMoves().front();s.table.play(move);copy.table.play(move);assert(encodeMatch(s)==encodeMatch(copy));
 }
 // Deck selection recognizes Neutral/mixed hands on adverse rules and repeated affinities on favorable ones.
 Hand repeated{},neutral{};repeated.fill(cards[nord].form);neutral.fill(cards[orc].form);
 assert(handRuleScore(neutral,Descension)>handRuleScore(repeated,Descension));
 assert(handRuleScore(repeated,Ascension)>handRuleScore(neutral,Ascension));
 assert(handRuleScore(neutral,Ascension|Reverse)>handRuleScore(repeated,Ascension|Reverse));
 assert(handRuleScore(repeated,Descension|Reverse)>handRuleScore(neutral,Descension|Reverse));
 for(unsigned flags:{unsigned(Descension),unsigned(Ascension|Reverse),unsigned(Ascension),unsigned(Descension|Reverse)}){
  Stock stock;for(int i:{nord,orc,khajiit,hybrid,low,high})stock[cards[i].form]=2;
  auto hand=opponentHand(stock,-1,37,flags|Dominion|Same);assert(validHand(hand,stock));
 }
 // Interactive lessons execute the same production engine and show the intended thresholds.
 for(unsigned rule:{Ascension,Descension,Dominion}){
  auto demo=ruleExample(rule);int steps=0;
  while(!demo.complete()){auto m=demo.expected();assert(demo.match.turn==0?demo.advance(m.hand,m.square):demo.advance());++steps;}
  assert(steps==5&&demo.exampleStep+1==demo.captions.size());
  if(rule==Dominion)assert(demo.match.modifier(demo.match.board[4].card,4)==-1);
  else assert(demo.match.affinityModifier(demo.match.hands[0][0])==(rule==Ascension?2:-2));
 }
 // Search uses the same modifiers as animated play; every recorded placement
 // must produce the same ownership, score and effective values as its fast path.
 cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());
 const auto start=std::chrono::steady_clock::now();
 for(unsigned rules:{unsigned(Ascension|Same|Plus|Affinity),unsigned(Descension|Same|Plus|Affinity),unsigned(Dominion|Legion|Same|Plus),unsigned(Dominion|Descension|Reverse|Decimation),unsigned(Dominion|Ascension|Reverse|FallenAce),unsigned(Dominion|Ascension|Same|SameWall|Swap|ThreeOpen)})
  for(unsigned seed=0;seed<32;++seed){
   Match m(seed,rules);while(!m.finished()){
    const auto move=chooseMove(m,1000,2);auto fast=m;assert(fast.play(move,false).legal&&m.play(move).legal);
    assert(fast.score()==m.score());for(int i=0;i<9;++i){assert(fast.board[i].owner==m.board[i].owner);assert(fast.modifier(fast.board[i].card,i)==m.modifier(m.board[i].card,i));}
   }
  }
 std::cout<<"192 combined-rule games completed in "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
 std::cout<<"Affinity and Dominion: thresholds, ownership, incoming captures, mixed identities, additive/unbounded ranks, printed Same/Plus, saved matches, deck preferences and interactive examples passed\n";
}
