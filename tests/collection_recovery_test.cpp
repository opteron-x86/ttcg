#include "CollectionRecovery.h"
#include "Development.h"
#include "ExpansionLoader.h"
#include "ExpansionRewards.h"
#include "CollectionAPIService.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
void install(std::string id) {
 const auto json=nlohmann::json{{"schemaVersion",1},{"setId",id},{"name",id},{"cards",nlohmann::json::array({{
  {"id","visitor"},{"name","Visitor"},{"tier",1},{"rarity","Common"},{"sides",{2,3,4,5}},
  {"groups",{"Spirit"}},{"affinities",nlohmann::json::array()},{"unique",false}
 }})}};
 auto definition=parseExpansion(json.dump());registerExpansion(definition.id,definition.name,std::move(definition.cards));
}
void resetCatalog() {
 cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());
 expansionCards.clear();expansionKeys.clear();cardSets.clear();
}
int main() {
 install("removed");install("kept");install("seen-only");
 const auto old=expansionCardID("removed:visitor"),seen=expansionCardID("seen-only:visitor");
 CollectionSave original;grantStarter(original);
 assert(grantExpansionCard(original,"removed:visitor",3,false));assert(grantExpansionCard(original,"removed:visitor",2,true));
 assert(grantExpansionCard(original,"kept:visitor",2,false));
 original.discovered[seen]=1;original.bettingPurses[123]={500,120};
 original.deck[0]=old;original.deck[1]=old|foilFlag;saveActiveDeck(original);
 assert(manageDeck(original,"copy",1,"Expansion deck"));
 original.players[123].competitive.wins=4;original.reputation[123]={2,8};
 original.opponents[123][old]=1;original.merchants[456].resale[old|foilFlag]=1;original.merchants[456].generation=1;
 original.exchanges[789]={1,123,123,old,0,25,ExchangeStatus::Posted};
 const auto installedWire=encodeCollection(original);
 resetCatalog();install("kept");CollectionSave missing;assert(decodeCollection(installedWire,missing));
 const auto kept=expansionCardID("kept:visitor"),orphan=missing.deck[0];
 assert(cardIndex(orphan)<0&&count(missing.player,orphan)==3);
 assert(missingCollectionSets(missing)==std::set<std::string>{"removed"});
 const auto missingWire=encodeCollection(missing);
 const auto ownedBefore=cardCount(missing.player),knownBefore=count(missing.discovered,kept);
 const auto decksBefore=missing.decks;const auto active=missing.activeDeck;
 assert(removeUnavailableCards(missing));
 assert(missingCollectionSets(missing).empty()&&count(missing.player,orphan)==0&&count(missing.player,orphan|foilFlag)==0);
 assert(cardCount(missing.player)==ownedBefore&&count(missing.player,kept)==2&&count(missing.discovered,kept)==knownBefore&&missing.bettingPurses[123].gold==500);
 for(auto [id,n]:missing.discovered)assert(cardIndex(id)>=0);
 assert(missing.activeDeck==active&&missing.decks.size()==decksBefore.size());
 for(unsigned i=0;i<missing.decks.size();++i){assert(missing.decks[i].id==decksBefore[i].id&&missing.decks[i].name==decksBefore[i].name);assert(!missing.decks[i].cards[0]&&!missing.decks[i].cards[1]);assert(missing.decks[i].cards[2]==decksBefore[i].cards[2]);}
 assert(missing.opponents[123].at(orphan)==1&&missing.merchants[456].resale.at(orphan|foilFlag)==1);
 assert(missing.exchanges[789].requested==orphan&&missing.players[123].competitive.wins==4&&missing.reputation[123].wins==2);
 const auto cleanedWire=encodeCollection(missing);assert(removeUnavailableCards(missing)&&encodeCollection(missing)==cleanedWire);
 CollectionSave reloaded;assert(decodeCollection(cleanedWire,reloaded)&&missingCollectionSets(reloaded).empty());
 // Reinstall restores an untouched collection, but cannot resurrect removed
 // ownership or discovery. NPC/merchant/quest references still resolve by key.
 install("removed");install("seen-only");
 const auto restored=expansionCardID("removed:visitor");
 assert(decodeCollection(missingWire,reloaded)&&count(reloaded.player,restored)==3&&count(reloaded.player,restored|foilFlag)==2);
 assert(decodeCollection(cleanedWire,reloaded)&&count(reloaded.player,restored)==0&&count(reloaded.discovered,restored)==0);
 assert(count(reloaded.discovered,expansionCardID("seen-only:visitor"))==0);
 assert(reloaded.opponents[123].at(restored)==1&&reloaded.exchanges[789].requested==restored);

 DevelopmentSettings development;development.enabled=true;
 for(int failure=1;failure<=3;++failure){
  auto s=original;auto d=development;d.addAllCards=d.addAllFoils=d.revealAllCards=true;
  const auto before=encodeCollection(s);int writes=0;
  assert(!d.clearCollection(s,[&](auto,int){return ++writes!=failure;}));
  assert(writes==failure&&encodeCollection(s)==before);
  assert(d.addAllCards==(failure==1)&&d.addAllFoils==(failure<=2)&&d.revealAllCards);
 }
 for(int mode=0;mode<6;++mode){
  auto s=original;auto d=development;
  if(mode==0)d.enabled=false;
  if(mode==1)s.contract.held=true;
  if(mode==2)s.contract.paying=true;
  if(mode==3)s.goldCredit=1;
  if(mode==4)s.tournaments.events.emplace_back().foilUpgrades=1;
  if(mode==5)s.tournaments.events.emplace_back().joined=true;
  auto before=encodeCollection(s);int writes=0;
  assert(!d.clearCollection(s,[&](auto,int){++writes;return true;})&&writes==0&&encodeCollection(s)==before);
  if(mode){assert(!removeUnavailableCards(s)&&encodeCollection(s)==before);}
 }
 assert(!collectionRecoveryBlocked(original,true).empty());
 // Start from a missing-set save again to cover all finishes and orphan copies.
 resetCatalog();install("kept");assert(decodeCollection(missingWire,reloaded));
 std::map<std::string,std::string> ini{{"Enabled","1"},{"AddAllCards","1"},{"AddAllFoils","1"},{"RevealAllCards","1"}};
 auto d=DevelopmentSettings::load([&](const char* key){return ini[key];});
 collectionAPI::Store api;assert(api.refresh(reloaded));const auto previousRevision=api.getRevision();
 const auto npcs=reloaded.opponents;const auto discovery=reloaded.discovered;const auto quests=reloaded.exchanges;
 assert(d.clearCollection(reloaded,[&](const char* key,int value){ini[key]=std::to_string(value);return true;}));
 assert(reloaded.player.empty()&&reloaded.bettingPurses[123].gold==500&&reloaded.discovered.empty()&&!discovery.empty());
 assert(reloaded.deck==Hand{}&&reloaded.starter&&reloaded.activeDeck==active);
 for(const auto& deck:reloaded.decks)assert(deck.cards==Hand{});
 assert(reloaded.opponents==npcs&&reloaded.exchanges==quests&&reloaded.players[123].competitive.wins==4&&reloaded.reputation[123].wins==2);
 assert(api.refresh(reloaded)&&api.getRevision()!=previousRevision);
 assert(api.acquire()->summary.standardCopies==0&&api.acquire()->summary.foilCopies==0&&api.acquire()->summary.discoveredCards==0);
 CollectionSave cleared;assert(decodeCollection(encodeCollection(reloaded),cleared));
 d=DevelopmentSettings::load([&](const char* key){return ini[key];});
 assert(!d.addAllCards&&!d.addAllFoils&&!d.revealAllCards&&!d.grantCards(cleared)&&!d.grantFoils(cleared));
 assert(DevelopmentSettings{}.albumAccess(cleared)&&!claimAlbum(cleared,true)&&!grantStarter(cleared));
 assert(cardCount(cleared.player)==0&&missingCollectionSets(cleared).empty());
 assert(grantExpansionCard(cleared,"kept:visitor",1,false)&&cardCount(cleared.player)==1);
 // A character with only developer album access stays able to open the album.
 CollectionSave developerOnly;assert(development.clearCollection(developerOnly,[](auto,int){return true;}));
 assert(DevelopmentSettings{}.albumAccess(developerOnly)&&!claimAlbum(developerOnly,true));
 std::cout<<"Collection recovery: missing expansions, reinstall, decks, progress, API notifications, grants, write failures and pending-operation guards passed\n";
}
