#include "Lesson.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ttcg;
int main() {
 Match m(4); assert(m.score()[0]==5&&m.score()[1]==5);
 auto before=m; assert(!m.play({-1,0}).legal); assert(!m.play({0,9}).legal); assert(m.placed==before.placed);
 assert(m.play({0,0}).legal); assert(!m.play({1,0}).legal);
 // The actual native resolver is exercised, including both starting players.
 for(unsigned flags:{0u,3u,4u,32u,35u,36u,48u,51u}) for(unsigned seed=0;seed<60;++seed) {
   Match game(seed,flags); std::mt19937 rng(seed);
   while(!game.finished()) {
     auto legal=game.legalMoves(); assert(!legal.empty());
     auto action=legal[rng()%legal.size()];
     const auto before=game; auto fast=game;
     auto result=game.play(action); auto unrecorded=fast.play(action,false);
     assert(result.legal&&unrecorded.stages.empty());
     assert(result.flipped==unrecorded.flipped&&result.same==unrecorded.same&&result.plus==unrecorded.plus);
     unsigned seen=0;
     for(const auto& stage:result.stages) {
       assert(stage.captures&&!(seen&stage.captures));
       if(stage.rule) assert((stage.highlights&stage.captures)==stage.captures);
       for(int i=0;i<9;++i) if(stage.captures&(1u<<i))
         assert(before.board[i].owner==1-before.turn&&game.board[i].owner==before.turn);
       seen|=stage.captures;
     }
     for(int i=0;i<9;++i) {
       assert(bool(seen&(1u<<i))==result.flipped[i]);
       assert(game.board[i].owner==fast.board[i].owner&&game.board[i].card==fast.board[i].card);
     }
     auto score=game.score(); assert(score[0]+score[1]==10);
     for(auto slot:game.board) assert(slot.owner>=-1&&slot.owner<=1);
   }
   assert(game.legalMoves().empty()); assert(!game.play({0,0}).legal);
 }
 // Same includes a friendly matching participant, but ordinary comparisons do not.
 auto same=Lesson(true).match;same.board[1].owner=0;
 auto basic=same;basic.rules=Open;
 auto result=same.play({3,0});assert(result.same&&result.flipped[3]);
 assert(result.stages[0].rule==Same&&result.stages[0].highlights==11);
 assert(!basic.play({3,0}).flipped[3]);
 // Plus starts a three-wave capture chain; each wave highlights its own source.
 auto waves=Lesson(true,true).match;waves.board[5]={Lesson::card("murkwater-goblin"),1};++waves.placed;
 auto cascade=waves.play({3,0});assert(cascade.stages.size()==3&&cascade.plus);
 assert(cascade.stages[0].rule==Plus&&cascade.stages[0].captures==10&&cascade.stages[0].highlights==11);
 assert(cascade.stages[1].rule==CaptureStage::Combo&&cascade.stages[1].captures==4&&cascade.stages[1].highlights==6);
 assert(cascade.stages[2].rule==CaptureStage::Combo&&cascade.stages[2].captures==32&&cascade.stages[2].highlights==36);
 // Ordinary capture never recurses. Use actual profiles satisfying both edges.
 int attacker=Lesson::card("legends-220827"),defender=Lesson::card("oldgate-warden"),bystander=Lesson::card("legends-299178");
 assert(cards[attacker].sides[0]>cards[defender].sides[2]&&cards[defender].sides[3]>cards[bystander].sides[1]);
 Match noCombo(1);noCombo.turn=0;noCombo.hands[0][0]=attacker;noCombo.board[1]={defender,1};noCombo.board[0]={bystander,1};noCombo.placed=2;
 auto ordinary=noCombo.play({0,4});assert(ordinary.flipped[1]&&!ordinary.flipped[0]);
 Match draw;draw.placed=9;for(int i=0;i<9;++i)draw.board[i]={i,i<5?0:1};
 draw.used[0].fill(true);draw.used[1].fill(true);draw.used[1][4]=false;assert(draw.finished()&&draw.score()[0]==5&&draw.score()[1]==5);
 auto reverse=Lesson::workshop(Reverse);assert(reverse.advance(1,4)&&reverse.advance());assert(reverse.match.board[4].owner==1);
 // Affinity marks 1–5 distinct squares. Attributes may repeat; no marker when off.
 Match affinity(17,Affinity),repeat(17,Affinity),unmarked(17);
 assert(affinity.tiles==repeat.tiles);
 unsigned countsSeen=0,attributesSeen=0;bool repeatedAttribute=false;
 for(unsigned seed=0;seed<1000;++seed) {
   Match layout(seed,Affinity),again(seed,Affinity),off(seed);
   Match dealt(layout.hands[0],layout.hands[1],seed,Affinity);
   assert(layout.tiles==again.tiles&&layout.tiles==dealt.tiles);
   assert(std::all_of(off.tiles.begin(),off.tiles.end(),[](auto t){return !t;}));
   int count=0;unsigned seen=0;
   for(unsigned tile:layout.tiles)if(tile) {
     assert(tile<=16&&(tile&(tile-1))==0);
     ++count;repeatedAttribute|=bool(seen&tile);seen|=tile;
   }
   assert(count>=1&&count<=5);countsSeen|=1u<<count;attributesSeen|=seen;
 }
 assert(countsSeen==62&&attributesSeen==31&&repeatedAttribute);
 assert(std::all_of(unmarked.tiles.begin(),unmarked.tiles.end(),[](auto t){return !t;}));
 const int attuned=Lesson::card("murkwater-goblin"),neutral=Lesson::card("legends-257893");
 affinity.tiles.fill(0);affinity.tiles[4]=cards[attuned].affinityMask;
 assert(affinity.modifier(attuned,4)==1&&affinity.modifier(neutral,4)==-1&&affinity.modifier(attuned,0)==0);
 int one=-1;for(int i=0;i<cards.size();++i)if(cards[i].sides[0]==1&&!(cards[i].affinityMask&affinity.tiles[4])){one=i;break;}
 assert(one>=0&&affinity.rank(one,4,0)==0);
 // Printed-rank Same remains valid when either tile changes effective numbers.
 auto elementalSame=Lesson(true).match;elementalSame.rules|=Affinity;elementalSame.tiles={};elementalSame.tiles[0]=1;
 assert(elementalSame.play({3,0}).same);
 // A matching tile protects an equal printed edge; a mismatch exposes it.
 for(unsigned rule:{Affinity,Legion,Decimation}){auto d=Lesson::workshop(rule);assert(d.advance(1,4)&&d.advance());assert(d.match.board[4].owner==(rule==Decimation?1:0));}
 // Every affinity bit is used by at least one finished card, and every printed side is valid.
 unsigned attributes=0;for(const auto& card:cards){assert(card.tier>=1&&card.tier<=10);for(int n:card.sides)assert(n>=1&&n<=10);if(!card.pending)attributes|=card.affinityMask;}
 assert(attributes==31&&creatureGroups.size()==18);
 auto start=std::chrono::steady_clock::now();
 for(unsigned seed=0;seed<4;++seed) {
   Match game(seed,seed%2?3:0);
   while(!game.finished()) { auto action=chooseMove(game); assert(game.play(action).legal); }
   assert(game.score()[0]+game.score()[1]==10);
 }
 std::cout<<"Native rules and AI: passed ("<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<" ms AI matches)\n";
}
