#include "DevelopmentTournaments.h"
#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank{int gold=0;int count(int,CardID){return gold;}int change(int,CardID,int n){gold+=n;return n;}};
unsigned eventFor(const CollectionSave& s,unsigned circuit){
 for(const auto& e:s.tournaments.events)if(e.development&&e.circuit==circuit&&!e.awarded)return e.id;
 return 0;
}
void savedCorrectly(const CollectionSave& s){CollectionSave loaded;const auto before=encodeCollection(s);assert(decodeCollection(before,loaded)&&encodeCollection(loaded)==before);}
int main(){
 DevelopmentSettings settings;settings.enabled=true;settings.regularTournaments=true;
 CollectionSave s;Bank bank;
 // All ten actual inn routes, without couriers, roster duplication or remote
 // fields filling the list. Repeated updates keep the same event and deadline.
 for(unsigned hold=1;hold<=10;++hold){
  const auto made=updateDevelopmentTournaments(s,settings,100,hold);assert(made.size()==1);
  const auto id=made.front();const auto* e=tournament(s,id);assert(e&&e->hold==hold&&!e->fee&&!e->invitation&&e->ends==172&&e->discovered);
  assert(s.starter&&s.tournaments.events.size()==1&&tournamentEntryAvailable(*e,100));
  assert(tournamentsJson(s,100,hold,hold).find("\"canEnter\":true")!=std::string::npos);
  assert(tournamentsJson(s,100,hold).find("\"canEnter\":false")!=std::string::npos);
  assert(tournamentsJson(s,100,hold%10+1,hold).find("\"canEnter\":false")!=std::string::npos);
  assert(updateDevelopmentTournaments(s,settings,101,hold).empty()&&tournament(s,id)->ends==172);
  savedCorrectly(s);
 }
 auto old=eventFor(s,0);assert(updateDevelopmentTournaments(s,settings,172,10).size()==1&&!tournament(s,old));
 auto id=eventFor(s,0);assert(enterTournament(s,id,172,bank)&&bank.gold==0);
 assert(updateDevelopmentTournaments(s,settings,173,1).empty()&&tournament(s,id)->hold==10);
 settings.regularTournaments=false;assert(updateDevelopmentTournaments(s,settings,174,10).empty()&&tournament(s,id)->joined);
 assert(withdrawTournament(s,id));settings.regularTournaments=true;
 assert(updateDevelopmentTournaments(s,settings,175,10).empty()); // Result must be acknowledged first.
 leaveTournamentRewards(*tournament(s,id));assert(updateDevelopmentTournaments(s,settings,175,10).size()==1);
 assert(s.tournaments.events.size()==2&&tournament(s,id)->awarded);savedCorrectly(s);
 settings.enabled=false;updateDevelopmentTournaments(s,settings,175,10);assert(s.tournaments.events.size()==1&&tournament(s,id)->joined);

 // Each higher circuit works independently, with an actual queued/delivered
 // letter, no reputation or fee, and the selected venue enforced in the UI.
 for(unsigned circuit:{1u,2u})for(unsigned hold=1;hold<=10;++hold){
  CollectionSave test;settings={};settings.enabled=true;settings.tournamentHold=hold;
  settings.invitationalTournaments=circuit==1;settings.mastersTournaments=circuit==2;
  auto made=updateDevelopmentTournaments(test,settings,200,0);assert(made.size()==1);
  const auto event=made.front();auto* e=tournament(test,event);const auto letter=tournamentLetter(*e);
  assert(e->circuit==circuit&&e->hold==hold&&!e->fee&&e->invitation==1&&!e->starts&&!e->ends);
  assert(!enterTournament(test,event,200,bank)&&tournamentsJson(test,200,hold,hold).find("\"events\":[]")!=std::string::npos);
  assert(updateDevelopmentTournaments(test,settings,1000,hold).empty()); // Never expires in the mail.
  assert(reconcileTournamentInvitations(test,letter,0,1,1000).empty()&&e->invitation==2);
  assert(!enterTournament(test,event,1000,bank));savedCorrectly(test);
  assert(reconcileTournamentInvitations(test,letter,1,0,1001)==std::vector<unsigned>{event});
  assert(e->ends==1169&&tournamentEntryAvailable(*e,1001));
  assert(enterTournament(test,event,1001,bank)&&bank.gold==0);savedCorrectly(test);
 }
 // All switches coexist and survive reload without duplicate NPCs. Changing
 // the courier venue retires only unentered tests and releases their prizes.
 CollectionSave all;settings={};settings.enabled=settings.regularTournaments=settings.invitationalTournaments=settings.mastersTournaments=true;
 auto made=updateDevelopmentTournaments(all,settings,100,1);assert(made.size()==3);
 savedCorrectly(all);assert(updateDevelopmentTournaments(all,settings,101,1).empty());
 const auto masters=eventFor(all,2),inv=eventFor(all,1),regular=eventFor(all,0);
 settings.tournamentHold=6;assert(updateDevelopmentTournaments(all,settings,101,1).size()==2);
 assert(!tournament(all,masters)&&!tournament(all,inv)&&tournament(all,regular));savedCorrectly(all);
 for(unsigned h=1;h<=10;++h){updateDevelopmentTournaments(all,settings,101,h);assert(all.tournaments.events.size()==3);savedCorrectly(all);}
 // Forced tests don't add a second scheduled event or consume scheduled dates.
 tickTournaments(all,101,10,settings.tournamentSwitches());all.tournaments.nextRegular.fill(101);all.tournaments.nextCircuit.fill(101);
 const auto sequence=all.tournaments.sequence;tickTournaments(all,102,10,settings.tournamentSwitches());
 assert(all.tournaments.sequence==sequence&&all.tournaments.nextRegular[0]==101&&all.tournaments.nextCircuit[0]==101);
 settings={};updateDevelopmentTournaments(all,settings,102,10);assert(all.tournaments.events.empty());
 tickTournaments(all,102,10);assert(all.tournaments.sequence>sequence&&all.tournaments.nextRegular[0]>102);
 savedCorrectly(all);
 // Creation failure can retry without a consumed one-shot token or phantom
 // invitation. Dead/unavailable opponents are never revived by test settings.
 CollectionSave unavailable;settings.enabled=settings.mastersTournaments=true;
 for(unsigned i=0;i<opponents.size();++i)if(opponents[i].level==5)progress(unavailable,i).unavailable=true;
 assert(updateDevelopmentTournaments(unavailable,settings,100,1).empty());
 for(unsigned i=0;i<opponents.size();++i)progress(unavailable,i).unavailable=false;
 assert(updateDevelopmentTournaments(unavailable,settings,101,1).size()==1);savedCorrectly(unavailable);
 std::cout<<"Development tournaments: all ten inns, independent courier circuits, host gates, repeat runs, expiry, reload, toggles, reservations, calendar suppression and failed-field retry passed\n";
}
