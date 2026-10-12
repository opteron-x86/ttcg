#pragma once
#include "FreePlay.h"
namespace ttcg {
inline unsigned conflictingRules(unsigned rule){
 return rule==Open?ThreeOpen:rule==ThreeOpen?Open:rule==Legion?Decimation:rule==Decimation?Legion:rule==Order?Chaos:rule==Chaos?Order:0u;
}
inline unsigned withSpreadRule(unsigned rules,unsigned rule){return (rules&~conflictingRules(rule))|rule|(rule==SameWall?Same:0u);}
inline unsigned withoutCultureRule(unsigned rules,unsigned rule){return rules&~(rule|(rule==Same?SameWall:0u));}
inline const RegionalCulture& regionalCulture(const CollectionSave& s,unsigned hold){
 static const RegionalCulture empty{};auto it=s.culture.regions.find(hold);return it==s.culture.regions.end()?empty:it->second;
}
inline unsigned regionalRandom(unsigned& state){
 state=invitationHash(state+0x9E3779B9u);if(!state)state=0x54544347u;return state;
}
inline unsigned nextRegionalChange(unsigned hour,unsigned& random,bool trade=false){
 const unsigned minimum=trade?regionalTradeProtectionHours:regionalRuleProtectionHours;
 const unsigned maximum=(trade?45:21)*24;
 return std::min(hour+minimum+regionalRandom(random)%(maximum-minimum+1),regionalClockLimit);
}
inline void initializeRegionalCalendar(CollectionSave& s,unsigned hold,unsigned hour){
 auto& r=s.culture.regions[hold];if(r.nextRuleChange)return;
 r.ruleRandom=invitationHash(hold^hour^s.economyRandom^0x52554C45u)|1u;
 r.tradeRandom=invitationHash(hold^hour^s.economyRandom^0x54524144u)|1u;
 r.nextRuleChange=nextRegionalChange(hour,r.ruleRandom);
 r.nextTradeChange=nextRegionalChange(hour,r.tradeRandom,true);
}
inline unsigned protectedRegionalRules(const CollectionSave& s,unsigned hold,unsigned hour){
 unsigned mask=0;const auto& r=regionalCulture(s,hold);
 for(unsigned n=0;n<ruleFlags.size();++n)if(r.ruleProtection[n]>hour)mask|=ruleFlags[n];return mask;
}
inline bool regionalRulesAllowed(const CollectionSave& s,unsigned hold,unsigned rules,unsigned hour){
 return validRules(rules)&&!((holdTerms(s,hold).rules^rules)&protectedRegionalRules(s,hold,hour));
}
inline unsigned regionalAdditions(const CollectionSave& s,unsigned hold,unsigned candidates,unsigned hour){
 const auto local=holdTerms(s,hold).rules;unsigned result=0;
 for(auto bit:ruleFlags)if((candidates&bit)&&!(local&bit)&&regionalRulesAllowed(s,hold,withSpreadRule(local,bit),hour))result|=bit;
 return result;
}
inline unsigned regionalRemovals(const CollectionSave& s,unsigned hold,unsigned hour){
 const auto local=holdTerms(s,hold).rules;unsigned result=0;
 for(auto bit:ruleFlags)if((local&bit)&&regionalRulesAllowed(s,hold,withoutCultureRule(local,bit),hour))result|=bit;
 return result;
}
inline bool regionalTradeAllowed(const CollectionSave& s,unsigned hold,unsigned trade,unsigned hour){
 if(trade<TradeOne||trade>TradeAll)return false;
 const auto old=holdTerms(s,hold).trade;const auto& r=regionalCulture(s,hold);
 return trade==old||((!old||r.tradeProtection[old-1]<=hour)&&r.tradeProtection[trade-1]<=hour);
}
// All permanent changes share protection and scheduling, including player
// spreading/abolition and future services that edit a hold's terms.
inline bool applyRegionalTerms(CollectionSave& s,unsigned hold,HoldTerms terms,unsigned hour){
 if(!regionProfile(hold)||hour>100000000||!regionalRulesAllowed(s,hold,terms.rules,hour)||!regionalTradeAllowed(s,hold,terms.trade,hour))return false;
 const auto before=holdTerms(s,hold);
 if(before.rules==terms.rules&&before.trade==terms.trade)return false;
 initializeRegionalCalendar(s,hold,hour);auto& r=s.culture.regions[hold];
 if(before.rules!=terms.rules){
  const auto changed=before.rules^terms.rules;
  for(unsigned n=0;n<ruleFlags.size();++n)if(changed&ruleFlags[n])r.ruleProtection[n]=hour+regionalRuleProtectionHours;
  r.added=terms.rules&~before.rules;r.removed=before.rules&~terms.rules;r.changedAt=hour;
  r.nextRuleChange=nextRegionalChange(hour,r.ruleRandom);r.deferred&=~1u;
  // An unanswered offer describes the former terms. Keep the visit's asked
  // list, so this does not grant extra spread opportunities.
  if(r.offer.status==1)r.offer.status=2;
 }
 if(before.trade!=terms.trade){
  if(before.trade)r.tradeProtection[before.trade-1]=hour+regionalTradeProtectionHours;
  r.tradeProtection[terms.trade-1]=hour+regionalTradeProtectionHours;
  r.nextTradeChange=nextRegionalChange(hour,r.tradeRandom,true);r.deferred&=~2u;
 }
 s.holds[hold]=terms;return true;
}
inline unsigned naturalRegionalRules(const CollectionSave& s,unsigned hold,unsigned hour,unsigned& random){
 const auto current=holdTerms(s,hold).rules;
 const auto additions=regionalAdditions(s,hold,RuleMask,hour),removals=regionalRemovals(s,hold,hour);
 const auto count=std::popcount(current);
 // No forbidden presets or rule-count cap: drift favors two to four rules.
 const unsigned addChance=count<2?85:count<4?60:count==4?30:10;
 const bool add=additions&&(!removals||regionalRandom(random)%100<addChance);
 const auto choices=add?additions:removals;
 if(!choices)return current;
 auto pick=regionalRandom(random)%std::popcount(choices);
 for(auto bit:ruleFlags)if(choices&bit){if(!pick--)return add?withSpreadRule(current,bit):withoutCultureRule(current,bit);}
 return current;
}
inline bool tickRegionalRules(CollectionSave& s,unsigned hour,unsigned heldHold=0){
 if(hour>100000000)return false;bool changed=false;
 for(const auto& hold:holds){
  initializeRegionalCalendar(s,hold.id,hour);auto& r=s.culture.regions[hold.id];
  if(hold.id==heldHold||(s.culture.pending.kind&&s.culture.pending.hold==hold.id)){
   if(hour>=r.nextRuleChange)r.deferred|=1;
   if(hour>=r.nextTradeChange)r.deferred|=2;
   continue;
  }
  if(r.deferred&1)r.nextRuleChange=hour;
  if(r.deferred&2)r.nextTradeChange=hour;
  r.deferred=0;
  unsigned steps=0;
  while(std::min(r.nextRuleChange,r.nextTradeChange)<=hour){
   // Normal absences replay their calendar in order. Coalesce only extreme
   // time jumps (years) to keep a malformed or heavily edited clock bounded.
   if(++steps==128){if(r.nextRuleChange<=hour)r.nextRuleChange=hour;if(r.nextTradeChange<=hour)r.nextTradeChange=hour;}
   const bool trade=r.nextTradeChange<r.nextRuleChange;
   const auto at=trade?r.nextTradeChange:r.nextRuleChange;
   auto terms=holdTerms(s,hold.id);
   if(trade){
    std::vector<unsigned> choices;for(unsigned t=TradeOne;t<=TradeAll;++t)if(t!=terms.trade&&regionalTradeAllowed(s,hold.id,t,at))choices.push_back(t);
    if(!choices.empty())terms.trade=choices[regionalRandom(r.tradeRandom)%choices.size()];
   }else terms.rules=naturalRegionalRules(s,hold.id,at,r.ruleRandom);
   if(applyRegionalTerms(s,hold.id,terms,at))changed=true;
   else if(trade)r.nextTradeChange=nextRegionalChange(at,r.tradeRandom,true);
   else r.nextRuleChange=nextRegionalChange(at,r.ruleRandom);
  }
 }
 return changed;
}
}
