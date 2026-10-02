// Included in Plugin.cpp after the normal table helpers.
void tournamentBoard(){
 releaseOpponent();if(active)campaign::savedMatch={};
 if(!campaign::savedMatch.present)ttcg::forfeitTournamentFoils(campaign::saved);
 campaign::tournamentID=0;opponent={};opponentName.clear();challengeContext=false;
 ++epoch;++session;revision=0;active=false;thinking=false;settling=false;lastResult={};
 screen="album";albumSection="tournaments";campaign::updateTournaments();match=ttcg::Match(1,0);publish();
}
bool prepareTournament(unsigned id){
 if(!campaign::interfaceSettings.gamesEnabled)return false;
 if(campaign::savedMatch.present){
  if(active&&match.finished()){releaseOpponent();campaign::savedMatch={};}
  else {if(campaign::savedMatch.tournament==id&&resumeMatch())return true;campaign::notice="Finish your interrupted game first.";++revision;publish();return true;}
 }
 auto& saved=campaign::saved;auto* e=ttcg::tournament(saved,id);const auto hour=campaign::gameHour();
 if(!e||campaign::tournamentVenue()!=e->hold||!ttcg::tournamentOpen(*e,hour)||!ttcg::tournamentNode(*e))return false;
 ttcg::resolveTournament(saved,*e);const auto base=ttcg::tournamentRival(*e);const int i=ttcg::opponentIndex(base);
 if(!ttcg::tournamentEntrant(saved,i)){ttcg::startTournamentRound(saved,id,hour);++revision;publish();return true;}
 auto actor=campaign::resolveID<RE::Actor>(ttcg::opponents[i].reference);
 if((!actor||actor->IsDisabled())&&ttcg::opponents[i].alternateReference)actor=campaign::resolveID<RE::Actor>(ttcg::opponents[i].alternateReference);
 if(!actor||actor->IsDead()||actor->IsDisabled()){campaign::notice="Opponent unavailable.";++revision;publish();return true;}
 campaign::tournamentID=id;if(!campaign::prepare(actor,true)){campaign::tournamentID=0;++revision;publish();return true;}
 campaign::acceptChallenge(actor);opponent=actor->GetHandle();opponentName=actor->GetName();screen="lobby";albumSection.clear();challengeContext=true;
 ++epoch;++session;revision=0;active=false;thinking=false;settling=false;lastResult={};match=ttcg::Match(1,campaign::sessionRules);updateMusic();publish();return true;
}
bool tournamentCommand(std::istringstream& input){
 std::string action,extra;unsigned id=0;if(!(input>>action))return false;
 if(!campaign::interfaceSettings.gamesEnabled&&(action=="enter"||action=="round"||action=="continue"))return false;
 if(thinking||settling||(active&&!match.finished())||(campaign::saved.contract.pending()&&(active||(action!="board"&&action!="seen"))))return false;
 if(campaign::savedMatch.present&&!active&&action!="round"&&action!="board"&&action!="seen")return false;
 if(action=="board"){if(input>>extra)return false;tournamentBoard();return true;}
 if(action=="foil"){
  unsigned card=0;if(!(input>>id>>card)||(input>>extra)||!campaign::hasAlbum())return false;
  const auto* e=ttcg::tournament(campaign::saved,id);
  if(!e||campaign::tournamentVenue()!=e->hold)return false;
  campaign::notice=ttcg::upgradeTournamentFoil(campaign::saved,id,ttcg::displayCardID(int(card)))?"":"Choose an owned card without a foil finish.";
  ++revision;publish();return true;
 }
 if(!(input>>id)||(input>>extra)||!campaign::hasAlbum())return false;
 auto& saved=campaign::saved;
 if(action=="seen"){
  if(!ttcg::acknowledgeTournamentNotice(saved,id,campaign::gameHour()))return false;
  // A notice acknowledgement does not change the game. Keep queued gameplay
  // commands valid while publishing the updated discovery marker.
  publish();return true;
 }
 if(action=="continue"||action=="ack"||action=="leave"){
  auto* e=ttcg::tournament(saved,id);if(!e||!e->joined)return false;ttcg::leaveTournamentRewards(*e);
  if(action=="leave"){close();return true;}
  if(action=="continue"&&ttcg::tournamentNode(*e)&&prepareTournament(id))return true;
  tournamentBoard();return true;
 }
 if(active||screen!="album"||ttcg::pendingTournamentFoils(saved))return false;
 campaign::syncProgression();campaign::updateTournaments();const auto hour=campaign::gameHour();
 if(action=="pack"){
  campaign::notice=campaign::openTournamentPack()?"":"Tournament pack unavailable.";
 }else{
  auto* e=ttcg::tournament(saved,id);if(!e)return false;
  if(action=="enter"){
   const bool withHost=campaign::tournamentVenue()==e->hold&&campaign::tournamentRegistration==e->hold;
   campaign::Bank bank(0);const bool entered=withHost&&ttcg::enterTournament(saved,id,hour,bank);
   campaign::notice=entered?"":!withHost?"Speak to the host to enter.":ttcg::tournamentEntryActive(saved)?"Finish your current tournament first.":!ttcg::tournamentOpen(*e,hour)?"This tournament has closed.":!ttcg::tournamentInvited(*e)?"An invitation is required.":ttcg::playableCount(saved.player)<5?"You need five playable cards to enter.":"Not enough gold for entry.";
   campaign::updateTournaments();
   if(entered&&prepareTournament(id))return true;
  }else if(action=="withdraw"){
   if(!ttcg::withdrawTournament(saved,id))return false;campaign::notice.clear();campaign::updateTournaments();
  }else if(action=="round")return prepareTournament(id);
  else return false;
 }
 ++revision;publish();return true;
}
