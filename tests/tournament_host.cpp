#include "TournamentPresentation.h"
#include "DeckPresentation.h"
#include <iostream>
#include <sstream>
#include <cassert>
using namespace ttcg;
std::pair<Match,Move> finalCaptureFixture(){
 for(unsigned seed=1;seed<5000;++seed){Match before(seed,Open|Same|Plus);before.turn=0;std::mt19937 rng(seed);
  while(before.placed<8){const auto moves=before.legalMoves();before.play(moves[rng()%moves.size()]);}
  for(auto move:before.legalMoves()){auto after=before;auto result=after.play(move);
   if(after.score()[0]>after.score()[1]&&std::any_of(result.stages.begin(),result.stages.end(),[](const auto& stage){return stage.rule&&(stage.captures!=0);}))return {before,move};
  }
 }throw std::runtime_error("No final special-capture victory fixture");
}
int main(){
 CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;for(const auto& c:cards)if(c.starter)s.player[c.form]=2;
 struct Bank{int gold=1000;int count(int,CardID){return gold;}int change(int,CardID,int n){n=std::max(n,-gold);gold+=n;return n;}} bank;
 unsigned hour=100,id=newTournament(s,1,hour,false,1),context=0,session=1,revision=0;unsigned venue=1,registration=0,playingRival=0,currentHold=1;bool active=false;std::string screen="album",section="tournaments",revealKind;Match match(17,Open);std::vector<CardID> revealed;unsigned revealID=0;std::array<unsigned,2> physicalPacks{};
 bool settling=false;Result lastResult;Move finalMove{};unsigned resultSounds=0,ruleSounds=0;
 tournament(s,id)->invitation=1;tournament(s,id)->starts=0;tournament(s,id)->ends=0;
 // Keep reload-announcement checks independent of randomized rewards. The
 // immediate-use foil reward is exercised explicitly by test-foil-reward.
 tournament(s,id)->reward=1;tournament(s,id)->trophy=0;tournament(s,id)->quantity=2;
 recordTournamentInvitation(s,tournamentLetter(*tournament(s,id)),false,hour,id);
 unsigned carriedLetters=0,courierLetters=1;
 auto output=[&]{
  deliverTournamentPacks(s,[&](unsigned kind,int count){physicalPacks[kind]+=count;return count;});
  auto* e=tournament(s,context);auto rival=active?playingRival:e?tournamentRival(*e):0;auto i=opponentIndex(rival);auto data=snapshotJson(match,session,revision,i>=0?opponents[i].name:"",active,false,lastResult,settling,false,false);data.pop_back();
  data+=std::format(",\"testFinalMove\":{{\"hand\":{},\"square\":{}}},\"testResultSounds\":{},\"testRuleSounds\":{}",finalMove.hand,finalMove.square,resultSounds,ruleSounds);
  data+=",\"screen\":"+quote(screen)+",\"albumSection\":"+quote(section)+",\"matchContext\":"+(context?"true":"false")+",\"collection\":{"+savedDeckFields(s)+",\"deck\":[";
  for(unsigned n=0;n<5;++n){if(n)data+=',';data+=std::to_string(displayIndex(s.deck[n]));}data+="],\"owned\":[";
  for(unsigned n=0;n<cards.size()*2;++n){if(n)data+=',';data+=std::to_string(count(s.player,displayCardID(n)));}data+="],\"known\":[";
  for(unsigned n=0;n<cards.size()*2;++n){if(n)data+=',';data+=count(s.player,displayCardID(n))?"true":"false";}data+="],\"revealed\":[";
  for(unsigned n=0;n<revealed.size();++n){if(n)data+=',';data+=std::to_string(displayIndex(revealed[n]));}
  data+=std::format("],\"gold\":{},\"rivalGold\":1000,\"canStake\":false,\"canPlay\":true,\"fixedRules\":{},\"fixedTrade\":0,\"fixedWager\":0,\"maxWager\":0,\"tournamentID\":{},\"revealID\":{},\"revealKind\":{},\"packs\":[{},{}],\"notice\":\"\",\"tournaments\":{}}}}}",bank.gold,e?e->rules:0,context,revealID,quote(revealKind),physicalPacks[0],physicalPacks[1],tournamentsJson(s,hour,venue,registration,shiftGameDate({201,7,17,10},int(hour)-100),"Test Player",currentHold));
  std::cout<<data<<std::endl;section.clear();
 };
 output();for(std::string line;std::getline(std::cin,line);){std::istringstream in(line);std::string verb;unsigned v,r;in>>verb>>v>>r;if(v==session&&r==revision){
  // Exercise the same reward gate as Plugin.cpp. The final win records its
  // reward before playback, and only the browser's acknowledgement settles it.
  if(!verb.starts_with("test-")&&verb!="reload"&&!tournamentRewardAllowsCommand(s,verb)){output();continue;}
  if(verb=="result"||verb=="rule"){if(settling){if(verb=="result")++resultSounds;else ++ruleSounds;}output();continue;}
  if(verb=="settled"){settling=false;output();continue;}
  if(verb=="tournament"){std::string action;unsigned target=0;in>>action>>target;
   if(settling||(active&&!match.finished())){output();continue;}
   if(action=="enter"&&venue==tournament(s,target)->hold&&registration==venue&&enterTournament(s,target,hour,bank)){context=target;screen="lobby";match=Match(1,tournament(s,target)->rules);++session;}
   if(action=="seen"){acknowledgeTournamentNotice(s,target,hour);output();continue;}
   if(action=="withdraw")withdrawTournament(s,target);
   if(action=="round"&&venue==1){context=target;resolveTournament(s,*tournament(s,target));screen="lobby";match=Match(1,tournament(s,target)->rules);++session;}
   if(action=="continue"||action=="ack"){
    auto* event=tournament(s,target);leaveTournamentRewards(*event);active=false;
    if(action=="continue"&&tournamentNode(*event)){context=target;screen="lobby";match=Match(1,event->rules);}
    else{context=0;screen="album";section="tournaments";match=Match(1,0);}++session;
   }
   if(action=="board"){forfeitTournamentFoils(s);context=0;active=false;screen="album";section="tournaments";match=Match(1,0);++session;}
   if(action=="foil"){unsigned card;in>>card;if(venue==tournament(s,target)->hold)upgradeTournamentFoil(s,target,displayCardID(card));}
   if(action=="pack"){auto pack=openTournamentPack(s,[&]{if(!physicalPacks[1])return false;--physicalPacks[1];return true;});if(!pack.empty()){revealed=pack;revealKind="tournament";++revealID;}}
   if(action=="leave"){forfeitTournamentFoils(s);active=false;context=0;screen="album";++session;}
  }
  if(verb=="test-host"){registration=venue;discoverTournaments(s,currentHold,hour);}
  if(verb=="test-receive-letter"){carriedLetters=1;courierLetters=0;}
  if(verb=="test-venue")in>>venue;
  if(verb=="test-hold"){in>>currentHold;discoverTournaments(s,currentHold,hour);}
  if(verb=="test-regular"){unsigned h;in>>h;newTournament(s,h,hour);discoverTournaments(s,currentHold,hour);}
  if(verb=="test-post-result"){context=0;active=false;screen="lobby";match=Match(1,0);while(!match.finished()){if(!match.redeal())match.play(match.legalMoves().front());}active=true;++session;}
  if(verb=="test-time"){in>>hour;tickTournaments(s,hour,currentHold);}
  if(verb=="test-rewards")physicalPacks={1,2};
  if(verb=="test-courier-rewind"){
   s={};grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;bank.gold=10000;
   const auto masters=newTournament(s,1,32,true,2);id=newTournament(s,1,32,true,1);assert(masters&&id);hour=7;
   const auto note=tournamentLetter(*tournament(s,masters));assert(reconcileTournamentInvitations(s,note,90,0,hour)==std::vector<unsigned>{masters});
   context=0;venue=currentHold=1;registration=0;active=settling=false;lastResult={};screen="album";section="tournaments";match=Match(1,0);++session;
  }
  if(verb=="test-courier-delivery")for(auto& e:s.tournaments.events)if(e.invitation==1){
   const auto note=tournamentLetter(e);assert(recordTournamentInvitation(s,note,false,hour,e.id)==e.id);
   assert(reconcileTournamentInvitations(s,note,1,0,hour)==std::vector<unsigned>{e.id});
  }
  if(verb=="test-final-setup"){
   unsigned circuit,reward;in>>circuit>>reward;s={};grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;bank.gold=10000;physicalPacks={};
   id=newTournament(s,1,hour,true,circuit);auto* e=tournament(s,id);assert(e);e->rules=Open|Same|Plus;
   e->reward=reward;e->trophy=0;e->quantity=reward==FoilUpgrades?(circuit==2?5:circuit==1?3:1):reward==StandardPacks?3:reward==Trophy?1:circuit+1;
   if(reward==Trophy){for(const auto& card:cards)if(tournamentTrophy(card,circuit)){e->trophy=card.form;break;}assert(e->trophy);}
   if(circuit){const auto letter=tournamentLetter(*e);assert(recordTournamentInvitation(s,letter,false,hour,id));assert(recordTournamentInvitation(s,letter,true,hour,id));}
   else discoverTournaments(s,1,hour);
   assert(enterTournament(s,id,hour,bank));
   for(unsigned game=0;game<5;++game){assert(startTournamentRound(s,id,hour));assert(recordTournamentMatch(s,id,0));e->announcement=0;}
   assert(e->wins[6][0]==1&&!e->bracket[14]&&startTournamentRound(s,id,hour));
   auto fixture=finalCaptureFixture();match=fixture.first;finalMove=fixture.second;
   for(unsigned h=0;h<5;++h){s.deck[h]=cards[match.hands[0][h]].form;s.player[s.deck[h]]=2;}saveActiveDeck(s);
   context=id;playingRival=tournamentRival(*e);venue=registration=currentHold=1;active=true;screen="lobby";section.clear();settling=false;lastResult={};resultSounds=ruleSounds=0;++session;
  }
  if(verb=="test-epic-reward"){auto* e=tournament(s,id);in>>e->circuit;e->reward=EpicCards;e->trophy=0;e->quantity=e->circuit+1;e->purse=e->circuit==2?5000:e->circuit==1?1800:400;e->foilUpgrades=0;e->announcement=3;e->invitation=e->circuit?3:0;e->ends=e->starts+(e->circuit?7:3)*24;context=id;active=true;screen="lobby";while(!match.finished()){if(!match.redeal())match.play(match.legalMoves().front());}++session;}
  if(verb=="test-foil-reward"){auto* e=tournament(s,id);e->reward=3;e->trophy=0;e->quantity=2;e->foilUpgrades=2;e->announcement=3;venue=e->hold;context=id;active=true;screen="lobby";while(!match.finished()){if(!match.redeal())match.play(match.legalMoves().front());}++session;}
  if(verb=="test-no-normal-cards"){for(auto& [card,n]:s.player)if(!isFoil(card))n=0;}
  if(verb=="open-pack"&&physicalPacks[0]&&!pendingTournamentFoils(s)){revealed=packCards(tournamentHash(s.economyRandom));for(auto card:revealed)++s.player[card];--physicalPacks[0];revealKind="standard";++revealID;}
  if(verb=="reload"){CollectionSave loaded;if(!decodeCollection(encodeCollection(s),loaded))return 2;s=loaded;forfeitTournamentFoils(s);active=false;if(context)section="tournaments";context=0;screen="album";recoverTournamentInvitations(s,tournamentLetter(*tournament(s,id)),carriedLetters,courierLetters,hour);++session;}
  if(verb=="browse"){in>>screen;++session;}
  if(verb=="deck-manage"){std::string action,name;unsigned target;in>>action>>target;std::getline(in>>std::ws,name);manageDeck(s,action,target,name);++session;}
  if(verb=="start"){unsigned rules,trade,gold;in>>rules>>trade>>gold;if(!trade&&!gold&&startTournamentRound(s,context,hour)){auto e=tournament(s,context);auto rival=tournamentRival(*e);playingRival=rival;auto hand=opponentHand(tournamentStock(s,rival),opponentIndex(rival),e->seed,e->rules);match=Match(indices(s.deck),indices(hand),e->seed,e->rules);active=true;++session;}}
  if(verb=="play"&&active&&!settling&&match.turn==0){Move move;in>>move.hand>>move.square;auto result=match.play(move);if(result.legal){lastResult=result;settling=true;if(match.finished()){const auto score=match.score();assert(recordTournamentMatch(s,context,score[0]==score[1]?-1:score[0]>score[1]?0:1));}}}
  if(verb=="test-result"){int winner;in>>winner;recordTournamentMatch(s,context,winner);while(!match.finished()){if(!match.redeal())match.play(match.legalMoves().front());}}
  if(verb=="forfeit"){withdrawTournament(s,context);context=0;active=false;screen="album";section="tournaments";match=Match(1,0);++session;}
  if(verb=="close"){forfeitTournamentFoils(s);active=false;context=0;screen="album";++session;}
  ++revision;
 }output();}
}
