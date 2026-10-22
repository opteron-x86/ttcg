#include "DeckPresentation.h"
#include "DevelopmentPresentation.h"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace ttcg;
int main(int argc,char** argv){
 if(argc!=3)return 2;
 const std::filesystem::path root=argv[1],preferences=argv[2];
 CollectionSave saved;grantStarter(saved);DevelopmentSettings development;
 std::uint64_t session=700,revision=0;std::string screen="album",capture,notice;bool active=false,writable=true;
 auto load=[&]{std::ifstream file(preferences);std::string value;std::getline(file,value);cardBackPreference=cardBackFromSetting(value);refreshCardBacks(root);};load();
 auto output=[&]{
  auto out=snapshotJson(Match(1,0),session,revision,"Hulda",active,false,{},false,false,false);out.pop_back();
  out+=",\"screen\":"+quote(screen)+",\"matchContext\":true,\"settings\":{\"collection\":65,\"challenge\":66,\"capturing\":"+quote(capture)+",\"cardBackNotice\":"+quote(notice)+",\"development\":"+developmentJson(development,"")+"},\"collection\":{"+savedDeckFields(saved);
  for(auto field:{"owned","known"}){out+=",\""+std::string(field)+"\":[";for(unsigned i=0;i<cards.size()*2;++i){if(i)out+=',';out+=std::to_string(count(saved.player,displayCardID(i)));}out+="]";}
  out+=",\"deck\":[";for(unsigned i=0;i<5;++i){if(i)out+=',';out+=std::to_string(displayIndex(saved.deck[i]));}out+="]";
  out+=",\"gold\":1000,\"canPlay\":true,\"fixedRules\":0,\"fixedTrade\":0,\"maxWager\":0,\"packs\":[1,0],\"revealed\":[],\"revealID\":0}}";
  std::cout<<out<<std::endl;
 };
 output();for(std::string line;std::getline(std::cin,line);){
  std::istringstream input(line);std::string verb,action;std::uint64_t s=0,r=0;input>>verb>>s>>r;
  if(s==session&&r==revision){
   if(verb=="cardback"&&!active&&capture.empty())cardBackCommand(input,notice,[&](std::string_view id){if(!writable)return false;std::ofstream file(preferences);file<<cardBackSetting(id);return bool(file);},root);
   if(verb=="binding"){input>>action;if(action=="begin")input>>capture;else if(action=="cancel")capture.clear();}
   if(verb=="browse"){input>>screen;++session;}
   if(verb=="test-match"){active=true;screen="lobby";++session;}
   if(verb=="test-ready"){active=false;screen="album";++session;}
   if(verb=="test-readonly")writable=false;
   if(verb=="test-writable")writable=true;
   if(verb=="test-development")development.enabled=true;
   if(verb=="reload"){load();++session;}
  }
  output();
 }
}
