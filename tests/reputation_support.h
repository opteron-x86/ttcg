#pragma once
#include "FreePlay.h"
#include <cassert>
inline void earnReputation(ttcg::CollectionSave& s,unsigned hold,unsigned target=ttcg::Famous){
 using namespace ttcg;
 for(unsigned attempt=0;localReputation(s,hold).rank<target&&attempt<1000;++attempt){
  int chosen=-1;unsigned best=0;
  for(int i=1;i<int(opponents.size());++i){
   const auto& p=opponents[i];if(p.hold!=hold||!challengeReputationMet(s,i)||s.deadPlayers.contains(p.base))continue;
   const unsigned award=reputationAward(s,i);if(!award)continue;
   const bool fresh=!s.reputation.contains(p.base);const auto score=(fresh?1000u:0u)+award;
   if(score>best){best=score;chosen=i;}
  }
  assert(chosen>=0);recordCampaignResult(s,chosen,0,true,Open,true);
 }
 assert(localReputation(s,hold).rank>=target);
}

inline ttcg::Stock practiceStock(){ttcg::Stock stock;for(auto& c:ttcg::cards)if(c.starter)stock[c.form]=1;return stock;}
inline std::string reputationPoints(unsigned quarters){constexpr const char* fraction[]={"",".25",".5",".75"};return std::to_string(quarters/4)+fraction[quarters%4];}
