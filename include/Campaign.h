#pragma once
#include "Album.h"
#include "Opponents.h"
#include "Reputation.h"
namespace ttcg {
// Stable profile IDs keep saves independent of roster order and actor replacement.
inline constexpr std::uint32_t whiterunHold=1,falkreathHold=2,paleHold=5,hjaalmarchHold=4,winterholdHold=7,riftHold=6,solstheimRegion=10,reachHold=9,eastmarchHold=8,haafingarHold=3;
struct HoldProfile {std::uint32_t id;const char* name;HoldTerms defaults;unsigned rewardTier;};
inline constexpr std::array<HoldProfile,10> holds{{
 {1,"Whiterun",{Open,1},2},
 {2,"Falkreath",{Open|Same,1},3},
 {3,"Haafingar",{Open|Same|Plus,TradeAll},4},
 {4,"Hjaalmarch",{Basic,TradeOne},3},
 {5,"The Pale",{Open|Plus,TradeDiff},4},
 {6,"The Rift",{ThreeOpen|Plus,TradeDirect},4},
 {7,"Winterhold",{Open|Affinity,TradeDirect},4},
 {8,"Eastmarch",{Open|Legion,TradeDiff},4},
 {9,"The Reach",{ThreeOpen|Same,TradeDiff},4},
 {10,"Solstheim",{ThreeOpen|Decimation,TradeOne},4},
}};
inline const HoldProfile* regionProfile(unsigned id){for(const auto& h:holds)if(h.id==id)return &h;return nullptr;}
inline const PlayerRecord& progress(const CollectionSave& s,int i) {
 static const PlayerRecord empty{};if(i<0||i>=static_cast<int>(opponents.size()))return empty;
 auto it=s.players.find(opponents[i].base);return it==s.players.end()?empty:it->second;
}
inline PlayerRecord& progress(CollectionSave& s,int i) {return s.players[opponents.at(i).base];}
inline void acceptOpponentChallenge(CollectionSave& s,int i){if(i>=0&&i<static_cast<int>(opponents.size()))progress(s,i).known=true;}
inline HoldTerms holdTerms(const CollectionSave& s,std::uint32_t id=whiterunHold) {
 if(auto it=s.holds.find(id);it!=s.holds.end())return it->second;
 for(auto p:holds)if(p.id==id)return p.defaults;return {};
}
inline HoldTerms opponentTerms(const CollectionSave& s,int i){
 if(i<0||i>=int(opponents.size()))return {};
 const auto& p=opponents[i];if(!p.traveller)return holdTerms(s,p.hold);
 // A saved game count keeps an encounter's terms stable through lobby visits,
 // album edits and reloads. Finishing or forfeiting advances the next draw.
 const auto& r=progress(s,i).competitive;
 std::mt19937 rng(p.base^((r.wins+r.losses+r.draws)*2654435761u)^0x4D414951u);
 return {randomTournamentRules(rng,Reverse),TradeOne};
}
inline bool encounterReady(const CollectionSave& s,int i,unsigned hour){
 if(i<0||i>=int(opponents.size()))return false;
 if(!opponents[i].traveller)return true;
 const auto it=s.masterApproachAfter.find(opponents[i].base);
 return it==s.masterApproachAfter.end()||hour>=it->second;
}
inline void finishTravellerEncounter(CollectionSave& s,int i,unsigned hour){
 if(i>=0&&i<int(opponents.size())&&opponents[i].traveller)s.masterApproachAfter[opponents[i].base]=hour+168;
}
inline bool expertOpponent(int i) { return i>=0&&i<static_cast<int>(opponents.size())&&opponents[i].level>=4; }
inline unsigned credit(const CollectionSave& s,std::uint32_t id) {auto it=s.achievements.find(id);return it==s.achievements.end()?0:it->second;}
inline unsigned starterWins(const CollectionSave& s){unsigned n=0;for(auto [id,flags]:s.achievements)if(flags&1)++n;return n;}
inline unsigned winsAtLeast(const CollectionSave& s,unsigned level){unsigned n=0;const auto mask=126u&~((1u<<(level+1))-1u);for(auto [id,flags]:s.achievements)if(flags&mask)++n;return n;}
inline unsigned distinctWins(const CollectionSave& s){return winsAtLeast(s,0);}
inline const char* holdName(std::uint32_t id){for(auto h:holds)if(h.id==id)return h.name;return "Elsewhere";}
inline unsigned localVictories(const CollectionSave& s,std::uint32_t hold,unsigned level,bool competitive=true){
 unsigned n=0;for(const auto& p:opponents)if(p.hold==hold){
   if(competitive){if(credit(s,p.base)&(126u&~((1u<<(level+1))-1u)))++n;}
   else if(auto it=s.localWins.find(p.base);it!=s.localWins.end()&&(it->second&(63u&~((1u<<level)-1u))))++n;
 }
 return n;
}
inline bool unlockedOpponent(const CollectionSave& s,int i) {
 if(i<0||i>=static_cast<int>(opponents.size())||progress(s,i).unavailable||s.accessBlocked.contains(opponents[i].base))return false;
 if(opponents[i].college&&!s.collegeMember)return false;
 if(opponents[i].guild&&!s.guildMember)return false;
 if(opponents[i].base==0x1334F&&s.brandSheiJailed)return false;
 if(opponents[i].base==0x2427D&&!s.erandurReady)return false;
 if(opponents[i].base==0x411BA&&!s.valdrRescued)return false;
 return true;
}
inline bool grantStarter(CollectionSave& s) {
 if(s.starter)return false;
 for(auto& c:cards)if(c.starter&&count(s.player,c.form)>=1000000)return false;
 for(auto& c:cards)if(c.starter){++s.player[c.form];s.discovered[c.form]=1;}
 if(std::all_of(s.deck.begin(),s.deck.end(),[](auto id){return id==0;})){
  unsigned slot=0;for(const auto& c:cards)if(c.starter&&slot<s.deck.size())s.deck[slot++]=c.form;
 }
 saveActiveDeck(s);
 s.starter=true;s.holds.try_emplace(whiterunHold,holds[0].defaults);return true;
}
inline constexpr int albumPrice=100;
template<class Bank> bool buyAlbum(CollectionSave& s,Bank& bank){
 if(s.starter||s.developmentCardsGranted||s.contract.pending()||s.goldCredit||bank.count(0,0)<albumPrice)return false;
 for(auto& c:cards)if(c.starter&&count(s.player,c.form)>=1000000)return false;
 const int paid=-bank.change(0,0,-albumPrice);
 if(paid==albumPrice&&grantStarter(s))return true;
 if(paid>0)s.goldCredit+=paid-bank.change(0,0,paid);
 return false;
}
inline bool claimAlbum(CollectionSave& s,bool hasItem) {
 return hasItem&&!s.starter&&!s.developmentCardsGranted&&!s.contract.pending()&&!s.goldCredit&&grantStarter(s);
}
inline Stock practiceStock() {Stock stock;for(auto& c:cards)if(c.starter)stock[c.form]=1;return stock;}
// Child requests vary by actor and day, without retaining game history.
inline unsigned childRequestedRule(const CollectionSave& s,std::uint32_t base,unsigned day,unsigned currentRules=0) {
 const auto seed=base+day;
 if(seed%3!=0)return 0;
 constexpr std::array<unsigned,4> choices{Same,Plus,Reverse,Open};
 for(unsigned step=0;step<choices.size();++step){const auto rule=choices[(seed+step)%choices.size()];if(validRules(currentRules|rule)&&!(currentRules&rule)&&!(s.encounteredRules&rule))return rule;}
 for(unsigned step=0;step<choices.size();++step){auto rule=choices[(seed+step)%choices.size()];if(validRules(currentRules|rule)&&!(currentRules&rule))return rule;}return 0;
}
struct ChildProfile {std::uint32_t base;const char* name;int skill;const char* group;};
// Practice collections are separate from the adult roster and tournament field.
inline constexpr std::array<ChildProfile,17> childProfiles{{
 {0x1347E,"Frodnar",0,"Beast"},{0x13477,"Dorthe",0,"Nord"},
 {0x132A9,"Svari",0,"Beastfolk"},{0x13363,"Hroar",0,"Beast"},
 {0x13294,"Knud",0,"Nord"},{0x13BAD,"Mila Valentia",0,"Imperial"},
 {0x13BAF,"Lars Battle-Born",1,"Nord"},{0x19C01,"Gralnach",1,"Orc"},
 {0x13B78,"Skuli",1,"Construct"},{0x19A2C,"Clinton Lylvieve",1,"Breton"},
 {0x1337A,"Samuel",1,"Khajiit"},{0x135E5,"Agni",1,"Spirit"},
 {0x13BA9,"Braith",2,"Redguard"},{0x1434D,"Nelkir",2,"Daedra"},
 {0x1434B,"Dagny",2,"High Elf"},{0x1329B,"Minette Vinius",2,"Imperial"},
 {0x136BA,"Sissel",2,"Spirit"}
}};
inline const ChildProfile& childProfile(std::uint32_t base){
 for(const auto& p:childProfiles)if(p.base==base)return p;
 static constexpr ChildProfile fallback{0,"",0,"Beast"};return fallback;
}
inline const char* childSkillName(std::uint32_t base){return std::array{"Beginner","Regular","Expert"}[childProfile(base).skill];}
inline std::pair<int,int> childTiers(std::uint32_t base){return std::array{std::pair{1,2},std::pair{3,4},std::pair{5,7}}[childProfile(base).skill];}
inline Stock childStock(std::uint32_t base) {
 const auto& profile=childProfile(base);const auto [low,high]=childTiers(base);
 std::vector<CardID> pool,themed,reverse;
 for(const auto& c:cards)if(c.available&&!c.pending&&!c.unique&&std::string_view(c.rarity)!="Legendary"){
  if(c.tier<=2)reverse.push_back(c.form);
  if(c.tier<low||c.tier>high||(profile.skill==0&&std::string_view(c.rarity)=="Epic"))continue;
  pool.push_back(c.form);
  if(std::string_view(c.groups).find(profile.group)!=std::string_view::npos)themed.push_back(c.form);
 }
 std::mt19937 rng(base);std::shuffle(pool.begin(),pool.end(),rng);std::shuffle(themed.begin(),themed.end(),rng);std::shuffle(reverse.begin(),reverse.end(),rng);
 Stock stock;for(unsigned n=0;n<std::min(std::size_t(12),themed.size());++n)stock[themed[n]]=1;
 for(auto id:pool){if(stock.size()>=24)break;stock[id]=1;}
 // Higher practice decks keep a small low-tier reserve for Reverse. It never
 // dilutes their ordinary hands and cannot be traded into the adult economy.
 if(low>2)for(unsigned n=0;n<std::min(std::size_t(6),reverse.size());++n)stock[reverse[n]]=1;
 return stock;
}
inline void recordCompetitiveGame(CollectionSave& s,int i,int winner,unsigned rules){
 if(i<0||i>=static_cast<int>(opponents.size())||winner< -1||winner>1)return;
 auto& r=progress(s,i).competitive;
 auto& value=winner==0?r.wins:winner==1?r.losses:r.draws;value=std::min(value+1,1000000u);
 progress(s,i).known=true;s.encounteredRules|=rules&RuleMask;
}
inline void recordCampaignResult(CollectionSave& s,int i,int winner,bool competitive,unsigned rules,std::uint32_t seed=17,bool reputationStake=false) {
 if(i<0||i>=static_cast<int>(opponents.size())||winner< -1||winner>1||!competitive||!reputationStake)return;
 recordCompetitiveGame(s,i,winner,rules);
 if(winner==0){
   if(competitive&&reputationStake)recordReputationWin(s,i);
   s.localWins[opponents[i].base]|=1u<<opponents[i].level;
   auto& flags=s.achievements[opponents[i].base];flags|=1;
   if(competitive)flags|=1u<<(opponents[i].level+1);
 }

}
inline Stock personalPool(int i) {
 Stock pool;if(i<0||i>=static_cast<int>(opponents.size()))return pool;
 for(auto id:opponents[i].collection)if(id)++pool[id];
 return pool;
}
inline Stock playableOpponentStock(const Stock& stock,int i) {
 Stock permitted;
 if(i>=0&&i<int(opponents.size())&&opponents[i].traveller){
  for(auto [id,n]:stock){const int c=cardIndex(id);if(n>0&&c>=0&&((cards[c].tier>=6&&cards[c].tier<=7)||baseCardID(id)==opponents[i].signature||(cards[c].unique&&!cards[c].available&&!cards[c].pending)))permitted[id]=n;}
  return permitted;
 }
 // Skill constrains starting stock, not ownership. Acquired cards must remain
 // playable so their previous owner can win them back, regardless of tier.
 for(auto [id,n]:stock)if(n>0&&cardIndex(id)>=0)permitted[id]=n;
 return permitted;
}
inline bool opponentHasCards(const CollectionSave& s,int i) {
 if(i<0||i>=static_cast<int>(opponents.size()))return false;
 const auto& p=opponents[i];const auto found=s.opponents.find(opponentLedgerKey(p.base,p.reference));
 // Unmet players receive their authored collection on their first game.
 return playableCount(playableOpponentStock(found==s.opponents.end()?personalPool(i):found->second,i))>=5;
}
inline const char* cardsPreventingPlay(const Stock& player,const Stock& rival,int i) {
 if(playableCount(playableOpponentStock(rival,i))<5)return "They don't have enough cards for a game.";
 if(playableCount(player)<5)return "You don't have enough cards for a game.";
 return "";
}
inline Hand opponentHand(const Stock& stock,int i,std::uint32_t seed,unsigned rules=0) {
 const auto permitted=playableOpponentStock(stock,i);
 if(i>=0&&i<int(opponents.size())&&opponents[i].traveller)return selectHand(permitted,seed);
 if(rules&Reverse){
  // Reverse uses the entire owned collection, including low cards outside a theme.
  std::vector<CardID> pool;
  for(auto [id,n]:permitted)for(int copy=0;copy<std::min(n,deckLimit(id));++copy)pool.push_back(id);
  std::mt19937 rng(seed);std::shuffle(pool.begin(),pool.end(),rng);
  std::stable_sort(pool.begin(),pool.end(),[](auto a,auto b){
   const auto& x=cards[cardIndex(a)];const auto& y=cards[cardIndex(b)];
   if((x.tier<=2)!=(y.tier<=2))return x.tier<=2;
   return std::accumulate(x.sides.begin(),x.sides.end(),0)<std::accumulate(y.sides.begin(),y.sides.end(),0);
  });
  Hand hand{};Stock used;unsigned at=0;for(auto id:pool)if(count(used,baseCardID(id))<deckLimit(id)){hand[at++]=id;++used[baseCardID(id)];if(at==5)return hand;}
  return {};
 }
 // Original cards and later winnings share the same draw pool immediately.
 return selectHand(permitted,seed);
}
inline Hand childHand(const Stock& stock,std::uint32_t base,std::uint32_t seed,unsigned rules){
 if(rules&Reverse)return opponentHand(stock,-1,seed,rules);
 const auto [low,high]=childTiers(base);Stock normal;
 for(auto [id,n]:stock){const int i=cardIndex(id);if(i>=0&&cards[i].tier>=low&&cards[i].tier<=high)normal[id]=n;}
 return opponentHand(normal,-1,seed,rules);
}
inline void replenishOpponent(CollectionSave& s,int i,std::uint32_t actor,unsigned hour) {
 if(i<0||i>=static_cast<int>(opponents.size())||!actor||s.contract.pending())return;
 auto [entry,fresh]=s.opponents.try_emplace(actor);auto& stock=entry->second;
 const auto pool=personalPool(i);const int target=cardCount(pool);
 if(fresh)for(auto [id,n]:pool){stock[id]=0;for(int copy=0;copy<n;++copy)++stock[mintCard(s,id)];}
 auto& next=progress(s,i).restockAt;if(!next){next=hour+72;return;}if(hour<next)return;
 next=hour+72;std::vector<CardID> basics;
 for(auto [id,n]:pool){const auto& c=cards[cardIndex(id)];if(!c.unique&&(std::string_view(c.rarity)=="Common"||std::string_view(c.rarity)=="Rare"))basics.push_back(id);}
 if(basics.size()<4&&!opponents[i].traveller)for(auto& c:cards)if(c.starter&&std::string_view(c.rarity)=="Common"&&std::find(basics.begin(),basics.end(),c.form)==basics.end())basics.push_back(c.form);
 // Replace up to five ordinary themed copies every three days; rare finishes
 // are minted once and high-rarity prizes are never automatically recreated.
 auto playable=[&]{return playableCount(playableOpponentStock(stock,i));};
 unsigned added=0;for(unsigned round=0;round<2&&playable()<target&&added<5;++round)for(unsigned n=0;n<basics.size()&&playable()<target&&added<5;++n){auto id=basics[(hour+n)%basics.size()];if(count(stock,id)+count(stock,id|foilFlag)<2){++stock[mintCard(s,id)];++added;}}
}
}
