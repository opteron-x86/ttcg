#include "RuleCulture.h"
#include "reputation_support.h"
#include "Lesson.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int adult(unsigned hold,unsigned minimum=0){for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==hold&&opponents[i].level>=minimum)return i;assert(false);return -1;}
void famous(CollectionSave& s,unsigned hold){earnReputation(s,hold);}
CollectionSave roundtrip(const CollectionSave& s){CollectionSave t;assert(decodeCollection(encodeCollection(s),t));assert(encodeCollection(s)==encodeCollection(t));return t;}
unsigned offer(CollectionSave& s,int i,unsigned hour=0){prepareRuleSpread(s,i,hour);assert(spreadOffer(s,i,hour));return hour;}
int cultureFixtures(){
 const int i=adult(eastmarchHold);CollectionSave s;s.starter=true;s.culture.carriedHold=solstheimRegion;s.culture.carriedRules=Decimation;
 const auto hour=offer(s,i);s.culture.regions[eastmarchHold].offer.roll=0;s.culture.regions[eastmarchHold].offer.driftRoll=0;
 auto output=[&](unsigned time,std::string text=""){std::cout<<"{\"culture\":"<<cultureJson(s,i,eastmarchHold,time,true,text)<<",\"fixedRules\":"<<(s.culture.pending.kind?s.culture.pending.rules:holdTerms(s,eastmarchHold).rules)<<",\"holdID\":8,\"hold\":\"Eastmarch\"}"<<'\n';};
 output(hour);assert(answerSpread(s,i,hour,true));output(hour);assert(startCultureMatch(s,i,s.culture.pending.rules));auto c=finishRuleCulture(s,i,0,s.culture.pending.rules,hour);output(hour,cultureChangeText(c));
 const auto expert=adult(eastmarchHold,4);famous(s,eastmarchHold);
 auto expertOutput=[&](unsigned time,std::string text=""){std::cout<<"{\"culture\":"<<cultureJson(s,expert,eastmarchHold,time,true,text)<<",\"fixedRules\":"<<holdTerms(s,eastmarchHold).rules<<",\"holdID\":8,\"hold\":\"Eastmarch\"}"<<'\n';};
 expertOutput(hour);assert(challengeAbolition(s,expert,hour,Open));expertOutput(hour);assert(startCultureMatch(s,expert,s.culture.pending.rules));c=finishRuleCulture(s,expert,1,s.culture.pending.rules,hour);expertOutput(hour,cultureChangeText(c));
 return 0;
}
int main(int argc,char**){
 if(argc>1)return cultureFixtures();
 // Hidden hands is the zero state. Open and Three Open each reveal symmetrically.
 for(unsigned rule:{0u,unsigned(Open),unsigned(ThreeOpen)}){Match m(17,rule);for(int p=0;p<2;++p){int visible=0;for(int h=0;h<5;++h){assert(m.known(p,h,p));visible+=m.known(p,h,1-p);}assert(visible==(rule==Open?5:rule==ThreeOpen?3:0));}}
 assert(!validRules(Open|ThreeOpen)&&!validRules(Legion|Decimation));
 assert(validRules(Affinity|Legion)&&validRules(Affinity|Decimation));
 // Every modifier path uses the sum, with no clamping at 0/A.
 for(auto group:{Legion,Decimation}){auto d=Lesson::workshop(group);assert(d.advance(1,4));auto& m=d.match;m.rules|=Affinity;m.tiles={};const auto id=m.board[4].card;const auto mask=cards[id].affinityMask;assert(mask);
  m.tiles[4]=mask&(~mask+1);assert(m.modifier(id,4)==(group==Legion?2:0));
  for(unsigned bit=1;bit<=16;bit<<=1)if(!(mask&bit)){m.tiles[4]=bit;assert(m.modifier(id,4)==(group==Legion?0:-2));}
  m.tiles[4]=0;assert(m.modifier(id,4)==(group==Legion?1:-1));
 }
 const auto east=adult(eastmarchHold),falk=adult(falkreathHold),expert=adult(falkreathHold,4),otherExpert=adult(haafingarHold,4);
 CollectionSave s;s.starter=true;
 assert(holdTerms(s,whiterunHold).rules==Open&&holdTerms(s,hjaalmarchHold).rules==0);
 finishRuleCulture(s,falk,1,Open|Same,0);assert(s.culture.carriedHold==falkreathHold&&s.culture.carriedRules==(Open|Same));
 assert(s.culture.carriedHold==falkreathHold);
 // Individual offers and outcome rolls survive saves, menus and midnight.
 auto hour=offer(s,east);auto saved=roundtrip(s);auto before=encodeCollection(s);prepareRuleSpread(s,east,hour);assert(encodeCollection(s)==before);assert(spreadOffer(saved,east,hour)==Same);
 prepareRuleSpread(s,east,hour+240);assert(encodeCollection(s)==before&&spreadOffer(s,east,hour+240)==Same);
 assert(answerSpread(s,east,hour,false));assert(!spreadOffer(s,east,hour));prepareRuleSpread(s,east,hour);assert(!spreadOffer(s,east,hour));
 // Successful conflict replacement is mandatory and never triggers an extra abolition.
 for(auto incoming:{Legion,Decimation,Open,ThreeOpen}){
  CollectionSave t;t.starter=true;t.culture.carriedHold=falkreathHold;t.culture.carriedRules=incoming;
  const auto old=conflictingRules(incoming);t.holds[eastmarchHold]={old|Same,TradeDiff};auto h=offer(t,east);
  auto& o=t.culture.regions[eastmarchHold].offer;o.roll=0;o.driftRoll=0;
  assert(answerSpread(t,east,h,true));assert(t.culture.pending.rules==(incoming|Same));assert(holdTerms(t,eastmarchHold).rules==(old|Same));
  t=roundtrip(t);assert(startCultureMatch(t,east,incoming|Same));assert(!startCultureMatch(t,east,incoming|Same));
  const auto c=finishRuleCulture(t,east,0,incoming|Same,h);assert(c.added==incoming&&c.removed==old);
  assert(holdTerms(t,eastmarchHold).rules==(incoming|Same)&&holdTerms(t,eastmarchHold).trade==TradeDiff);
  assert(t.culture.carriedRules==(incoming|Same)&&!t.culture.pending.kind);
  assert(!t.culture.visitHold&&t.culture.asked.empty());roundtrip(t);
 }
 // A successful ordinary addition may rarely abolish one older rule; never itself.
 s=saved;auto& o=s.culture.regions[eastmarchHold].offer;o.roll=0;o.driftRoll=0;o.driftSeed=0;
 assert(answerSpread(s,east,hour,true));assert(startCultureMatch(s,east,s.culture.pending.rules));auto trial=s.culture.pending.rules;
 auto change=finishRuleCulture(s,east,-1,trial,hour);assert(change.added==Same&&std::popcount(change.removed)==1&&!(change.removed&Same));assert(holdTerms(s,eastmarchHold).rules&Same);roundtrip(s);
 // A failed first trial retains the incoming customs for two more players.
 s=saved;s.culture.regions[eastmarchHold].offer.roll=99;assert(answerSpread(s,east,hour,true));trial=s.culture.pending.rules;
 auto original=s;assert(!startCultureMatch(s,falk,trial));assert(encodeCollection(s)==encodeCollection(original));
 assert(startCultureMatch(s,east,trial));change=finishRuleCulture(s,east,0,trial,hour);assert(change.spreadFailed);assert(s.culture.carriedRules==(Open|Same)&&s.culture.carriedHold==falkreathHold);
 // Win bonus: a roll of 25 succeeds only on a win.
 for(int winner:{-1,0,1}){s=saved;s.culture.regions[eastmarchHold].offer.roll=25;s.culture.regions[eastmarchHold].offer.driftRoll=99;assert(answerSpread(s,east,hour,true));trial=s.culture.pending.rules;assert(startCultureMatch(s,east,trial));change=finishRuleCulture(s,east,winner,trial,hour);assert(bool(change.added)==(winner==0));}
 // Up to three distinct players ask on the same visit, without a daily gate.
 std::vector<int> locals;for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==eastmarchHold&&cultureOpponent(saved,i))locals.push_back(i);assert(locals.size()>=4);
 for(unsigned response=0;response<3;++response){
  auto visit=saved;visit.culture.regions.clear();visit.culture.visitHold=0;visit.culture.asked.clear();
  for(unsigned n=0;n<3;++n){
   const int i=locals[n];offer(visit,i,hour+n);assert(visit.culture.asked.size()==n+1);
   assert(!spreadOffer(visit,locals[(n+1)%4],hour+n)); // This NPC's offer is not shared.
   visit=roundtrip(visit);const auto wire=encodeCollection(visit);prepareRuleSpread(visit,i,hour+240);assert(encodeCollection(visit)==wire);
   if(response==0)assert(answerSpread(visit,i,hour+n,false));
   else if(response==1){visit.culture.regions[eastmarchHold].offer.roll=99;assert(answerSpread(visit,i,hour+n,true));assert(visit.culture.carriedHold==falkreathHold);trial=visit.culture.pending.rules;assert(startCultureMatch(visit,i,trial));assert(finishRuleCulture(visit,i,0,trial,hour+n).spreadFailed);}
   else abandonRuleCulture(visit,hour+n); // Closing the unanswered lobby also consumes that player's offer.
   prepareRuleSpread(visit,i,hour+240);assert(!spreadOffer(visit,i,hour+240));
   if(n<2){assert(visit.culture.carriedHold==falkreathHold);finishRuleCulture(visit,i,0,holdTerms(visit,eastmarchHold).rules,hour+n);assert(visit.culture.carriedHold==falkreathHold);}
   visit=roundtrip(visit);
  }
  assert(visit.culture.carriedHold==eastmarchHold&&visit.culture.carriedRules==holdTerms(visit,eastmarchHold).rules&&visit.culture.asked.empty());
  prepareRuleSpread(visit,locals[3],hour+240);assert(!spreadOffer(visit,locals[3],hour+240));
 }
 // A success can leave more foreign rules to try: each new player gets a
 // chance immediately, and the actual regional result becomes carried rules.
 auto visit=saved;visit.culture.regions.clear();visit.culture.visitHold=0;visit.culture.asked.clear();visit.culture.carriedRules=Same|Plus|Reverse;
 for(unsigned n=0;n<3;++n){const auto i=locals[n];offer(visit,i,hour);auto& o=visit.culture.regions[eastmarchHold].offer;o.roll=0;o.driftRoll=99;assert(answerSpread(visit,i,hour,true));trial=visit.culture.pending.rules;assert(startCultureMatch(visit,i,trial));assert(finishRuleCulture(visit,i,0,trial,hour).added);visit=roundtrip(visit);}
 assert(visit.culture.carriedHold==eastmarchHold&&visit.culture.carriedRules==(Open|Legion|Same|Plus|Reverse));
 // Leaving after fewer offers adopts the departed hold's real customs before
 // visiting the next one. Waiting in place never resets a visit.
 visit=saved;abandonRuleCulture(visit,hour);prepareRuleSpread(visit,falk,hour);assert(visit.culture.carriedHold==eastmarchHold&&spreadOffer(visit,falk,hour)==Legion);roundtrip(visit);
 // Cooldowns are regional, apply from acceptance, and survive abandonment/reload.
 s={};s.starter=true;assert(!canAbolish(s,expert,0));famous(s,falkreathHold);assert(canAbolish(s,expert,0));assert(!canAbolish(s,falk,0)||opponents[falk].level>=4);
 assert(!challengeAbolition(s,expert,0,Open|Same));assert(!challengeAbolition(s,expert,0,Decimation));
 assert(challengeAbolition(s,expert,0,Same));assert(s.culture.pending.rules==(Open|Same));assert(s.culture.regions[falkreathHold].abolitionAfter==168);
 s=roundtrip(s);assert(!canAbolish(s,expert,0));abandonRuleCulture(s,2);assert(!canAbolish(s,expert,169)&&canAbolish(s,expert,170));
 for(int i=1;i<int(opponents.size());++i)if(opponents[i].hold==falkreathHold)assert(!canAbolish(s,i,169));
 famous(s,haafingarHold);assert(canAbolish(s,otherExpert,2));
 for(int winner:{1,-1,0}){
  const auto h=s.culture.regions[falkreathHold].abolitionAfter;assert(challengeAbolition(s,expert,h,Same));
  assert(startCultureMatch(s,expert,Open|Same));change=finishRuleCulture(s,expert,winner,Open|Same,h+1);
  assert(change.abolitionFailed==(winner!=0));assert(bool(holdTerms(s,falkreathHold).rules&Same)==(winner!=0));
  assert(!canAbolish(s,expert,h+168));roundtrip(s);
 }
 auto h=s.culture.regions[falkreathHold].abolitionAfter;assert(challengeAbolition(s,expert,h,Open));assert(startCultureMatch(s,expert,Open));change=finishRuleCulture(s,expert,0,Open,h);assert(change.removed==Open&&holdTerms(s,falkreathHold).rules==0);assert(!canAbolish(s,expert,h+168));
 // A pending trial is not a completed game; abandoning it never changes custom.
 s=saved;assert(answerSpread(s,east,hour,true));const auto local=holdTerms(s,eastmarchHold).rules;abandonRuleCulture(s,hour);assert(holdTerms(s,eastmarchHold).rules==local&&!spreadOffer(s,east,hour));
 // Corrupt/truncated culture state must not overwrite a good save.
 s=roundtrip(saved);const auto encoded=encodeCollection(s);for(unsigned cut=1;cut<=12;++cut){auto bad=encoded;bad.resize(bad.size()-cut);assert(!decodeCollection(bad,s));assert(encodeCollection(s)==encoded);}
 auto bad=s;bad.culture.carriedRules=Open|ThreeOpen;assert(!decodeCollection(encodeCollection(bad),s));bad=s;bad.culture.pending={2,2,opponents[expert].base,0,Same};assert(!decodeCollection(encodeCollection(bad),s));
 bad=s;bad.culture.asked.assign(4,opponents[east].base);assert(!decodeCollection(encodeCollection(bad),s));bad=s;bad.culture.asked.push_back(opponents[east].base);assert(!decodeCollection(encodeCollection(bad),s));bad=s;bad.culture.visitHold=bad.culture.carriedHold;assert(!decodeCollection(encodeCollection(bad),s));
 std::cout<<"Rule culture: visibility, additive modifiers, travel, persistent offers, conflicts, drift, Famous challenges, regional cooldowns and save validation passed\n";
}
