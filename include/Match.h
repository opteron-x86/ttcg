#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>
#include <stdexcept>
#include "Cards.h"
#include "CreatureGroups.h"
#include "Rules.h"

namespace ttcg {
struct Move { int hand=-1, square=-1, group=-1; };
struct Slot { int card=-1, owner=-1, origin=-1; };
struct CaptureStage {
 // Rule bits name the initial special capture; Combo uses its own presentation bit.
 enum { Combo=8 };
 unsigned rule=0;
 std::uint16_t highlights=0, captures=0;
 int group=-1;
};
struct Result {
 bool legal=false; std::array<bool,9> flipped{}; bool same=false, plus=false, sameWall=false, fallenAce=false;
 std::vector<CaptureStage> stages;
};
struct Match {
 std::array<Slot,9> board{};
 std::array<std::array<int,5>,2> hands{};
 // Origin is a physical copy's original owner/slot, independent of its current hand.
 std::array<std::array<int,5>,2> origins{{{0,1,2,3,4},{5,6,7,8,9}}};
 std::array<std::array<bool,5>,2> used{};
 std::array<std::array<bool,5>,2> foilHands{}; // Original copies' finishes; indexed by origin.
 int turn=0, placed=0;
 unsigned rules=0;
 unsigned dealSeed=1,randomState=1,redeals=0,publicOrigins=0;
 int forcedHand=-1;
 std::array<int,2> swapped{-1,-1};
 int activeGroup=-1;
 std::array<unsigned,2> reveals{};
 void seedVisibility(std::uint32_t seed){
   std::mt19937 rng(seed^0x3A0FE1u);
   for(auto& mask:reveals){std::array<int,5> slots{0,1,2,3,4};std::shuffle(slots.begin(),slots.end(),rng);mask=(1u<<slots[0])|(1u<<slots[1])|(1u<<slots[2]);}
 }
 bool known(int owner,int hand,int observer) const {return owner==observer||origins[owner][hand]/5==observer||(publicOrigins&(1u<<origins[owner][hand]))||used[owner][hand]||finished()||((rules&Open)||((rules&ThreeOpen)&&(reveals[owner]&(1u<<hand))));}
 bool foil(int owner,int hand) const {const auto origin=origins[owner][hand];return foilHands[origin/5][origin%5];}
 unsigned random(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
 void selectTurnCard(){
  forcedHand=-1;if(!(rules&(Order|Chaos))||placed==9)return;
  std::array<int,5> choices{};unsigned count=0;for(int h=0;h<5;++h)if(!used[turn][h])choices[count++]=h;
  if(count)forcedHand=choices[rules&Chaos?random()%count:0];
 }
 bool playableHand(int hand) const {return hand>=0&&hand<5&&!used[turn][hand]&&(forcedHand<0||hand==forcedHand);}
 void prepareDeal(std::uint32_t seed){
  dealSeed=seed;randomState=(seed^0xC4A051u)|1u;
  if(rules&Swap){std::mt19937 rng(seed^0x5A4Fu);swapped={int(rng()%5),int(rng()%5)};
   std::swap(hands[0][swapped[0]],hands[1][swapped[1]]);std::swap(origins[0][swapped[0]],origins[1][swapped[1]]);
  }
  selectTurnCard();
 }
 int groupModifier(int card) const {return card>=0&&activeGroup>=0&&(cardGroups[card]&(1ull<<activeGroup))?(rules&Legion?1:rules&Decimation?-1:0):0;}
 std::vector<int> groupChoices(Move move) const {
   std::vector<int> choices;
   if(activeGroup>=0||!(rules&(Legion|Decimation))||!playableHand(move.hand)||move.square<0||move.square>=9||board[move.square].card>=0)return choices;
   const auto mask=cardGroups[hands[turn][move.hand]];
   for(unsigned g=0;g<creatureGroups.size();++g)if(mask&(1ull<<g)){
     int count=1;for(const auto& slot:board)if(slot.card>=0&&(cardGroups[slot.card]&(1ull<<g)))++count;
     if(count>=3)choices.push_back(g);
   }
   return choices;
 }
 // One to five marked squares, with independently chosen attributes; fixed per match.
 std::array<unsigned,9> tiles{};
 void seedTiles(std::uint32_t seed) {
   tiles={}; if(!(rules&Affinity))return;
   std::mt19937 rng(seed^0xA771F17u);
   const int count=std::uniform_int_distribution<int>(1,5)(rng);
   std::uniform_int_distribution<int> attribute(0,4);
   for(int i=0;i<count;++i)tiles[i]=1u<<attribute(rng);
   std::shuffle(tiles.begin(),tiles.end(),rng);
 }
 int modifier(int card,int square) const {
   if(!(rules&Affinity)||card<0||square<0||square>=9||!tiles[square])return groupModifier(card);
   return groupModifier(card)+(cards[card].affinityMask&tiles[square]?1:-1);
 }
 int rank(int card,int square,int side) const { return cards[card].sides[side]+modifier(card,square); }
 explicit Match(std::uint32_t seed=1, unsigned flags=0):rules(flags) {
   if(!validRules(flags))throw std::invalid_argument("Incompatible rules");
   seedTiles(seed);seedVisibility(seed);
   std::array<int,cards.size()> ids{}; for(std::size_t i=0;i<ids.size();++i) ids[i]=static_cast<int>(i);
   std::mt19937 rng(seed); std::shuffle(ids.begin(),ids.end(),rng);
   for(int p=0;p<2;++p) for(int h=0;h<5;++h) hands[p][h]=ids[p*5+h];
   turn=static_cast<int>(rng()%2);
   prepareDeal(seed);
 }
 Match(const std::array<int,5>& player,const std::array<int,5>& rival,std::uint32_t seed,unsigned flags=0):rules(flags) {
   if(!validRules(flags))throw std::invalid_argument("Incompatible rules");
   seedTiles(seed);seedVisibility(seed);
   for(const auto& hand:{player,rival}) for(int id:hand)
     if(id<0||id>=static_cast<int>(cards.size())) throw std::invalid_argument("Unknown card");
   hands={player,rival}; std::mt19937 rng(seed); turn=static_cast<int>(rng()%2);
   prepareDeal(seed);
 }
 static int neighbor(int pos,int side) {
   if(side==0) return pos>=3?pos-3:-1;
   if(side==1) return pos%3<2?pos+1:-1;
   if(side==2) return pos<6?pos+3:-1;
   return pos%3>0?pos-1:-1;
 }
 bool boardComplete() const {return placed==9;}
 bool suddenDeathPending() const {return boardComplete()&&(rules&SuddenDeath)&&redeals<5&&score()[0]==score()[1];}
 bool finished() const { return boardComplete()&&!suddenDeathPending(); }
 bool acePair(int a,int b) const {return (rules&FallenAce)&&((a==1&&b==10)||(a==10&&b==1));}
 bool beats(int a,int b,int printedA,int printedB) const {
  if(acePair(printedA,printedB))return rules&Reverse?printedA==10:printedA==1;
  return rules&Reverse ? a<b : a>b;
 }
 bool beats(int a,int b) const {return beats(a,b,a,b);}
 bool captures(int a,int square,int side,int b,int neighbor) const {
  return beats(rank(a,square,side),rank(b,neighbor,(side+2)%4),cards[a].sides[side],cards[b].sides[(side+2)%4]);
 }
 std::array<int,2> score() const {
   std::array<int,2> s{};
   for(auto b:board) if(b.owner>=0) ++s[b.owner];
   for(int p=0;p<2;++p) for(bool u:used[p]) if(!u) ++s[p];
   return s;
 }
 std::vector<Move> legalMoves() const {
   std::vector<Move> result;
   if(boardComplete()) return result;
   for(int h=0;h<5;++h) if(playableHand(h))
     for(int s=0;s<9;++s) if(board[s].card<0){auto groups=groupChoices({h,s});if(groups.size()>1)for(int g:groups)result.push_back({h,s,g});else result.push_back({h,s});}
   return result;
 }
 bool redeal(){
  if(!suddenDeathPending())return false;
  std::array<int,10> copyCards{},owners{};
  for(int p=0;p<2;++p)for(int h=0;h<5;++h){auto origin=origins[p][h];copyCards[origin]=hands[p][h];owners[origin]=p;if(known(p,h,1-p))publicOrigins|=1u<<origin;}
  for(const auto& slot:board)owners[slot.origin]=slot.owner;
  // Own originals in slot order, then acquired originals in their slot order.
  for(int p=0;p<2;++p){int at=0;for(int source:{p,1-p})for(int h=0;h<5;++h){const int origin=source*5+h;if(owners[origin]==p){hands[p][at]=copyCards[origin];origins[p][at++]=origin;}}}
  ++redeals;board={};used={};placed=0;activeGroup=-1;swapped={-1,-1};
  const auto seed=dealSeed+redeals*2654435761u;seedTiles(seed);seedVisibility(seed);
  turn=int(random()%2);selectTurnCard();return true;
 }
 Result play(Move m, bool recordStages=true) {
   Result result;
   if(boardComplete()||m.group< -1||m.group>=static_cast<int>(creatureGroups.size())||!playableHand(m.hand)||m.square<0||m.square>=9||board[m.square].card>=0) return result;
   const auto choices=groupChoices(m);
   if(choices.size()>1&&std::find(choices.begin(),choices.end(),m.group)==choices.end())return result;
   if(m.group>=0&&std::find(choices.begin(),choices.end(),m.group)==choices.end())return result;
   const int owner=turn, id=hands[owner][m.hand];
   if(!choices.empty()){
     activeGroup=choices.size()==1?choices[0]:m.group;
     if(recordStages){CaptureStage stage;stage.rule=rules&(Legion|Decimation);stage.group=activeGroup;stage.highlights=1u<<m.square;
       for(int i=0;i<9;++i)if(board[i].card>=0&&(cardGroups[board[i].card]&(1ull<<activeGroup)))stage.highlights|=1u<<i;
       result.stages.push_back(stage);
     }
   }
   result.legal=true;
   const auto before=board;
   std::array<bool,9> special{};
   std::array<int,4> neighbors{}, sums{}; neighbors.fill(-1); sums.fill(-1);
   std::array<bool,4> equal{};
   int equalCount=0;bool wallMatch=false;
   for(int side=0;side<4;++side) {
     int n=neighbor(m.square,side); neighbors[side]=n;
     if(n<0){if((rules&SameWall)&&cards[id].sides[side]==10){++equalCount;wallMatch=true;}continue;}
     if(before[n].card<0) continue;
     int a=cards[id].sides[side], b=cards[before[n].card].sides[(side+2)%4];
     sums[side]=a+b;
     equal[side]=a==b; if(equal[side]) ++equalCount;
     // Same/Plus use printed ranks. Basic captures use tile-adjusted ranks.
     if(before[n].owner!=owner&&captures(id,m.square,side,before[n].card,n)){result.flipped[n]=true;result.fallenAce|=acePair(a,b);}
   }
   for(int side=0;side<4;++side) {
     int n=neighbors[side]; if(n<0||before[n].card<0||before[n].owner==owner) continue;
     if((rules&Same)&&equalCount>=2&&equal[side]) { special[n]=true; result.same=true; }
     if((rules&Plus)&&sums[side]>=0) for(int other=0;other<4;++other)
       if(other!=side&&sums[other]==sums[side]) { special[n]=true; result.plus=true; }
   }
   result.sameWall=result.same&&wallMatch;
   board[m.square]={id,owner,origins[owner][m.hand]};publicOrigins|=1u<<origins[owner][m.hand];used[owner][m.hand]=true; ++placed;
   std::vector<int> queue;
   std::array<int,9> depth{};
   CaptureStage normal, specialStage;
   normal.rule=result.fallenAce?FallenAce:0u;
   if(result.fallenAce)normal.highlights=1u<<m.square;
   specialStage.rule=(result.same?Same:0u)|(result.sameWall?SameWall:0u)|(result.plus?Plus:0u);
   if(recordStages&&specialStage.rule) {
     specialStage.highlights=1u<<m.square;
     for(int side=0;side<4;++side) {
       int n=neighbors[side]; if(n<0||before[n].card<0) continue;
       bool participates=result.same&&equal[side];
       if(result.plus) for(int other=0;other<4;++other)
         if(other!=side&&sums[other]==sums[side]) participates=true;
       if(participates) specialStage.highlights|=1u<<n;
     }
   }
   for(int n=0;n<9;++n) {
     if(special[n]) { result.flipped[n]=true; queue.push_back(n); }
     if(result.flipped[n]) {
       board[n].owner=owner;
       if(recordStages) (special[n]?specialStage:normal).captures|=1u<<n;
     }
   }
   if(recordStages) {
     if(normal.captures) result.stages.push_back(normal);
     if(specialStage.captures) result.stages.push_back(specialStage);
   }
   const auto comboStart=result.stages.size();
   // Only Same/Plus captures seed Combo; basic captures never recurse.
   for(std::size_t i=0;i<queue.size();++i) for(int side=0;side<4;++side) {
     int n=neighbor(queue[i],side);
     if(n<0||board[n].card<0||board[n].owner==owner) continue;
     if(captures(board[queue[i]].card,queue[i],side,board[n].card,n)) {
       board[n].owner=owner; result.flipped[n]=true; queue.push_back(n);
       depth[n]=depth[queue[i]]+1;
       if(recordStages) {
         const auto stage=comboStart+depth[n]-1;
         if(result.stages.size()<=stage) result.stages.push_back({CaptureStage::Combo,0,0});
         result.stages[stage].captures|=1u<<n;
         result.stages[stage].highlights|=(1u<<queue[i])|(1u<<n);
       }
     }
   }
   turn=1-owner;
   selectTurnCard();
   return result;
 }
};
inline int evaluate(const Match& m) {
 auto s=m.score(); int value=(s[1]-s[0])*100;
 if(m.boardComplete()) return value*100;
 for(int p=0;p<9;++p) if(m.board[p].card>=0) for(int side=0;side<4;++side) {
   int n=Match::neighbor(p,side);
   if(n<0||m.board[n].card>=0) continue;
   int rank=m.rank(m.board[p].card,p,side);
   value+=(m.board[p].owner==1?1:-1)*(m.rules&Reverse?11-rank:rank);
 }
 return value;
}
inline int search(const Match& m,int depth,int alpha,int beta,int& budget);
inline int searchMoves(const Match& m,int depth,int alpha,int beta,int& budget) {
 int best=m.turn==1?-1000000:1000000;
 for(auto move:m.legalMoves()) {
   auto next=m; next.play(move,false); int value=search(next,depth-1,m.rules&Chaos?-1000000:alpha,m.rules&Chaos?1000000:beta,budget);
   if(m.turn==1) { best=std::max(best,value); alpha=std::max(alpha,best); }
   else { best=std::min(best,value); beta=std::min(beta,best); }
   if(beta<=alpha||budget<=0) break;
 }
 return best;
}
inline int search(const Match& m,int depth,int alpha,int beta,int& budget) {
 if(--budget<=0||depth==0||m.boardComplete()) return evaluate(m);
 if(!(m.rules&Chaos))return searchMoves(m,depth,alpha,beta,budget);
 // Future Chaos choices are chance nodes, not the actual game's RNG stream.
 // Only the current turn's selection is known at the root of chooseMove.
 int sum=0,count=0;
 for(int h=0;h<5;++h)if(!m.used[m.turn][h]){auto chance=m;chance.forcedHand=h;sum+=searchMoves(chance,depth,-1000000,1000000,budget);++count;if(budget<=0)break;}
 return count?sum/count:evaluate(m);
}
inline Move chooseMove(const Match& m,int nodeLimit=100000,int maxDepth=9) {
 auto moves=m.legalMoves(); if(moves.empty()) return {};
 Move completed=moves.front();
 // Iterative deepening with a node ceiling. Open hands, deterministic search.
 int budget=nodeLimit;
 for(int depth=1;depth<=std::min(maxDepth,9-m.placed);++depth) {
   int best=m.turn==1?-1000000:1000000; Move candidate=completed;
   for(auto move:moves) {
     auto next=m; next.play(move,false); int value=search(next,depth-1,-1000000,1000000,budget);
     if(budget<=0) return completed;
     if((m.turn==1&&value>best)||(m.turn==0&&value<best)) { best=value; candidate=move; }
   }
   completed=candidate;
 }
 return completed;
}
}
