#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
struct Bank{int gold=10000;int count(int,CardID){return gold;}int change(int,CardID,int n){n=std::max(n,-gold);gold+=n;return n;}};
void deliver(CollectionSave& s,unsigned id){auto* e=tournament(s,id);if(e->circuit){const auto letter=tournamentLetter(*e);assert(recordTournamentInvitation(s,letter,false,e->announced,id));assert(recordTournamentInvitation(s,letter,true,e->announced, id));}else discoverTournaments(s,e->hold,e->starts);}
void play(CollectionSave& s,unsigned id,int winner,unsigned hour=148){assert(startTournamentRound(s,id,hour));assert(!startTournamentRound(s,id,hour));assert(recordTournamentMatch(s,id,winner));assert(!recordTournamentMatch(s,id,winner));}
int main(){
 CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;Bank bank;
 for(unsigned h=1;h<=10;++h){auto id=newTournament(s,h,100);assert(id);auto* e=tournament(s,id);assert(e->hold==h&&e->starts==100&&e->ends==172&&validRules(e->rules));assert(!e->invitation&&!e->joined);for(auto base:e->bracket)if(base)assert(opponents[opponentIndex(base)].level>=2);}
 auto id=s.tournaments.events[0].id;const auto fee=tournament(s,id)->fee;const auto owned=s.player;
 assert(!enterTournament(s,id,101,bank));deliver(s,id);
 assert(enterTournament(s,id,101,bank));assert(bank.gold==10000-int(fee));assert(!enterTournament(s,id,102,bank));deliver(s,2);assert(!enterTournament(s,2,102,bank));
 assert(!startTournamentRound(s,id,99));resolveTournament(s,*tournament(s,id));
 for(unsigned n=8;n<15;++n)assert(!tournament(s,id)->bracket[n]);
 for(unsigned node:{8u,12u,14u}){
  auto* e=tournament(s,id);e->announcement=0;const auto before=e->wins;
  play(s,id,-1);assert(e->wins==before&&tournamentNode(*e)==node);
  play(s,id,0);assert(e->wins[node-8][0]==1&&tournamentNode(*e)==node&&!e->announcement);
  const unsigned end=node==8?12:node==12?14:15;
  for(unsigned n=node+1;n<end;++n)assert(e->wins[n-8][0]+e->wins[n-8][1]==1&&!e->bracket[n]);
  CollectionSave reload;assert(decodeCollection(encodeCollection(s),reload));s=reload;e=tournament(s,id);
  play(s,id,1);assert(!e->eliminated&&tournamentNode(*e)==node);
  // Future entrants remain hidden even if another table finished its series early.
  const auto json=tournamentsJson(s,148,1);assert(json.find("\"skill\"")==std::string::npos);
  play(s,id,0);assert(e->wins[node-8][0]==2&&e->wins[node-8][1]==1);
  assert(e->announcement==(node==8?1u:node==12?2u:3u));
  for(unsigned n=node;n<end;++n)assert(e->bracket[n]);
  if(node<14){assert(s.player==owned&&!e->awarded);for(unsigned n=end;n<15;++n){assert(!e->bracket[n]);assert(e->wins[n-8][0]+e->wins[n-8][1]==0);}}
 }
 auto* e=tournament(s,id);assert(e->bracket[14]==tournamentPlayer&&e->awarded&&s.goldCredit==int(e->purse));
 const auto awarded=encodeCollection(s);awardTournament(s,*e);assert(encodeCollection(s)==awarded);
 CollectionSave loaded;assert(decodeCollection(awarded,loaded)&&encodeCollection(loaded)==awarded);
 for(std::size_t n=0;n<awarded.size();++n){auto cut=awarded;cut.resize(n);assert(!decodeCollection(cut,loaded));}
 assert(!decodeCollection(awarded,loaded,23));
 assert(!decodeCollection(awarded,loaded,25));
 assert(!decodeCollection(awarded,loaded,24));leaveTournamentRewards(*tournament(s,id));
 // Reputation unlocks progressively stronger invitation fields and rewards.
 CollectionSave rep;grantStarter(rep);rep.collegeMember=rep.guildMember=rep.erandurReady=rep.valdrRescued=true;
 assert(tournamentEligibility(rep,1)==0);unsigned regulars=0;
 for(const auto& p:opponents)if(p.hold==1&&p.level==2&&regulars++<2)rep.reputation[p.base]={3,24};
 assert(tournamentEligibility(rep,1)==1);auto regular=newTournament(rep,1,100);assert(tournament(rep,regular)->invitation==0);resolveTournament(rep,*tournament(rep,regular),true);
 for(const auto& p:opponents)if(p.hold==1&&p.level<5)rep.reputation[p.base]={2,40};
 assert(tournamentEligibility(rep,1)==3);
 for(unsigned circuit=0;circuit<3;++circuit){auto c=newTournament(rep,1,200+circuit,false,circuit);auto* event=tournament(rep,c);assert(event->invitation==(circuit?1u:0u)&&event->circuit==circuit);
  for(unsigned n=0;n<8;++n)assert(tournamentField(rep,opponentIndex(event->bracket[n]),circuit));
  if(circuit==2){assert(event->reward!=StandardPacks&&event->purse==5000);assert(event->fee==500);}
 }
 // Development uses the same courier window as an invitational, with no fee.
 auto dev=newTournament(rep,1,300,true,1);auto* de=tournament(rep,dev);assert(de->invitation==1&&!de->joined&&!de->fee);assert(!enterTournament(rep,dev,301,bank));deliver(rep,dev);assert(enterTournament(rep,dev,310,bank)&&de->starts==300&&de->ends==468);
 // Two losses eliminate, and forfeiting ends the series immediately.
 s.goldCredit=0;id=newTournament(s,1,200);deliver(s,id);assert(enterTournament(s,id,201,bank));const auto cardsBefore=s.player;
 play(s,id,1,248);assert(!tournament(s,id)->eliminated);play(s,id,1,248);e=tournament(s,id);assert(e->eliminated&&e->awarded&&e->bracket[14]!=tournamentPlayer&&s.player==cardsBefore);
 auto loss=newTournament(s,1,300);deliver(s,loss);assert(enterTournament(s,loss,301,bank));assert(withdrawTournament(s,loss));assert(tournament(s,loss)->awarded&&!withdrawTournament(s,loss));
 auto replacementID=newTournament(s,2,400);auto* replacement=tournament(s,replacementID);auto missing=replacement->bracket[1];s.deadPlayers.insert(missing);resolveTournament(s,*replacement);assert(std::find(replacement->bracket.begin(),replacement->bracket.end(),missing)==replacement->bracket.end());assert(decodeCollection(encodeCollection(s),loaded));
 // A completed tier VIII trophy is owned, playable, and recovered only from its estate.
 CollectionSave estate;grantStarter(estate);estate.collegeMember=estate.guildMember=estate.erandurReady=estate.valdrRescued=true;
 unsigned trophyEvent=0;for(unsigned n=0;n<20;++n){auto id=newTournament(estate,1,400+n,false,1);if(tournament(estate,id)->reward==0){trophyEvent=id;break;}resolveTournament(estate,*tournament(estate,id),true);}assert(trophyEvent);auto* prize=tournament(estate,trophyEvent);auto trophy=prize->trophy;resolveTournament(estate,*prize,true);auto champion=prize->bracket[14];int championIndex=opponentIndex(champion);assert(count(tournamentStock(estate,champion),trophy)==1);
 bool drawn=false;for(unsigned seed=0;seed<200;++seed){auto hand=opponentHand(tournamentStock(estate,champion),championIndex,seed);drawn|=std::find(hand.begin(),hand.end(),trophy)!=hand.end();}assert(drawn);
 estate.deadPlayers.insert(champion);unsigned recoveryID=0;for(unsigned n=0;n<20;++n){auto id=newTournament(estate,2,500+n,false,1);if(tournament(estate,id)->reward==0){recoveryID=id;break;}resolveTournament(estate,*tournament(estate,id),true);}assert(recoveryID);assert(tournament(estate,recoveryID)->trophy==trophy&&count(tournamentStock(estate,champion),trophy)==0);
 CollectionSave expiry;grantStarter(expiry);auto expires=newTournament(expiry,1,100);deliver(expiry,expires);assert(enterTournament(expiry,expires,101,bank));expiry.tournaments.nextRegular.fill(1000);expiry.tournaments.nextCircuit.fill(1000);const auto untouched=expiry.player;tickTournaments(expiry,172);assert(tournament(expiry,expires)->eliminated&&tournament(expiry,expires)->awarded&&expiry.player==untouched);assert(!enterTournament(expiry,expires,173,bank));
 unsigned foils=0,epics=0,commons=0;for(unsigned seed=1;seed<=1000;++seed){auto pack=tournamentPack(seed);assert(pack.size()==5);std::set<unsigned> ids;for(auto c:pack){assert(ids.insert(baseCardID(c)).second);const auto& card=cards[cardIndex(c)];assert(card.available&&!card.unique&&card.tier>=3&&card.tier<=7);foils+=isFoil(c);epics+=std::string_view(card.rarity)=="Epic";commons+=std::string_view(card.rarity)=="Common";}}assert(foils>150&&foils<350&&epics>1100&&epics<1400&&commons>1350&&commons<1650);
 // Reverse includes low cards from outside the authored collection, respecting copy limits.
 Stock reverseStock=personalPool(championIndex);for(const auto& c:cards)if(c.starter){reverseStock[c.form]=2;reverseStock[c.form|foilFlag]=2;}
 for(unsigned seed=0;seed<100;++seed){auto hand=opponentHand(reverseStock,championIndex,seed,Reverse|Open);assert(validHand(hand,reverseStock));for(auto id:hand)assert(cards[cardIndex(id)].tier<=2);}
 auto jan=shiftGameDate({201,11,31,23},2);assert(jan.year==202&&jan.month==0&&jan.day==1&&jan.hour==1);
 auto feb=shiftGameDate({202,2,1,0},-1);assert(feb.month==1&&feb.day==28&&feb.hour==23);
 assert(gameDateText(shiftGameDate({201,7,17,10},48))=="19 Last Seed, 4E 201 · 10:00");
 std::cout<<"Tournaments: best-of-three, synchronized NPC rounds, draws/losses, saved scores, current save schema, invitation gates, reputation classes, trophy recovery, unfinished-Master fallback, dates, Reverse decks and once-only rewards passed\n";
}
