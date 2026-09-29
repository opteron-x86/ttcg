#pragma once
#include "RuleCultureState.h"
#include "TournamentState.h"
#include "Match.h"
#include <bit>
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <numeric>
#include <random>
#include <string_view>
#include <string>
#include <vector>
namespace ttcg {
using CardID=std::uint32_t; // Virtual card identity, independent of catalog position.
using Hand=std::array<CardID,5>;
using Stock=std::map<CardID,int>;
inline constexpr CardID foilFlag=0x80000000u;
inline CardID baseCardID(CardID id){return id&~foilFlag;}
inline bool isFoil(CardID id){return (id&foilFlag)!=0;}
inline constexpr int maximumWager=200;
inline int cardIndex(CardID id) {
 id=baseCardID(id);
 for(std::size_t i=0;i<cards.size();++i) if(cards[i].form==id) return static_cast<int>(i);
 return -1;
}
// Finish belongs to a copy, never to the gameplay catalog or card strength.
inline CardID displayCardID(int i){return i<0||i>=int(cards.size()*2)?0:cards[i%cards.size()].form|(i>=int(cards.size())?foilFlag:0);}
inline int displayIndex(CardID id){const int i=cardIndex(id);return i<0?-1:i+(isFoil(id)?int(cards.size()):0);}
inline int finishMultiplier(CardID id){if(!isFoil(id))return 1;const auto rarity=std::string_view(cards[cardIndex(id)].rarity);return rarity=="Legendary"?20:rarity=="Epic"?12:8;}
inline int cardValue(CardID id){const int i=cardIndex(id);if(i<0)return 0;const auto& c=cards[i];const std::string_view rarity=c.rarity;const int factor=rarity=="Legendary"?8:rarity=="Epic"?4:rarity=="Rare"?2:1;return c.tier*8*factor*finishMultiplier(id);}
inline int prizeValue(CardID id){const int i=cardIndex(id);return i<0?-1:cardValue(id)*100+std::accumulate(cards[i].sides.begin(),cards[i].sides.end(),0);}
inline int count(const Stock& stock,CardID id) { auto i=stock.find(id); return i==stock.end()?0:i->second; }
inline int deckLimit(CardID id) {const int i=cardIndex(id);return i<0?0:cards[i].unique?1:2;}
inline bool validHand(const Hand& hand,const Stock& stock) {
 Stock need,identities; for(auto id:hand) { if(cardIndex(id)<0) return false; ++need[id]; if(++identities[baseCardID(id)]>deckLimit(id))return false; }
 for(auto [id,n]:need) if(count(stock,id)<n||n>deckLimit(id)) return false;
 return true;
}
inline bool validDeck(const Hand& hand,const Stock& stock) {
 Stock need,identities; for(auto id:hand) { if(!id) continue; if(cardIndex(id)<0) return false; ++need[id]; if(++identities[baseCardID(id)]>deckLimit(id))return false; }
 for(auto [id,n]:need) if(count(stock,id)<n||n>deckLimit(id)) return false;
 return true;
}
// Preserve slot order, retaining the earliest owned copies of duplicate cards.
inline void retainOwnedDeck(Hand& deck,const Stock& stock) {
 Stock used,identities;
 for(auto& id:deck)if(id){if(cardIndex(id)<0||count(used,id)>=count(stock,id)||count(identities,baseCardID(id))>=deckLimit(id)){id=0;continue;}++used[id];++identities[baseCardID(id)];}
}
inline int cardCount(const Stock& stock) {
 int n=0; for(auto [id,amount]:stock) if(cardIndex(id)>=0) n+=amount; return n;
}
inline int playableCount(const Stock& stock) {
 Stock identities;for(auto [id,amount]:stock)if(cardIndex(id)>=0)identities[baseCardID(id)]+=amount;int n=0;for(auto [id,amount]:identities)n+=std::min(amount,deckLimit(id));return n;
}
inline Hand selectHand(const Stock& stock,std::uint32_t seed) {
 std::vector<CardID> pool;
 for(auto [id,n]:stock) if(cardIndex(id)>=0) for(int i=0;i<std::min(n,deckLimit(id));++i) pool.push_back(id);
 Hand result{}; if(pool.size()<5) return result;
 std::mt19937 rng(seed); std::shuffle(pool.begin(),pool.end(),rng);
 Stock used;unsigned at=0;for(auto id:pool)if(count(used,baseCardID(id))<deckLimit(id)){++used[baseCardID(id)];result[at++]=id;if(at==5)return result;}return Hand{};
}
inline std::array<int,5> indices(const Hand& hand) {
 std::array<int,5> result{}; for(int i=0;i<5;++i) result[i]=cardIndex(hand[i]); return result;
}
inline int prizeChoice(const Hand& hand) {
 int best=0,value=-1;
 for(int h=0;h<5;++h) {
   int v=prizeValue(hand[h]);
   if(v>value) { value=v; best=h; }
 }
 return best;
}
enum TradeRule : unsigned { TradeNone=0, TradeOne=1, TradeDiff=2, TradeDirect=3, TradeAll=4 };
inline const char* tradeName(unsigned trade) {constexpr const char* names[]={"None","One","Diff","Direct","All"};return trade<=TradeAll?names[trade]:"None";}
struct Contract {
 bool held=false, paying=false;
 int wager=0, outcome=-2;
 std::uint32_t opponent=0;
 std::array<Hand,2> hands{};
 std::array<Stock,2> escrow{}, credits{};
 unsigned trade=0,selected=0;
 bool completed=false,forfeited=false;
 std::array<int,2> finalScore{5,5};
 std::array<int,10> finalOwners{0,0,0,0,0,1,1,1,1,1};
 unsigned rule() const {return trade;}
 bool pending() const { return held||paying; }
 bool capture(const Match& match) {
   if(!held||completed||!match.finished())return false;
   std::array<bool,10> seen{},origins{},played{};
   auto owners=finalOwners;
   for(int p=0;p<2;++p)for(int h=0;h<5;++h){
     const auto origin=match.origins[p][h];
     if(origin<0||origin>=10||origins[origin]||match.hands[p][h]!=cardIndex(hands[origin/5][origin%5]))return false;
     origins[origin]=true;played[origin]=match.used[p][h];owners[origin]=p;
   }
   for(const auto& slot:match.board) {
     if(slot.origin<0||slot.origin>=10||slot.owner<0||slot.owner>1||seen[slot.origin])return false;
     const int p=slot.origin/5,h=slot.origin%5;
     if(slot.card!=cardIndex(hands[p][h]))return false;
     seen[slot.origin]=true;owners[slot.origin]=slot.owner;
   }
   for(int origin=0;origin<10;++origin)if(seen[origin]!=played[origin])return false;
   finalOwners=owners;finalScore=match.score();completed=true;
   outcome=finalScore[0]==finalScore[1]?-1:finalScore[0]>finalScore[1]?0:1;return true;
 }
 void forfeit() {if(held&&!completed){completed=true;forfeited=true;outcome=1;finalScore={0,10};finalOwners.fill(1);}}
 unsigned required(int winner) const {
   if(winner<0)return 0;
   switch(rule()) {case TradeOne:return 1;case TradeDiff:return std::min(5,std::abs(finalScore[0]-finalScore[1]));case TradeAll:return 5;default:return 0;}
 }
 bool choicePending() const {return held&&outcome==0&&(rule()==TradeOne||rule()==TradeDiff)&&required(0)>0;}
 unsigned automaticChoice(int winner) const {
   if(winner<0)return 0;
   std::array<int,5> order{0,1,2,3,4};
   std::stable_sort(order.begin(),order.end(),[&](int a,int b){
     auto value=[&](int h){return prizeValue(hands[1-winner][h]);};
     return value(a)>value(b);
   });unsigned mask=0;for(unsigned n=0;n<required(winner);++n)mask|=1u<<order[n];return mask;
 }
 std::array<int,10> transfers(int winner,unsigned mask) const {
   std::array<int,10> owners{0,0,0,0,0,1,1,1,1,1};
   if(rule()==TradeDirect&&completed)owners=finalOwners;
   else if(winner>=0)for(int h=0;h<5;++h)if(mask&(1u<<h))owners[(1-winner)*5+h]=winner;
   return owners;
 }
 template<class Bank> bool flush(Bank& bank) {
   if(!paying) return true;
   for(int p=0;p<2;++p) for(auto& [id,n]:credits[p]) if(n>0) {
     int added=std::clamp(bank.change(p,id,n),0,n); n-=added;
     if(n) return false;
   }
   paying=false; credits={}; return true;
 }
 template<class Bank> bool settleSelection(Bank& bank,int winner,unsigned mask) {
   if(paying)return flush(bank);
   if(!held)return true;
   if(winner< -1||winner>1||(mask&~31u))return false;
   if(completed&&winner!=outcome)return false;
   if(rule()>TradeOne&&winner>=0&&!completed)return false;
   if(std::popcount(mask)!=static_cast<int>(required(winner)))return false;
   auto next=escrow;
   if(winner>=0&&wager){next[1-winner][0]-=wager;next[winner][0]+=wager;}
   if(rule()!=TradeNone) {
     const auto owners=transfers(winner,mask);
     for(int origin=0;origin<10;++origin)if(owners[origin]!=origin/5){
       auto id=hands[origin/5][origin%5];--next[origin/5][id];++next[owners[origin]][id];
     }
   }
   for(const auto& stock:next)for(auto [id,n]:stock)if(n<0)return false;
   selected=mask;credits=std::move(next);held=false;paying=true;escrow={};
   return flush(bank);
 }
 template<class Bank> bool reserve(Bank& bank,const std::array<Hand,2>& decks,unsigned kind,int gold,std::uint32_t actor) {
   if(pending()||kind>TradeAll||gold<0||gold>maximumWager||!actor)return false;
   std::array<Stock,2> need{};
   for(int p=0;p<2;++p) {
     Stock identities;for(auto id:decks[p])if(cardIndex(id)<0||++identities[baseCardID(id)]>deckLimit(id))return false;
     if(kind)for(auto id:decks[p])++need[p][id];
     if(gold)need[p][0]=gold;
     for(auto [id,n]:need[p])if(bank.count(p,id)<n)return false;
   }
   *this={};hands=decks;trade=kind;wager=gold;opponent=actor;held=true;
   for(int p=0;p<2;++p)for(auto [id,n]:need[p]) {
     int taken=std::clamp(-bank.change(p,id,-n),0,n);escrow[p][id]+=taken;
     if(taken!=n){settleSelection(bank,-1,0);return false;}
   }
   return true;
 }
};
struct OpponentRecord { unsigned wins=0,losses=0,draws=0; };
struct PlayerRecord {
 OpponentRecord competitive{};
 bool known=false,unavailable=false;
 unsigned restockAt=0;
};
struct HoldTerms { unsigned rules=0,trade=1; };
struct SavedDeck { std::uint32_t id=0; std::string name; Hand cards{}; };
inline constexpr unsigned maxSavedDecks=20;
inline bool validDeckName(std::string_view name) {
 if(name.empty()||name.size()>48||name.front()==' '||name.back()==' ')return false;
 for(std::size_t i=0;i<name.size();){
   unsigned c=static_cast<unsigned char>(name[i++]);if(c<32||c==127)return false;if(c<128)continue;
   unsigned more=0,minimum=0;
   if(c>=0xC2&&c<=0xDF){more=1;minimum=0x80;c&=31;}
   else if(c>=0xE0&&c<=0xEF){more=2;minimum=0x800;c&=15;}
   else if(c>=0xF0&&c<=0xF4){more=3;minimum=0x10000;c&=7;}
   else return false;
   if(i+more>name.size())return false;
   while(more--){auto next=static_cast<unsigned char>(name[i++]);if((next&0xC0)!=0x80)return false;c=(c<<6)|(next&63);}
   if(c<minimum||c>0x10FFFF||(c>=0xD800&&c<=0xDFFF))return false;
 }
 return true;
}
struct InvitationDecision { unsigned until=0; bool accepted=false; };
struct ReputationRecord { unsigned wins=0,quarters=0; };
struct TournamentReputation {
 // Keep earned credit after the event leaves the album's recent history.
 std::map<std::uint32_t,ReputationRecord> opponents;
 std::array<unsigned,3> championships{};
 unsigned renownQuarters=0;
};
struct MerchantStock {
 Stock stock, resale;
 unsigned restockAt=0, generation=0;
 int goldCredit=0;
};
inline constexpr unsigned collectionFormat=28;
struct CollectionSave {
 RuleCultureState culture;
 TournamentState tournaments;
 bool starter=false;
 Hand deck{};
 Contract contract;
 Stock player,discovered;
 std::map<std::uint32_t,Stock> opponents;
 std::map<std::uint32_t,MerchantStock> merchants;
 int goldCredit=0;
 unsigned encounteredRules=0;
 std::map<std::uint32_t,PlayerRecord> players;
 std::map<std::uint32_t,HoldTerms> holds;
 bool developmentCardsGranted=false,developmentFoilsGranted=false;
 // Bit 0: any staked victory; bits 1..6: skill level at victory.
 std::map<std::uint32_t,unsigned> achievements,localWins;
 std::map<std::uint32_t,ReputationRecord> reputation;
 std::array<TournamentReputation,10> tournamentReputation;
 std::set<std::uint32_t> deadPlayers;
 std::map<std::uint32_t,unsigned> childHolds;
 bool valdrRescued=false,erandurReady=false;
 std::set<std::uint32_t> accessBlocked;
 std::set<std::uint32_t> tournamentBlocked; // Rebuilt from current world state, not serialized.
 bool guildMember=false,brandSheiJailed=false,collegeMember=false;
 std::array<unsigned,2> dealReveals{};
 std::vector<SavedDeck> decks;
 std::uint32_t activeDeck=1,nextDeck=2,economyRandom=0;
 std::map<std::uint32_t,InvitationDecision> invitations;
 std::map<std::uint32_t,unsigned> masterApproachAfter,holdApproachAfter;
};
inline bool merchantOwnsCard(const CollectionSave& s,CardID id) {
 id=baseCardID(id);
 for(const auto& [key,m]:s.merchants)for(const auto* stock:{&m.stock,&m.resale})
  if(count(*stock,id)||count(*stock,id|foilFlag))return true;
 return false;
}
inline void ensureSavedDecks(CollectionSave& s) {
 if(s.decks.empty()){s.decks.push_back({1,"Deck 1",s.deck});s.activeDeck=1;s.nextDeck=2;}
}
inline void saveActiveDeck(CollectionSave& s) {
 ensureSavedDecks(s);for(auto& d:s.decks)if(d.id==s.activeDeck){d.cards=s.deck;return;}
}
inline void retainOwnedDecks(CollectionSave& s) {
 retainOwnedDeck(s.deck,s.player);for(auto& d:s.decks)retainOwnedDeck(d.cards,s.player);saveActiveDeck(s);
}
inline int deckCopiesNeeded(const CollectionSave& s,CardID id) {
 int n=std::count(s.deck.begin(),s.deck.end(),id);
 for(const auto& d:s.decks)n=std::max(n,static_cast<int>(std::count(d.cards.begin(),d.cards.end(),id)));
 return n;
}
inline bool manageDeck(CollectionSave& s,std::string_view action,unsigned id,std::string name={}) {
 if(s.contract.pending()||s.goldCredit)return false;
 ensureSavedDecks(s);
 auto it=std::find_if(s.decks.begin(),s.decks.end(),[&](const auto& d){return d.id==id;});
 if(action=="new"||action=="copy") {
   if(!validDeckName(name)||s.decks.size()>=maxSavedDecks||s.nextDeck==0||s.nextDeck==UINT32_MAX)return false;
   if(action=="copy"&&it==s.decks.end())return false;
   const Hand hand=action=="copy"?it->cards:Hand{};
   s.activeDeck=s.nextDeck++;s.deck=hand;s.decks.push_back({s.activeDeck,std::move(name),hand});return true;
 }
 if(it==s.decks.end())return false;
 if(action=="select"){s.activeDeck=id;s.deck=it->cards;return true;}
 if(action=="rename"){if(!validDeckName(name))return false;it->name=std::move(name);return true;}
 if(action=="delete") {
   if(s.decks.size()==1)return false;
   s.decks.erase(it);if(s.activeDeck==id){s.activeDeck=s.decks.front().id;s.deck=s.decks.front().cards;}return true;
 }
 return false;
}
inline void retainDealVisibility(CollectionSave& save,Match& match){
 if(!(match.rules&ThreeOpen))return;
 if(save.dealReveals==std::array<unsigned,2>{})save.dealReveals=match.reveals;else match.reveals=save.dealReveals;
}
inline std::vector<std::uint32_t> encodeCollection(const CollectionSave& s) {
 std::vector<std::uint32_t> out{static_cast<unsigned>(s.starter)};
 out.insert(out.end(),s.deck.begin(),s.deck.end()); const auto& c=s.contract;
 for(auto n:{static_cast<unsigned>(c.held),static_cast<unsigned>(c.paying),static_cast<unsigned>(c.wager),static_cast<unsigned>(c.outcome+2),c.opponent}) out.push_back(n);
 for(auto hand:c.hands) out.insert(out.end(),hand.begin(),hand.end());
 for(auto stocks:{c.escrow,c.credits}) for(const auto& stock:stocks) {
   out.push_back(static_cast<unsigned>(stock.size()));
   for(auto [id,n]:stock) { out.push_back(id); out.push_back(static_cast<unsigned>(n)); }
 }
 auto stock=[&](const Stock& entries) { out.push_back(static_cast<unsigned>(entries.size())); for(auto [id,n]:entries) { out.push_back(id); out.push_back(static_cast<unsigned>(n)); } };
 stock(s.player); stock(s.discovered);
 out.push_back(static_cast<unsigned>(s.opponents.size()));
 for(auto& [actor,entries]:s.opponents) { out.push_back(actor); stock(entries); }
 out.push_back(static_cast<std::uint32_t>(s.goldCredit+1000000));
 out.push_back(s.encounteredRules);

 
   out.push_back(static_cast<unsigned>(s.players.size()));
   for(const auto& [id,p]:s.players) {
     out.push_back(id);for(auto r:{p.competitive}){out.push_back(r.wins);out.push_back(r.losses);out.push_back(r.draws);}
     out.push_back(unsigned(p.known)|(unsigned(p.unavailable)<<1));out.push_back(p.restockAt);
   }
   out.push_back(static_cast<unsigned>(s.holds.size()));
   for(auto [id,h]:s.holds){out.push_back(id);out.push_back(h.rules);out.push_back(h.trade);}
 
 out.push_back(s.developmentCardsGranted);
 out.push_back(static_cast<unsigned>(s.achievements.size()));for(auto [id,flags]:s.achievements){out.push_back(id);out.push_back(flags);}
 
   out.push_back(s.valdrRescued);
   out.push_back(static_cast<unsigned>(s.localWins.size()));for(auto [id,flags]:s.localWins){out.push_back(id);out.push_back(flags);}
   out.push_back(static_cast<unsigned>(s.reputation.size()));for(auto [id,r]:s.reputation){out.push_back(id);out.push_back(r.wins);out.push_back(r.quarters);}
   out.push_back(static_cast<unsigned>(s.deadPlayers.size()));for(auto id:s.deadPlayers)out.push_back(id);
   out.push_back(static_cast<unsigned>(s.childHolds.size()));for(auto [id,hold]:s.childHolds){out.push_back(id);out.push_back(hold);}
 
 
   for(auto n:{c.rule(),c.selected,unsigned(c.completed),unsigned(c.forfeited),unsigned(c.finalScore[0]),unsigned(c.finalScore[1])})out.push_back(n);
   for(auto owner:c.finalOwners)out.push_back(owner);
 
 out.push_back(s.erandurReady);
 out.push_back(s.dealReveals[0]);out.push_back(s.dealReveals[1]);
 out.push_back(s.collegeMember);
 out.push_back(s.guildMember);out.push_back(s.brandSheiJailed);
 
   auto copy=s;ensureSavedDecks(copy);saveActiveDeck(copy);
   out.push_back(copy.activeDeck);out.push_back(copy.nextDeck);out.push_back(static_cast<unsigned>(copy.decks.size()));
   for(const auto& d:copy.decks){out.push_back(d.id);out.push_back(static_cast<unsigned>(d.name.size()));for(unsigned char c:d.name)out.push_back(c);out.insert(out.end(),d.cards.begin(),d.cards.end());}
 
 out.push_back(s.economyRandom);out.push_back(s.developmentFoilsGranted);
 
  out.push_back(static_cast<unsigned>(s.invitations.size()));for(auto [id,d]:s.invitations){out.push_back(id);out.push_back(d.until);out.push_back(d.accepted);}
  for(const auto* values:{&s.masterApproachAfter,&s.holdApproachAfter}){out.push_back(static_cast<unsigned>(values->size()));for(auto [id,hour]:*values){out.push_back(id);out.push_back(hour);}}
 
 encodeRuleCulture(out,s.culture);
 encodeTournaments(out,s.tournaments);
 for(const auto& r:s.tournamentReputation){
  out.push_back(r.opponents.size());for(auto [id,p]:r.opponents){out.push_back(id);out.push_back(p.wins);out.push_back(p.quarters);}
  out.insert(out.end(),r.championships.begin(),r.championships.end());out.push_back(r.renownQuarters);
 }
 out.push_back(s.merchants.size());
 for(const auto& [key,m]:s.merchants){out.push_back(key);stock(m.stock);stock(m.resale);out.push_back(m.restockAt);out.push_back(m.generation);out.push_back(m.goldCredit);}
 return out;
}
inline bool decodeCollection(const std::vector<std::uint32_t>& in,CollectionSave& saved,unsigned version=collectionFormat) {
 if(version!=20&&version!=21&&version!=26&&version!=27&&version!=collectionFormat)return false;
 const bool legacy=version==20;
 CollectionSave s; std::size_t at=0; bool ok=true;
 auto get=[&]() { if(at>=in.size()) { ok=false; return 0u; } return in[at++]; };
 auto boolean=[&]() { auto v=get(); if(v>1) ok=false; return v!=0; };
 s.starter=boolean();
 for(auto& id:s.deck) { id=get(); if(id&&cardIndex(id)<0) ok=false; }
 auto& c=s.contract; c.held=boolean(); c.paying=boolean(); c.wager=static_cast<int>(get()); c.outcome=static_cast<int>(get())-2; c.opponent=get();
 if(c.wager<0||c.wager>maximumWager||c.outcome< -2||c.outcome>1||(c.held&&c.paying)||((c.held||c.paying)&&!c.opponent)) ok=false;
 for(auto& hand:c.hands) for(auto& id:hand) { id=get(); if((id||c.pending())&&cardIndex(id)<0) ok=false; }
 for(auto* stocks:{&c.escrow,&c.credits}) for(auto& stock:*stocks) {
   auto size=get(); if(size>cards.size()*2+1) return false;
   for(unsigned i=0;i<size;++i) { auto id=get(); auto n=get(); if((id&&cardIndex(id)<0)||n>(id?10u:unsigned(maximumWager*2))||stock.contains(id)) ok=false; stock[id]=static_cast<int>(n); }
 }
 auto stock=[&](Stock& entries) {
   auto size=get(); if(size>cards.size()*2) { ok=false; return; }
   for(unsigned i=0;i<size;++i) { auto id=get(),n=get(); if(cardIndex(id)<0||n>1000000||entries.contains(id)) ok=false; entries[id]=static_cast<int>(n); }
 };
 stock(s.player); stock(s.discovered);
 auto actors=get(); if(actors>1024) return false;
 for(unsigned i=0;i<actors;++i) { auto actor=get(); if(!actor||s.opponents.contains(actor)) return false; stock(s.opponents[actor]); }
 // Read and discard retired v20 shop data; it never enters the current model.
 if(legacy){auto n=get();if(n>1024)return false;for(unsigned i=0;i<n;++i){get();Stock discarded;stock(discarded);get();get();}}
 auto credit=get(); if(credit>2000000) return false; s.goldCredit=static_cast<int>(credit)-1000000;
 
   if(legacy)boolean();
   s.encounteredRules=get();if(legacy){boolean();get();}if(s.encounteredRules&~RuleMask)return false;
 

 
   auto size=get();if(size>1024)return false;
   for(unsigned i=0;i<size;++i){auto id=get();if(!id||s.players.contains(id))return false;auto& p=s.players[id];
     for(auto* r:{&p.competitive}){r->wins=get();r->losses=get();r->draws=get();if(r->wins>1000000||r->losses>1000000||r->draws>1000000)return false;}
     auto flags=get();if(flags&~3u)return false;p.known=flags&1;p.unavailable=flags&2;
     p.restockAt=get();if(p.restockAt>100000000)return false;
   }
   if(legacy){size=get();if(size>1024)return false;for(unsigned i=0;i<size;++i){get();get();get();}}
   size=get();if(size>64)return false;
   for(unsigned i=0;i<size;++i){auto id=get();if(!id||s.holds.contains(id))return false;auto& h=s.holds[id];h.rules=get();h.trade=get();if(!validRules(h.rules)||h.trade>4)return false;}
 
 s.developmentCardsGranted=boolean();
 
   if(legacy){boolean();get();get();get();get();}
   size=get();if(size>1024)return false;for(unsigned i=0;i<size;++i){auto id=get(),flags=get();if(!id||flags>127||s.achievements.contains(id))return false;s.achievements[id]=flags;}
 
 
   s.valdrRescued=boolean();
   size=get();if(size>1024)return false;
   for(unsigned i=0;i<size;++i){auto id=get(),flags=get();if(!id||(flags&~63u)||s.localWins.contains(id))return false;s.localWins[id]=flags;}
   size=get();if(size>1024)return false;
   for(unsigned i=0;i<size;++i){auto id=get(),wins=get(),quarters=get();if(!id||!wins||wins>1000000||!quarters||quarters>8*wins+40||s.reputation.contains(id))return false;s.reputation[id]={wins,quarters};}
   size=get();if(size>1024)return false;
   for(unsigned i=0;i<size;++i){auto id=get();if(!id||!s.deadPlayers.insert(id).second)return false;}
   if(legacy){size=get();if(size>64)return false;for(unsigned i=0;i<size;++i)for(unsigned j=0;j<5;++j)get();}
   size=get();if(size>1024)return false;for(unsigned i=0;i<size;++i){auto id=get(),hold=get();if(!id||!hold||hold>64||s.childHolds.contains(id))return false;s.childHolds[id]=hold;}
 
 
   c.trade=get();c.selected=get();c.completed=boolean();c.forfeited=boolean();
   c.finalScore={static_cast<int>(get()),static_cast<int>(get())};
   for(auto& owner:c.finalOwners){owner=get();if(owner<0||owner>1)ok=false;}
   if(c.trade>TradeAll||(c.selected&~31u)||c.finalScore[0]<0||c.finalScore[0]>10||c.finalScore[1]!=10-c.finalScore[0]||(c.forfeited&&!c.completed))ok=false;
   if(c.completed){int n=std::count(c.finalOwners.begin(),c.finalOwners.end(),0);if(n!=c.finalScore[0]||c.outcome!=(n==5?-1:n>5?0:1))ok=false;}
   if(c.held&&c.trade>TradeOne&&c.outcome!=-2&&!c.completed)ok=false;
   s.erandurReady=boolean();
   if(legacy){size=get();if(size>64)return false;for(unsigned i=0;i<size;++i)for(unsigned j=0;j<5;++j)get();}

 if(c.held){
   for(int p=0;p<2;++p){Stock expected;if(c.rule())for(auto id:c.hands[p])++expected[id];if(c.wager)expected[0]=c.wager;
     for(auto [id,n]:expected)if(count(c.escrow[p],id)!=n)ok=false;
     for(auto [id,n]:c.escrow[p])if(n!=count(expected,id))ok=false;
     if(!c.credits[p].empty())ok=false;
   }
 }
 for(auto& mask:s.dealReveals)mask=get();if(s.dealReveals!=std::array<unsigned,2>{})for(auto mask:s.dealReveals)if((mask&~31u)||std::popcount(mask)!=3)ok=false;
 s.collegeMember=boolean();
 s.guildMember=boolean();s.brandSheiJailed=boolean();
 
   s.activeDeck=get();s.nextDeck=get();size=get();if(size<1||size>maxSavedDecks||!s.nextDeck)return false;
   std::set<unsigned> ids;bool active=false;
   for(unsigned n=0;n<size;++n){
     SavedDeck d;d.id=get();auto length=get();if(!d.id||d.id>=s.nextDeck||!ids.insert(d.id).second||length>48)return false;
     for(unsigned j=0;j<length;++j){auto c=get();if(c>255)return false;d.name+=static_cast<char>(c);}
     if(!validDeckName(d.name))return false;
     Stock copies;for(auto& id:d.cards){id=get();if(id&&(cardIndex(id)<0||++copies[baseCardID(id)]>deckLimit(id)))return false;}
     if(d.id==s.activeDeck){active=true;if(d.cards!=s.deck)return false;}
     s.decks.push_back(std::move(d));
   }
   if(!active)return false;

 s.economyRandom=get();s.developmentFoilsGranted=boolean();
 
  if(legacy)boolean();
  auto n=get();if(n>1024)return false;
  for(unsigned i=0;i<n;++i){auto id=get(),until=get();bool accepted=boolean();if(!id||until>100000000||s.invitations.contains(id))return false;s.invitations[id]={until,accepted};}
  for(auto* values:{&s.masterApproachAfter,&s.holdApproachAfter}){n=get();if(n>1024)return false;for(unsigned i=0;i<n;++i){auto id=get(),hour=get();if(!id||hour>100000000||values->contains(id))return false;(*values)[id]=hour;}}
 
 if(!decodeRuleCulture(get,s.culture,version<27))return false;
 if(version>=22&&!decodeTournaments(get,s.tournaments,[](unsigned id){return cardIndex(id)>=0;},[](unsigned rules){return validRules(rules);},version))return false;
 if(version>=26)for(auto& r:s.tournamentReputation){
  auto count=get();if(count>1024)return false;
  for(unsigned i=0;i<count;++i){auto id=get(),wins=get(),quarters=get();if(!id||!wins||wins>1000000||!quarters||quarters>8*wins+40||r.opponents.contains(id))return false;r.opponents[id]={wins,quarters};}
  for(auto& wins:r.championships){wins=get();if(wins>1000000)return false;}
  r.renownQuarters=get();if(r.renownQuarters>1000000)return false;
 }
 if(version>=28){
  const auto n=get();if(n>1024)return false;
  for(unsigned i=0;i<n;++i){
   const auto key=get();if(!key||s.merchants.contains(key))return false;
   auto& m=s.merchants[key];stock(m.stock);stock(m.resale);m.restockAt=get();m.generation=get();const auto credit=get();
   if(m.restockAt>100000000||!m.generation||credit>1000000)return false;m.goldCredit=int(credit);
  }
 }
 if(!ok||at!=in.size()) return false;
 saved=std::move(s); return true;
}
}
