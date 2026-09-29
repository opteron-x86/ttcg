#pragma once
#include <array>
#include <bit>
#include <random>
#include <vector>
namespace ttcg {
enum Rule : unsigned {
 Basic=0, Same=1, Plus=2, Reverse=4, Open=16, Affinity=32, ThreeOpen=64, Legion=128, Decimation=256,
 Order=512, Chaos=1024, FallenAce=2048, SuddenDeath=4096, SameWall=8192, Swap=16384
};
inline constexpr std::array<unsigned,14> ruleFlags{Open,ThreeOpen,Same,SameWall,Plus,Reverse,FallenAce,Affinity,Legion,Decimation,Order,Chaos,Swap,SuddenDeath};
inline constexpr unsigned RuleMask=32759; // Bit 8 is the Combo presentation event.
inline bool validRules(unsigned r){
 return !(r&~RuleMask)&&!((r&Open)&&(r&ThreeOpen))&&!((r&Legion)&&(r&Decimation))&&
        !((r&Order)&&(r&Chaos))&&(!(r&SameWall)||(r&Same));
}
inline const char* ruleName(unsigned rule){
 switch(rule){case Open:return "Open";case ThreeOpen:return "Three Open";case Same:return "Same";case SameWall:return "Same Wall";
 case Plus:return "Plus";case Reverse:return "Reverse";case FallenAce:return "Fallen Ace";case Affinity:return "Affinity";
 case Legion:return "Legion";case Decimation:return "Decimation";case Order:return "Order";case Chaos:return "Chaos";
 case Swap:return "Swap";case SuddenDeath:return "Sudden Death";default:return "";}
}
inline constexpr std::array<unsigned,12> tournamentRuleCountWeights{1000,10000,35000,35000,11000,4500,2000,900,400,150,40,10};
inline unsigned randomTournamentRules(std::mt19937& rng,unsigned blocked=0){
 // Count weights sum to 100000: two/three rules occupy 70% of events, with a
 // strictly diminishing tail. Every valid set, including hidden/basic play,
 // has a nonzero chance; no rule or combination is reserved for a circuit.
 static const auto pools=[] {
  std::array<std::vector<unsigned>,12> sets;
  for(unsigned mask=0;mask<=RuleMask;++mask)if(validRules(mask))sets[std::popcount(mask)].push_back(mask);
  return sets;
 }();
 if(blocked){
  std::array<std::vector<unsigned>,12> allowed;unsigned total=0;
  for(unsigned n=0;n<pools.size();++n){for(auto mask:pools[n])if(!(mask&blocked))allowed[n].push_back(mask);if(!allowed[n].empty())total+=tournamentRuleCountWeights[n];}
  auto roll=rng()%total;unsigned n=0;
  for(;n<allowed.size();++n)if(!allowed[n].empty()){if(roll<tournamentRuleCountWeights[n])break;roll-=tournamentRuleCountWeights[n];}
  return allowed[n][rng()%allowed[n].size()];
 }
 auto roll=rng()%100000;unsigned count=0;
 while(roll>=tournamentRuleCountWeights[count])roll-=tournamentRuleCountWeights[count++];
 const auto& pool=pools[count];return pool[rng()%pool.size()];
}
}
