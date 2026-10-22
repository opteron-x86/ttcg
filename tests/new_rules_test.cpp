#include "Lesson.h"
#include "Tournaments.h"
#include "RuleCulture.h"
#include "Development.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;

static void invariant(const Match& m){
 std::set<int> copies;for(int p=0;p<2;++p)for(int h=0;h<5;++h){assert(m.origins[p][h]>=0&&m.origins[p][h]<10);copies.insert(m.origins[p][h]);}
 assert(copies.size()==10&&m.score()[0]+m.score()[1]==10);
 for(const auto& cell:m.board)if(cell.card>=0)assert(copies.contains(cell.origin));
}
static Match drawnBoard(const Match& start){
 for(unsigned seed=1;seed<4000;++seed){auto m=start;std::mt19937 rng(seed);
  while(!m.boardComplete()){auto moves=m.legalMoves();assert(!moves.empty());assert(m.play(moves[rng()%moves.size()]).legal);}
  if(m.score()[0]==5)return m;
 }throw std::runtime_error("No drawn fixture");
}
struct Bank{
 std::array<Stock,2> stock;
 int count(int p,CardID id){return ttcg::count(stock[p],id);}
 int change(int p,CardID id,int n){stock[p][id]+=n;return n;}
};
int main(int argc,char**){
 if(argc>1){
  std::cout<<'{';bool first=true;
  auto emit=[&](const char* key,const Match& m,unsigned session,unsigned rev,bool settling=false,Result r={}){
   if(!first)std::cout<<',';first=false;std::cout<<quote(key)<<':'<<snapshotJson(m,session,rev,"Hulda",true,false,r,settling);
  };
  auto order=ruleExample(Order).match,chaos=ruleExample(Chaos).match;emit("order",order,110,0);emit("chaos",chaos,111,0);
  auto swap=ruleExample(Swap).match;swap.prepareDeal(swap.dealSeed);emit("swap",swap,112,0,true);
  auto sudden=ruleExample(SuddenDeath);emit("suddenBefore",sudden.match,113,0);
  auto move=sudden.expected();assert(sudden.advance(move.hand,move.square));emit("suddenDraw",sudden.match,113,1,true,sudden.last);
  assert(sudden.advance());emit("suddenRedeal",sudden.match,113,2,true);
  auto wall=ruleExample(SameWall);emit("wallBefore",wall.match,114,0);assert(wall.advance(0,0));emit("wallAfter",wall.match,114,1,true,wall.last);
  auto traded=drawnBoard(Match(16,Swap|SuddenDeath|Open));assert(traded.redeal());
  traded.foilHands[0][0]=true;while(!traded.finished()){if(!traded.redeal())traded.play(traded.legalMoves().front());}
  emit("trade",traded,115,0);
  std::cout<<"}\n";return 0;
 }
 for(unsigned mask=0;mask<2*(RuleMask+9);++mask){
  const bool expected=!(mask&~RuleMask)&&!((mask&Open)&&(mask&ThreeOpen))&&!((mask&Order)&&(mask&Chaos))&&!((mask&Legion)&&(mask&Decimation))&&!((mask&Ascension)&&(mask&Descension))&&(!(mask&SameWall)||(mask&Same));
  assert(validRules(mask)==expected);
 }
 unsigned combinations=0;
 for(unsigned flags=0;flags<=RuleMask;++flags)if(validRules(flags)){
  ++combinations;Match m(flags+19,flags);std::mt19937 rng(flags+3);
  while(!m.finished()){
   if(m.redeal())continue;
   auto moves=m.legalMoves();assert(!moves.empty());auto move=moves[rng()%moves.size()];
   auto fast=m;assert(fast.play(move,false).legal&&m.play(move).legal);
   for(int q=0;q<9;++q)assert(fast.board[q].owner==m.board[q].owner);
   invariant(m);
  }
  assert(m.redeals<=5);
 }
 assert(combinations==31104);
 // Both players obey forced selection. Rejected inputs never reroll Chaos.
 for(unsigned rule:{Order,Chaos})for(unsigned seed=1;seed<70;++seed){
  Match m(seed,rule|Legion|Affinity);unsigned seen[2]{};
  while(!m.finished()){
   const int p=m.turn,h=m.forcedHand;assert(h>=0&&!m.used[p][h]&&!(seen[p]&(1u<<h)));
   if(rule==Order)for(int earlier=0;earlier<h;++earlier)assert(m.used[p][earlier]);
   int square=0;while(m.board[square].card>=0)++square;
   auto rng=m.randomState;assert(!m.play({(h+1)%5,square}).legal&&m.randomState==rng);
   for(auto choice:m.legalMoves())assert(choice.hand==h);
   auto move=chooseMove(m,600,2);assert(move.hand==h);seen[p]|=1u<<h;
   assert(m.play(move).legal);invariant(m);
  }
 }
 // Search may know this turn's Chaos choice, never future choices from the RNG.
 for(unsigned seed=1;seed<30;++seed){Match a(seed,Open|Chaos|Swap|Same|FallenAce);a.turn=1;a.selectTurnCard();auto b=a;b.randomState^=0x12AB5678;
  auto x=chooseMove(a,12000,3),y=chooseMove(b,12000,3);assert(x.hand==y.hand&&x.square==y.square&&x.group==y.group);
  a.rules&=~Open;b=a;for(int h=0;h<5;++h)if(!a.known(0,h,1))b.hands[0][h]=(b.hands[0][h]+143)%cards.size();
  x=opponentMove(a,2,seed);y=opponentMove(b,2,seed);assert(x.hand==y.hand&&x.square==y.square&&x.group==y.group);
 }
 for(unsigned extra:std::array<unsigned,4>{0u,Reverse,Affinity|Legion,Reverse|Affinity|Decimation}){
  Match m(1,FallenAce|extra);const bool reverse=extra&Reverse;
  assert(m.beats(-1,12,1,10)==!reverse&&m.beats(12,-1,10,1)==reverse);
  assert(m.beats(2,10,1,9)==reverse); // A modified 9 is not a printed Ace.
  assert(m.beats(10,2,9,1)==!reverse);
 }
 auto ace=ruleExample(FallenAce);assert(ace.advance(0,0)&&ace.last.fallenAce&&ace.match.board[1].owner==0);
 auto wall=ruleExample(SameWall);assert(wall.advance(0,0)&&wall.last.same&&wall.last.sameWall);
 assert(wall.last.stages[0].rule==(Same|SameWall)&&wall.last.stages.back().rule==CaptureStage::Combo&&wall.match.board[2].owner==0);
 // Printed wall matches survive modifiers; Plus gets no wall contribution.
 auto modified=ruleExample(SameWall);modified.match.rules|=Affinity;modified.match.tiles[0]=1;
 assert(modified.advance(0,0)&&modified.last.sameWall);
 auto noWall=ruleExample(SameWall);noWall.match.rules=Same|Plus|Open;assert(noWall.advance(0,0)&&!noWall.last.same&&!noWall.last.plus);
 auto alone=ruleExample(SameWall).match;alone.board={};alone.used={};alone.placed=0;
 auto emptyResult=alone.play({0,0});assert(emptyResult.legal&&!emptyResult.sameWall&&!emptyResult.same);
 // Six drawn boards finish a single game; Swap runs only on the first deal.
 auto original=Lesson{}.match;
 auto sd=Match(original.hands[0],original.hands[1],23,Swap|SuddenDeath|Order|Affinity|Legion);
 const auto originalHands=original.hands;sd.foilHands[0][0]=sd.foilHands[1][2]=true;
 assert(sd.swapped[0]>=0&&sd.swapped[1]>=0);
 for(unsigned n=0;n<6;++n){
  sd=drawnBoard(sd);assert(sd.redeals==n);invariant(sd);
  for(int p=0;p<2;++p)for(int h=0;h<5;++h){auto origin=sd.origins[p][h];assert(sd.hands[p][h]==originalHands[origin/5][origin%5]);assert(sd.foil(p,h)==(origin==0||origin==7));}
  assert(sd.finished()==(n==5));assert(sd.legalMoves().empty());
  auto owners=sd.origins;std::array<int,10> controller{};for(int p=0;p<2;++p)for(auto origin:owners[p])controller[origin]=p;
  for(auto cell:sd.board)controller[cell.origin]=cell.owner;
  const auto previouslyPublic=sd.publicOrigins;
  if(n<5){assert(sd.redeal()&&!sd.finished()&&sd.placed==0&&sd.activeGroup==-1);assert(sd.swapped[0]==-1&&sd.swapped[1]==-1&&sd.forcedHand==0);
   for(int p=0;p<2;++p){int prior=-1;for(int h=0;h<5;++h){int origin=sd.origins[p][h];assert(controller[origin]==p);int order=(origin/5==p?0:5)+origin%5;assert(order>prior);prior=order;if(previouslyPublic&(1u<<origin))assert(sd.known(p,h,1-p));}}
  }else assert(!sd.redeal());
 }
 // All rule sets have support. Counts are weighted; selection is deterministic.
 std::mt19937 rng(77),duplicate(77);std::set<unsigned> sampled;std::array<unsigned,tournamentRuleCountWeights.size()> counts{};
 for(unsigned n=0;n<100000;++n){auto flags=randomTournamentRules(rng);assert(flags==randomTournamentRules(duplicate)&&validRules(flags));sampled.insert(flags);++counts[std::popcount(flags)];}
 for(unsigned n=0;n<10;++n)assert(counts[n]>0);assert(counts[2]>counts[8]*20&&counts[3]>counts[9]*20);
 assert(counts[2]>34000&&counts[2]<36000&&counts[3]>34000&&counts[3]<36000);
 for(unsigned n=4;n<10;++n)assert(counts[n]<counts[n-1]);
 unsigned all=0;bool hidden=false;for(auto flags:sampled){all|=flags;hidden|=!(flags&(Open|ThreeOpen));}assert(all==RuleMask&&hidden&&sampled.contains(0)&&sampled.size()>1000);
 // Exhaust the compatibility helpers, including dependent-rule removal.
 for(unsigned mask=0;mask<=RuleMask;++mask)if(validRules(mask))for(auto rule:ruleFlags){assert(validRules(withSpreadRule(mask,rule)));assert(validRules(withoutCultureRule(mask,rule)));}
 CollectionSave save;auto id=newTournament(save,1,100);assert(id);auto flags=tournament(save,id)->rules;CollectionSave loaded;
 save.culture.regions[1].added=Same|SameWall;save.culture.regions[1].removed=Order;
 assert(decodeCollection(encodeCollection(save),loaded));assert(tournament(loaded,id)->rules==flags&&loaded.culture.regions[1].added==(Same|SameWall));
 // Settle the original physical copies, even after exchanges, redeals and duplicates.
 Hand a{},b{};for(int h=0;h<5;++h){a[h]=cards[original.hands[0][h]].form;b[h]=cards[original.hands[1][h]].form;}
 a[1]=a[0]|foilFlag;b[2]=b[2]|foilFlag;
 unsigned redealGames=0,unplayedTransfers=0;
 for(unsigned seed=1;seed<=140;++seed){
  const auto flags=withSpreadRule(randomTournamentRules(rng),Swap)|SuddenDeath;
  Match m(indices(a),indices(b),seed,flags);for(int p=0;p<2;++p)for(int h=0;h<5;++h)m.foilHands[p][h]=isFoil((p?b:a)[h]);
  while(!m.finished()){if(m.redeal())continue;auto choices=m.legalMoves();assert(m.play(choices[rng()%choices.size()]).legal);invariant(m);}
  redealGames+=m.redeals>0;
  for(unsigned trade=TradeNone;trade<=TradeAll;++trade){
   Bank bank;for(auto id:a)++bank.stock[0][id];for(auto id:b)++bank.stock[1][id];bank.stock[0][0]=bank.stock[1][0]=50;const auto before=bank.stock;
   Contract c;assert(c.reserve(bank,{a,b},trade,5,123));assert(c.capture(m)&&!c.capture(m));
   auto owners=c.transfers(c.outcome,c.automaticChoice(c.outcome));auto expected=before;
   for(int p=0;p<2;++p)for(int h=0;h<5;++h){const auto origin=m.origins[p][h];assert(c.finalOwners[origin]==(m.used[p][h]?std::find_if(m.board.begin(),m.board.end(),[&](auto cell){return cell.origin==origin;})->owner:p));if(!m.used[p][h]&&p!=origin/5)++unplayedTransfers;}
   if(trade)for(int origin=0;origin<10;++origin){auto id=(origin<5?a:b)[origin%5];--expected[origin/5][id];++expected[owners[origin]][id];}
   if(c.outcome>=0){expected[c.outcome][0]+=5;expected[1-c.outcome][0]-=5;}
   assert(c.settleSelection(bank,c.outcome,c.automaticChoice(c.outcome)));
   for(int p=0;p<2;++p)for(auto [id,n]:expected[p])assert(bank.count(p,id)==n);
   auto settled=bank.stock;assert(c.settleSelection(bank,c.outcome,0)&&bank.stock==settled);
  }
 }
 assert(redealGames&&unplayedTransfers);
 for(unsigned rule:{Order,Chaos,FallenAce,SuddenDeath,SameWall,Swap}){auto demo=ruleExample(rule);while(!demo.complete()){auto move=demo.expected();assert(demo.match.turn==0?demo.advance(move.hand,move.square):demo.advance());}assert(!demo.advance());}
 std::cout<<"New rules: all 31104 valid combinations, forced choices, fair Chaos search, printed Fallen Ace, wall Combo, six-board draw limit, weighted tournaments, culture dependencies and 700 copy/foil-safe settlements passed\n";
}
