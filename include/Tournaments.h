#pragma once
#include "Campaign.h"
namespace ttcg {
struct TournamentVenue {unsigned hold;const char* name;const char* host;};
inline constexpr std::array<TournamentVenue,10> tournamentVenues{{
 {1,"The Bannered Mare","Hulda"},{2,"Dead Man's Drink","Valga Vinicia"},{3,"The Winking Skeever","Corpulus Vinius"},
 {4,"Moorside Inn","Jonna"},{5,"Windpeak Inn","Thoring"},{6,"The Bee and Barb","Keerava"},
 {7,"The Frozen Hearth","Dagur"},{8,"Candlehearth Hall","Elda Early-Dawn"},{9,"Silver-Blood Inn","Kleppr"},{10,"The Retching Netch","Geldis Sadri"}
}};
inline unsigned tournamentHash(unsigned x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline Tournament* tournament(CollectionSave& s,unsigned id){for(auto& e:s.tournaments.events)if(e.id==id)return &e;return nullptr;}
inline const Tournament* tournament(const CollectionSave& s,unsigned id){for(const auto& e:s.tournaments.events)if(e.id==id)return &e;return nullptr;}
inline unsigned tournamentNode(const Tournament& e){if(!e.joined||e.eliminated||e.bracket[14])return 0;for(unsigned n:{8u,12u,14u}){if(!e.bracket[n])return n;if(e.bracket[n]!=tournamentPlayer)return 0;}return 0;}
inline unsigned tournamentRival(const Tournament& e){auto n=tournamentNode(e);return n?e.bracket[(n-8)*2+1]:0;}
inline bool tournamentOpen(const Tournament& e,unsigned hour){return e.ends&&hour>=e.starts&&hour<e.ends&&!e.bracket[14];}
inline bool tournamentInvited(const Tournament& e){return !e.circuit||e.invitation==3;}
inline bool tournamentVisible(const Tournament& e,unsigned hour){return e.joined||(e.discovered&&tournamentOpen(e,hour));}
inline bool tournamentEntryAvailable(const Tournament& e,unsigned hour){return tournamentOpen(e,hour)&&tournamentInvited(e)&&e.discovered&&!e.joined;}
inline bool tournamentEntryActive(const CollectionSave& s){return std::any_of(s.tournaments.events.begin(),s.tournaments.events.end(),[](const auto& e){return e.joined&&!e.eliminated&&!e.bracket[14];});}
inline bool tournamentReserved(const CollectionSave& s,unsigned base,unsigned except=0){
 return std::any_of(s.tournaments.events.begin(),s.tournaments.events.end(),[&](const auto& e){return e.id!=except&&!e.awarded&&std::find(e.bracket.begin(),e.bracket.begin()+8,base)!=e.bracket.begin()+8;});
}
inline void discoverTournaments(CollectionSave& s,unsigned hold,unsigned hour){
 for(auto& e:s.tournaments.events)if(!e.circuit&&e.hold==hold&&tournamentOpen(e,hour))e.discovered=true;
}
inline void discoverAllTournaments(CollectionSave& s,unsigned hour){
 // Information reveals active events; eligibility and courier receipt still
 // govern registration. Future courier windows remain undisclosed.
 for(auto& e:s.tournaments.events)if(tournamentOpen(e,hour))e.discovered=true;
}
inline bool acknowledgeTournamentNotice(CollectionSave& s,unsigned id,unsigned hour){
 auto* e=tournament(s,id);if(!e||!tournamentVisible(*e,hour))return false;e->notified=true;return true;
}
inline unsigned tournamentNotice(const CollectionSave& s,unsigned hold,unsigned hour){
 for(const auto& e:s.tournaments.events)if(!e.circuit&&e.hold==hold&&e.discovered&&!e.notified&&tournamentOpen(e,hour)&&!e.joined)return e.id;
 return 0;
}
inline bool tournamentEntrant(const CollectionSave& s,int i){return i>=0&&i<int(opponents.size())&&opponents[i].level>=2&&unlockedOpponent(s,i)&&!s.tournamentBlocked.contains(opponents[i].base)&&!s.deadPlayers.contains(opponents[i].base)&&opponentHasCards(s,i);}
inline unsigned tournamentEligibility(const CollectionSave& s,unsigned hold){
 const auto r=localReputation(s,hold);
 const auto renown=widerRenown(s);
 if((r.rank>=Famous&&r.quarters>=400&&r.wins>=30)||(renown>=40*4&&tournamentChampionships(s,1)>=2))return 3;
 if(r.rank>=Respected||renown>=16*4)return 2;
 return r.rank>=Unsung?1:0;
}
inline const char* tournamentCircuitName(unsigned circuit){return circuit==2?"Tessera Masters":circuit==1?"Invitational":"Tournament";}
inline bool tournamentField(const CollectionSave& s,int i,unsigned circuit){return tournamentEntrant(s,i)&&(opponents[i].traveller||(circuit==2?opponents[i].level==5:circuit==1?opponents[i].level>=3:opponents[i].level<=3));}
inline unsigned tournamentLetter(const Tournament& e){return 0xF80+(e.hold-1)*3+e.circuit;}
inline unsigned recordTournamentInvitation(CollectionSave& s,unsigned letter,bool delivered,unsigned hour,unsigned queuedEvent=0){
 // A courier can carry several copies of a hold's letter. Receive the oldest
 // queued invitation first; acknowledge the exact event that sent each copy.
 for(auto& e:s.tournaments.events)if(tournamentLetter(e)==letter&&e.circuit&&!e.awarded&&e.invitation==(delivered?2u:1u)&&(!queuedEvent||e.id==queuedEvent)){
  // New-game initialization and time-changing mods can move the clock back.
  // Physical dispatch/receipt still happened; never reject it and send again.
  e.announced=std::min(e.announced,hour);
  e.invitation=delivered?3:2;if(delivered){e.starts=hour;e.ends=hour+7*24;e.discovered=true;}return e.id;
 }
 return 0;
}
inline std::vector<unsigned> recoverTournamentInvitations(CollectionSave& s,unsigned letter,unsigned carried,unsigned courierCopies,unsigned hour){
 // Older saves may contain a delivered note whose Papyrus callback failed.
 // Only recover copies missing from the courier's outstanding deliveries;
 // keeping an old invitation must not unlock the next event before delivery.
 const unsigned pending=std::count_if(s.tournaments.events.begin(),s.tournaments.events.end(),[&](const auto& e){return tournamentLetter(e)==letter&&e.invitation==2;});
 unsigned received=std::min(carried,pending>courierCopies?pending-courierCopies:0u);
 std::vector<unsigned> result;
 for(unsigned n=0;n<received;++n)if(auto id=recordTournamentInvitation(s,letter,true,hour))result.push_back(id);
 return result;
}
inline std::vector<unsigned> reconcileTournamentInvitations(CollectionSave& s,unsigned letter,unsigned carried,unsigned courierCopies,unsigned hour){
 auto queued=std::count_if(s.tournaments.events.begin(),s.tournaments.events.end(),[&](const auto& e){return e.invitation==2&&tournamentLetter(e)==letter;});
 // Physical courier stock confirms dispatch even if its Papyrus acknowledgement
 // failed. This check must run before recovery and while a request is in flight.
 for(auto& e:s.tournaments.events)if(e.invitation==1&&tournamentLetter(e)==letter&&courierCopies>queued){
  if(recordTournamentInvitation(s,letter,false,hour,e.id))++queued;
 }
 // Repair the first development invitation stranded by the old clock guard:
 // the note was delivered after time moved back, so dispatch never persisted.
 // A later event sharing this BOOK must still receive its own courier copy.
 if(carried&&!courierCopies&&!queued)for(auto& e:s.tournaments.events){
  if(e.development&&e.invitation==1&&e.announced>hour&&tournamentLetter(e)==letter&&
     std::none_of(s.tournaments.events.begin(),s.tournaments.events.end(),[&](const auto& old){return old.id<e.id&&tournamentLetter(old)==letter;})){
   if(recordTournamentInvitation(s,letter,false,hour,e.id))++queued;
   break;
  }
 }
 // One-time repair for the retired Whiterun switch. New tests never set this
 // token; a retained invitation must not unlock their next event.
 if(s.tournaments.developmentToken&&carried&&!courierCopies&&!queued){
  for(auto& e:s.tournaments.events)if(e.development&&e.hold==1&&e.circuit==1&&e.invitation==1&&tournamentLetter(e)==letter){
   if(recordTournamentInvitation(s,letter,false,hour,e.id))s.tournaments.developmentToken=0;
   break;
  }
 }
 return recoverTournamentInvitations(s,letter,carried,courierCopies,hour);
}
inline void advanceTournamentByDefault(Tournament& e,unsigned node,unsigned winner){
 e.bracket[node]=winner;e.wins[node-8][winner==e.bracket[(node-8)*2]?0:1]=2;
}
inline Stock tournamentStock(const CollectionSave& s,unsigned base){int i=opponentIndex(base);if(i<0)return {};const auto& p=opponents[i];auto it=s.opponents.find(opponentLedgerKey(p.base,p.reference));return it==s.opponents.end()?personalPool(i):it->second;}
inline std::vector<CardID> tournamentPack(unsigned seed){
 std::mt19937 rng(seed);std::vector<CardID> result;
 // Five distinct identities; tiers III–VII. 5% foil per copy.
 for(unsigned n=0;n<5;++n){unsigned r=rng()%100;const char* rarity=r<30?"Common":r<75?"Rare":"Epic";
  unsigned t=rng()%100;int tier=t<15?3:t<40?4:t<70?5:t<90?6:7;std::vector<CardID> pool;
  for(const auto& c:cards)if(c.available&&!c.unique&&c.tier==tier&&std::string_view(c.rarity)==rarity&&std::none_of(result.begin(),result.end(),[&](auto id){return baseCardID(id)==c.form;}))pool.push_back(c.form);
  if(pool.empty())return {};
  auto id=pool[rng()%pool.size()];if(rng()%100<5)id|=foilFlag;result.push_back(id);
 }return result;
}
inline std::vector<CardID> tournamentPrizeCards(const Tournament& e){
 if(e.reward==Trophy)return {e.trophy};
 if(e.reward!=EpicCards)return {};
 // The saved event seed fixes the actual cards/finishes at announcement. Each
 // tier IV–VII has equal weight; identities within this prize are distinct.
 std::mt19937 rng(tournamentHash(e.seed^0x45504943));std::vector<CardID> result;
 for(unsigned n=0;n<e.quantity;++n){const int tier=4+rng()%4;std::vector<CardID> pool;
  for(const auto& c:cards)if(!c.pending&&c.available&&!c.unique&&c.tier==tier&&std::string_view(c.rarity)=="Epic"&&std::none_of(result.begin(),result.end(),[&](auto id){return baseCardID(id)==c.form;}))pool.push_back(c.form);
  if(pool.empty())return {};
  auto id=pool[rng()%pool.size()];if(e.circuit||rng()%100<10)id|=foilFlag;result.push_back(id);
 }return result;
}
inline const Tournament* pendingTournamentFoils(const CollectionSave& s){for(const auto& e:s.tournaments.events)if(e.foilUpgrades)return &e;return nullptr;}
inline void leaveTournamentRewards(Tournament& e){e.foilUpgrades=0;e.announcement=0;}
inline void forfeitTournamentFoils(CollectionSave& s){for(auto& e:s.tournaments.events)if(e.reward==3&&e.bracket[14]==tournamentPlayer&&e.announcement==3)leaveTournamentRewards(e);}
template<class Deliver> void deliverTournamentPacks(CollectionSave& s,Deliver deliver){
 for(unsigned kind=0;kind<2;++kind){auto& credit=kind?s.tournaments.packCredit:s.tournaments.standardPackCredit;
  if(credit)credit-=std::clamp(deliver(kind,int(credit)),0,int(credit));
 }
}
inline bool canAddTournamentCards(const CollectionSave& s,const std::vector<CardID>& ids){
 Stock needed;for(auto id:ids){if(cardIndex(id)<0)return false;++needed[id];}for(auto [id,n]:needed)if(count(s.player,id)>1000000-n)return false;
 return true;
}
inline bool addTournamentCards(CollectionSave& s,const std::vector<CardID>& ids){
 if(!canAddTournamentCards(s,ids))return false;
 for(auto id:ids){++s.player[id];s.discovered[id]=1;s.discovered[baseCardID(id)]=1;}return true;
}
// Validate the complete prize before consuming its physical inventory item.
template<class Consume> std::vector<CardID> openTournamentPack(CollectionSave& s,Consume consume){
 if(s.contract.pending()||pendingTournamentFoils(s))return {};
 auto ids=tournamentPack(tournamentHash(s.economyRandom));if(ids.size()!=5||!canAddTournamentCards(s,ids)||!consume())return {};
 addTournamentCards(s,ids);s.economyRandom=tournamentHash(s.economyRandom+1);return ids;
}
inline bool upgradeTournamentFoil(CollectionSave& s,unsigned eventID,CardID id){
 auto* e=tournament(s,eventID);
 if(!e||!e->foilUpgrades||!e->awarded||e->reward!=3||e->bracket[14]!=tournamentPlayer||e->announcement!=3||s.contract.pending()||isFoil(id)||cardIndex(id)<0||count(s.player,id)<1||count(s.player,id|foilFlag)>=1000000)return false;
 --s.player[id];++s.player[id|foilFlag];s.discovered[id|foilFlag]=1;--e->foilUpgrades;
 // Replace one normal copy in each saved deck if it would otherwise become missing.
 auto fix=[&](Hand& hand){if(std::count(hand.begin(),hand.end(),id)>count(s.player,id)){auto it=std::find(hand.begin(),hand.end(),id);if(it!=hand.end())*it=id|foilFlag;}};
 fix(s.deck);for(auto& d:s.decks)fix(d.cards);saveActiveDeck(s);return true;
}
inline unsigned tournamentNPCWinner(const CollectionSave& s,const Tournament& e,unsigned node,unsigned game=0){
 const auto a=e.bracket[(node-8)*2],b=e.bracket[(node-8)*2+1];int ai=opponentIndex(a),bi=opponentIndex(b);
 if(!tournamentEntrant(s,ai))return b;if(!tournamentEntrant(s,bi))return a;
 auto seed=tournamentHash(e.seed^node^(game*2654435761u));auto ah=opponentHand(tournamentStock(s,a),ai,seed,e.rules),bh=opponentHand(tournamentStock(s,b),bi,seed^0xABC123,e.rules);
 // Resolve actual games with the same rule engine; bounded searches avoid blocking the game thread.
 for(unsigned replay=0;replay<3;++replay){Match m(indices(ah),indices(bh),seed+replay,e.rules);
  while(!m.finished()){if(m.redeal())continue;auto belief=m;
   const int skill=opponents[m.turn?bi:ai].skill;if(!(belief.rules&Open)){std::mt19937 hidden(seed+m.placed+m.redeals*9);for(int h=0;h<5;++h)if(!m.known(1-m.turn,h,m.turn))belief.hands[1-m.turn][h]=hidden()%cards.size();}auto move=chooseMove(belief,300+skill*500,1+int(skill>1));
   if(!m.play(move).legal)m.play(m.legalMoves().front());
  }auto score=m.score();if(score[0]!=score[1])return score[0]>score[1]?a:b;
 }return seed&1?a:b;
}
inline void awardTournament(CollectionSave& s,Tournament& e){
 if(e.awarded||!e.bracket[14])return;const auto winner=e.bracket[14];
 const auto prize=tournamentPrizeCards(e);
 if((e.reward==Trophy&&prize.size()!=1)||(e.reward==EpicCards&&prize.size()!=e.quantity))return;
 if(winner==tournamentPlayer){
  if(s.goldCredit>1000000-int(e.purse))return;
  if(!prize.empty()&&!addTournamentCards(s,prize))return;
  if(e.reward==1&&s.tournaments.packCredit>1000000-e.quantity)return;
  if(e.reward==2&&s.tournaments.standardPackCredit>1000000-e.quantity)return;
  s.goldCredit+=e.purse;if(e.reward==1)s.tournaments.packCredit+=e.quantity;if(e.reward==2)s.tournaments.standardPackCredit+=e.quantity;if(e.reward==3&&e.announcement==3)e.foilUpgrades=e.quantity;
 }else{
  auto i=opponentIndex(winner);if(i<0)return;const auto& p=opponents[i];const auto key=opponentLedgerKey(p.base,p.reference);
  if(!s.opponents.contains(key))s.opponents[key]=personalPool(i);auto& stock=s.opponents[key];
  for(auto id:prize)if(cardIndex(id)<0||count(stock,id)>=1000000)return;
  for(auto id:prize)++stock[id];
  if(e.reward==1||e.reward==2)for(unsigned n=0;n<e.quantity;++n){auto ids=e.reward==1?tournamentPack(tournamentHash(e.seed+n)):packCards(tournamentHash(e.seed+n));for(auto id:ids){if(e.reward==2)id=mintCard(s,id);if(count(stock,id)<1000000)++stock[id];}}
  if(e.reward==3){std::vector<CardID> ids;for(auto [id,n]:stock)if(n>0&&!isFoil(id))ids.push_back(id);std::sort(ids.begin(),ids.end(),[](auto a,auto b){return prizeValue(a)>prizeValue(b);});for(unsigned n=0;n<std::min(e.quantity,unsigned(ids.size()));++n){--stock[ids[n]];++stock[ids[n]|foilFlag];}}
  s.tournaments.npcGold[winner]=std::min(1000000u,s.tournaments.npcGold[winner]+e.purse);
 }
 if(e.joined&&winner!=tournamentPlayer){const int i=opponentIndex(winner);if(i>=0)progress(s,i).known=true;}
 e.awarded=true;
}
inline bool replaceTournamentEntrant(const CollectionSave& s,Tournament& e,unsigned base){
 const int old=opponentIndex(base);if(old<0)return false;int best=-1;unsigned distance=999;
 for(unsigned i=0;i<opponents.size();++i)if(!opponents[i].traveller&&tournamentField(s,i,e.circuit)&&!tournamentReserved(s,opponents[i].base,e.id)&&std::find(e.bracket.begin(),e.bracket.end(),opponents[i].base)==e.bracket.end()){
  unsigned score=10*std::abs(int(opponents[i].level)-int(opponents[old].level))+(opponents[i].hold!=e.hold);
  if(score<distance){best=i;distance=score;}
 }
 if(best<0)return false;
 // A substitute inherits the bracket score, not earned defeats of the old NPC.
 const auto node=tournamentNode(e);if(node&&tournamentRival(e)==base)e.reputationWins[node==8?0:node==12?1:2]=0;
 for(auto& id:e.bracket)if(id==base)id=opponents[best].base;return true;
}
inline void playTournamentNPCGame(CollectionSave& s,Tournament& e,unsigned node){
 if(e.bracket[node])return;auto a=e.bracket[(node-8)*2],b=e.bracket[(node-8)*2+1];if(!a||!b)return;
 auto& w=e.wins[node-8];const auto winner=tournamentNPCWinner(s,e,node,w[0]+w[1]);
 if(++w[winner==a?0:1]==2)e.bracket[node]=winner;
}
inline void resolveTournament(CollectionSave& s,Tournament& e,bool finish=false){
 if(!e.matchNode&&!e.bracket[14])for(unsigned n=0;n<8;++n){const auto base=e.bracket[n];if(base!=tournamentPlayer&&!tournamentEntrant(s,opponentIndex(base)))replaceTournamentEntrant(s,e,base);}
 if(finish&&e.joined&&!e.eliminated&&!e.bracket[14])e.announcement=4;
 if(finish)for(unsigned node=8;node<15;++node){if(e.bracket[node])continue;auto a=e.bracket[(node-8)*2],b=e.bracket[(node-8)*2+1];if(!a||!b)continue;
  if(a==tournamentPlayer||b==tournamentPlayer){advanceTournamentByDefault(e,node,a==tournamentPlayer?b:a);e.eliminated=true;e.matchNode=0;}
  else while(!e.bracket[node])playTournamentNPCGame(s,e,node);
 }
 awardTournament(s,e);
}
inline bool recordTournamentMatch(CollectionSave& s,unsigned id,int winner,bool recordResults=true){
 auto* e=tournament(s,id);if(!e||!e->matchNode||e->matchNode!=tournamentNode(*e)||winner< -1||winner>1)return false;
 const auto node=e->matchNode;const int rival=opponentIndex(e->bracket[(node-8)*2+1]);e->lastNode=node;e->matchNode=0;
 if(recordResults)recordCompetitiveGame(s,rival,winner,e->rules);
 if(winner<0){e->replays=std::min(1000000u,e->replays+1);return true;}
 auto& w=e->wins[node-8];++w[winner];const bool decided=w[winner]==2;
 auto& earned=e->reputationWins[node==8?0:node==12?1:2];
 if(recordResults&&winner==0&&rival>=0)++earned;
 if(decided&&winner==0&&earned==2)recordTournamentReputation(s,*e,rival,node==14);
 if(decided)e->bracket[node]=winner==0?tournamentPlayer:e->bracket[(node-8)*2+1];
 // Other tables play one game with yours. Complete this stage before revealing the next.
 const unsigned end=node==8?12:node==12?14:15;
 for(unsigned n=node+1;n<end;++n){playTournamentNPCGame(s,*e,n);if(decided)while(!e->bracket[n])playTournamentNPCGame(s,*e,n);}
 if(decided){e->eliminated=winner==1;e->announcement=winner==1?4:node==8?1:node==12?2:3;}
 resolveTournament(s,*e,e->eliminated);return true;
}
inline bool withdrawTournament(CollectionSave& s,unsigned id){auto* e=tournament(s,id);if(!e||!e->joined||e->eliminated||e->bracket[14])return false;e->eliminated=true;e->matchNode=0;e->announcement=4;resolveTournament(s,*e,true);return true;}
template<class Bank> bool enterTournament(CollectionSave& s,unsigned id,unsigned hour,Bank& bank){
 auto* e=tournament(s,id);if(!e||!tournamentEntryAvailable(*e,hour)||s.contract.pending()||s.goldCredit||playableCount(s.player)<5)return false;
 if(tournamentEntryActive(s)||pendingTournamentFoils(s))return false;
 if(bank.count(0,0)<int(e->fee))return false;const int paid=-bank.change(0,0,-int(e->fee));if(paid!=int(e->fee)){s.goldCredit+=paid;return false;}
 e->joined=true;e->notified=true;e->bracket[0]=tournamentPlayer;return true;
}
inline bool tournamentTrophy(const Card& c,unsigned circuit){return !c.pending&&c.unique&&!c.available&&std::string_view(c.rarity)=="Legendary"&&(circuit==1?c.tier==8:circuit==2&&(c.tier==9||c.tier==10));}
inline void selectTournamentReward(CollectionSave& s,Tournament& e,std::mt19937& rng){
 auto& ts=s.tournaments;const auto circuit=e.circuit;
 const auto category=rng()%4;e.trophy=0;
 e.reward=category==0?(circuit?Trophy:EpicCards):category==1?TournamentPacks:category==2?(circuit?EpicCards:StandardPacks):FoilUpgrades;
 e.quantity=e.reward==TournamentPacks?circuit+1:e.reward==StandardPacks?3:e.reward==EpicCards?circuit+1:e.reward==FoilUpgrades?(circuit==2?5:circuit==1?3:1):1;
 if(e.reward==Trophy){std::vector<CardID> pool;for(const auto& c:cards)if(tournamentTrophy(c,circuit)&&!ts.trophies.contains(c.form)&&count(s.player,c.form)==0&&count(s.player,c.form|foilFlag)==0){bool owned=merchantOwnsCard(s,c.form);for(const auto& [key,stock]:s.opponents)owned|=count(stock,c.form)>0||count(stock,c.form|foilFlag)>0;if(!owned)pool.push_back(c.form);}
  CardID recovered=0;unsigned estate=0;
  for(auto base:s.deadPlayers){int i=opponentIndex(base);if(i<0)continue;const auto key=opponentLedgerKey(base,opponents[i].reference);auto found=s.opponents.find(key);if(found==s.opponents.end())continue;
   for(auto [id,n]:found->second)if(n>0&&ts.trophies.contains(baseCardID(id))&&tournamentTrophy(cards[cardIndex(id)],circuit)){
    bool elsewhere=merchantOwnsCard(s,id)||count(s.player,baseCardID(id))||count(s.player,baseCardID(id)|foilFlag);
    for(const auto& [other,stock]:s.opponents)if(other!=key)elsewhere|=count(stock,baseCardID(id))>0||count(stock,baseCardID(id)|foilFlag)>0;
    for(const auto& old:ts.events)if(!old.awarded&&baseCardID(old.trophy)==baseCardID(id))elsewhere=true;
    if(!elsewhere){recovered=id;estate=key;break;}
   }
   if(recovered)break;
  }
  if(recovered){e.trophy=recovered;--s.opponents[estate][recovered];}
  else if(pool.empty()){e.reward=TournamentPacks;e.quantity=circuit+1;}else{e.trophy=pool[rng()%pool.size()];ts.trophies.insert(e.trophy);}
 }
}
inline void refreshTournamentRewards(CollectionSave& s){
 // Reapply the current prize policy to unentered offers from development
 // builds. Keep the event, courier invitation and draw intact.
 for(auto& e:s.tournaments.events){
  if(e.joined||e.awarded)continue;
  const bool valid=e.reward==Trophy?e.circuit&&cardIndex(e.trophy)>=0&&tournamentTrophy(cards[cardIndex(e.trophy)],e.circuit)&&e.quantity==1:
   e.reward==TournamentPacks?e.quantity==e.circuit+1:e.reward==StandardPacks?!e.circuit&&e.quantity==3:
   e.reward==EpicCards?e.quantity==e.circuit+1:e.reward==FoilUpgrades&&e.quantity==(e.circuit==2?5u:e.circuit==1?3u:1u);
  if(valid)continue;
  const auto old=baseCardID(e.trophy);e.trophy=0;
  if(old&&!merchantOwnsCard(s,old)&&!count(s.player,old)&&!count(s.player,old|foilFlag)&&
    std::none_of(s.opponents.begin(),s.opponents.end(),[&](const auto& item){return count(item.second,old)||count(item.second,old|foilFlag);})&&
    std::none_of(s.tournaments.events.begin(),s.tournaments.events.end(),[&](const auto& other){return baseCardID(other.trophy)==old;}))s.tournaments.trophies.erase(old);
  std::mt19937 rng(tournamentHash(e.seed^0x5052495A));selectTournamentReward(s,e,rng);
 }
}
inline unsigned newTournament(CollectionSave& s,unsigned hold,unsigned hour,bool development=false,unsigned circuit=0){
 auto& ts=s.tournaments;if(hold<1||hold>10||circuit>2)return 0;if(ts.events.size()>=40&&std::none_of(ts.events.begin(),ts.events.end(),[](const auto& e){return e.awarded&&(!e.joined||!e.announcement);} ))return 0;
 std::mt19937 rng(tournamentHash(++ts.sequence^hour^0x74746367));Tournament e;e.id=ts.sequence;e.seed=rng();e.hold=hold;e.announced=hour;e.starts=hour;e.ends=hour+(circuit?7:3)*24;e.development=development;
 e.rules=randomTournamentRules(rng);
 const auto eligible=tournamentEligibility(s,hold);
 e.circuit=circuit;e.invitation=circuit&&(development||eligible>circuit)?1:0;
 if(e.invitation){e.starts=0;e.ends=0;}
 e.fee=development?0:circuit==2?500:circuit==1?150:50;e.purse=circuit==2?5000:circuit==1?1800:400;
 std::vector<unsigned> locals,visitors;for(unsigned i=0;i<opponents.size();++i)if(!opponents[i].traveller&&tournamentField(s,i,circuit)&&!tournamentReserved(s,opponents[i].base))(opponents[i].hold==hold?locals:visitors).push_back(i);
 std::shuffle(locals.begin(),locals.end(),rng);std::shuffle(visitors.begin(),visitors.end(),rng);locals.insert(locals.end(),visitors.begin(),visitors.end());if(locals.size()<8)return 0;
 for(unsigned n=0;n<8;++n)e.bracket[n]=opponents[locals[n]].base;
 // M'aiq is a rare wildcard in every circuit, never an ordinary filler.
 // Keep him out of the player's entry slot and every Reverse combination.
 const auto guestRoll=tournamentHash(e.seed^0x4D414951u);
 if(!(e.rules&Reverse)&&guestRoll%100<5){
  const int guest=opponentIndex(0x954BF);
  if(tournamentField(s,guest,circuit)&&!tournamentReserved(s,opponents[guest].base))e.bracket[1+(guestRoll/100)%7]=opponents[guest].base;
 }
 selectTournamentReward(s,e,rng);
 if(ts.events.size()>=40){auto it=std::find_if(ts.events.begin(),ts.events.end(),[](const auto& old){return old.awarded&&!old.joined;});if(it==ts.events.end())it=std::find_if(ts.events.begin(),ts.events.end(),[](const auto& old){return old.awarded&&!old.announcement;});if(it==ts.events.end())return 0;ts.events.erase(it);}
 ts.events.push_back(e);return e.id;
}
inline void tickTournaments(CollectionSave& s,unsigned hour,unsigned currentHold=0,std::array<bool,3> developmentCircuits={}){
 auto& ts=s.tournaments;if(!s.starter&&!s.developmentCardsGranted)return;
 for(auto& e:ts.events)if(!e.awarded&&e.ends){if(hour>=e.ends)resolveTournament(s,e,true);else if(tournamentOpen(e,hour))resolveTournament(s,e);}
 constexpr unsigned rotation[]{1,8,2,6,3,9,4,7,5,10};
 if(!ts.nextRegular[0]){
  // Spread the first local season over 37 days; subsequent dates are per hold.
  for(unsigned n=0;n<10;++n)ts.nextRegular[rotation[n]-1]=hour+(1+n*37/10)*24;
  ts.nextCircuit={hour+(21+tournamentHash(hour^0x1A71)%15)*24,hour+(60+tournamentHash(hour^0xAA57)%31)*24};
 }else{
  unsigned activeRegular=std::count_if(ts.events.begin(),ts.events.end(),[](const auto& e){return !e.circuit&&!e.awarded;});
  unsigned nextHold=0,earliest=~0u;
  for(unsigned h=0;h<10;++h)if(!developmentCircuits[0]&&hour>=ts.nextRegular[h]){
   if(ts.nextRegular[h]<earliest){earliest=ts.nextRegular[h];nextHold=h+1;}
   ts.nextRegular[h]=hour+(30+tournamentHash(hour^(h+1)*2654435761u)%16)*24;
  }
  // Missed dates never spawn a backlog after waiting or a long absence.
  if(nextHold&&activeRegular<3)newTournament(s,nextHold,hour,false,0);
  for(unsigned c=1;c<=2;++c)if(!developmentCircuits[c]&&hour>=ts.nextCircuit[c-1]){
   const unsigned days=c==1?21+tournamentHash(hour^ts.sequence^c)%15:60+tournamentHash(hour^ts.sequence^c)%31;
   ts.nextCircuit[c-1]=hour+days*24;
   const bool waiting=std::any_of(ts.events.begin(),ts.events.end(),[&](const auto& e){return e.circuit==c&&!e.awarded&&!e.ends;});
   // One outstanding courier invitation per circuit. Keep the next scheduled
   // date even when its field cannot be filled or a prior letter is undelivered.
   if(!waiting){const auto h=rotation[ts.circuitRotation[c-1]++%10];newTournament(s,h,hour,false,c);}
  }
 }
 discoverTournaments(s,currentHold,hour);
}
inline bool startTournamentRound(CollectionSave& s,unsigned id,unsigned hour){
 auto* e=tournament(s,id);if(!e||!e->joined||e->eliminated||e->matchNode||!tournamentOpen(*e,hour)||s.contract.pending()||s.goldCredit||!validHand(s.deck,s.player))return false;
 resolveTournament(s,*e);const unsigned node=tournamentNode(*e),rival=tournamentRival(*e);if(!node||!rival)return false;
 if(!tournamentEntrant(s,opponentIndex(rival))){e->lastNode=node;advanceTournamentByDefault(*e,node,tournamentPlayer);const unsigned end=node==8?12:node==12?14:15;for(unsigned n=node+1;n<end;++n)while(!e->bracket[n])playTournamentNPCGame(s,*e,n);e->announcement=node==8?1:node==12?2:3;resolveTournament(s,*e);return false;}
 e->matchNode=node;return true;
}
}
