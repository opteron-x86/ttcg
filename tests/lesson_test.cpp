#include "Lesson.h"
#include <cassert>
#include <iostream>
int main(int argc,char**) {
 ttcg::Lesson lesson;
 constexpr std::array<unsigned,9> captures{0,0,1<<4,0,0,(1<<2)|(1<<4),0,1<<3,(1<<4)|(1<<6)};
 constexpr std::array<int,10> playerScore{5,5,5,6,6,6,4,4,3,5};
 if(argc>1)std::cout<<lesson.json()<<'\n';
 for(int step=0;step<9;++step){
   assert(lesson.match.rules==ttcg::Open&&lesson.match.turn==step%2);
   const auto before=lesson.json();
   assert(!lesson.advance(99,99));assert(lesson.json()==before);
   if(lesson.match.turn==0){assert(!lesson.advance());assert(lesson.json()==before);}
   else {const auto m=lesson.moves[step];assert(!lesson.advance(m.hand,m.square));assert(lesson.json()==before);}
   const auto m=lesson.moves[step];assert(step%2?lesson.advance():lesson.advance(m.hand,m.square));
   unsigned flipped=0;for(int i=0;i<9;++i)if(lesson.last.flipped[i])flipped|=1<<i;
   assert(flipped==captures[step]);
   const auto score=lesson.match.score();assert(score[0]==playerScore[step+1]&&score[0]+score[1]==10);
   if(argc>1)std::cout<<lesson.json()<<'\n';
 }
 // The words and highlighted ranks must stay true when the library changes.
 const auto& m=lesson.match;
 const auto side=[&](int q,int n){return ttcg::cards[m.board[q].card].sides[n];};
 assert(side(3,1)>side(4,3));
 assert(side(1,2)==side(4,0));
 assert(side(5,0)>side(2,2)&&side(5,3)>side(4,1));
 assert(side(8,0)<=side(5,2));
 assert(side(6,0)>side(3,2));
 for(bool plus:{false,true}){ttcg::Lesson combo(true,plus);assert(combo.advance(3,0));assert(combo.last.stages.size()==2);
  assert(combo.last.stages[0].rule==(plus?ttcg::Plus:ttcg::Same));
  assert(combo.last.stages[0].captures==10&&combo.last.stages[1].rule==ttcg::CaptureStage::Combo&&combo.last.stages[1].captures==4);}
 assert(m.finished()&&!lesson.advance()&&!lesson.advance(0,0));
 assert(m.used[1][4]==false&&m.score()[1]==5); // Four red cards on board plus one in hand.
 lesson=ttcg::Lesson{};assert(lesson.match.placed==0&&lesson.match.score()[0]==5);
 // The Affinity defence must depend on the bonus: 3 beats printed 2, but
 // ties the adjusted 3. The attack must not take the Kwama above it either.
 auto affinity=ttcg::Lesson::affinityLesson();auto move=affinity.expected();
 assert(affinity.advance(move.hand,move.square));
 assert(affinity.match.board[4].owner==0);
 auto plain=affinity.match;plain.rules&=~ttcg::Affinity;
 const auto attack=affinity.expected();
 assert(ttcg::cards[plain.hands[plain.turn][attack.hand]].sides[1]==3);
 assert(ttcg::cards[plain.board[4].card].sides[3]==2);
 plain.play(attack);assert(plain.board[4].owner==1&&plain.board[0].owner==0);
 assert(affinity.advance());assert(affinity.match.board[4].owner==0&&affinity.match.board[0].owner==0);
 auto demos=ttcg::allRuleDemosJson();assert(demos.find("\"hidden\":")==std::string::npos);assert(demos.find("\"same\":")!=std::string::npos&&demos.find("\"affinity\":")!=std::string::npos);
 if(argc==1)std::cout<<"Lesson: nine legal moves, guarded input, diagonal/equal comparisons, captures both ways, two-card captures, scoring and replay passed\n";
}
