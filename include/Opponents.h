#pragma once
#include "Match.h"
#include <string_view>
namespace ttcg {
struct OpponentProfile {
 std::uint32_t base;
 const char* name;
 const char* location;
 const char* skillName;
 unsigned rules;
 int skill,wager;
 std::array<std::uint32_t,32> collection; // Permanent collection IDs; unused slots are 0.
 std::uint32_t reference,knownGlobal;
 const char* region;
 std::uint32_t alternateBase=0,alternateReference=0;
 std::uint32_t hold=1; // Permanent Whiterun hold ID.
 unsigned level=0;
 bool college=false,guild=false;
 const char* master="Skyrim.esm";
 bool approaches=true; // Only Masters use this policy.
 bool traveller=false;
 std::uint32_t signature=0,dialogueFaction=0;
};
#include "OpponentProfiles.inc"
inline int opponentIndex(std::uint32_t base) {
 for(std::size_t i=0;i<opponents.size();++i) if(opponents[i].base==base||(opponents[i].alternateBase&&opponents[i].alternateBase==base)) return static_cast<int>(i);
 return -1;
}
inline std::uint32_t opponentLedgerKey(std::uint32_t base,std::uint32_t reference) {
 const int i=opponentIndex(base);return i>=0?opponents[i].reference:reference;
}
inline Move opponentMove(const Match& actual,int skill,std::uint32_t seed) {
 auto moves=actual.legalMoves(); if(moves.empty())return {};
 std::mt19937 rng(seed);
 if(skill<=0) {
   // Beginners still favor a capture, with occasional unplanned placements.
   if(rng()%3==0)return moves[rng()%moves.size()];
 }
 auto belief=actual;
 if(!(actual.rules&Open)) {
   // Never inspect the player's unrevealed ranks. Build one plausible hand
   // from the public catalog; already-played cards remain known.
   for(int h=0;h<5;++h) if(!actual.known(0,h,1)) belief.hands[0][h]=static_cast<int>(rng()%cards.size());
 }
 return chooseMove(belief,skill<=0?1000:skill==1?5000:skill==2?20000:70000,skill<=0?1:skill==1?2:skill==2?4:7);
}
}
