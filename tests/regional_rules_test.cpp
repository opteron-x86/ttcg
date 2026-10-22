#include "RuleCulture.h"
#include "Tournaments.h"
#include "reputation_support.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int localExpert(unsigned hold){for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==hold&&opponents[i].level>=4)return i;assert(false);return -1;}
CollectionSave reload(const CollectionSave& s){CollectionSave t;assert(decodeCollection(encodeCollection(s),t));assert(encodeCollection(t)==encodeCollection(s));return t;}
void output(const CollectionSave& s,unsigned hold,unsigned hour){
 const auto terms=holdTerms(s,hold);
 std::cout<<"{\"id\":"<<hold<<",\"name\":"<<quote(holdName(hold))<<",\"rules\":"<<terms.rules<<",\"trade\":"<<terms.trade<<",\"ruleChanges\":"<<regionalChangesJson(s,hold,hour,shiftGameDate({},hour))<<"}\n";
}
int main(int argc,char**){
 if(argc>1){
  CollectionSave s;initializeRegionalCalendar(s,eastmarchHold,0);
  assert(applyRegionalTerms(s,eastmarchHold,{Open|Decimation,TradeDirect},0));output(s,eastmarchHold,0);
  output(s,eastmarchHold,regionalRuleProtectionHours);output(s,eastmarchHold,regionalTradeProtectionHours);
  assert(applyRegionalTerms(s,eastmarchHold,{Same|SameWall|Plus|Reverse|FallenAce|Affinity|Legion|Order|Swap|SuddenDeath,TradeAll},regionalTradeProtectionHours));output(s,eastmarchHold,regionalTradeProtectionHours);
  return 0;
 }
 CollectionSave s;s.starter=true;
 assert(!tickRegionalRules(s,10));assert(s.culture.regions.size()==holds.size());
 std::set<unsigned> dates;
 for(auto h:holds){const auto& r=regionalCulture(s,h.id);assert(holdTerms(s,h.id).rules==h.defaults.rules);assert(holdTerms(s,h.id).trade==h.defaults.trade);
  assert(r.nextRuleChange>=10+14*24&&r.nextRuleChange<=10+21*24);assert(r.nextTradeChange>=10+30*24&&r.nextTradeChange<=10+45*24);dates.insert(r.nextRuleChange);
 }
 assert(dates.size()>5);s=reload(s);const auto scheduled=encodeCollection(s);assert(!tickRegionalRules(s,10));assert(encodeCollection(s)==scheduled);
 // All seventeen rules can be introduced, and existing rules can disappear.
 unsigned observed=0,removed=0;
 for(unsigned seed=1;seed<5000;++seed){
  CollectionSave t;unsigned random=seed;
  const auto next=naturalRegionalRules(t,hjaalmarchHold,0,random);assert(validRules(next));observed|=next;
  t.holds[1]={Open|Same|SameWall|Plus|Reverse|FallenAce|Affinity|Legion|Order|Swap|SuddenDeath,TradeOne};
  const auto old=t.holds[1].rules;removed|=old&~naturalRegionalRules(t,1,0,random);
 }
 assert(observed==RuleMask);assert(removed==(Open|Same|SameWall|Plus|Reverse|FallenAce|Affinity|Legion|Order|Swap|SuddenDeath));
 // Protection covers dependency removals, conflicts, spread and abolition.
 s={};s.starter=true;const auto expert=localExpert(1);earnReputation(s,1);
 assert(applyRegionalTerms(s,1,{Open|SameWall|Same,TradeDiff},20));
 assert(!(regionalRemovals(s,1,20)&(Same|SameWall)));assert(!challengeAbolition(s,expert,20,Same));
 assert(!applyRegionalTerms(s,1,{Open,TradeDiff},20));
 assert(!applyRegionalTerms(s,1,{Open|SameWall|Same,TradeDirect},20));
 s=reload(s);assert(protectedRegionalRules(s,1,20)==(Same|SameWall));
 assert(!regionalRulesAllowed(s,1,withoutCultureRule(holdTerms(s,1).rules,Same),20+regionalRuleProtectionHours-1));
 assert(regionalRulesAllowed(s,1,Open,20+regionalRuleProtectionHours));
 assert(applyRegionalTerms(s,1,{Open,TradeDiff},20+regionalRuleProtectionHours));
 assert(!regionalAdditions(s,1,Same|SameWall,20+regionalRuleProtectionHours));
 s.culture.carriedHold=2;s.culture.carriedRules=Same|SameWall;prepareRuleSpread(s,expert,20+regionalRuleProtectionHours);
 assert(!spreadOffer(s,expert,20+regionalRuleProtectionHours));
 // Conflict replacement cannot remove a newly protected visibility rule.
 s.holds[2]={ThreeOpen,TradeOne};assert(applyRegionalTerms(s,2,{Open,TradeOne},100));
 assert(!regionalAdditions(s,2,ThreeOpen,101));
 assert(regionalAdditions(s,2,ThreeOpen,100+regionalRuleProtectionHours)==ThreeOpen);
 // Unrelated edits retain each rule's own expiry and reset only its calendar.
 s={};assert(applyRegionalTerms(s,1,{Open|Plus,TradeOne},100));
 const auto tradeDate=regionalCulture(s,1).nextTradeChange;
 assert(applyRegionalTerms(s,1,{Open|Plus|Swap,TradeOne},150));
 assert(regionalCulture(s,1).nextTradeChange==tradeDate);
 assert(protectedRegionalRules(s,1,100+regionalRuleProtectionHours)==Swap);
 assert(protectedRegionalRules(s,1,150+regionalRuleProtectionHours)==0);
 // Date text and markers use the same expiry as actual change eligibility.
 assert(regionalChangesJson(s,1,100+regionalRuleProtectionHours).find("\"rule\":2,")==std::string::npos);
 assert(regionalChangesJson(s,1,150+regionalRuleProtectionHours)=="[]");
 // A locked lobby defers this hold, while all other regions keep evolving.
 s={};tickRegionalRules(s,0);auto original=holdTerms(s,1);const auto due=regionalCulture(s,1).nextRuleChange;
 tickRegionalRules(s,60*24,1);assert(holdTerms(s,1).rules==original.rules&&holdTerms(s,1).trade==original.trade);
 assert(regionalCulture(s,1).deferred==3);assert(s.holds.size()>=9);s=reload(s);
 tickRegionalRules(s,60*24);const auto& resumed=regionalCulture(s,1);assert(!resumed.deferred);
 assert(resumed.changedAt==60*24&&resumed.nextRuleChange>=74*24&&resumed.nextTradeChange>=90*24);
 // A committed or interrupted trial keeps its terms until resolved.
 s={};s.starter=true;s.culture.carriedHold=2;s.culture.carriedRules=Same;prepareRuleSpread(s,expert,0);
 assert(answerSpread(s,expert,0,true));tickRegionalRules(s,0);const auto trial=s.culture.pending.rules;
 tickRegionalRules(s,60*24);assert(holdTerms(s,1).rules==Open&&s.culture.pending.rules==trial);s=reload(s);
 assert(startCultureMatch(s,expert,trial));s.culture.pending.roll=0;
 auto result=finishRuleCulture(s,expert,0,trial,60*24);assert(result.added==Same);
 tickRegionalRules(s,60*24);assert(holdTerms(s,1).rules&Same);assert(regionalCulture(s,1).nextRuleChange>=74*24);
 // A large time jump and ordinary hourly updates produce the same calendar.
 CollectionSave jump,incremental;tickRegionalRules(jump,0);incremental=jump;
 for(unsigned hour=1;hour<=365*24;++hour)tickRegionalRules(incremental,hour);
 tickRegionalRules(jump,365*24);assert(encodeCollection(jump)==encodeCollection(incremental));
 const auto final=encodeCollection(jump);assert(!tickRegionalRules(jump,365*24)&&encodeCollection(jump)==final);
 // Observe all trade modes; no-stakes mode is never chosen for a hold.
 std::set<unsigned> trades;
 for(unsigned hour=366*24;hour<3000*24;hour+=24){tickRegionalRules(jump,hour);for(auto h:holds){auto terms=holdTerms(jump,h.id);assert(validRules(terms.rules)&&terms.trade>=TradeOne&&terms.trade<=TradeAll);trades.insert(terms.trade);}}
 assert(trades.size()==4);reload(jump);
 // Tournament and personal-opponent terms are not rewritten by the calendar.
 CollectionSave isolated;tickRegionalRules(isolated,0);decltype(isolated.tournaments.events)::value_type event;event.id=1;event.rules=Open|Reverse|Chaos;isolated.tournaments.events.push_back(event);
 int traveller=-1;for(unsigned i=0;i<opponents.size();++i)if(opponents[i].traveller){traveller=i;break;}
 assert(traveller>=0);const auto personal=opponentTerms(isolated,traveller);tickRegionalRules(isolated,200*24);
 assert(isolated.tournaments.events[0].rules==event.rules);assert(opponentTerms(isolated,traveller).rules==personal.rules);
 // Missing regional protections and schedules start empty.
 auto wire=encodeCollection(s);std::vector<std::uint32_t> appendix;encodeRegionalCalendars(appendix,s.culture);wire.resize(wire.size()-appendix.size()-3);
 CollectionSave legacy;assert(decodeCollection(wire,legacy,30));assert(!regionalCulture(legacy,1).nextRuleChange);assert(holdTerms(legacy,1).rules==holdTerms(s,1).rules);
 tickRegionalRules(legacy,60*24);assert(regionalCulture(legacy,1).nextRuleChange>=74*24);
 // Reject malformed deadlines, seeds and truncated appended data atomically.
 for(unsigned kind=0;kind<4;++kind){auto bad=s;auto& r=bad.culture.regions[1];if(kind==0)r.nextRuleChange=regionalClockLimit+1;if(kind==1)r.ruleRandom=0;if(kind==2)r.deferred=4;if(kind==3)r.ruleProtection[0]=regionalClockLimit+1;
  const auto good=encodeCollection(legacy);assert(!decodeCollection(encodeCollection(bad),legacy));assert(encodeCollection(legacy)==good);
 }
 auto shortWire=encodeCollection(s);shortWire.pop_back();assert(!decodeCollection(shortWire,legacy));
 tickRegionalRules(s,100000000);reload(s);
 std::cout<<"Regional rules: independent clocks, all rules/trades, dependencies, protected spread/abolition, deferred sessions, deterministic catch-up, expiry, personal/tournament isolation and save migration passed\n";
}
