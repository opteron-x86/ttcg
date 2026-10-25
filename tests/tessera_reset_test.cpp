#include "DevelopmentPresentation.h"
#include "Exchange.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int main(){
 CollectionSave history;grantStarter(history);const auto starter=history;
 const auto requested=history.deck[0],reward=history.deck[1];
 history.player[requested]=25;history.player[requested|foilFlag]=2;history.discovered[requested|foilFlag]=1;
 history.missingCards[missingCardBase]="old-pack:card";history.player[missingCardBase]=4;history.discovered[missingCardBase]=1;
 assert(manageDeck(history,"copy",1,"Test deck"));
 history.players[123].competitive={9,4,2};history.players[123].known=true;
 history.reputation[123]={9,12};history.tournamentReputation[0].championships[1]=3;
 history.opponents[123][reward]=5;history.bettingPurses[123]={50,120};
 history.merchants[456].generation=1;history.merchants[456].stock[requested]=3;history.merchants[456].resale[requested|foilFlag]=2;
 assert(postExchange(history,789,123,123,requested,reward,0)==ExchangeResult::Done);
 assert(acceptExchange(history,789,1)==ExchangeResult::Done);
 history.exchanges[790]={7,123,123,missingCardBase,0,25,ExchangeStatus::Posted};
 history.holds[1].rules=Same|Open;history.encounteredRules=Open;
 history.culture.carriedHold=1;history.culture.carriedRules=Open;history.culture.regions[1].added=Open;
 history.invitations[123]={200,true};history.encounterAfter[123]=200;
 history.residents.seed=37;history.residents.grants.insert("json:test:skyrim.esm:123");
 history.residents.profiles[124]={125,37,1,1,1,true,true,false,"Test player","Whiterun","", "",{requested}};
 history.developmentCardsGranted=history.developmentFoilsGranted=true;
 history.tournaments.sequence=41;assert(newTournament(history,1,100,true,0));
 const auto sequence=history.tournaments.sequence;
 const auto before=encodeCollection(history);CollectionSave checked;assert(decodeCollection(before,checked));
 DevelopmentSettings dev;dev.enabled=true;dev.addAllCards=dev.addAllFoils=true;
 for(auto [key,field]:DevelopmentSettings::switches)dev.*field=true;
 dev.recordResults=false;dev.rules=Open;dev.tradeRule=4;dev.wager=25;dev.opponentSkill=3;dev.ruleLesson=4;dev.tournamentHold=8;
 // Every failure point must preserve all saved progress, not just ownership.
 for(int failedWrite=1;failedWrite<=16;++failedWrite){
  auto s=history;auto settings=dev;int writes=0;
  assert(!settings.resetTessera(s,[&](auto,int){return ++writes!=failedWrite;}));
  assert(writes==failedWrite&&encodeCollection(s)==before);
 }
 for(int mode=0;mode<10;++mode){
  auto s=history;auto settings=dev;
  if(mode==0)settings.enabled=false;
  if(mode==1)s.contract.held=true;
  if(mode==2)s.contract.paying=true;
  if(mode==3)s.goldCredit=1;
  if(mode==4)s.tournaments.events.front().foilUpgrades=1;
  if(mode==5)s.tournaments.events.front().joined=true;
  if(mode==6)s.merchants[456].goldCredit=1;
  if(mode==7)s.tournaments.packCredit=1;
  if(mode==8)s.tournaments.standardPackCredit=1;
  if(mode==9)s.tournaments.npcGold[123]=5;
  const auto saved=encodeCollection(s);int writes=0;
  assert(!settings.resetTessera(s,[&](auto,int){++writes;return true;})&&writes==0&&encodeCollection(s)==saved);
 }
 assert(!tesseraResetBlocked(history,true).empty());
 std::map<std::string,std::string> ini{{"Enabled","1"},{"UnrelatedSetting","preserve"}};
 auto s=history;assert(dev.resetTessera(s,[&](std::string_view key,int value){ini[std::string(key)]=std::to_string(value);return true;}));
 assert(s.starter&&s.player==starter.player&&s.discovered==starter.discovered&&s.deck==starter.deck&&cardCount(s.player)==8);
 assert(s.decks.size()==1&&s.decks[0].name=="Deck 1"&&s.activeDeck==1&&s.nextDeck==2);
 assert((s.players.empty()&&s.reputation.empty()&&s.tournamentReputation[0].championships==std::array<unsigned,3>{}));
 assert(s.opponents.empty()&&s.bettingPurses.empty()&&s.merchants.empty()&&s.residents.profiles.empty()&&s.residents.grants.empty()&&!s.residents.seed);
 assert(s.culture.regions.empty()&&!s.culture.carriedRules&&s.invitations.empty()&&s.encounterAfter.empty()&&!s.encounteredRules);
 assert(s.holds.at(1).rules==starter.holds.at(1).rules&&!s.developmentCardsGranted&&!s.developmentFoilsGranted);
 assert(s.tournaments.events.empty()&&s.tournaments.trophies.empty()&&s.tournaments.sequence==sequence);
 for(const auto& [owner,offer]:s.exchanges)assert(!offer.active()&&offer.status==ExchangeStatus::Cancelled);
 assert(s.exchanges.at(789).serial==1&&s.exchanges.at(790).serial==7&&s.missingCards.at(missingCardBase)=="old-pack:card");
 assert(missingCollectionSets(s).empty()&&checkExchange(s,789,1)==ExchangeResult::Stale);
 const auto wire=encodeCollection(s);assert(decodeCollection(wire,checked)&&encodeCollection(checked)==wire);
 auto loaded=DevelopmentSettings::load([&](const char* key){return ini[key];});DevelopmentSettings defaults;defaults.enabled=true;
 assert(developmentJson(loaded)==developmentJson(defaults)&&!loaded.addAllCards&&!loaded.addAllFoils&&ini.at("UnrelatedSetting")=="preserve");
 assert(!loaded.grantCards(checked)&&!loaded.grantFoils(checked)&&!claimAlbum(checked,true)&&checked.player==starter.player);
 // Fresh gameplay works; pre-reset quest serials and tournament IDs stay stale.
 checked.opponents[123][reward]=1;
 assert(postExchange(checked,789,123,123,requested,reward,0)==ExchangeResult::Done&&checked.exchanges.at(789).serial==2);
 assert(acceptExchange(checked,789,2)==ExchangeResult::Done&&completeExchange(checked,789,1)==ExchangeResult::Stale);
 assert(newTournament(checked,1,100,true,0)>sequence);
 std::cout<<"Tessera reset: starter album, erased progress, saved defaults, pending-operation guards, failed writes, reload and stale callbacks passed\n";
}
