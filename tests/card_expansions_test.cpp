#include "ExpansionLoader.h"
#include "ExpansionRewards.h"
#include "CardArt.h"
#include "Campaign.h"
#include "Shop.h"
#include "SavedMatch.h"
#include "Tournaments.h"
#include <cassert>
#include <iostream>

using namespace ttcg;
using Json=nlohmann::json;
Json definition(std::string set="test-set"){
 return Json{{"schemaVersion",1},{"setId",set},{"name","Test expansion"},{"cards",Json::array({{
  {"id","visitor"},{"name","Visitor"},{"tier",1},{"rarity","Common"},{"sides",{10,10,10,10}},
  {"groups",{"Khajiit","Spirit"}},{"affinities",{"Agility","Intelligence"}},{"unique",false}
 }})}};
}
void install(const Json& json){auto d=parseExpansion(json.dump());registerExpansion(d.id,d.name,std::move(d.cards));}
// Only tests rebuild the registry. Skyrim loads it once, before saves and AI.
void reset(){cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());expansionCards.clear();expansionKeys.clear();cardSets.clear();}
bool rejects(Json json){try{parseExpansion(json.dump());return false;}catch(const std::exception&){return true;}}
struct Bank {
 CollectionSave& save;
 int count(int p,CardID id){return ttcg::count(p?save.opponents[123]:save.player,id);}
 int change(int p,CardID id,int n){auto& stock=p?save.opponents[123]:save.player;const auto moved=std::max(-count(p,id),n);stock[id]+=moved;return moved;}
};
int main(){
 // Author balance is unconstrained: tier I / four 10s, multiple groups/affinities.
 const auto doc=definition();install(doc);auto id=expansionCardID("test-set:visitor");auto index=cardIndex(id);
 assert(cards.size()==baseCards.size()+1&&cards[index].sides[0]==10&&cards[index].affinityMask==10);
 assert(std::popcount(cardGroups[index])==2&&cards[index].tier==1);
 for(auto field:{"name","id","tier","groups","unique","affinities"}){auto bad=doc;bad["cards"][0].erase(field);assert(rejects(bad));}
 for(auto [field,value]:std::vector<std::pair<std::string,Json>>{{"tier",0},{"tier",11},{"tier",1.5},{"sides",{1,2,3}},{"sides",{1,2,3,11}},{"sides",{1,2,3,true}},{"rarity","Mythic"},{"groups",{"New group"}},{"groups",Json::array()},{"groups",{"Spirit","Spirit"}},{"affinities",{"Neutral"}},{"affinities",{"Agility","Agility"}},{"unique",1},{"id","Visitor"},{"name","Bad\nname"},{"unknown",true}}){auto bad=doc;bad["cards"][0][field]=value;assert(rejects(bad));}
 for(auto path:{"../outside.png","cards/Other.png","expansions/test-set/../out.png","expansions/other-set/art.png","expansions/test-set/art.svg","expansions/test-set//art.png","expansions/test-set/a:stream.png","expansions/test-set/a.png?x"}){auto bad=doc;bad["cards"][0]["art"]=path;assert(rejects(bad));}
 auto duplicate=doc;duplicate["cards"].push_back(duplicate["cards"][0]);assert(rejects(duplicate));
 auto unique=doc;unique["cards"][0]["unique"]=true;unique["cards"][0]["acquisition"]={{"standardPacks",true}};assert(rejects(unique));
 auto bad=doc;bad["schemaVersion"]=2;assert(rejects(bad));bad=doc;bad["setId"]="tessera";assert(rejects(bad));
 try{parseExpansion("{\"setId\":\"a\",\"setId\":\"b\"}");assert(false);}catch(const std::exception&){}
 // No acquisition route is implicit; base pools retain identical seeded draws.
 reset();std::vector<std::vector<CardID>> ordinary,premium;
 for(unsigned seed=0;seed<500;++seed){ordinary.push_back(packCards(seed));premium.push_back(tournamentPack(seed));}
 MerchantStock baseShop;refreshShop(baseShop,55,0,1,false);const auto child=childStock(0x13BBA);
 install(doc);
 for(unsigned seed=0;seed<500;++seed){assert(packCards(seed)==ordinary[seed]);assert(tournamentPack(seed)==premium[seed]);}
 MerchantStock unchanged;refreshShop(unchanged,55,0,1,false);assert(baseShop.stock==unchanged.stock&&childStock(0x13BBA)==child);
 // Opt-in cards enter existing buckets; extra trader allocation is bounded.
 reset();auto publicDoc=doc;
 publicDoc["cards"][0]["acquisition"]={{"standardPacks",true},{"tournamentPacks",true},{"merchants","all"}};
 for(unsigned n=1;n<12;++n){auto c=publicDoc["cards"][0];c["id"]="visitor-"+std::to_string(n);c["tier"]=3;publicDoc["cards"].push_back(c);}install(publicDoc);
 unsigned packs=0,tournaments=0;
 for(unsigned seed=0;seed<200;++seed){for(auto c:packCards(seed))packs+=c>=expansionCardBase;for(auto c:tournamentPack(seed))tournaments+=baseCardID(c)>=expansionCardBase;}
 assert(packs>0&&tournaments>0);
 MerchantStock ordinaryShop,specialShop;refreshShop(ordinaryShop,55,0,1,false);refreshShop(specialShop,55,0,1,true);
 auto extra=[](const auto& shop){return std::count_if(shop.stock.begin(),shop.stock.end(),[](auto p){return baseCardID(p.first)>=expansionCardBase;});};
 assert(extra(ordinaryShop)==2&&extra(specialShop)==3);for(auto [c,n]:baseShop.stock)assert(count(ordinaryShop.stock,c)==n);
 // Save remapping follows keys through ordering, name, art and metadata changes.
 reset();install(doc);id=expansionCardID("test-set:visitor");
 CollectionSave s;grantStarter(s);assert(grantExpansionCard(s,"test-set:visitor",2,true));
 s.deck[0]=id|foilFlag;saveActiveDeck(s);s.opponents[123][id]=1;
 s.merchants[456].resale[id|foilFlag]=1;s.merchants[456].generation=1;
 auto wire=encodeCollection(s);
 reset();install(definition("before"));auto renamed=doc;renamed["cards"][0]["name"]="Renamed Visitor";renamed["cards"][0]["sides"]={2,3,4,5};install(renamed);
 const auto moved=expansionCardID("test-set:visitor");assert(moved!=id);
 CollectionSave loaded;assert(decodeCollection(wire,loaded));assert(count(loaded.player,moved|foilFlag)==2&&loaded.deck[0]==(moved|foilFlag));assert(count(loaded.opponents.at(123),moved)==1);
 assert(count(loaded.merchants.at(456).resale,moved|foilFlag)==1);
 // Missing sets keep ledgers/decks; unrelated expansions never inherit copies.
 wire=encodeCollection(loaded);reset();install(definition("unrelated"));assert(decodeCollection(wire,loaded));
 auto orphan=loaded.deck[0];assert(cardIndex(orphan)<0&&count(loaded.player,orphan)==2&&!validHand(loaded.deck,loaded.player));
 retainOwnedDecks(loaded);assert(loaded.deck[0]==orphan&&loaded.decks[0].cards[0]==orphan);
 assert(count(loaded.player,expansionCardID("unrelated:visitor")|foilFlag)==0);
 refreshShop(loaded.merchants.at(456),456,100,1,false);assert(count(loaded.merchants.at(456).resale,orphan)==1);
 wire=encodeCollection(loaded);reset();install(doc);assert(decodeCollection(wire,loaded));id=expansionCardID("test-set:visitor");assert(count(loaded.player,id|foilFlag)==2&&loaded.deck[0]==(id|foilFlag)&&validHand(loaded.deck,loaded.player));
 // Interrupted tables remap by the same dictionary. Missing cards cancel the
 // table, but its escrow remains recoverable even after another save/load.
 CollectionSave held;grantStarter(held);grantExpansionCard(held,"test-set:visitor",1,true);Hand player=held.deck,rival=held.deck;player[0]=id|foilFlag;
 for(auto c:rival)++held.opponents[123][c];Bank bank{held};assert(held.contract.reserve(bank,{player,rival},TradeAll,0,123));
 SavedMatch table;table.present=true;table.opponent=123;table.ledger=123;table.table=Match(indices(player),indices(rival),11,Open);table.table.foilHands[0][0]=true;
 const auto tableWire=encodeMatch(table);wire=encodeCollection(held);
 reset();install(definition("before"));install(doc);assert(decodeCollection(wire,loaded));auto remappedTable=tableWire;for(unsigned i=27;i<37;++i)remappedTable[i]=remapSavedCard(loaded,remappedTable[i]);
 SavedMatch resumed;assert(decodeMatch(remappedTable,resumed)&&matchContractValid(resumed,loaded.contract));
 reset();assert(decodeCollection(wire,loaded));remappedTable=tableWire;for(unsigned i=27;i<37;++i)remappedTable[i]=remapSavedCard(loaded,remappedTable[i]);assert(!decodeMatch(remappedTable,resumed));
 Bank refund{loaded};assert(loaded.contract.settleSelection(refund,-1,0));wire=encodeCollection(loaded);
 install(doc);assert(decodeCollection(wire,loaded));id=expansionCardID("test-set:visitor");assert(count(loaded.player,id|foilFlag)==1);
 // NPC winnings are selectable and keep finish and group mechanics.
 Stock winnings;for(auto c:held.deck)++winnings[c];winnings[id|foilFlag]=1;
 bool selected=false;for(unsigned seed=0;seed<50;++seed){auto hand=opponentHand(winnings,0,seed);selected|=std::find(hand.begin(),hand.end(),id|foilFlag)!=hand.end();assert(validHand(hand,winnings));}assert(selected);
 // Unique API rewards check player, merchant, NPC and held-credit ownership.
 reset();auto uniqueDoc=doc;uniqueDoc["cards"][0]["unique"]=true;install(uniqueDoc);id=expansionCardID("test-set:visitor");CollectionSave rewards;
 assert(!grantExpansionCard(rewards,"missing:card",1,false)&&!grantExpansionCard(rewards,"test-set:visitor",2,false));
 assert(grantExpansionCard(rewards,"test-set:visitor",1,true));assert(!grantExpansionCard(rewards,"test-set:visitor",1,false));
 rewards.player.clear();rewards.contract.credits[1][id]=1;assert(!grantExpansionCard(rewards,"test-set:visitor",1,true));
 rewards.contract={};rewards.opponents[123][id]=1;assert(!grantExpansionCard(rewards,"test-set:visitor",1,true));rewards.opponents.clear();
 rewards.merchants[456].resale[id]=1;assert(!grantExpansionCard(rewards,"test-set:visitor",1,true));
 // One invalid manifest cannot partially register a set or block a valid one.
 reset();const auto root=std::filesystem::temp_directory_path()/"tessera-expansion-tests";std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 std::ofstream(root/"valid.json")<<doc;std::ofstream(root/"invalid.json")<<duplicate;auto report=loadCardExpansions(root);assert(report.sets==1&&report.cards==1&&report.errors.size()==1);
 reset();std::ofstream(root/"conflict.json")<<doc;report=loadCardExpansions(root);assert(report.sets==0&&report.errors.size()==3);
 std::filesystem::remove_all(root);
 // Missing or omitted artwork displays a card back, keeping the card usable.
 install(doc);refreshCardArt("TesseraUI/art");assert(cardImages.size()==cards.size()&&cardImages.back().art=="backs/mosaic.webp");
 std::cout<<"Expansion manifests, acquisition, registry remapping, missing sets, escrow, NPC winnings and unique rewards verified\n";
}
