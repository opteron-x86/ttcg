#pragma once
#include "FreePlay.h"
#include "Presentation.h"
namespace ttcg {
inline constexpr unsigned cultureCooldown=168;
inline constexpr unsigned spreadPlayerLimit=3,spreadWinChance=35,spreadOtherChance=20,incidentalAbolitionChance=2;
inline unsigned conflictingRules(unsigned rule){
 return rule==Open?ThreeOpen:rule==ThreeOpen?Open:rule==Legion?Decimation:rule==Decimation?Legion:rule==Order?Chaos:rule==Chaos?Order:0u;
}
inline unsigned withSpreadRule(unsigned rules,unsigned rule){return (rules&~conflictingRules(rule))|rule|(rule==SameWall?Same:0u);}
inline unsigned withoutCultureRule(unsigned rules,unsigned rule){return rules&~(rule|(rule==Same?SameWall:0u));}
inline unsigned chooseCultureRule(unsigned mask,unsigned seed){
 std::vector<unsigned> choices;for(unsigned bit:ruleFlags)if(mask&bit)choices.push_back(bit);
 return choices.empty()?0:choices[seed%choices.size()];
}
inline const RegionalCulture& regionalCulture(const CollectionSave& s,unsigned hold){
 static const RegionalCulture empty{};auto it=s.culture.regions.find(hold);return it==s.culture.regions.end()?empty:it->second;
}
inline bool cultureOpponent(const CollectionSave& s,int i){return i>=0&&i<int(opponents.size())&&!opponents[i].traveller&&unlockedOpponent(s,i)&&(i!=0||s.starter);}
inline void carryLocalRules(CollectionSave& s,unsigned hold){
 auto& c=s.culture;
 for(auto& [id,r]:c.regions)if(r.offer.status==1)r.offer.status=2;
 c.carriedHold=hold;c.carriedRules=holdTerms(s,hold).rules;c.visitHold=0;c.asked.clear();
}
inline void finishRuleVisit(CollectionSave& s,unsigned hold){
 const auto& c=s.culture;if(c.pending.kind)return;
 if(c.visitHold!=hold||c.asked.size()>=spreadPlayerLimit||!(c.carriedRules&~holdTerms(s,hold).rules))carryLocalRules(s,hold);
}
inline unsigned spreadOffer(const CollectionSave& s,int i,unsigned){
 if(!cultureOpponent(s,i)||s.culture.pending.kind)return 0;
 const auto hold=opponents[i].hold;const auto& r=regionalCulture(s,hold);const auto& o=r.offer;
 return s.culture.visitHold==hold&&o.opponent==opponents[i].base&&o.status==1&&o.source==s.culture.carriedHold&&o.carried==s.culture.carriedRules&&o.local==holdTerms(s,hold).rules?o.rule:0;
}
inline void prepareRuleSpread(CollectionSave& s,int i,unsigned hour){
 if(!cultureOpponent(s,i)||s.culture.pending.kind)return;
 auto& c=s.culture;const auto hold=opponents[i].hold;
 // Moving on ends the previous visit. Merely reopening a lobby or waiting
 // another day in the same hold cannot replenish its three opportunities.
 if(c.visitHold&&c.visitHold!=hold)carryLocalRules(s,c.visitHold);
 const auto source=c.carriedHold;
 if(!source||source==hold)return;
 const auto local=holdTerms(s,hold).rules,foreign=c.carriedRules&~local;
 if(!foreign){carryLocalRules(s,hold);return;}
 const auto base=opponents[i].base;
 if(std::find(c.asked.begin(),c.asked.end(),base)!=c.asked.end())return;
 if(c.asked.size()>=spreadPlayerLimit){carryLocalRules(s,hold);return;}
 c.visitHold=hold;c.asked.push_back(base);
 const auto seed=invitationHash(hold^(source<<8)^invitationHash(hour)^invitationHash(base)^invitationHash(c.carriedRules)^invitationHash(local)^0x43554C54u);
 auto& o=c.regions[hold].offer;
 o={base,source,c.carriedRules,local,chooseCultureRule(foreign,invitationHash(seed)),invitationHash(seed^1)%100,invitationHash(seed^2)%100,invitationHash(seed^3),1};
}
inline bool answerSpread(CollectionSave& s,int i,unsigned hour,bool accept){
 const auto rule=spreadOffer(s,i,hour);if(!rule)return false;
 const auto hold=opponents[i].hold;auto& o=s.culture.regions[hold].offer;o.status=2;
 if(accept)s.culture.pending={1,hold,opponents[i].base,withSpreadRule(o.local,rule),rule,o.roll,o.driftRoll,o.driftSeed,false};
 else finishRuleVisit(s,hold);
 return true;
}
inline bool canAbolish(const CollectionSave& s,int i,unsigned hour){
 return cultureOpponent(s,i)&&opponents[i].level>=4&&!s.culture.pending.kind&&localReputation(s,opponents[i].hold).famous()&&hour>=regionalCulture(s,opponents[i].hold).abolitionAfter&&holdTerms(s,opponents[i].hold).rules;
}
inline bool challengeAbolition(CollectionSave& s,int i,unsigned hour,unsigned rule){
 if(!singleRule(rule)||!canAbolish(s,i,hour))return false;
 const auto hold=opponents[i].hold,rules=holdTerms(s,hold).rules;if(!(rules&rule))return false;
 auto& region=s.culture.regions[hold];region.abolitionAfter=hour+cultureCooldown;region.offer.status=2;
 s.culture.pending={2,hold,opponents[i].base,rules,rule,0,0,0,false};return true;
}
inline bool canStartCultureMatch(const CollectionSave& s,int i,unsigned rules){
 const auto& p=s.culture.pending;
 return !p.kind||(i>=0&&i<int(opponents.size())&&!p.started&&p.opponent==opponents[i].base&&p.rules==rules);
}
inline bool startCultureMatch(CollectionSave& s,int i,unsigned rules){
 if(!canStartCultureMatch(s,i,rules))return false;
 if(s.culture.pending.kind)s.culture.pending.started=true;
 return true;
}
inline void abandonRuleCulture(CollectionSave& s,unsigned hour){
 const auto& p=s.culture.pending;
 if(p.kind==2){auto& r=s.culture.regions[p.hold];r.abolitionAfter=std::max(r.abolitionAfter,hour+cultureCooldown);}
 s.culture.pending={};
 if(s.culture.visitHold){const auto hold=s.culture.visitHold;s.culture.regions[hold].offer.status=2;finishRuleVisit(s,hold);}
}
struct CultureChange {unsigned hold=0,added=0,removed=0;bool abolitionFailed=false,spreadFailed=false;};
inline CultureChange finishRuleCulture(CollectionSave& s,int i,int winner,unsigned rules,unsigned hour){
 CultureChange change;if(!cultureOpponent(s,i)||winner< -1||winner>1)return change;
 const auto hold=opponents[i].hold;const auto p=s.culture.pending;
 if(p.kind&&(!p.started||p.opponent!=opponents[i].base||p.hold!=hold||p.rules!=rules))return change;
 auto terms=holdTerms(s,hold);const auto before=terms.rules;
 if(p.kind){
  change.hold=hold;auto& r=s.culture.regions[hold];
  if(p.kind==2){
   r.abolitionAfter=hour+cultureCooldown;
   if(winner==0)terms.rules=withoutCultureRule(terms.rules,p.rule);else change.abolitionFailed=true;
  }else if(p.roll<(winner==0?spreadWinChance:spreadOtherChance)){
   terms.rules=withSpreadRule(terms.rules,p.rule);
   // Do not incidentally abolish a dependency of the rule just introduced.
   const auto removable=before&~(p.rule==SameWall?Same:0u);
   if(!(before&conflictingRules(p.rule))&&p.driftRoll<incidentalAbolitionChance)terms.rules=withoutCultureRule(terms.rules,chooseCultureRule(removable,p.driftSeed));
   change.added=terms.rules&~before;
  }else change.spreadFailed=true;
  change.removed=before&~terms.rules;
  if(terms.rules!=before){s.holds[hold]=terms;r.added=change.added;r.removed=change.removed;r.changedAt=hour;}
  s.culture.pending={};
 }
 // Keep the incoming customs for the remaining local players. After the
 // third opportunity (or when no foreign rules remain), carry actual local
 // rules, never a temporary trial that failed to spread.
 finishRuleVisit(s,hold);return change;
}
inline std::string cultureChangeText(const CultureChange& c){
 if(c.abolitionFailed)return "The rule stays. You can challenge for abolition again in seven game days.";
 if(c.spreadFailed)return "The visiting rule did not catch on. Local rules are unchanged.";
 std::string text;for(unsigned bit:ruleFlags)if(c.added&bit){if(!text.empty())text+=' ';text+=std::string(ruleName(bit))+" has spread to "+holdName(c.hold)+".";}
 for(unsigned bit:ruleFlags)if(c.removed&bit){if(!text.empty())text+=' ';text+=std::string(ruleName(bit))+" has been abolished in "+holdName(c.hold)+".";}
 if((c.removed&(Open|ThreeOpen))&&!(c.added&(Open|ThreeOpen)))text+=" Unplayed hands are now hidden.";
 return text;
}
inline std::string recentCultureChange(const CollectionSave& s,unsigned hold){const auto& r=regionalCulture(s,hold);return cultureChangeText({hold,r.added,r.removed});}
inline std::string cultureJson(const CollectionSave& s,int i,unsigned hold,unsigned hour,bool available,std::string_view notice={}){
 const auto& cr=regionalCulture(s,hold);const auto& pending=s.culture.pending;
 const auto offer=available?spreadOffer(s,i,hour):0;
 return std::format("{{\"offerRule\":{},\"replaces\":{},\"source\":{},\"trialRule\":{},\"abolitionRule\":{},\"canAbolish\":{},\"expert\":{},\"famous\":{},\"retryHours\":{},\"localRules\":{},\"result\":{},\"carriedHold\":{},\"carriedRules\":{}}}",offer,holdTerms(s,hold).rules&conflictingRules(offer),quote(holdName(cr.offer.source)),pending.kind==1?pending.rule:0,pending.kind==2?pending.rule:0,available&&canAbolish(s,i,hour)?"true":"false",available&&i>=0&&opponents[i].level>=4?"true":"false",localReputation(s,hold).famous()?"true":"false",cr.abolitionAfter>hour?cr.abolitionAfter-hour:0,holdTerms(s,hold).rules,quote(notice),quote(s.culture.carriedHold?holdName(s.culture.carriedHold):""),s.culture.carriedRules);
}
}
