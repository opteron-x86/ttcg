#pragma once
#include "Match.h"
#include <map>
namespace ttcg {
struct SpreadOffer {
 unsigned opponent=0,source=0,carried=0,local=0,rule=0,roll=0,driftRoll=0,driftSeed=0,status=0;
};
struct RegionalCulture {
 unsigned abolitionAfter=0,added=0,removed=0,changedAt=0;
 SpreadOffer offer;
};
struct CultureAttempt {
 unsigned kind=0,hold=0,opponent=0,rules=0,rule=0,roll=0,driftRoll=0,driftSeed=0;
 bool started=false;
};
struct RuleCultureState {
 unsigned carriedHold=0,carriedRules=0;
 unsigned visitHold=0;
 std::vector<unsigned> asked;
 std::map<unsigned,RegionalCulture> regions;
 CultureAttempt pending;
};
inline bool singleRule(unsigned rule){return rule&&!(rule&(rule-1))&&!(rule&~RuleMask);}
inline void encodeRuleCulture(std::vector<std::uint32_t>& out,const RuleCultureState& c){
 out.push_back(c.carriedHold);out.push_back(c.carriedRules);out.push_back(c.visitHold);out.push_back(c.asked.size());out.insert(out.end(),c.asked.begin(),c.asked.end());out.push_back(static_cast<unsigned>(c.regions.size()));
 for(const auto& [id,r]:c.regions){
  for(auto v:{id,r.abolitionAfter,r.added,r.removed,r.changedAt})out.push_back(v);
  const auto& o=r.offer;for(auto v:{o.opponent,o.source,o.carried,o.local,o.rule,o.roll,o.driftRoll,o.driftSeed,o.status})out.push_back(v);
 }
 const auto& p=c.pending;for(auto v:{p.kind,p.hold,p.opponent,p.rules,p.rule,p.roll,p.driftRoll,p.driftSeed,unsigned(p.started)})out.push_back(v);
}
template<class Get> bool decodeRuleCulture(Get get,RuleCultureState& c,bool legacy=false){
 c.carriedHold=get();c.carriedRules=get();
 if(c.carriedHold>10||!validRules(c.carriedRules)||(!c.carriedHold&&c.carriedRules))return false;
 if(!legacy){
  c.visitHold=get();const auto count=get();if(c.visitHold>10||count>3||(!c.visitHold&&count)||(c.visitHold&&(!c.carriedHold||c.visitHold==c.carriedHold||!count)))return false;
  for(unsigned n=0;n<count;++n){auto id=get();if(!id||std::find(c.asked.begin(),c.asked.end(),id)!=c.asked.end())return false;c.asked.push_back(id);}
 }
 const auto size=get();if(size>10)return false;
 for(unsigned n=0;n<size;++n){
  auto id=get();if(!id||id>10||c.regions.contains(id))return false;auto& r=c.regions[id];
  if(legacy&&get()>100000168)return false; // Retired regional spread cooldown.
  r.abolitionAfter=get();r.added=get();r.removed=get();r.changedAt=get();
  if(r.abolitionAfter>100000168||r.changedAt>100000000||(r.added&~RuleMask)||(r.removed&~RuleMask)||(r.added&r.removed))return false;
  auto& o=r.offer;o.opponent=get();o.source=get();o.carried=get();o.local=get();o.rule=get();o.roll=get();o.driftRoll=get();o.driftSeed=get();o.status=get();
  if(o.source>10||!validRules(o.carried)||!validRules(o.local)||(o.rule&&!singleRule(o.rule))||o.roll>=100||o.driftRoll>=100||o.status>2)return false;
  if(o.status==1&&(!o.opponent||!o.source||o.source==id||!o.rule||!(o.carried&o.rule)||(o.local&o.rule)))return false;
  if(legacy)o={}; // Old daily offers have no individual player/visit identity.
  else if(o.status==1&&(c.visitHold!=id||o.source!=c.carriedHold||o.carried!=c.carriedRules||std::find(c.asked.begin(),c.asked.end(),o.opponent)==c.asked.end()))return false;
 }
 if(c.visitHold&&!c.regions.contains(c.visitHold))return false;
 auto& p=c.pending;p.kind=get();p.hold=get();p.opponent=get();p.rules=get();p.rule=get();p.roll=get();p.driftRoll=get();p.driftSeed=get();auto started=get();p.started=started!=0;
 if(p.kind>2||p.hold>10||!validRules(p.rules)||p.roll>=100||p.driftRoll>=100||started>1)return false;
 if(p.kind){if(!p.hold||!p.opponent||!singleRule(p.rule)||!(p.rules&p.rule)||!c.regions.contains(p.hold))return false;}
 else if(p.hold||p.opponent||p.rules||p.rule||p.roll||p.driftRoll||p.driftSeed||p.started)return false;
 return true;
}
}
