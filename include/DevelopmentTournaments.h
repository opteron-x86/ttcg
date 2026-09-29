#pragma once
#include "Development.h"
#include "Tournaments.h"
namespace ttcg {
// Keep a regular test at the inn being visited, and one courier event per
// enabled higher circuit. Staging ten simultaneous fields would exhaust the
// real opponent roster and hide failures behind duplicate participants.
inline std::vector<unsigned> updateDevelopmentTournaments(CollectionSave& s,const DevelopmentSettings& settings,unsigned hour,unsigned venue){
 if(s.contract.pending())return {};
 const auto enabled=settings.tournamentSwitches();auto& events=s.tournaments.events;
 const bool testing=std::any_of(enabled.begin(),enabled.end(),[](bool on){return on;});
 if(testing&&!settings.albumAccess(s))grantStarter(s);
 std::erase_if(events,[&](const auto& e){
  if(!e.development||e.joined)return false;
  const bool discard=!enabled[e.circuit]||(e.ends&&hour>=e.ends)||
    (e.circuit?e.hold!=settings.tournamentHold:venue&&e.hold!=venue);
  // An unplayed test is cancelled, not simulated for rewards. Release its
  // unclaimed trophy so changing the test venue cannot exhaust the prize pool.
  if(discard&&!e.awarded&&e.trophy)s.tournaments.trophies.erase(baseCardID(e.trophy));
  return discard;
 });
 std::vector<unsigned> created;
 if(!testing||tournamentEntryActive(s)||pendingTournamentFoils(s))return created;
 // Masters have the smallest field. Reserve them before choosing visitors for
 // the other circuits; every bracket still uses distinct, available real NPCs.
 for(unsigned circuit:{2u,1u,0u}){
  if(!enabled[circuit]||(!circuit&&!venue))continue;
  const bool existing=std::any_of(events.begin(),events.end(),[&](const auto& e){return e.development&&e.circuit==circuit&&(!e.awarded||e.announcement);});
  if(existing)continue;
  if(auto id=newTournament(s,circuit?settings.tournamentHold:venue,hour,true,circuit)){
   if(!circuit)tournament(s,id)->discovered=true;
   created.push_back(id);
  }
 }
 return created;
}
}
