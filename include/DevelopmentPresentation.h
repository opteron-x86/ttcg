#pragma once
#include "Development.h"
#include "Presentation.h"
namespace ttcg {
inline std::string developmentJson(const DevelopmentSettings& d,const std::string& notice={}) {
 std::ostringstream o;o<<"{\"enabled\":"<<(d.enabled?"true":"false");
 if(d.enabled){
  for(auto [key,field]:DevelopmentSettings::switches)o<<','<<quote(key)<<':'<<(d.*field?1:0);
  o<<",\"TournamentHold\":"<<d.tournamentHold<<",\"Rules\":"<<d.rules<<",\"TradeRule\":"<<d.tradeRule
   <<",\"Wager\":"<<d.wager<<",\"OpponentSkill\":"<<d.opponentSkill<<",\"RuleLesson\":"<<d.ruleLesson<<",\"notice\":"<<quote(notice);
 }
 o<<'}';return o.str();
}
}
