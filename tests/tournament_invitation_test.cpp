#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank{int gold=1000;int count(int,CardID){return gold;}int change(int,CardID,int n){gold+=n;return n;}};
int main(){
 CollectionSave s;grantStarter(s);Bank bank;
 const auto id=newTournament(s,1,100,true,1),letter=tournamentLetter(*tournament(s,id));
 assert(!enterTournament(s,id,101,bank));
 assert(tournamentsJson(s,101,1,1).find("\"events\":[]")!=std::string::npos);
 assert(recordTournamentInvitation(s,letter,false,101,id)==id);
 assert(!recordTournamentInvitation(s,letter,false,101,id));
 assert(!enterTournament(s,id,101,bank));
 // Reproduce the live failure: queued in the save, physical note in the player's
 // inventory, and no ObjectReference/Papyrus delivery callback.
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));s=loaded;
 assert(recoverTournamentInvitations(s,letter,0,1,101).empty());
 assert(recoverTournamentInvitations(s,letter,1,1,101).empty());
 assert(recoverTournamentInvitations(s,letter,0,0,101).empty());
 assert(recoverTournamentInvitations(s,letter,1,0,101)==std::vector<unsigned>{id});
 assert(recoverTournamentInvitations(s,letter,1,0,101).empty());
 assert(tournamentsJson(s,101,1).find("\"canEnter\":false")!=std::string::npos);
 assert(tournamentsJson(s,101,0,1).find("\"canEnter\":false")!=std::string::npos);
 assert(tournamentsJson(s,101,1,1).find("\"canEnter\":true")!=std::string::npos);
 const auto reserve=tournament(s,id)->bracket[0];const auto beforePreview=encodeCollection(s);
 const auto preview=tournamentsJson(s,101,1,1);
 assert(preview.find("\"bracket\":[{\"id\":0,\"name\":\"\",\"reserved\":true}")!=std::string::npos);
 assert(encodeCollection(s)==beforePreview&&reserve!=tournamentPlayer);
 assert(enterTournament(s,id,101,bank)&&bank.gold==1000);
 assert(tournamentsJson(s,101,1,1,{},"Test Player").find("\"bracket\":[{\"id\":20,\"name\":\"Test Player\",\"reserved\":false}")!=std::string::npos);
 assert(decodeCollection(encodeCollection(s),loaded)&&tournament(loaded,id)->joined);

 // Successive invitations in a hold share one BOOK form. Recover the
 // missed receipt while leaving the next copy, still with the courier, locked.
 CollectionSave duplicates;grantStarter(duplicates);
 const auto first=newTournament(duplicates,1,200,true,1),second=newTournament(duplicates,1,210,true,1);
 assert(recordTournamentInvitation(duplicates,letter,false,211,first)==first);
 assert(recordTournamentInvitation(duplicates,letter,false,211,second)==second);
 const auto third=newTournament(duplicates,1,220,true,1);
 assert(!recordTournamentInvitation(duplicates,letter,false,221,first));
 assert(tournament(duplicates,third)->invitation==1);
 assert(recoverTournamentInvitations(duplicates,letter,1,1,222)==std::vector<unsigned>{first});
 assert(tournament(duplicates,second)->invitation==2&&tournament(duplicates,third)->invitation==1);
 assert(recoverTournamentInvitations(duplicates,letter,1,1,222).empty());
 assert(recordTournamentInvitation(duplicates,letter,true,222)==second);
 // A native event arriving after inventory recovery cannot consume an unsent invitation.
 assert(!recordTournamentInvitation(duplicates,letter,true,222));
 assert(tournament(duplicates,third)->invitation==1);
 // A fast delivery can precede its queue acknowledgement. Recovery waits for
 // that exact request, then recognizes the delivered copy without another event.
 assert(recoverTournamentInvitations(duplicates,letter,3,0,222).empty());
 assert(recordTournamentInvitation(duplicates,letter,false,222,third)==third);
 assert(recoverTournamentInvitations(duplicates,letter,3,0,222)==std::vector<unsigned>{third});
 assert(!recordTournamentInvitation(duplicates,letter,false,222,third));
 const auto fourth=newTournament(duplicates,1,230,true,1),fifth=newTournament(duplicates,1,231,true,1);
 assert(recordTournamentInvitation(duplicates,letter,false,232,fourth)==fourth);
 assert(recordTournamentInvitation(duplicates,letter,false,232,fifth)==fifth);
 assert((recoverTournamentInvitations(duplicates,letter,2,0,232)==std::vector<unsigned>{fourth,fifth}));
 assert(recoverTournamentInvitations(duplicates,letter,2,0,232).empty());

 // A delayed courier opens a full seven-day window on receipt, even months
 // after announcement. Recovery and duplicate callbacks never move its deadline.
 CollectionSave expired;grantStarter(expired);
 auto past=newTournament(expired,1,100,true,1);assert(recordTournamentInvitation(expired,letter,false,101,past)==past);
 tickTournaments(expired,800);assert(!tournament(expired,past)->ends&&!tournament(expired,past)->awarded);
 assert(!recordTournamentInvitation(expired,letter+1,true,899));
 assert(recoverTournamentInvitations(expired,letter,1,0,900)==std::vector<unsigned>{past});
 assert(tournament(expired,past)->starts==900&&tournament(expired,past)->ends==1068);
 assert(!recordTournamentInvitation(expired,letter,true,1000));
 assert(enterTournament(expired,past,1000,bank)&&tournament(expired,past)->ends==1068);
 // Missing dispatch acknowledgement used to strand the in-flight request and
 // keep a delivered letter invisible. Actual courier stock now confirms it.
 CollectionSave missed;grantStarter(missed);
 const auto missing=newTournament(missed,1,100,true,1);
 assert(reconcileTournamentInvitations(missed,letter,0,1,101).empty());
 assert(tournament(missed,missing)->invitation==2);
 assert(reconcileTournamentInvitations(missed,letter,1,0,102)==std::vector<unsigned>{missing});
 assert(tournamentEntryAvailable(*tournament(missed,missing),102));
 // An observed fast transfer acknowledges the exact request before inventory
 // reconciliation. Late callbacks never advance a second event on that BOOK.
 const auto next=newTournament(missed,1,103,true,1);
 assert(recordTournamentInvitation(missed,letter,false,103,next)==next);
 assert(reconcileTournamentInvitations(missed,letter,2,0,103)==std::vector<unsigned>{next});
 const auto later=newTournament(missed,1,104,true,1);
 assert(!recordTournamentInvitation(missed,letter,false,104,next));
 assert(reconcileTournamentInvitations(missed,letter,2,0,104).empty()&&tournament(missed,later)->invitation==1);
 // Reproduce the live save: announced at hour 32, courier polling at hour 7.
 // Acknowledgement must not fail and requeue the same note every five seconds.
 CollectionSave rewind;grantStarter(rewind);
 const auto masters=newTournament(rewind,1,32,true,2),invitational=newTournament(rewind,1,32,true,1);
 for(auto event:{masters,invitational}){
  auto* e=tournament(rewind,event);const auto note=tournamentLetter(*e);
  assert(recordTournamentInvitation(rewind,note,false,7,event)==event);
  for(unsigned poll=0;poll<90;++poll)assert(!recordTournamentInvitation(rewind,note,false,7,event));
  assert(e->invitation==2&&e->announced==7);
  assert(reconcileTournamentInvitations(rewind,note,90,0,6)==std::vector<unsigned>{event});
  assert(e->starts==6&&e->ends==174&&e->announced==6&&tournamentEntryAvailable(*e,6));
  assert(reconcileTournamentInvitations(rewind,note,90,0,7).empty()&&e->ends==174);
 }
 assert(decodeCollection(encodeCollection(rewind),loaded));
 resolveTournament(rewind,*tournament(rewind,masters),true);
 const auto successorMaster=newTournament(rewind,1,8,true,2);
 assert(successorMaster);
 assert(reconcileTournamentInvitations(rewind,tournamentLetter(*tournament(rewind,successorMaster)),90,0,8).empty());
 assert(tournament(rewind,successorMaster)->invitation==1); // Old copies are not a new invitation.
 CollectionSave strandedClock;grantStarter(strandedClock);const auto strandedMaster=newTournament(strandedClock,1,32,true,2);const auto strandedLetter=tournamentLetter(*tournament(strandedClock,strandedMaster));
 assert(reconcileTournamentInvitations(strandedClock,strandedLetter,90,0,7)==std::vector<unsigned>{strandedMaster});
 assert(tournamentEntryAvailable(*tournament(strandedClock,strandedMaster),7));
 assert(reconcileTournamentInvitations(strandedClock,strandedLetter,90,0,7).empty());
 std::cout<<"Invitations: missed inventory callback recovery, host/venue entry gates, duplicate notes, retained old letters, delayed acknowledgements, late delivery windows and saved entry passed\n";
}
