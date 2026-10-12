#pragma once
#include "Collection.h"
#include <span>

namespace ttcg {
// One unfinished table per character. Physical copies are saved by card ID and
// origin, never by the catalog index, including Swap and Sudden Death deals.
struct SavedMatch {
 bool present=false,practice=false,competitive=false,culture=false,waiting=false;
 std::uint32_t opponent=0,ledger=0,waitPackage=0;
 unsigned tournament=0;
 Match table{};
 std::array<Hand,2> originals() const {
  std::array<Hand,2> result{};
  for(int p=0;p<2;++p)for(int h=0;h<5;++h){const auto origin=table.origins[p][h];
   result[origin/5][origin%5]=cards[table.hands[p][h]].form|(table.foil(p,h)?foilFlag:0);
  }
  return result;
 }
};
inline constexpr unsigned savedMatchFormat=1;
inline std::uint32_t matchDecisionSeed(const Match& table){return table.dealSeed^(table.redeals*2654435761u)^(unsigned(table.placed)*2246822519u)^table.randomState;}
inline std::vector<std::uint32_t> encodeMatch(const SavedMatch& s) {
 if(!s.present)return {};
 const auto& m=s.table;
 std::vector<std::uint32_t> out{s.opponent,s.ledger,s.tournament,s.waitPackage,
  unsigned(s.practice)|(unsigned(s.competitive)<<1)|(unsigned(s.culture)<<2)|(unsigned(s.waiting)<<3),
  m.rules,unsigned(m.turn),unsigned(m.placed),m.dealSeed,m.randomState,m.redeals,
  unsigned(m.forcedHand+1),unsigned(m.activeGroup+1),m.publicOrigins,m.reveals[0],m.reveals[1],unsigned(m.swapped[0]+1),unsigned(m.swapped[1]+1)};
 for(auto tile:m.tiles)out.push_back(tile);
 for(const auto& hand:s.originals())for(auto card:hand)out.push_back(card);
 for(int p=0;p<2;++p)for(int h=0;h<5;++h)out.push_back(unsigned(m.origins[p][h])|(unsigned(m.used[p][h])<<4));
 for(const auto& cell:m.board){out.push_back(unsigned(cell.origin+1));out.push_back(unsigned(cell.owner+1));}
 return out;
}
inline bool decodeMatch(std::span<const std::uint32_t> data,SavedMatch& destination) {
 if(data.size()!=65)return false;
 SavedMatch s;auto& m=s.table;unsigned at=0;auto get=[&](){return data[at++];};
 s.opponent=get();s.ledger=get();s.tournament=get();s.waitPackage=get();const auto flags=get();
 if(!s.opponent||flags>15||s.tournament>1000000)return false;
 s.practice=flags&1;s.competitive=flags&2;s.culture=flags&4;s.waiting=flags&8;
 if((s.practice&&(s.competitive||s.culture||s.tournament))||(s.tournament&&s.culture))return false;
 m.rules=get();m.turn=int(get());m.placed=int(get());m.dealSeed=get();m.randomState=get();m.redeals=get();
 const auto forced=get(),group=get();m.publicOrigins=get();m.reveals={get(),get()};const auto swap0=get(),swap1=get();
 if(!validRules(m.rules)||m.turn<0||m.turn>1||m.placed<0||m.placed>9||!m.randomState||m.redeals>5||
    forced>5||group>creatureGroups.size()||m.publicOrigins>1023||m.reveals[0]>31||m.reveals[1]>31||swap0>5||swap1>5)return false;
 m.forcedHand=int(forced)-1;m.activeGroup=int(group)-1;m.swapped={int(swap0)-1,int(swap1)-1};
 if((m.redeals&&!(m.rules&SuddenDeath))||(group&&!(m.rules&(Legion|Decimation)))||
    ((swap0||swap1)&&(!(m.rules&Swap)||m.redeals||!swap0||!swap1)))return false;
 for(auto& tile:m.tiles){tile=get();if(tile>16||(tile&&!std::has_single_bit(tile))||(tile&&!(m.rules&Affinity)))return false;}
 std::array<Hand,2> originals;
 for(int p=0;p<2;++p)for(int h=0;h<5;++h){auto id=get();if(cardIndex(id)<0)return false;originals[p][h]=id;m.foilHands[p][h]=isFoil(id);}
 std::array<bool,10> seen{},used{},onBoard{};unsigned usedCount=0;
 for(int p=0;p<2;++p)for(int h=0;h<5;++h){const auto v=get(),origin=v&15u;if(v>25||origin>=10||seen[origin])return false;
  seen[origin]=true;m.origins[p][h]=int(origin);m.used[p][h]=used[origin]=(v&16)!=0;usedCount+=used[origin];m.hands[p][h]=cardIndex(originals[origin/5][origin%5]);
 }
 unsigned placed=0;
 for(auto& cell:m.board){const auto origin=get(),owner=get();if(origin>10||owner>2||bool(origin)!=bool(owner))return false;
  cell={};if(!origin)continue;
  if(onBoard[origin-1]||!used[origin-1])return false;
  onBoard[origin-1]=true;++placed;cell={cardIndex(originals[(origin-1)/5][(origin-1)%5]),int(owner)-1,int(origin)-1};
 }
 if(placed!=unsigned(m.placed)||usedCount!=placed||onBoard!=used)return false;
 if(m.forcedHand>=0&&(!m.playableHand(m.forcedHand)||!(m.rules&(Order|Chaos))||m.boardComplete()))return false;
 if((m.rules&(Order|Chaos))&&!m.boardComplete()&&m.forcedHand<0)return false;
 const auto score=m.score();if(score[0]+score[1]!=10)return false;
 s.present=true;destination=std::move(s);return true;
}
// An unfinished staked table must own the exact escrowed physical copies.
inline bool matchContractValid(const SavedMatch& s,const Contract& c) {
 if(!s.present)return true;
 if(!c.pending())return s.table.finished()||s.practice||s.tournament||!c.opponent;
 if(s.tournament||s.practice||s.opponent!=c.opponent||s.originals()!=c.hands)return false;
 if(s.table.finished()){
  auto captured=c;captured.held=true;captured.completed=false;
  return c.completed&&!c.forfeited&&captured.capture(s.table)&&captured.outcome==c.outcome&&captured.finalOwners==c.finalOwners&&captured.finalScore==c.finalScore;
 }
 return c.held&&!c.completed&&!c.paying;
}
}
