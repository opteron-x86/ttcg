#pragma once
#include "Presentation.h"

namespace ttcg {
// Demonstration cards are borrowed. This state never touches a collection or contract.
struct Lesson {
 static constexpr unsigned hiddenHands=1u<<31; // Lesson topic only; never a match rule.
 static int card(std::string_view id) {
   for(std::size_t i=0;i<cards.size();++i)if(cards[i].id==id)return static_cast<int>(i);
   throw std::invalid_argument("Missing lesson card");
 }
 Match match{{card("legends-270737"),card("legends-214319"),card("legends-220827"),card("legends-253882"),card("legends-291257")},
             {card("legends-209126"),card("legends-209346"),card("legends-299057"),card("legends-277252"),card("imperial-grunt")},1,Open};
 static constexpr std::array<Move,9> moves{{{0,0},{0,4},{1,3},{1,1},{2,2},{2,5},{3,8},{3,6},{4,7}}};
 bool sameLesson=false,plusLesson=false,affinityTour=false,riftTour=false;
 static constexpr std::array<Move,9> riftMoves{{{0,8},{0,1},{1,6},{1,3},{2,7},{2,2},{3,0},{3,4},{4,5}}};
 static constexpr std::array<Move,7> affinityMoves{{{0,0},{0,8},{1,4},{1,3},{2,2},{2,6},{3,1}}};
 unsigned workshopRule=0;
 std::vector<Move> exampleMoves;
 std::vector<std::string> captions;
 unsigned exampleStep=0;
 bool example() const{return !captions.empty();}
 static constexpr std::array<Move,4> groupMoves{{{0,0},{0,8},{1,4},{1,3}}};
 bool shortWorkshop() const{return !riftTour&&workshopRule&&(workshopRule!=Open&&workshopRule!=ThreeOpen&&workshopRule!=hiddenHands);}
 static Lesson workshop(unsigned rule){
   if(rule!=Open&&rule!=Reverse&&rule!=hiddenHands&&rule!=ThreeOpen&&rule!=Legion&&rule!=Decimation&&rule!=Affinity)throw std::invalid_argument("Unknown rule lesson");
   Lesson demo;demo.workshopRule=rule;
   if(rule==Open||rule==hiddenHands||rule==ThreeOpen){demo.match.rules=rule==hiddenHands?Basic:rule;demo.match.seedVisibility(73);return demo;}
   demo.match=Match({card("legends-257893"),card("legends-209123"),card("murkwater-goblin"),card("oldgate-warden"),card("frostbite-spider")},
     {card("frostbite-spider"),card(rule==Reverse?"legends-299178":"legends-214331"),card("legends-209123"),card("legends-299178"),card("dwarven-spider")},1,rule|Open);
   demo.match.turn=0;if(rule==Affinity){demo.match.tiles={};demo.match.tiles[4]=16;}
   for(int n=0;n<2;++n)demo.match.play(groupMoves[n]);return demo;
 }
 static Lesson riftLesson(){
   Lesson d(true,true);d.riftTour=true;d.sameLesson=d.plusLesson=false;d.workshopRule=ThreeOpen|Plus;
   d.match=Match(d.match.hands[0],d.match.hands[1],73,ThreeOpen|Plus);d.match.turn=0;
   d.match.reveals[0]=21; // The player's Plus card in slot 3 is hidden from the rival.
   return d;
 }
 static Lesson affinityLesson(){
   auto d=workshop(Affinity);d.affinityTour=true;
   int neutral=-1,attuned=-1;
   for(std::size_t i=0;i<cards.size();++i)if(!cards[i].pending&&cards[i].tier<=2){if(!cards[i].affinityMask&&neutral<0)neutral=i;if(cards[i].affinityMask&&attuned<0)attuned=i;}
   if(neutral<0||attuned<0)throw std::invalid_argument("Missing affinity lesson cards");
   d.match.hands[0][2]=neutral;d.match.hands[0][3]=attuned;
   d.match.tiles[2]=8;d.match.tiles[6]=8; // Neutral and Barded Guar mismatch Agility.
   const auto mask=cards[attuned].affinityMask;d.match.tiles[1]=mask&(~mask+1u);
   return d;
 }
 static constexpr std::array<Move,7> sameMoves{{{0,8},{0,1},{1,6},{1,3},{2,7},{2,2},{3,0}}};
 bool complete() const{return example()?exampleStep==exampleMoves.size():match.placed==(affinityTour?7:shortWorkshop()?4:sameLesson?7:9);}
 Move expected() const{return example()?exampleMoves[exampleStep]:riftTour?riftMoves[match.placed]:affinityTour?affinityMoves[match.placed]:shortWorkshop()?groupMoves[match.placed]:sameLesson?sameMoves[match.placed]:moves[match.placed];}
 Result last;
 std::vector<std::pair<int,int>> comparisons;
 explicit Lesson(bool same=false,bool plus=false):sameLesson(same||plus),plusLesson(plus){
   match.turn=0;if(!sameLesson)return;
   match=Match({card("legends-209436"),card("legends-223709"),card("legends-253882"),card("legends-234017"),card("legends-214331")},
               {card("legends-284352"),card("legends-209155"),card("legends-220453"),card("legends-245458"),card("legends-276452")},1,Same|Open);
   if(plusLesson)match=Match({card("thieves-guild-recruit"),card("legends-214287"),card("murkwater-goblin"),card("legends-234082"),card("legends-282540")},
     {card("legends-253909"),card("imperial-grunt"),card("legends-214138"),card("legends-289746"),card("legends-223299")},1,Plus|Open);
   match.turn=0;for(unsigned n=0;n<6;++n)match.play(sameMoves[n]);
 }
 bool advance(int hand=-1,int square=-1) {
   if(complete())return false;
   const auto expected=this->expected();
   if(example()&&expected.hand<0){
     if(expected.hand==-1&&!match.redeal())return false;
     if(expected.hand==-2)match.prepareDeal(match.dealSeed);
     last={};comparisons.clear();++exampleStep;return true;
   }
   if(match.turn==0?(hand!=expected.hand||square!=expected.square):(hand!=-1||square!=-1))return false;
   comparisons.clear();
   for(int side=0;side<4;++side){const int n=Match::neighbor(expected.square,side);
     if(n>=0&&match.board[n].card>=0&&match.board[n].owner!=match.turn){comparisons.emplace_back(expected.square,side);comparisons.emplace_back(n,(side+2)%4);}
   }
   last=match.play(expected);if(last.legal&&example())++exampleStep;return last.legal;
 }
 std::string json() const {
   const int step=example()?exampleStep:match.placed-(shortWorkshop()?2:sameLesson?6:0);const auto score=match.score();
   const auto next=!complete()?expected():Move{};
   const auto previous=step?(example()?exampleMoves[step-1]:riftTour?riftMoves[match.placed-1]:affinityTour?affinityMoves[match.placed-1]:shortWorkshop()?groupMoves[match.placed-1]:sameLesson?sameMoves[match.placed-1]:moves[step-1]):Move{};
   std::string out=std::format("{{\"step\":{},\"hand\":{},\"square\":{},\"lastHand\":{},\"lastSquare\":{},\"score\":[{},{}],\"hands\":[",step,!complete()&&match.turn==0?next.hand:-1,!complete()&&match.turn==0?next.square:-1,previous.hand,previous.square,score[0],score[1]);
   for(int p=0;p<2;++p){if(p)out+=',';out+='[';for(int h=0;h<5;++h){if(h)out+=',';out+=match.used[p][h]?"null":p==1&&!match.known(p,h,0)?"-2":std::to_string(match.hands[p][h]);}out+=']';}
   out+="],\"board\":[";
   for(int i=0;i<9;++i){if(i)out+=',';const auto b=match.board[i];out+=b.card<0?"null":std::format("{{\"card\":{},\"owner\":{},\"flip\":{}}}",b.card,b.owner,last.flipped[i]?"true":"false");}
   out+="],\"comparisons\":[";
   for(std::size_t i=0;i<comparisons.size();++i){if(i)out+=',';out+=std::format("[{},{}]",comparisons[i].first,comparisons[i].second);}
   out+="],\"caption\":"+quote(example()?captions[exampleStep]:"")+",\"swapped\":["+std::to_string(match.swapped[0])+","+std::to_string(match.swapped[1])+"],\"plusLesson\":"+std::string(plusLesson?"true":"false")+",\"sameLesson\":"+std::string(sameLesson?"true":"false")+",\"total\":"+std::to_string(example()?exampleMoves.size():affinityTour?5:shortWorkshop()?2:sameLesson?1:9)+",\"stages\":[";
   for(std::size_t i=0;i<last.stages.size();++i){if(i)out+=',';const auto& stage=last.stages[i];out+=std::format("{{\"rule\":{},\"highlights\":{},\"captures\":{}}}",stage.rule,stage.highlights,stage.captures);}
   out+="],\"riftTour\":"+std::string(riftTour?"true":"false")+",\"affinityTour\":"+std::string(affinityTour?"true":"false")+",\"workshopRule\":"+std::to_string(workshopRule)+",\"rules\":"+std::to_string(match.rules)+",\"activeGroup\":"+quote(match.activeGroup<0?"":creatureGroups[match.activeGroup])+",\"tiles\":[";
   for(int i=0;i<9;++i){if(i)out+=',';out+=std::to_string(match.tiles[i]);}out+="],\"modifiers\":[";
   for(int i=0;i<9;++i){if(i)out+=',';out+=std::to_string(match.modifier(match.board[i].card,i));}out+="],\"handModifiers\":[";
   for(int p=0;p<2;++p){if(p)out+=',';out+='[';for(int h=0;h<5;++h){if(h)out+=',';out+=std::to_string(p==1&&!match.known(p,h,0)?0:match.groupModifier(match.hands[p][h]));}out+=']';}
   return out+"]}";
 }
 std::string framesJson() const {
   Lesson d=*this;std::string out="[";bool first=true;
   auto emit=[&]{if(!first)out+=',';first=false;out+=d.json();};
   emit();
   while(!d.complete()){
     const auto move=d.expected();
     if(!(d.match.turn==0?d.advance(move.hand,move.square):d.advance()))break;
     emit();
   }
   return out+"]";
 }
};
inline Lesson ruleExample(unsigned rule){
 Lesson d;d.workshopRule=rule;
 d.match=Match(d.match.hands[0],d.match.hands[1],37,Open|(rule==SameWall?Same|SameWall:rule==Swap?Basic:rule));
 d.match.turn=0;d.match.selectTurnCard();
 if(rule==Order||rule==Chaos){
   auto preview=d.match;
   for(int square:{0,4,8,2}){
     const auto move=Move{preview.forcedHand,square};
     d.captions.push_back(rule==Order?(preview.turn?"Your opponent must also play in deck order.":"Play the first remaining card in your deck."):
       (preview.turn?"Chaos has chosen your opponent’s card. They choose its square.":"Chaos has chosen the highlighted card. You choose its square."));
     d.exampleMoves.push_back(move);preview.play(move);
   }
   d.captions.push_back(rule==Order?"Deck order determines the card. Placement is still your choice.":"A new card is chosen from the remaining hand at the start of each turn.");
 }else if(rule==Swap){
   d.match.rules|=Swap;d.exampleMoves={{-2,-2}};
   d.captions={"Before the first turn, one random card from each hand changes sides.","The highlighted cards have been swapped. Decks are unchanged; the trade rule settles ownership after the game."};
 }else if(rule==FallenAce||rule==SameWall){
   int attacker=-1,target=-1,chain=-1;
   for(int a=0;a<int(cards.size())&&attacker<0;++a)if(!cards[a].pending&&(rule==FallenAce?cards[a].sides[1]==1:cards[a].sides[0]==10))
     for(int b=0;b<int(cards.size())&&attacker<0;++b)if(!cards[b].pending&&b!=a&&cards[b].sides[3]==(rule==FallenAce?10:cards[a].sides[1])){
       if(rule==FallenAce){attacker=a;target=b;break;}
       for(int c=0;c<int(cards.size());++c)if(!cards[c].pending&&c!=a&&c!=b&&cards[b].sides[1]>cards[c].sides[3]){attacker=a;target=b;chain=c;break;}
     }
   if(attacker<0)throw std::invalid_argument("Missing rule example cards");
   d.match.hands[0][0]=attacker;d.match.hands[1][0]=target;
   if(rule==FallenAce){d.match.turn=1;d.match.play({0,1});}
   else{d.match.hands[1][1]=chain;for(auto move:std::array<Move,4>{{{1,8},{0,1},{2,6},{1,2}}})d.match.play(move);}
   d.exampleMoves={{0,0}};
   d.captions=rule==FallenAce?std::vector<std::string>{"Place the highlighted card. Its printed 1 faces an A.","Fallen Ace lets 1 capture A. With Reverse, A captures 1 instead."}:
     std::vector<std::string>{"The wall counts as A. Match it and the adjacent card to trigger Same.","Same Wall captured the matching card. That card then captured its neighbour through Combo."};
 }else if(rule==SuddenDeath){
   bool found=false;
   for(unsigned seed=1;seed<2000&&!found;++seed){
     Match preview(d.match.hands[0],d.match.hands[1],seed,Open|SuddenDeath);preview.turn=0;std::mt19937 rng(seed);
     for(int n=0;n<8;++n){auto choices=preview.legalMoves();preview.play(choices[rng()%choices.size()]);}
     for(auto move:preview.legalMoves()){auto end=preview;end.play(move);if(!end.suddenDeathPending())continue;
       d.match=preview;d.exampleMoves={move,{-1,-1}};found=true;break;
     }
   }
   if(!found)throw std::invalid_argument("Missing Sudden Death example");
   d.captions={"Play the last card to finish this board at five apiece.","A draw starts another board. Each player receives the five cards they control, including the unplayed card.","Play again with the redistributed hands. After five redeals, another draw ends the game."};
 }else throw std::invalid_argument("Unknown rule example");
 return d;
}
inline std::string allRuleDemosJson(){
 std::string out="{";
 auto add=[&](const char* key,Lesson d){if(out.size()>1)out+=',';out+=std::format("\"{}\":{}",key,d.framesJson());};
 add("play",Lesson{});
 add("open",Lesson::workshop(Open));
 add("threeopen",Lesson::workshop(ThreeOpen));
 add("hidden",Lesson::workshop(Lesson::hiddenHands));
 add("same",Lesson(true));
 add("plus",Lesson(true,true));
 add("reverse",Lesson::workshop(Reverse));
 add("affinity",Lesson::affinityLesson());
 add("legion",Lesson::workshop(Legion));
 add("decimation",Lesson::workshop(Decimation));
 for(const auto& [key,rule]:std::array<std::pair<const char*,unsigned>,6>{{{"order",Order},{"chaos",Chaos},{"fallenace",FallenAce},{"suddenDeath",SuddenDeath},{"samewall",SameWall},{"swap",Swap}}})add(key,ruleExample(rule));
 return out+"}";
}
}
