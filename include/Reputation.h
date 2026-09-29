#pragma once
#include "Collection.h"
#include "Opponents.h"
namespace ttcg {
enum ReputationRank : unsigned { Unknown,Unsung,Recognized,Respected,Famous,Legendary };
inline const char* reputationTitle(unsigned rank){constexpr const char* titles[]={"Unknown","Unsung","Recognized","Respected","Famous","Legendary"};return titles[std::min(rank,5u)];}
struct ReputationRequirements {
 unsigned recognizedPoints=14,respectedPoints=30,famousPoints=55;
 unsigned recognizedPlayers=6,respectedPlayers=10,famousPlayers=16;
 unsigned respectedStrong=3,famousStrong=6,famousExperts=2;
};
inline ReputationRequirements reputationRequirements(unsigned hold){
 // Deliberate regional targets, independent of roster additions and deaths.
 if(hold==1)return {};
 if(hold==6||hold==10)return {10,20,35,4,6,10,2,4,2};
 return {10,20,35,3,5,8,2,3,2};
}
inline constexpr std::array<unsigned,3> tournamentChampionshipPoints{6,12,20};
inline constexpr std::array<unsigned,3> tournamentPairingRenown{0,2,4};
inline constexpr std::array<unsigned,3> tournamentChampionshipRenown{0,8,20};
inline unsigned tournamentChampionships(const CollectionSave& s,unsigned circuit){
 unsigned total=0;for(const auto& r:s.tournamentReputation)total+=r.championships[circuit];return total;
}
inline unsigned widerRenown(const CollectionSave& s){
 unsigned total=0;for(const auto& r:s.tournamentReputation)total+=r.renownQuarters;
 for(const auto& p:opponents)if(p.traveller)if(auto it=s.reputation.find(p.base);it!=s.reputation.end()&&it->second.wins)total+=32;
 return total;
}
struct LocalReputation {
 unsigned wins=0,players=0,quarters=0,strongWins=0,expertWins=0,masterWins=0,mastersRequired=0,rank=Unknown;
 bool famous()const{return rank>=Famous;}
 const char* title()const{return reputationTitle(rank);}
};
inline LocalReputation localReputation(const CollectionSave& s,unsigned hold){
 LocalReputation r;
 if(!hold||hold>s.tournamentReputation.size())return r;
 const auto& tournament=s.tournamentReputation[hold-1];
 for(unsigned c=0;c<3;++c)r.quarters+=tournament.championships[c]*tournamentChampionshipPoints[c]*4;
 for(const auto& p:opponents){
  unsigned wins=0,quarters=0;
  if(p.hold==hold)if(auto it=s.reputation.find(p.base);it!=s.reputation.end()){wins=it->second.wins;quarters=it->second.quarters;}
  if(auto it=tournament.opponents.find(p.base);it!=tournament.opponents.end()){wins+=it->second.wins;quarters+=it->second.quarters;}
  const bool won=wins!=0;
  if(p.hold==hold&&p.level==5){r.mastersRequired+=won||!s.deadPlayers.contains(p.base);r.masterWins+=won;}
  if(!won)continue;
  ++r.players;r.wins+=wins;r.quarters+=quarters;r.strongWins+=p.level>=3;r.expertWins+=p.level>=4;
 }
 const auto t=reputationRequirements(hold);
 if(r.wins<3||r.players<2||r.quarters<24)return r;
 r.rank=Unsung;
 if(r.players<t.recognizedPlayers||r.quarters<t.recognizedPoints*4)return r;
 r.rank=Recognized;
 if(r.players<t.respectedPlayers||r.quarters<t.respectedPoints*4||r.strongWins<t.respectedStrong)return r;
 r.rank=Respected;
 if(r.players<t.famousPlayers||r.quarters<t.famousPoints*4||r.strongWins<t.famousStrong||r.expertWins<t.famousExperts)return r;
 r.rank=Famous;
 if(r.masterWins&&r.masterWins==r.mastersRequired)r.rank=Legendary;
 return r;
}
inline unsigned reputationFloor(unsigned rank){return rank>=Legendary?4:rank>=Famous?3:rank>=Respected?2:rank>=Recognized?1:0;}
inline unsigned requiredReputationRank(int i){return i>=0&&i<static_cast<int>(opponents.size())&&!opponents[i].traveller&&opponents[i].level>=3?opponents[i].level-1:Unknown;}
inline bool challengeReputationMet(const CollectionSave& s,int i){return i>=0&&i<static_cast<int>(opponents.size())&&localReputation(s,opponents[i].hold).rank>=requiredReputationRank(i);}
inline unsigned reputationWins(const CollectionSave& s,unsigned base){
 unsigned wins=0;
 if(auto it=s.reputation.find(base);it!=s.reputation.end())wins=it->second.wins;
 for(const auto& r:s.tournamentReputation)if(auto it=r.opponents.find(base);it!=r.opponents.end())wins+=it->second.wins;
 return wins;
}
inline unsigned reputationMultiplier(unsigned wins){return wins>=1000000?0:wins==0?4:wins==1?2:1;}
inline unsigned reputationAward(const CollectionSave& s,int i,unsigned hostHold=0){
 if(i<0||i>=static_cast<int>(opponents.size()))return 0;
 const auto& p=opponents[i];if(p.level<reputationFloor(localReputation(s,hostHold?hostHold:p.hold).rank))return 0;
 constexpr unsigned points[]={1,2,3,4,6,8};return points[std::min(p.level,5u)]*reputationMultiplier(reputationWins(s,p.base));
}
inline bool reputationStake(const Contract& c){return c.held&&c.completed&&!c.forfeited&&(c.trade!=TradeNone||c.wager>0);}
inline void recordReputationWin(CollectionSave& s,int i){
 const auto award=reputationAward(s,i);if(!award)return;
 auto& record=s.reputation[opponents[i].base];++record.wins;record.quarters+=award;
}
inline void recordTournamentReputation(CollectionSave& s,const Tournament& e,int i,bool championship){
 if(!e.hold||e.hold>s.tournamentReputation.size()||e.circuit>2||i<0||i>=int(opponents.size()))return;
 auto& r=s.tournamentReputation[e.hold-1];
 // Use the shared opponent history before writing this pairing's result.
 const auto award=reputationAward(s,i,e.hold);
 if(award){
  const auto renown=tournamentPairingRenown[e.circuit]*reputationMultiplier(reputationWins(s,opponents[i].base));
  r.renownQuarters=std::min(1000000u,r.renownQuarters+renown);
  auto& record=r.opponents[opponents[i].base];++record.wins;record.quarters+=award;
 }
 if(championship&&r.championships[e.circuit]<1000000){
  ++r.championships[e.circuit];r.renownQuarters=std::min(1000000u,r.renownQuarters+4*tournamentChampionshipRenown[e.circuit]);
 }
}
inline std::string reputationPoints(unsigned quarters){constexpr const char* fraction[]={"",".25",".5",".75"};return std::to_string(quarters/4)+fraction[quarters%4];}
inline std::string reputationObjective(const CollectionSave& s,unsigned hold){return localReputation(s,hold).title();}
}
