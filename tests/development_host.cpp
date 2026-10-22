#include "DevelopmentPresentation.h"
#include "DevelopmentTournaments.h"
#include "TournamentPresentation.h"
#include "DeckPresentation.h"
#include <iostream>
using namespace ttcg;
int main(){
 CollectionSave saved;grantStarter(saved);DevelopmentSettings settings;settings.enabled=true;
 std::map<std::string,std::string> ini{{"Enabled","1"}};std::string notice;unsigned session=300,revision=0;
 auto output=[&]{
  auto out=snapshotJson(Match(1,Open),session,revision,"Hulda",false,false,{},false,false,false);out.pop_back();
  out+=",\"screen\":\"album\",\"revealCards\":"+std::string(settings.revealAllCards?"true":"false")+",\"settings\":{\"collection\":65,\"challenge\":66,\"development\":"+developmentJson(settings,notice)+"},\"collection\":{"+savedDeckFields(saved);
  out+=",\"owned\":[";for(unsigned i=0;i<cards.size()*2;++i){if(i)out+=',';out+=std::to_string(count(saved.player,displayCardID(i)));}out+="]";
  out+=",\"deck\":[";for(unsigned i=0;i<5;++i){if(i)out+=',';out+=std::to_string(displayIndex(saved.deck[i]));}out+="]";
  out+=",\"gold\":1000,\"canPlay\":true,\"fixedRules\":16,\"packs\":[0,0],\"tournaments\":"+tournamentsJson(saved,100,1,1,{201,7,17,10},"Test player",1)+"}}";
  std::cout<<out<<std::endl;
 };
 output();for(std::string line;std::getline(std::cin,line);){
  std::istringstream input(line);std::string verb,action,key;unsigned s,r;int value=0;input>>verb>>s>>r;
  if(s==session&&r==revision){
   if(verb=="development"&&settings.enabled){
    input>>action;
    if(action=="set"&&input>>key>>value)settings.setPersisted(key,value,[&](std::string_view key,int value){ini[std::string(key)]=std::to_string(value);return true;});
    else if(action=="cards"||action=="foils")notice="Added "+std::to_string(settings.addMissingCards(saved,action=="foils"))+" copies.";
    updateDevelopmentTournaments(saved,settings,100,1);++revision;
   }
   if(verb=="test-disabled"){ini["Enabled"]="0";settings=DevelopmentSettings::load([&](const char* key){return ini[key];});++revision;}
   if(verb=="reload"){CollectionSave loaded;if(!decodeCollection(encodeCollection(saved),loaded))return 2;saved=std::move(loaded);settings=DevelopmentSettings::load([&](const char* key){return ini[key];});++session;revision=0;}
  }
  output();
 }
}
