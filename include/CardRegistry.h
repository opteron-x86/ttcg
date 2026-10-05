#pragma once
#include "Cards.h"
#include "CreatureGroups.h"
#include <deque>
#include <map>
#include <string>
#include <string_view>

namespace ttcg {
// Numeric IDs are private session handles. Saves bind them to qualified keys.
inline constexpr unsigned expansionCardBase=0x40000000u, missingCardBase=0x20000000u;
inline constexpr unsigned maximumExpansionCards=4096;
struct ExpansionCard {
 std::string key,name,art,thumbnail,rarity,groups,affinities,focus,setId,setName;
 std::array<int,4> sides{};
 int tier=1;
 unsigned affinityMask=0,merchants=0; // 1 regular, 2 specialist
 unsigned tournamentPrizes=0; // 1 Invitational, 2 Masters; opt-in only
 std::uint64_t groupMask=0;
 bool unique=false,standardPacks=false,tournamentPacks=false;
};
struct CardSet {std::string id,name;unsigned count=0;};
inline std::deque<ExpansionCard> expansionCards;
inline std::vector<CardSet> cardSets;
inline std::vector<std::string> expansionLoadErrors;
inline std::map<std::string,unsigned,std::less<>> expansionKeys;
inline bool validExpansionIdentifier(std::string_view value){
 if(value.empty()||value.size()>64||value.front()<'a'||value.front()>'z')return false;
 for(char c:value)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
 return true;
}
inline bool validExpansionKey(std::string_view value){
 auto colon=value.find(':');return colon!=value.npos&&validExpansionIdentifier(value.substr(0,colon))&&validExpansionIdentifier(value.substr(colon+1));
}
inline const ExpansionCard* expansionCard(const Card& card){
 const auto n=card.form-expansionCardBase;
 return n<expansionCards.size()?&expansionCards[n]:nullptr;
}
inline unsigned expansionCardID(std::string_view key){auto it=expansionKeys.find(key);return it==expansionKeys.end()?0:it->second;}
inline bool officialCard(const Card& card){return card.form<missingCardBase;}
inline bool standardPackCard(const Card& card){const auto* e=expansionCard(card);return !e||e->standardPacks;}
inline bool tournamentPackCard(const Card& card){const auto* e=expansionCard(card);return !e||e->tournamentPacks;}
// Called only during startup, before any save, match or AI worker is active.
inline void registerExpansion(std::string id,std::string name,std::vector<ExpansionCard> entries){
 cardSets.push_back({std::move(id),std::move(name),static_cast<unsigned>(entries.size())});
 for(auto& entry:entries){
  const unsigned numeric=expansionCardBase+static_cast<unsigned>(expansionCards.size());
  expansionCards.push_back(std::move(entry));const auto& e=expansionCards.back();
  expansionKeys.emplace(e.key,numeric);
  cards.push_back({e.name.c_str(),e.art.c_str(),e.sides,e.tier,e.focus.c_str(),e.key.c_str(),numeric,e.rarity.c_str(),e.groups.c_str(),"[]",e.affinities.c_str(),e.affinityMask,false,e.unique,true,false});
  cardGroups.push_back(e.groupMask);
 }
}
}
