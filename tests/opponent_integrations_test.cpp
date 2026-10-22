#include "OpponentLoader.h"
#include "Tournaments.h"
#include <cassert>
#include <iostream>
#include <fstream>
using namespace ttcg;
using Json=nlohmann::json;
Json definition(){return Json{{"schemaVersion",1},{"id","test-npcs"},{"opponents",Json::array({{{"id","resident"},{"actorBase",{{"plugin","Town.esp"},{"formId","800"}}},{"skill","Regular"},{"homeRegion","Whiterun"},{"collection",{{"theme","Nord"}}}}})}};}
bool rejects(const Json& j){try{parseOpponentIntegration(j.dump());return false;}catch(const std::exception&){return true;}}
int main(){
 auto d=parseOpponentIntegration(definition().dump());assert(d.opponents.size()==1&&d.opponents[0].level==2&&d.opponents[0].hold==1&&d.opponents[0].actor.plugin=="town.esp");
 auto expanded=definition();expanded["opponents"][0]["wager"]=2000;expanded["opponents"][0]["rules"]={"Descension","Dominion","Reverse"};
 const auto upgraded=parseOpponentIntegration(expanded.dump()).opponents[0];assert(upgraded.wager==2000&&upgraded.rules==(Descension|Dominion|Reverse));
 expanded["opponents"][0]["rules"].push_back("Ascension");assert(rejects(expanded));expanded["opponents"][0].erase("rules");expanded["opponents"][0]["wager"]=2001;assert(rejects(expanded));
 auto bad=definition();bad["opponents"][0]["rules"]={"Order","Chaos"};assert(rejects(bad));bad=definition();bad["opponents"][0]["rules"]={"Same Wall"};assert(rejects(bad));
 bad=definition();bad["opponents"][0]["actorBase"]["formId"]="FE000800";assert(rejects(bad));bad=definition();bad["opponents"][0]["actorBase"]["plugin"]="../bad.esp";assert(rejects(bad));
 bad=definition();bad["opponents"][0]["skill"]="God";assert(rejects(bad));bad=definition();bad["opponents"][0]["surprise"]=true;assert(rejects(bad));
 bad=definition();bad["opponents"][0]["dialogue"]={{"mode","generic"},{"accept",{{"plugin","Town.esp"},{"formId","901"}}}};assert(rejects(bad));
 bad=definition();bad["opponents"][0]["dialogue"]={{"mode","custom"},{"accept",{{"plugin","Town.esp"},{"formId","901"}}}};
 bad["opponents"][0]["availability"]=Json::array({{{"quest",{{"plugin","Town.esp"},{"formId","900"}}},{"stageDone",20}}});assert(parseOpponentIntegration(bad.dump()).opponents[0].gates[0].value==20);
 try{parseOpponentIntegration("{\"id\":\"a\",\"id\":\"b\"}");assert(false);}catch(const std::exception&){}
 // Stable plugin/local identities, including ESL locals and case differences.
 const auto base=worldFormID({"Town.esp",0x800}),ref=worldFormID({"Town.esp",0x801});assert(base==worldFormID({"town.ESP",0x800}));
 assert(base!=worldFormID({"OtherTown.esl",0x800}));assert(worldFormID({"Dragonborn.esm",0x123})==0xDB000123);
 // Participation and collection are generated once. Manual enrollment only
 // promotes selection; it never rerolls an existing character.
 unsigned selected=0;for(unsigned seed=0;seed<100;++seed){selected+=selectResident(1,seed);assert(selectResident(2,seed));assert(!selectResident(0,seed));assert(selectResident(0,seed,true));}assert(selected==60);
 for(unsigned level=0;level<6;++level){const auto stock=generatedNpcStock(level,"Nord",57);assert(playableCount(stock)>=18);for(auto [id,n]:stock)assert(!cards[cardIndex(id)].unique);assert(validHand(opponentHand(stock,-1,17),stock));}
 CollectionSave s;s.residents.seed=123;
 createResident(s,base,ref,"Resident","Whiterun","Nord",1,0);assert(!s.residents.profiles.at(base).selected);const auto original=s.residents.profiles.at(base);
 assert(createResident(s,base,ref,"Changed name","Somewhere","Daedra",8,0,true));const auto& record=s.residents.profiles.at(base);assert(record.source==1&&record.starting==original.starting&&record.seed==original.seed&&record.level==original.level&&record.hold==1);
 const int i=opponentIndex(base);assert(i>=int(baseOpponents.size()));replenishOpponent(s,i,ref,10);const auto initial=s.opponents.at(ref);s.opponents[ref].clear();replenishOpponent(s,i,ref,11);assert(cardCount(s.opponents.at(ref))==0);assert(!opponentHasCards(s,i));replenishOpponent(s,i,ref,82);assert(cardCount(s.opponents.at(ref))>0);
 // Known-player discovery and regional credit share the normal campaign path.
 assert(!progress(s,i).known);acceptOpponentChallenge(s,i);assert(progress(s,i).known);recordReputationWin(s,i);assert(s.reputation.contains(base));assert(!opponents[i].requiredMaster);
 // Explicit cards include high-tier expansion uniques and foils. One-time
 // assignments preserve losses and refuse to mint duplicate unique ownership.
 ExpansionCard c;c.key="test-cards:signature";c.name="Signature";c.rarity="Legendary";c.tier=10;c.unique=true;c.sides={10,10,10,10};c.groups="[\"Nord\"]";c.affinities="[]";c.focus="Neutral";c.setId="test-cards";c.setName="Test";c.art=c.thumbnail="backs/mosaic.webp";c.groupMask=1;
 registerExpansion("test-cards","Test",{c});const auto signature=expansionCardID(c.key);
 assert(grantNpcCards(s,ref,"test:signature",{{c.key,1,true}}));assert(count(s.opponents[ref],signature|foilFlag)==1);s.opponents[ref].erase(signature|foilFlag);s.player[signature|foilFlag]=1;
 assert(!grantNpcCards(s,ref,"test:signature",{{c.key,1,true}}));assert(grantNpcCards(s,ref,"test:second-request",{{c.key,1,true}}));assert(count(s.opponents[ref],signature|foilFlag)==0);
 // No partial grant when any dependency is missing.
 const auto stockBefore=s.opponents[ref];assert(!grantNpcCards(s,ref,"test:missing",{{"tessera:murkwater-goblin",1,false},{"missing:card",1,false}}));assert(s.opponents[ref]==stockBefore&&!s.residents.grants.contains("test:missing"));
 // Runtime identity order changes cannot move collections or reputation to a
 // different actor. Dormant profiles retain their history and assignment tokens.
 s.players[base].known=true;s.invitations[base]={100,true};s.encounterAfter[base]=50;s.residents.profiles[base].starting.push_back(signature|foilFlag);
 const auto wire=encodeCollection(s);worldForms().clear();worldFormID({"First.esl",0x800});worldFormID({"First.esl",0x801});const auto newRef=worldFormID({"Town.esp",0x801});assert(newRef!=ref);CollectionSave loaded;assert(decodeCollection(wire,loaded));
 const auto newBase=worldFormID({"Town.esp",0x800});assert(loaded.opponents.at(newRef)==s.opponents.at(ref)&&loaded.players.at(newBase).known&&loaded.reputation.at(newBase).wins==s.reputation.at(base).wins);
 assert(loaded.residents.profiles.at(newBase).reference==newRef&&loaded.residents.grants==s.residents.grants&&loaded.invitations.at(newBase).until==100);
 resetRuntimeOpponents();for(auto& [b,r]:loaded.residents.profiles)registerResident(b,r);assert(opponentIndex(newBase)>=0&&progress(loaded,opponentIndex(newBase)).known);
 // Authored profiles can use exact collections, fixed rules and custom speech.
 auto custom=d.opponents[0];custom.key="test-npcs:custom";custom.actor={"Follower.esp",0x800};custom.generated=false;custom.level=5;custom.personalRules=true;custom.rules=Open|Plus;custom.trade=TradeAll;custom.reputationGate=0;custom.restock=false;custom.cards={{"tessera:murkwater-goblin",2,false},{"tessera:thieves-guild-recruit",2,false},{"tessera:dwarven-spider",1,true}};
 const auto cb=worldFormID(custom.actor),cr=worldFormID({"Follower.esp",0x801});assert(createResident(loaded,cb,cr,"Follower","Solitude","",3,0,false,&custom));const int ci=opponentIndex(cb);assert(opponents[ci].level==5&&opponentTerms(loaded,ci).rules==(Open|Plus)&&!opponents[ci].tournamentEligible);
 replenishOpponent(loaded,ci,cr,100);assert(cardCount(loaded.opponents[cr])==5&&count(loaded.opponents[cr],2090|foilFlag)==1);loaded.opponents[cr].clear();replenishOpponent(loaded,ci,cr,1000);assert(loaded.opponents[cr].empty());
 // Adding an authored definition changes the template and skill, never the
 // live collection or a quest's persistent enable flag.
 const auto owned=loaded.opponents.at(newRef);auto& adopted=loaded.residents.profiles.at(newBase);assert(adoptOpponentDefinition(adopted,custom));
 assert(adopted.source==2&&adopted.level==5&&loaded.opponents.at(newRef)==owned);
 adopted.enabled=false;assert(adoptOpponentDefinition(adopted,custom)&&!adopted.enabled);
 auto unavailable=custom;unavailable.cards.push_back({"absent:signature",1,false});const auto starting=adopted.starting;
 assert(!adoptOpponentDefinition(adopted,unavailable)&&adopted.starting==starting);
 custom.requiredMaster=true;registerResident(cb,loaded.residents.profiles.at(cb),&custom);assert(opponents[ci].requiredMaster);
 registerResident(cb,loaded.residents.profiles.at(cb));assert(!opponents[ci].requiredMaster&&!opponents[ci].tournamentEligible&&loaded.opponents.at(cr).empty());
 // An unknown land does not produce an invalid save or Whiterun child history.
 loaded.childHolds[0xDEAD]=0;CollectionSave check;assert(decodeCollection(encodeCollection(loaded),check)&&check.childHolds.at(0xDEAD)==0);
 // Shipped examples use the same parser. Conflicting integrations cannot win
 // by filename order, and base-roster edits must use explicit grants.
 for(const auto* path:{"examples/opponent-integration/SKSE/Plugins/Tessera/Opponents/example-opponents.json","examples/opponent-authoring/custom-opponent.json.template"}){std::ifstream input(path);assert(input);std::string bytes{std::istreambuf_iterator<char>(input),{}};assert(!parseOpponentIntegration(bytes).id.empty());}
 const auto dir=std::filesystem::temp_directory_path()/"ttcg-opponent-integration-test";std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);
 auto one=definition(),two=one;two["id"]="another-integration";
 {std::ofstream(dir/"a.json")<<one;std::ofstream(dir/"b.json")<<two;}
 assert(loadOpponentIntegrations(dir).size()==2&&opponentDefinitions.empty());
 std::filesystem::remove(dir/"b.json");assert(loadOpponentIntegrations(dir).empty()&&opponentDefinitions.size()==1);
 one["opponents"][0]["actorBase"]={{"plugin","Skyrim.esm"},{"formId","13BA3"}};{std::ofstream(dir/"a.json")<<one;}
 assert(loadOpponentIntegrations(dir).size()==1&&opponentDefinitions.empty());std::filesystem::remove_all(dir);
 std::cout<<"NPC integrations: strict definitions, selection, manual enrollment, collections, grants, depleted stock, persistent identities and shared progression passed\n";
}
