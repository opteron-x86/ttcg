#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank{int gold=10000;int count(int,CardID){return gold;}int change(int,CardID,int n){n=std::max(n,-gold);gold+=n;return n;}};
CollectionSave world(){CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;return s;}
void checkReservations(const CollectionSave& s){
 std::set<unsigned> npcs,prizes;
 for(const auto& e:s.tournaments.events)if(!e.awarded){for(unsigned n=0;n<8;++n)if(e.bracket[n]!=tournamentPlayer)assert(npcs.insert(e.bracket[n]).second);if(e.trophy)assert(prizes.insert(baseCardID(e.trophy)).second);}
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(encodeCollection(loaded)==encodeCollection(s));
}
int main(){
 Bank bank;auto s=world();const auto id=newTournament(s,1,100);auto* e=tournament(s,id);
 assert(e&&e->starts==100&&e->ends==172&&!e->invitation&&!e->discovered);
 assert(!recordTournamentInvitation(s,tournamentLetter(*e),false,100,id));
 assert(!enterTournament(s,id,100,bank));discoverTournaments(s,2,100);assert(!e->discovered);
 discoverTournaments(s,1,100);assert(e->discovered&&tournamentNotice(s,1,100)==id&&!tournamentNotice(s,2,100));
 assert(tournamentsJson(s,100,1,0).find("\"canEnter\":false")!=std::string::npos);
 assert(tournamentsJson(s,100,1,1).find("\"canEnter\":true")!=std::string::npos);
 const auto discovered=tournamentsJson(s,100,1,0,{},"You",1);
 assert(discovered.find("\"unseen\":true")!=std::string::npos&&discovered.find("\"currentHold\":1")!=std::string::npos);
 CollectionSave unseen;assert(decodeCollection(encodeCollection(s),unseen));
 assert(tournamentNotice(unseen,1,100)==id&&!tournament(unseen,id)->notified);
 assert(acknowledgeTournamentNotice(s,id,100)&&!tournamentNotice(s,1,100));
 assert(tournamentsJson(s,100,1).find("\"unseen\":false")!=std::string::npos);
 CollectionSave seen;assert(decodeCollection(encodeCollection(s),seen));assert(!tournamentNotice(seen,1,100)&&tournament(seen,id)->notified);
 assert(enterTournament(s,id,171,bank)&&e->ends==172&&bank.gold==9950);
 assert(startTournamentRound(s,id,171));assert(recordTournamentMatch(s,id,-1));
 assert(!startTournamentRound(s,id,172));tickTournaments(s,172,1);
 assert(e->eliminated&&e->awarded&&e->bracket[14]!=tournamentPlayer&&e->announcement==4);
 assert(tournamentsJson(s,172,1).find("\"joined\":true")!=std::string::npos);
 assert(!enterTournament(s,id,172,bank));checkReservations(s);
 auto never=world();auto missed=newTournament(never,2,200);discoverTournaments(never,2,200);tickTournaments(never,272,2);
 assert(tournament(never,missed)->awarded);assert(tournamentsJson(never,272,2).find("\"events\":[]")!=std::string::npos);
 // Three regular fields and a courier event can coexist, without shared NPCs/prizes.
 auto overlap=world();for(unsigned h=1;h<=3;++h)assert(newTournament(overlap,h,400));
 auto invite=newTournament(overlap,4,400,true,1);assert(invite);checkReservations(overlap);
 // An inn's information topic reveals every active regular event across holds,
 // while a courier event's window still waits for the actual letter.
 discoverAllTournaments(overlap,400);
 for(unsigned h=1;h<=3;++h)assert(tournament(overlap,h)->discovered);
 assert(!tournament(overlap,invite)->discovered&&!tournamentEntryAvailable(*tournament(overlap,invite),400));
 assert(tournamentsJson(overlap,400,1,0).find("\"canEnter\":true")==std::string::npos);
 assert(tournamentsJson(overlap,400,1,1).find("\"canEnter\":true")!=std::string::npos);
 discoverTournaments(overlap,1,400);assert(enterTournament(overlap,1,400,bank));
 discoverTournaments(overlap,2,400);const auto before=bank.gold;assert(!enterTournament(overlap,2,400,bank)&&bank.gold==before);
 assert(withdrawTournament(overlap,1));assert(enterTournament(overlap,2,400,bank));
 auto* second=tournament(overlap,2);auto lost=second->bracket[1];overlap.deadPlayers.insert(lost);resolveTournament(overlap,*second);assert(second->bracket[1]!=lost);checkReservations(overlap);
 // Initial regular dates are staggered across the regions. Higher circuits are
 // independent and keep their class regardless of the player's reputation.
 auto calendar=world();tickTournaments(calendar,100,1);auto& ts=calendar.tournaments;
 assert(ts.events.empty());std::set<unsigned> initialDates(ts.nextRegular.begin(),ts.nextRegular.end());assert(initialDates.size()==10);
 assert(ts.nextCircuit[0]>=100+21*24&&ts.nextCircuit[0]<=100+35*24);
 assert(ts.nextCircuit[1]>=100+60*24&&ts.nextCircuit[1]<=100+90*24);
 std::set<unsigned> holds,circuits;unsigned lastSequence=0;
 for(unsigned hour=101;hour<=100+180*24;hour+=24){
  const auto oldRegular=ts.nextRegular;const auto oldCircuit=ts.nextCircuit;tickTournaments(calendar,hour,1);
  for(unsigned h=0;h<10;++h)if(ts.nextRegular[h]!=oldRegular[h])assert(ts.nextRegular[h]>=hour+30*24&&ts.nextRegular[h]<=hour+45*24);
  for(unsigned c=0;c<2;++c)if(ts.nextCircuit[c]!=oldCircuit[c])assert(ts.nextCircuit[c]>=hour+(c?60:21)*24&&ts.nextCircuit[c]<=hour+(c?90:35)*24);
  assert(std::count_if(ts.events.begin(),ts.events.end(),[](const auto& e){return !e.circuit&&!e.awarded;})<=3);
  for(const auto& event:ts.events)if(event.id>lastSequence){circuits.insert(event.circuit);if(!event.circuit)holds.insert(event.hold);assert(event.invitation==0);}
  lastSequence=ts.sequence;checkReservations(calendar);
 }
 assert(holds.size()==10&&circuits.size()==3);
 // One event per circuit at most after a large jump; all missed dates are rebased.
 auto seq=ts.sequence;tickTournaments(calendar,100000,1);assert(ts.sequence-seq<=3);seq=ts.sequence;tickTournaments(calendar,100000,1);assert(ts.sequence==seq);
 for(auto date:ts.nextRegular)assert(date>100000);checkReservations(calendar);
 auto capped=world();tickTournaments(capped,10);for(unsigned h=1;h<=3;++h)assert(newTournament(capped,h,20));capped.tournaments.nextRegular.fill(20);capped.tournaments.nextCircuit.fill(999999);auto beforeCap=capped.tournaments.sequence;tickTournaments(capped,21);assert(capped.tournaments.sequence==beforeCap);
 // An undelivered letter cannot expire or accumulate another letter on that circuit.
 auto courier=world();auto pending=newTournament(courier,1,100,true,1);tickTournaments(courier,100);courier.tournaments.nextCircuit[0]=200;courier.tournaments.nextRegular.fill(999999);courier.tournaments.nextCircuit[1]=999999;seq=courier.tournaments.sequence;tickTournaments(courier,200);assert(courier.tournaments.sequence==seq&&!tournament(courier,pending)->ends);
 checkReservations(courier);
 std::cout<<"Tournament schedules: all holds, independent circuits, three-day entry, delivery windows, overlap cap, shared reservations, missed-date rebasing, discovery, notification deduplication and history passed\n";
}
