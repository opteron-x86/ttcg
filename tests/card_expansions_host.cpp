#include "ExpansionLoader.h"
#include "ExpansionRewards.h"
#include "Campaign.h"
#include "DeckPresentation.h"
#include "ArtTransfer.h"
#include "TournamentPresentation.h"
#include <iostream>
int main(int argc,char** argv){
 const auto sample=std::filesystem::path("examples/card-expansion");
 const auto report=ttcg::loadCardExpansions(sample/"SKSE/Plugins/Tessera/Expansions");
 if(!report.errors.empty()||report.cards!=5)return 1;
 const auto art=sample/"TesseraUI/art";ttcg::refreshCardArt(art);
 if(argc==3&&std::string_view(argv[1])=="--art"){
  auto result=ttcg::readCardArt(argv[2],art);
  std::cout<<nlohmann::json({{"data",result.data},{"error",result.error}}).dump();return result.error.empty()?0:2;
 }
 const bool trophy=argc==2&&std::string_view(argv[1])=="--trophy";
 ttcg::CollectionSave saved;ttcg::grantStarter(saved);
 for(const auto& e:ttcg::expansionCards)if(!trophy||!e.unique)ttcg::grantExpansionCard(saved,e.key,1,false);
 ttcg::grantExpansionCard(saved,"tessera-example:grove-sentinel",1,true);
 if(trophy){
  const auto id=ttcg::expansionCardID("tessera-example:the-wayfarer");
  for(const auto& c:ttcg::cards)if(c.unique&&c.form!=id)saved.tournaments.trophies.insert(c.form);
  for(unsigned seed=0;seed<100;++seed){auto trial=saved;trial.tournaments.sequence=seed;auto eventID=ttcg::newTournament(trial,1,100,true,1);auto* e=ttcg::tournament(trial,eventID);
   if(!e||e->trophy!=id)continue;
   e->invitation=3;e->starts=100;e->ends=268;e->joined=e->discovered=true;e->bracket[0]=ttcg::tournamentPlayer;
   for(unsigned node=8;node<15;++node){e->bracket[node]=e->bracket[(node-8)*2];e->wins[node-8][0]=2;}
   e->announcement=3;e->lastNode=14;ttcg::awardTournament(trial,*e);saved=std::move(trial);break;
  }
  if(saved.tournaments.events.empty()||!saved.tournaments.events[0].awarded)return 3;
 }
 auto data=nlohmann::json::parse(ttcg::snapshotJson(ttcg::Match{},1,0,"",false,false,{},false));
 data["screen"]="album";data["matchContext"]=false;
 auto& c=data["collection"];c=nlohmann::json::parse("{"+ttcg::savedDeckFields(saved)+"}");
 c["deck"]=nlohmann::json::array();for(auto id:saved.deck)c["deck"].push_back(ttcg::displayIndex(id));
 c["owned"]=nlohmann::json::array();c["known"]=nlohmann::json::array();
 for(unsigned i=0;i<ttcg::cards.size()*2;++i){auto id=ttcg::displayCardID(i);c["owned"].push_back(ttcg::count(saved.player,id));c["known"].push_back(ttcg::count(saved.discovered,id)>0);}
 c["notice"]="";c["packs"]={0,0};c["revealed"]=nlohmann::json::array();c["revealID"]=0;
 c["fixedRules"]=ttcg::Open;c["fixedTrade"]=1;c["fixedWager"]=0;c["ruleLesson"]=0;c["gold"]=500;c["canPlay"]=true;
 if(trophy){data["albumSection"]="tournaments";c["tournamentID"]=saved.tournaments.events[0].id;c["tournaments"]=nlohmann::json::parse(ttcg::tournamentsJson(saved,101,1));}
 std::cout<<data.dump()<<std::endl;
}
