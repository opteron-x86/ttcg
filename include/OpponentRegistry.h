#pragma once
#include "Campaign.h"
#include "ExpansionRewards.h"
#include <deque>
namespace ttcg {
inline constexpr std::array<const char*,6> opponentSkillNames{"Beginner","Learner","Regular","Experienced","Expert","Master"};
struct OpponentGate {WorldForm form;unsigned kind=0,value=0;bool player=false;}; // quest stage done, minimum stage, faction, global
struct OpponentCardGrant {std::string card;unsigned quantity=1;bool foil=false;};
struct OpponentDefinition {
 std::string key;WorldForm actor,reference;unsigned level=2,hold=0;bool traveller=false,tournaments=false,requiredMaster=false;
 bool generated=true,restock=true,enabled=true,customDialogue=false,personalRules=false;
 unsigned rules=Open,trade=TradeOne,wager=50;int reputationGate=-1;
 std::string theme;std::vector<OpponentCardGrant> cards;std::vector<OpponentGate> gates;
 WorldForm accept,decline,noCards;
};
struct NpcGrantDefinition {std::string key;WorldForm actor;std::vector<OpponentCardGrant> cards;std::vector<OpponentGate> gates;};
inline std::vector<OpponentDefinition> opponentDefinitions;
inline std::vector<NpcGrantDefinition> npcGrantDefinitions;
inline std::vector<WorldForm> excludedNpcs;
inline std::set<std::string> excludedNpcPlugins;
inline std::vector<std::string> opponentLoadErrors;
inline const OpponentDefinition* opponentDefinition(std::uint32_t base){for(const auto& d:opponentDefinitions)if(worldFormID(d.actor)==base)return &d;return nullptr;}
inline bool excludedNpc(std::uint32_t base){
 if(const auto* master=identityMaster(base);master&&excludedNpcPlugins.contains(normalizedPlugin(master)))return true;
 for(const auto& form:excludedNpcs)if(worldFormID(form)==base)return true;return false;
}
inline bool resolveNpcCards(const std::vector<OpponentCardGrant>& cards,Stock& stock){
 Stock unique;
 for(const auto& entry:cards){auto id=namedCardID(entry.card);const auto index=cardIndex(id);if(index<0||!entry.quantity||entry.quantity>99)return false;
  if(::ttcg::cards[index].unique&&++unique[id]>1)return false;
  if(::ttcg::cards[index].unique&&entry.quantity!=1)return false;
  stock[id|(entry.foil?foilFlag:0)]+=entry.quantity;
 }
 return true;
}
inline Stock generatedNpcStock(unsigned level,std::string_view theme,std::uint32_t seed){
 // Reuse the reviewed vanilla distributions, replacing identities within the
 // same tier and rarity. This keeps one source for the collection budgets.
 std::vector<unsigned> donors;for(unsigned i=0;i<baseOpponents.size();++i)if(baseOpponents[i].level==level&&!baseOpponents[i].traveller)donors.push_back(i);
 if(donors.empty())return {};std::mt19937 rng(seed);const auto& donor=baseOpponents[donors[rng()%donors.size()]];Stock stock;
 for(auto id:donor.collection)if(id){const int index=cardIndex(id);if(index<0)continue;const auto& original=cards[index];if(original.unique)continue;
  std::vector<CardID> all,themed;for(const auto& c:baseCards)if(c.available&&!c.pending&&!c.unique&&c.tier==original.tier&&std::string_view(c.rarity)==original.rarity&&!count(stock,c.form)&&!count(stock,c.form|foilFlag)){
   all.push_back(c.form);if(!theme.empty()&&std::string_view(c.groups).find("\""+std::string(theme)+"\"")!=std::string_view::npos)themed.push_back(c.form);
  }
  const auto& pool=!themed.empty()&&rng()%100<65?themed:all;if(pool.empty())continue;
  auto chosen=pool[rng()%pool.size()];if(rng()%1000==0)chosen|=foilFlag;++stock[chosen];
 }
 return stock;
}
inline unsigned residentLevel(std::uint32_t seed){const auto roll=seed%100;return roll<10?0:roll<35?1:roll<75?2:roll<95?3:4;}
inline bool selectResident(unsigned mode,std::uint32_t seed,bool forced=false){return forced||mode==2||(mode==1&&seed%100<60);}
struct ResidentStrings {std::string name,location,region;};
inline auto& residentStrings(){static std::deque<ResidentStrings> values;return values;}
inline void resetRuntimeOpponents(){opponents.assign(baseOpponents.begin(),baseOpponents.end());opponentPools().clear();residentStrings().clear();}
inline int registerResident(std::uint32_t base,ResidentRecord& record,const OpponentDefinition* definition=nullptr){
 if(!record.selected)return -1;
 int index=opponentIndex(base);if(index>=0&&!opponents[index].dynamic)return index;
 residentStrings().push_back({record.name,record.location,record.traveller?"Travellers":holdName(record.hold)});auto& text=residentStrings().back();
 OpponentProfile p{};p.base=base;p.reference=record.reference;p.name=text.name.c_str();p.location=text.location.c_str();p.region=text.region.c_str();
 p.level=record.level;p.skill=record.level<2?0:record.level<4?1:record.level==4?2:3;p.skillName=opponentSkillNames[record.level];p.hold=record.hold;p.traveller=record.traveller;
 p.wager=std::array{10,25,50,75,100,200}[record.level];p.dynamic=true;p.automatic=record.source==0;p.tournamentEligible=record.source!=2&&record.hold!=0;p.requiredMaster=false;
 // Travellers and unaffiliated residents have no regional entry gate.
 p.reputationGate=record.hold&&!record.traveller?-1:0;
 if(definition){p.wager=definition->wager;p.tournamentEligible=definition->tournaments;p.requiredMaster=definition->requiredMaster;p.personalRules=definition->personalRules;p.rules=definition->rules;p.personalTrade=definition->trade;p.reputationGate=definition->reputationGate;p.restock=definition->restock;}
 Stock stock;for(auto id:record.starting)++stock[id];opponentPools()[base]=std::move(stock);
 if(index<0){index=static_cast<int>(opponents.size());opponents.push_back(p);}else opponents[index]=p;
 return index;
}
inline bool adoptOpponentDefinition(ResidentRecord& record,const OpponentDefinition& d){
 Stock stock;if(d.generated)stock=generatedNpcStock(d.level,d.theme,record.seed);
 if(!resolveNpcCards(d.cards,stock)||playableCount(stock)<5||cardCount(stock)>128)return false;
 if(record.source!=2)record.enabled=d.enabled;
 record.source=2;record.selected=true;record.definition=d.key;record.level=d.level;record.hold=d.hold;record.traveller=d.traveller;record.theme=d.theme;
 record.starting.clear();for(auto [id,n]:stock)for(int copy=0;copy<n;++copy)record.starting.push_back(id);return true;
}
inline bool createResident(CollectionSave& s,std::uint32_t base,std::uint32_t reference,std::string name,std::string location,std::string theme,unsigned hold,unsigned mode,bool force=false,const OpponentDefinition* definition=nullptr){
 if(!base||!reference||name.empty()||name.size()>128||hold>10||excludedNpc(base))return false;
 if(std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))return false;
 std::erase_if(location,[](unsigned char c){return c<32||c==127;});
 if(!s.residents.seed)s.residents.seed=0x71C63127u;
 auto found=s.residents.profiles.find(base);if(found!=s.residents.profiles.end()){
  auto& r=found->second;
  if(definition&&(r.source!=2||r.definition!=definition->key)){if(!adoptOpponentDefinition(r,*definition))return false;registerResident(base,r,definition);}
  if(force&&r.source==0){r.selected=true;r.source=1;}
  else if(!r.selected&&selectResident(mode,r.seed))r.selected=true;
  if(r.reference!=reference)return false; // Alternate incarnations require an authored reference mapping.
  if(r.selected&&opponentIndex(base)<0)registerResident(base,r,definition);
  return r.selected;
 }
 if(s.residents.profiles.size()>=maximumResidents)return false;
 ResidentRecord r;r.reference=reference;r.name=std::move(name);r.location=location.substr(0,128);r.theme=std::move(theme);r.hold=hold;
 r.seed=identitySeed(worldKey(base))^s.residents.seed;r.source=definition?2:force?1:0;r.selected=definition||selectResident(mode,r.seed,force);
 r.level=definition?definition->level:residentLevel(identitySeed(worldKey(base)+":skill")^r.seed);
 if(definition){r.definition=definition->key;r.traveller=definition->traveller;r.enabled=definition->enabled;r.hold=definition->hold;r.theme=definition->theme;}
 Stock stock;if(!definition||definition->generated)stock=generatedNpcStock(r.level,r.theme,r.seed);
 if(definition&&!resolveNpcCards(definition->cards,stock))return false;
 if(playableCount(stock)<5||cardCount(stock)>128)return false;
 for(auto [id,n]:stock)for(int copy=0;copy<n;++copy)r.starting.push_back(id);
 auto& saved=s.residents.profiles.emplace(base,std::move(r)).first->second;
 if(saved.selected)registerResident(base,saved,definition);return saved.selected;
}
inline bool grantNpcCards(CollectionSave& s,unsigned ledger,std::string_view token,const std::vector<OpponentCardGrant>& cards){
 if(!ledger||token.empty()||token.size()>256||s.residents.grants.contains(std::string(token))||s.residents.grants.size()>=16384||s.contract.pending())return false;
 Stock add;if(!resolveNpcCards(cards,add))return false;
 auto& stock=s.opponents[ledger];for(auto [id,n]:add)if(count(stock,id)>1000000-n)return false;
 for(auto [id,n]:add){const auto& c=::ttcg::cards[cardIndex(id)];if(c.unique&&uniqueCardClaimed(s,id))continue;stock[id]+=n;}
 s.residents.grants.emplace(token);return true;
}
}
