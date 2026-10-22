#include "Development.h"
#include "Lesson.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;
int main(int argc,char**argv){
 if(argc>1&&std::string_view(argv[1])=="--ui"){
   auto demo=Lesson::workshop(Decimation);demo.match.rules=(demo.match.rules&~Open)|ThreeOpen;
   std::cout<<snapshotJson(demo.match,50,0,"Thoring",true,false,{},false)<<'\n';
   auto r=demo.match.play({1,4});std::cout<<snapshotJson(demo.match,50,1,"Thoring",true,false,r,true)<<'\n';
   auto tie=Lesson::workshop(Legion).match;
   std::cout<<snapshotJson(tie,60,0,"Rustleif",true,false,{},false)<<'\n';return 0;
 }
 if(argc>1){unsigned r=std::stoul(argv[1]);auto d=Lesson::workshop(r);std::cout<<d.json()<<'\n';while(!d.complete()){auto m=d.expected();assert(d.match.turn==0?d.advance(m.hand,m.square):d.advance());std::cout<<d.json()<<'\n';}return 0;}
 for(unsigned r:std::array<unsigned,6>{Open|ThreeOpen,Legion|Decimation,Order|Chaos,SameWall,Ascension|Descension,1u<<18}){assert(!validRules(r));bool threw=false;try{Match m(1,r);}catch(const std::invalid_argument&){threw=true;}assert(threw);}
 // Visibility is symmetric, fixed by the deal and independent of the cards.
 for(unsigned seed=0;seed<100;++seed){Match m(seed,ThreeOpen);for(int p=0;p<2;++p){assert(std::popcount(m.reveals[p])==3);int n=0;for(int h=0;h<5;++h){assert(m.known(p,h,p));n+=m.known(p,h,1-p);}assert(n==3);}auto copy=m;assert(copy.reveals==m.reveals);
   m.turn=1;auto altered=m;for(int h=0;h<5;++h)if(!m.known(0,h,1))altered.hands[0][h]=(altered.hands[0][h]+199)%cards.size();
   auto a=opponentMove(m,2,seed),b=opponentMove(altered,2,seed);assert(a.hand==b.hand&&a.square==b.square&&a.group==b.group);
 }
 CollectionSave save,loaded;Match a(7,ThreeOpen);retainDealVisibility(save,a);auto mask=a.reveals;assert(decodeCollection(encodeCollection(save),loaded));Match b(9999,ThreeOpen);retainDealVisibility(loaded,b);assert(b.reveals==mask);
 auto bytes=encodeCollection(save);bytes.pop_back();assert(!decodeCollection(bytes,loaded));
 // All three group cards count, regardless of owner. Effects precede captures.
 for(unsigned rule:{Legion,Decimation,Affinity}){auto d=Lesson::workshop(rule);assert(d.match.activeGroup<0);assert(!d.advance(0,4));assert(d.advance(1,4));
   const int change=rule==Decimation?-1:1;assert(d.match.modifier(d.match.board[4].card,4)==change);
   if(rule!=Affinity){assert(d.last.stages.size()==1&&d.last.stages[0].rule==rule&&d.last.stages[0].captures==0);assert(d.match.groupModifier(d.match.hands[1][2])==change);assert(d.match.board[8].owner==1);}
   assert(d.advance());assert(d.match.board[4].owner==(rule==Decimation?1:0));assert(d.complete());
 }
 // Each consolidated card has one group, so a third matching card needs no choice.
 auto single=Lesson::workshop(Legion).match;auto choices=single.groupChoices({1,4});assert(choices.size()==1);
 auto wrong=single;assert(!wrong.play({1,4,(choices[0]+1)%int(creatureGroups.size())}).legal);
 assert(single.play({1,4}).legal&&single.activeGroup==choices[0]);
 for(auto mask:cardGroups)assert(std::popcount(mask)==1);
 // Hidden cards receive no identifying modifier in snapshots.
 auto d=Lesson::workshop(Legion);assert(d.advance(1,4));d.match.rules&=~Open;
 auto hidden=snapshotJson(d.match,1,1,"Opponent",true,false,d.last,true);
 for(int h=1;h<5;++h)assert(!d.match.known(1,h,0));
 assert(hidden.find("[1,0,0,0,0]]")!=std::string::npos);
 assert(hidden.find("\"activeGroup\":\"Beast\"")!=std::string::npos);
 // Printed ranks remain fixed, numeric modifiers may reach zero or eleven.
 Match bounds(2,Decimation);bounds.activeGroup=int(std::find(creatureGroups.begin(),creatureGroups.end(),"Beast")-creatureGroups.begin());int low=Lesson::card("legends-257893");assert(cards[low].sides[1]==1&&bounds.rank(low,4,1)==0);
 // Same/Plus read printed values, then Combo uses the group-adjusted values.
 for(unsigned rule:{Legion,Decimation})for(bool plus:{false,true}){
  auto m=Lesson(true,plus).match;m.rules|=rule;
  auto mask=cardGroups[m.hands[0][3]];m.activeGroup=std::countr_zero(mask);
  auto r=m.play({3,0});assert(r.legal&&(plus?r.plus:r.same));assert(r.flipped[1]&&r.flipped[3]);
 }
 // Reverse compares modified values too: reducing the Wolf's side to zero defends it.
 auto reversed=Lesson::workshop(Decimation);reversed.match.rules|=Reverse;assert(reversed.advance(1,4)&&reversed.advance());assert(reversed.match.board[4].owner==0);
 // INI validation and every implemented rule allows competitive terms; children still have no gold wagers.
 for(unsigned r:std::array<unsigned,4>{ThreeOpen,Legion,Decimation,ThreeOpen|Legion}){std::map<std::string,std::string> ini{{"Enabled","1"},{"Rules",std::to_string(r)},{"FriendlyGame","0"},{"TradeRule","4"},{"Wager","25"},{"RuleLesson","3"}};auto dev=DevelopmentSettings::load([&](const char*k){return ini[k];});assert(dev.rules==int(r)&&dev.ruleLesson==3&&dev.terms({0,1}).trade==4&&dev.fixedWager(false,true)==25);assert(dev.fixedWager(true,true)==0&&dev.maxWager(true,true,200)==0);}
 int same=0,plus=0,combo=0,triggers=0;
 for(unsigned rules:std::array<unsigned,8>{ThreeOpen,Legion,Decimation,ThreeOpen|Legion|Same|Plus,ThreeOpen|Decimation|Same|Plus|Reverse,Affinity|Same|Plus|Reverse,Open|Affinity|Legion|Same|Plus,Affinity|Decimation|Same|Plus|Reverse})for(unsigned seed=0;seed<100;++seed){Match m(seed+70,rules);int activated=-1,activations=0;while(!m.finished()){
   auto move=chooseMove(m,1000,2);auto fast=m;assert(fast.play(move,false).legal);auto r=m.play(move);assert(fast.activeGroup==m.activeGroup);for(int q=0;q<9;++q)assert(fast.board[q].owner==m.board[q].owner);assert(r.legal&&m.score()[0]+m.score()[1]==10);
   for(auto st:r.stages){same+=(st.rule&Same)!=0;plus+=(st.rule&Plus)!=0;combo+=st.rule==8;if(st.rule&(Legion|Decimation)){++activations;++triggers;}}
   if(activated>=0)assert(m.activeGroup==activated);activated=m.activeGroup;
 }assert(activations<=1);for(int p=0;p<2;++p)for(int h=0;h<5;++h)assert(m.known(p,h,1-p));}
 assert(same&&plus&&combo&&triggers);std::cout<<"Advanced rules: 800 matches, symmetric visibility/AI, fixed reveal saves, single-group activation, shared modifiers, capture ordering, lessons and competitive INI terms passed\n";
}
