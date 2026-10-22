#pragma once
#include <array>
#include <bit>
#include <random>
#include <vector>
namespace ttcg {
enum Rule : unsigned {
 Basic=0, Same=1, Plus=2, Reverse=4, Open=16, Affinity=32, ThreeOpen=64, Legion=128, Decimation=256,
 Order=512, Chaos=1024, FallenAce=2048, SuddenDeath=4096, SameWall=8192, Swap=16384,
 Ascension=32768, Descension=65536, Dominion=131072
};
// Append new flags: the order is also used by saved regional protection clocks.
inline constexpr std::array<unsigned,17> ruleFlags{Open,ThreeOpen,Same,SameWall,Plus,Reverse,FallenAce,Affinity,Legion,Decimation,Order,Chaos,Swap,SuddenDeath,Ascension,Descension,Dominion};
inline constexpr unsigned RuleMask=262135; // Bit 8 is the Combo presentation event.
inline bool validRules(unsigned r){
 return !(r&~RuleMask)&&!((r&Open)&&(r&ThreeOpen))&&!((r&Legion)&&(r&Decimation))&&
        !((r&Order)&&(r&Chaos))&&!((r&Ascension)&&(r&Descension))&&(!(r&SameWall)||(r&Same));
}
inline unsigned conflictingRules(unsigned rule){
 return rule==Open?ThreeOpen:rule==ThreeOpen?Open:rule==Legion?Decimation:rule==Decimation?Legion:rule==Order?Chaos:rule==Chaos?Order:rule==Ascension?Descension:rule==Descension?Ascension:0u;
}
inline unsigned withSpreadRule(unsigned rules,unsigned rule){return (rules&~conflictingRules(rule))|rule|(rule==SameWall?Same:0u);}
inline unsigned withoutCultureRule(unsigned rules,unsigned rule){return rules&~(rule|(rule==Same?SameWall:0u));}
inline const char* ruleName(unsigned rule){
 switch(rule){case Open:return "Open";case ThreeOpen:return "Three Open";case Same:return "Same";case SameWall:return "Same Wall";
 case Plus:return "Plus";case Reverse:return "Reverse";case FallenAce:return "Fallen Ace";case Affinity:return "Affinity";
 case Legion:return "Legion";case Decimation:return "Decimation";case Order:return "Order";case Chaos:return "Chaos";
 case Swap:return "Swap";case SuddenDeath:return "Sudden Death";
 case Ascension:return "Ascension";case Descension:return "Descension";case Dominion:return "Dominion";default:return "";}
}
inline constexpr std::array<unsigned,14> tournamentRuleCountWeights{1000,10000,35000,35000,11000,4500,2000,900,400,150,40,7,2,1};
inline unsigned randomTournamentRules(std::mt19937& rng,unsigned blocked=0){
 // Count weights sum to 100000: two/three rules occupy 70% of events, with a
 // strictly diminishing tail. Every valid set, including hidden/basic play,
 // has a nonzero chance; no rule or combination is reserved for a circuit.
 const auto build=[](unsigned excluded) {
  std::array<std::vector<unsigned>,tournamentRuleCountWeights.size()> sets;
  for(unsigned mask=0;mask<=RuleMask;++mask)if(!(mask&excluded)&&validRules(mask))sets[std::popcount(mask)].push_back(mask);
  return sets;
 };
 static const auto pools=build(0);
 // M'aiq's exclusion is queried on every availability refresh. Cache it too.
 static const auto noReverse=build(Reverse);
 const auto sample=[&](const auto& allowed){
  unsigned total=0;for(unsigned n=0;n<allowed.size();++n)if(!allowed[n].empty())total+=tournamentRuleCountWeights[n];
  auto roll=rng()%total;
  for(unsigned n=0;n<allowed.size();++n)if(!allowed[n].empty()){
   if(roll<tournamentRuleCountWeights[n])return allowed[n][rng()%allowed[n].size()];roll-=tournamentRuleCountWeights[n];
  }
  return 0u;
 };
 return blocked==0?sample(pools):blocked==Reverse?sample(noReverse):sample(build(blocked));
}
}
