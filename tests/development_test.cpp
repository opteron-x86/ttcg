#include "DevelopmentPresentation.h"
#include "Campaign.h"
#include <cassert>
#include <iostream>
using namespace ttcg;
int main() {
 std::map<std::string,std::string> ini;
 auto read=[&](const char* key){return ini[key];};
 auto d=DevelopmentSettings::load(read);CollectionSave s;
 assert(!d.enabled&&d.recordResults&&!d.grantCards(s)&&d.fixedWager(false,true)==-1);
 ini={{"AddAllCards","1"},{"FriendlyGame","1"},{"ShowAllPlayers","1"},{"Rules","55"},{"TradeRule","0"},{"Wager","25"},{"RecordResults","0"}};
 d=DevelopmentSettings::load(read);assert(!d.addAllCards&&!d.showAllPlayers&&d.rules==-1&&d.tradeRule==-1&&d.wager==-1&&d.recordResults);
 ini={{"Enabled","1"}};d=DevelopmentSettings::load(read);
 assert(d.enabled&&!d.showAllPlayers&&!d.unlockAllPlayers&&!d.revealAllCards&&!d.allowAnyNPC&&d.recordResults);
 assert(d.terms({0,1}).trade==1&&d.terms({2,1}).rules==2&&d.maxWager(false,true,10)==10);
 ini={{"Enabled","1"},{"AddAllCards","1"},{"FriendlyGame","1"},{"ShowAllPlayers","1"},{"Rules","55"},{"TradeRule","1"},{"Wager","25"},{"OpponentSkill","0"},{"RecordResults","0"}};
 d=DevelopmentSettings::load(read);assert(d.showAllPlayers&&!d.unlockAllPlayers&&!d.recordResults&&d.opponentSkill==0);
 // Legacy FriendlyGame settings no longer override adult stakes.
 assert(d.terms({0,1}).rules==55&&d.terms({0,1}).trade==1&&d.fixedWager(false,true)==25&&d.maxWager(false,true,10)==25);
 ini["FriendlyGame"]="0";d=DevelopmentSettings::load(read);
 assert(d.terms({0,1}).trade==1&&d.fixedWager(false,true)==25&&d.maxWager(false,true,5)==25);
 assert(d.fixedWager(true,true)==0&&d.maxWager(true,true,200)==0&&d.maxWager(false,false,25)==0);
 for(int trade=0;trade<=4;++trade){ini["TradeRule"]=std::to_string(trade);assert(DevelopmentSettings::load(read).terms({0,1}).trade==unsigned(trade));}
 ini["Rules"]="8";ini["TradeRule"]="5";ini["Wager"]="15";ini["OpponentSkill"]="4";
 d=DevelopmentSettings::load(read);assert(d.rules==-1&&d.tradeRule==-1&&d.wager==-1&&d.opponentSkill==-1);
 for(const auto text:{"3junk","9999999999999999999999","-4","0x03"}) {ini["Rules"]=text;assert(DevelopmentSettings::load(read).rules==-1);}
 ini["Rules"]=" 35 ";d=DevelopmentSettings::load(read);assert(d.rules==35);
 for(auto key:{"RegularTournaments","InvitationalTournaments","MastersTournaments"}){
  ini[key]="1";d=DevelopmentSettings::load(read);const auto on=d.tournamentSwitches();assert(std::count(on.begin(),on.end(),true)==1);
  for(auto invalid:{"-1","2","yes","1junk"}){ini[key]=invalid;auto switches=DevelopmentSettings::load(read).tournamentSwitches();assert(std::none_of(switches.begin(),switches.end(),[](bool v){return v;}));}
  ini[key]="0";
 }
 ini["RegularTournaments"]=ini["InvitationalTournaments"]=ini["MastersTournaments"]="1";
 d=DevelopmentSettings::load(read);assert((d.tournamentSwitches()==std::array<bool,3>{true,true,true}));
 for(unsigned h=1;h<=10;++h){ini["TournamentHold"]=std::to_string(h);assert(DevelopmentSettings::load(read).tournamentHold==h);}
 for(auto invalid:{"0","-1","11","1junk"}){ini["TournamentHold"]=invalid;assert(DevelopmentSettings::load(read).tournamentHold==1);}
 ini["Enabled"]="0";assert((DevelopmentSettings::load(read).tournamentSwitches()==std::array<bool,3>{}));ini["Enabled"]="1";
 // Visibility and eligibility do not write discoveries.
 assert(d.showsPlayer(s,9)&&d.unlockedPlayer(s,9));auto unchanged=encodeCollection(s);
 ini["UnlockAllPlayers"]="1";d=DevelopmentSettings::load(read);assert(d.unlockedPlayer(s,9)&&!d.unlockedPlayer(s,-1)&&!d.unlockedPlayer(s,static_cast<int>(opponents.size()))&&encodeCollection(s)==unchanged);
 progress(s,9).unavailable=true;assert(!d.unlockedPlayer(s,9));progress(s,9).unavailable=false;
 assert(!s.starter&&!progress(s,9).known);
 ini["Enabled"]="0";auto normal=DevelopmentSettings::load(read);assert(!normal.showsPlayer(s,9)&&normal.unlockedPlayer(s,9));
 grantStarter(s);assert(normal.unlockedPlayer(s,5)&&normal.unlockedPlayer(s,9));s={};
 // Additive, one-time grants leave deck, ownership and progress alone.
 s.player[cards[0].form]=2;s.deck[2]=cards[0].form;s.players[42].competitive.wins=2;
 assert(d.grantCards(s));assert(s.developmentCardsGranted&&count(s.player,cards[0].form)==3&&cardCount(s.player)==int(cards.size())+2);
 assert(s.deck[2]==cards[0].form&&!s.starter&&s.players[42].competitive.wins==2);
 assert(d.albumAccess(s)&&DevelopmentSettings{}.albumAccess(s)&&!DevelopmentSettings{}.albumAccess(CollectionSave{}));
 auto wire=encodeCollection(s);CollectionSave loaded;assert(decodeCollection(wire,loaded)&&loaded.developmentCardsGranted&&!d.grantCards(loaded)&&encodeCollection(loaded)==wire);
 ini["Enabled"]="0";assert(!DevelopmentSettings::load(read).grantCards(loaded)&&loaded.player==s.player);
 ini["Enabled"]="1";assert(!DevelopmentSettings::load(read).grantCards(loaded));
 // Sold copies stay gone; catalog identities that were never discovered still arrive later.
 loaded.player.erase(cards[1].form);const auto afterSale=loaded.player;assert(!d.grantCards(loaded)&&loaded.player==afterSale);
 loaded.discovered.erase(cards[1].form);assert(d.grantCards(loaded)&&count(loaded.player,cards[1].form)==1&&loaded.discovered[cards[1].form]==1);
 assert(!d.grantFoils(loaded));ini["AddAllFoils"]="1";d=DevelopmentSettings::load(read);
 assert(d.grantFoils(loaded)&&loaded.developmentFoilsGranted&&count(loaded.player,cards[0].form|foilFlag)==1);
 auto foils=loaded.player;assert(!d.grantFoils(loaded)&&loaded.player==foils);
 loaded.player.erase(cards[2].form|foilFlag);loaded.discovered.erase(cards[2].form|foilFlag);
 assert(d.grantFoils(loaded)&&count(loaded.player,cards[2].form|foilFlag)==1);
 // Corrupt the grant flag itself; later save sections need not end in a boolean.
 auto withoutGrant=s;withoutGrant.developmentCardsGranted=false;const auto other=encodeCollection(withoutGrant);
 auto flag=std::mismatch(wire.begin(),wire.end(),other.begin()).first;assert(flag!=wire.end());*flag=2;assert(!decodeCollection(wire,loaded));
 CollectionSave blocked;blocked.contract.held=true;assert(!d.grantCards(blocked)&&!blocked.developmentCardsGranted&&blocked.player.empty());
 blocked.contract={};blocked.goldCredit=1;assert(!d.grantCards(blocked));blocked.goldCredit=0;
 blocked.player[cards.back().form]=1000000;auto before=blocked.player;assert(!d.grantCards(blocked)&&before==blocked.player&&!blocked.developmentCardsGranted);
 blocked.player.clear();assert(d.grantCards(blocked)&&d.albumAccess(blocked));
 // Explicit Settings actions fill missing copies even after a sale, without
 // requiring the automatic startup flags or altering decks/existing stacks.
 DevelopmentSettings buttons;buttons.enabled=true;CollectionSave album;grantStarter(album);const auto deck=album.deck;
 const auto initial=cardCount(album.player);assert(buttons.addMissingCards(album,false)==int(cards.size())-initial);assert(album.deck==deck&&buttons.albumAccess(album));
 const auto allCards=album.player;assert(buttons.addMissingCards(album,false)==0&&album.player==allCards);
 album.player.erase(cards[4].form);assert(buttons.addMissingCards(album,false)==1&&count(album.player,cards[4].form)==1);
 CollectionSave foilAlbum;assert(buttons.addMissingCards(foilAlbum,true)==int(cards.size()));assert(DevelopmentSettings{}.albumAccess(foilAlbum)&&foilAlbum.developmentFoilsGranted);
 assert(!foilAlbum.developmentCardsGranted&&buttons.addMissingCards(foilAlbum,true)==0);
 CollectionSave unavailable;assert(DevelopmentSettings{}.addMissingCards(unavailable,false)==-1);unavailable.goldCredit=1;assert(buttons.addMissingCards(unavailable,false)==-1&&unavailable.player.empty());
 unavailable.goldCredit=0;unavailable.contract.held=true;assert(buttons.addMissingCards(unavailable,true)==-1);
 unavailable.contract={};unavailable.tournaments.events.emplace_back().foilUpgrades=1;assert(buttons.addMissingCards(unavailable,true)==-1);
 // Settings validate before writing; a failed write cannot change runtime
 // behavior. Reload uses exactly the same INI keys exposed in the UI.
 std::map<std::string,std::string> persisted{{"Enabled","1"}};auto write=[&](std::string_view key,int value){persisted[std::string(key)]=std::to_string(value);return true;};
 assert(!buttons.setPersisted("Enabled",0,write)&&!buttons.setPersisted("Rules",Open|ThreeOpen,write)&&!buttons.setPersisted("TournamentHold",11,write));
 assert(!buttons.setPersisted("RegularTournaments",1,[](auto,int){return false;})&&!buttons.regularTournaments);
 for(auto [key,field]:DevelopmentSettings::switches){assert(buttons.setPersisted(key,1,write));assert(buttons.*field);assert(!buttons.setPersisted(key,2,write));}
 for(auto [key,value]:std::array<std::pair<const char*,int>,6>{{{"TournamentHold",8},{"Rules",Same|SameWall|Chaos},{"TradeRule",4},{"Wager",25},{"OpponentSkill",3},{"RuleLesson",4}}})assert(buttons.setPersisted(key,value,write));
 auto reloaded=DevelopmentSettings::load([&](const char* key){return persisted[key];});assert(developmentJson(reloaded)==developmentJson(buttons));
 assert(developmentJson(DevelopmentSettings{})=="{\"enabled\":false}");
 std::cout<<"Development settings: independent defaults, overrides, validation, safe grants and current-save persistence passed\n";
}
