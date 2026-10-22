#include "TournamentPresentation.h"
#include "reputation_support.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
namespace {
struct Bank {int count(int,CardID){return 10000;}int change(int,CardID,int n){return n;}} bank;
CollectionSave world(){CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;return s;}
void reload(CollectionSave& s){CollectionSave loaded;const auto wire=encodeCollection(s);assert(decodeCollection(wire,loaded));assert(encodeCollection(loaded)==wire);s=std::move(loaded);}
unsigned enter(CollectionSave& s,unsigned hold=1,unsigned circuit=0){
 const auto id=newTournament(s,hold,100,true,circuit);assert(id);
 auto* e=tournament(s,id);e->rules=Open;e->reward=1;e->quantity=2;e->trophy=0;
 if(circuit){const auto letter=tournamentLetter(*e);assert(recordTournamentInvitation(s,letter,false,100,id));assert(recordTournamentInvitation(s,letter,true,100,id));}
 else discoverTournaments(s,hold,100);
 assert(enterTournament(s,id,101,bank));return id;
}
void game(CollectionSave& s,unsigned id,int winner,bool record=true){
 assert(startTournamentRound(s,id,101));assert(recordTournamentMatch(s,id,winner,record));
 const auto saved=encodeCollection(s);assert(!recordTournamentMatch(s,id,winner,record));assert(encodeCollection(s)==saved);
}
void firstRival(CollectionSave& s,unsigned id,int i){
 auto& bracket=tournament(s,id)->bracket;const auto base=opponents[i].base;
 auto it=std::find(bracket.begin()+1,bracket.begin()+8,base);
 if(it!=bracket.begin()+8)std::swap(bracket[1],*it);else bracket[1]=base;
}
int opponent(unsigned hold,unsigned level){for(unsigned i=0;i<opponents.size();++i)if(opponents[i].hold==hold&&opponents[i].level==level)return i;assert(false);return -1;}
void finish(CollectionSave& s,unsigned id){while(tournamentNode(*tournament(s,id)))game(s,id,0);}
}
int main(){
 // Every circuit: six game wins become three credited pairings, plus one bonus.
 // Real NPC tables progress alongside the player, and every saved stage survives.
 for(unsigned circuit=0;circuit<3;++circuit){
  auto s=world();auto id=enter(s,1,circuit);unsigned points=0,renown=0;
  for(unsigned stage=0;stage<3;++stage){
   const int i=opponentIndex(tournamentRival(*tournament(s,id)));const auto base=opponents[i].base;
   points+=reputationAward(s,i,1);renown+=4*tournamentPairingRenown[circuit];
   game(s,id,0);assert(localReputation(s,1).wins==stage);reload(s);
   game(s,id,0);assert(progress(s,i).competitive.wins==2);assert(reputationWins(s,base)==1);
   assert(localReputation(s,1).wins==stage+1);reload(s);
  }
  assert(localReputation(s,1).quarters==points+4*tournamentChampionshipPoints[circuit]);
  assert(localReputation(s,1).rank==Unsung&&localReputation(s,1).players==3);
  assert(tournamentChampionships(s,circuit)==1);
  assert(widerRenown(s)==renown+4*tournamentChampionshipRenown[circuit]);
  assert(s.reputation.empty());
  for(unsigned h=2;h<=10;++h)assert(localReputation(s,h).quarters==0);
  const auto saved=encodeCollection(s);resolveTournament(s,*tournament(s,id),true);awardTournament(s,*tournament(s,id));assert(encodeCollection(s)==saved);
  assert(!startTournamentRound(s,id,101)&&!recordTournamentMatch(s,id,0));
  // Pruning the finite event history cannot erase achievements or repeat credit.
  s.tournaments.events.clear();reload(s);assert(localReputation(s,1).quarters==points+4*tournamentChampionshipPoints[circuit]);assert(tournamentChampionships(s,circuit)==1);
 }
 // Individual games belong in Journal statistics, including a draw and a loss.
 // Only the second played victory awards host-hold credit against a visitor.
 auto s=world();auto id=enter(s,1,1);const int visitor=opponent(2,3);firstRival(s,id,visitor);
 game(s,id,-1);game(s,id,0);game(s,id,1);reload(s);
 assert(localReputation(s,1).quarters==0&&widerRenown(s)==0);
 assert(startTournamentRound(s,id,101));reload(s);assert(recordTournamentMatch(s,id,0));
 assert(progress(s,visitor).competitive.wins==2&&progress(s,visitor).competitive.losses==1&&progress(s,visitor).competitive.draws==1);
 assert(localReputation(s,1).wins==1&&localReputation(s,1).quarters==16&&localReputation(s,1).strongWins==1);
 assert(localReputation(s,2).wins==0&&widerRenown(s)==8);
 reload(s);const auto saved=encodeCollection(s);assert(!recordTournamentMatch(s,id,0));assert(saved==encodeCollection(s));
 assert(withdrawTournament(s,id));assert(localReputation(s,1).quarters==16&&tournamentChampionships(s,1)==0);

 // Repeat credit follows the person across ordinary play, tournaments and holds.
 s=world();const int regular=opponent(1,2);recordCampaignResult(s,regular,0,true,Open,true);
 id=enter(s,2);firstRival(s,id,regular);game(s,id,0);game(s,id,0);
 assert(localReputation(s,2).quarters==6&&localReputation(s,1).quarters==12);
 recordCampaignResult(s,regular,0,true,Open,true);assert(localReputation(s,1).quarters==15);
 assert(withdrawTournament(s,id));id=enter(s);firstRival(s,id,regular);game(s,id,0);game(s,id,0);
 assert(localReputation(s,1).quarters==18&&localReputation(s,1).players==1&&localReputation(s,1).wins==3);
 assert(reputationWins(s,opponents[regular].base)==4);reload(s);assert(reputationAward(s,regular)==3);

 // Rank floors apply at the venue. Beating a visiting Master grants strength
 // proof there, but cannot satisfy the visitor's home-hold Master requirement.
 s=world();earnReputation(s,1,Legendary);id=enter(s,1,1);firstRival(s,id,visitor);
 const auto prior=localReputation(s,1);game(s,id,0);game(s,id,0);
 assert(localReputation(s,1).quarters==prior.quarters&&localReputation(s,1).players==prior.players&&widerRenown(s)==0);
 s=world();id=enter(s,1,2);const int master=opponent(2,5);firstRival(s,id,master);game(s,id,0);game(s,id,0);
 assert(localReputation(s,1).expertWins==1&&localReputation(s,1).masterWins==0&&localReputation(s,2).masterWins==0);
 assert(withdrawTournament(s,id));id=enter(s,2,2);firstRival(s,id,master);game(s,id,0);game(s,id,0);
 assert(localReputation(s,2).masterWins==1&&localReputation(s,2).quarters==16&&widerRenown(s)==24);

 // Disabling results still settles the full bracket and prizes, with no records.
 s=world();id=enter(s,1,1);while(tournamentNode(*tournament(s,id)))game(s,id,0,false);
 assert(tournament(s,id)->awarded&&s.goldCredit==1800);assert(localReputation(s,1).wins==0&&localReputation(s,1).quarters==0&&widerRenown(s)==0);
 for(const auto& [base,p]:s.players)assert(!p.competitive.wins&&!p.competitive.losses&&!p.competitive.draws);
 // Re-enabling midway cannot credit an earlier unrecorded win, even after load.
 s=world();id=enter(s,1,1);game(s,id,0,false);reload(s);game(s,id,0,true);assert(localReputation(s,1).wins==0);
 s=world();id=enter(s,1,1);game(s,id,0);game(s,id,0);game(s,id,0);game(s,id,0);
 const auto beforeFinal=localReputation(s,1).quarters;game(s,id,0,false);reload(s);game(s,id,0);
 assert(localReputation(s,1).quarters==beforeFinal&&!tournamentChampionships(s,1));
 // A replacement during a series similarly cannot inherit earned victories.
 s=world();id=enter(s);const auto old=tournamentRival(*tournament(s,id));game(s,id,0);s.deadPlayers.insert(old);
 game(s,id,0);assert(tournamentRival(*tournament(s,id))!=old&&localReputation(s,1).wins==0);

 // With no replacement available, a walkover wins the event without an earned
 // championship bonus. Earlier completed pairings retain their credit.
 s=world();id=enter(s);game(s,id,0);game(s,id,0);game(s,id,0);game(s,id,0);
 const auto beforeWalkover=localReputation(s,1).quarters;
 for(const auto& p:opponents)s.deadPlayers.insert(p.base);
 assert(!startTournamentRound(s,id,101));assert(tournament(s,id)->bracket[14]==tournamentPlayer&&tournament(s,id)->awarded);
 assert(localReputation(s,1).quarters==beforeWalkover&&tournamentChampionships(s,0)==0);
 s=world();id=enter(s);game(s,id,0);resolveTournament(s,*tournament(s,id),true);assert(localReputation(s,1).quarters==0&&tournamentChampionships(s,0)==0);

 // Fixed targets, independent of authored population and temporary availability.
 for(unsigned h=1;h<=10;++h){const auto r=reputationRequirements(h);
  assert(r.recognizedPoints==(h==1?14u:10u)&&r.respectedPoints==(h==1?30u:20u)&&r.famousPoints==(h==1?55u:35u));
  assert(r.famousPlayers==(h==1?16u:h==6||h==10?10u:8u));
 }
 // Wider renown opens future invitations across holds. Existing announcements
 // keep their snapshot, and Masters additionally require two Invitational wins.
 s=world();auto announcement=newTournament(s,3,100,false,1);assert(tournament(s,announcement)->invitation==0);
 s.tournamentReputation[0].renownQuarters=63;assert(tournamentEligibility(s,3)==0);
 s.tournamentReputation[0].renownQuarters=64;assert(tournamentEligibility(s,3)==2);assert(tournament(s,announcement)->invitation==0);
 auto invited=newTournament(s,4,101,false,1);assert(tournament(s,invited)->invitation==1);
 s.tournamentReputation[0].renownQuarters=160;assert(tournamentEligibility(s,3)==2);
 s.tournamentReputation[0].championships[1]=1;assert(tournamentEligibility(s,3)==2);
 s.tournamentReputation[1].championships[1]=1;reload(s);assert(tournamentEligibility(s,3)==3);
 auto masters=newTournament(s,5,102,false,2);assert(tournament(s,masters)->invitation==1);
 // Four fully played Invitationals reach that route even with repeat opponents.
 s=world();for(unsigned n=0;n<4;++n){id=enter(s,1,1);finish(s,id);s.goldCredit=0;leaveTournamentRewards(*tournament(s,id));}
 assert(tournamentChampionships(s,1)==4&&widerRenown(s)>=40*4);reload(s);assert(tournamentEligibility(s,10)==3);

 // New earned-state validation rejects impossible counts atomically.
 s=world();id=enter(s);game(s,id,0);reload(s);
 auto invalid=s;tournament(invalid,id)->reputationWins[0]=2;
 CollectionSave destination;grantStarter(destination);const auto untouched=encodeCollection(destination);
 assert(!decodeCollection(encodeCollection(invalid),destination)&&encodeCollection(destination)==untouched);
 invalid=s;invalid.tournamentReputation[0].opponents[opponents[regular].base]={1,1000};assert(!decodeCollection(encodeCollection(invalid),destination));
 invalid=s;invalid.tournamentReputation[0].renownQuarters=1000001;assert(!decodeCollection(encodeCollection(invalid),destination));
 assert(!decodeCollection(encodeCollection(s),destination,25));
 std::cout<<"Tournament reputation: pairing/championship awards, game records, host credit, shared diminishing returns, Master proofs, disabled recording, substitutes/walkovers, persistent history, fixed targets and invitation renown passed\n";
}
