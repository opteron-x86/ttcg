#include "ExpansionLoader.h"
#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>

using namespace ttcg;
using Json=nlohmann::json;
Json manifest(std::string set="prizes"){
 return Json{{"schemaVersion",1},{"setId",set},{"name","Prize examples"},{"cards",Json::array({{
  {"id","champion"},{"name","Expansion champion"},{"tier",7},{"rarity","Legendary"},
  {"sides",{10,8,7,9}},{"groups",{"Khajiit"}},{"affinities",Json::array()},{"unique",true},
  {"acquisition",{{"tournamentPrizes",{"invitational","masters"}}}}
 }})}};
}
void install(const Json& json){auto d=parseExpansion(json.dump());registerExpansion(d.id,d.name,std::move(d.cards));}
void reset(){cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());expansionCards.clear();expansionKeys.clear();cardSets.clear();opponentPools().clear();}
bool rejects(const Json& json){try{parseExpansion(json.dump());return false;}catch(const std::exception&){return true;}}
CollectionSave world(CardID only=0){
 CollectionSave s;grantStarter(s);s.collegeMember=s.guildMember=s.erandurReady=s.valdrRescued=true;
 if(only)for(const auto& c:cards)if(c.unique&&c.form!=only)s.tournaments.trophies.insert(c.form);
 return s;
}
Tournament draw(CollectionSave& s,unsigned circuit,unsigned seed=0){Tournament e;e.circuit=circuit;std::mt19937 rng(seed);selectTournamentReward(s,e,rng);return e;}
CollectionSave announced(CardID id,unsigned circuit){
 for(unsigned seed=0;seed<100;++seed){auto s=world(id);s.tournaments.sequence=seed;
  const auto eventID=newTournament(s,1,100,true,circuit);assert(eventID);auto& e=*tournament(s,eventID);
  if(e.reward==Trophy){assert(e.trophy==id);e.invitation=3;e.starts=100;e.ends=268;e.discovered=true;return s;}
 }
 throw std::runtime_error("No trophy event found");
}
void finish(Tournament& e,bool player){
 if(player){e.bracket[0]=tournamentPlayer;e.joined=true;e.announcement=3;e.lastNode=14;}
 for(unsigned node=8;node<15;++node){e.bracket[node]=e.bracket[(node-8)*2];e.wins[node-8]={2,0};}
}
int main(){
 const auto doc=manifest();
 for(const Json& value:std::vector<Json>{nullptr,false,1,"invitational",Json::object(),{"regular"},{"Masters"},{"invitational","invitational"},{"invitational",1},{"invitational","masters","regular"}}){
  auto bad=doc;bad["cards"][0]["acquisition"]["tournamentPrizes"]=value;assert(rejects(bad));
 }
 for(auto rarity:{"Common","Rare","Epic"}){auto bad=doc;bad["cards"][0]["rarity"]=rarity;assert(rejects(bad));}
 auto bad=doc;bad["cards"][0]["unique"]=false;assert(rejects(bad));
 for(auto field:{"standardPacks","tournamentPacks","merchants"}){bad=doc;bad["cards"][0]["acquisition"][field]=std::string_view(field)=="merchants"?Json("all"):Json(true);assert(rejects(bad));}
 // Author-selected tiers are accepted in either circuit; an empty route is off.
 for(int tier=1;tier<=10;++tier){auto d=doc;d["cards"][0]["tier"]=tier;assert(parseExpansion(d.dump()).cards[0].tournamentPrizes==3);}
 auto off=doc;off["cards"][0]["acquisition"]["tournamentPrizes"]=Json::array();off["cards"][0]["unique"]=false;assert(parseExpansion(off.dump()).cards[0].tournamentPrizes==0);
 // Existing manifests must not change any base tournament draw or category.
 std::vector<std::array<unsigned,3>> baseline;
 for(unsigned circuit=0;circuit<3;++circuit)for(unsigned seed=0;seed<256;++seed){auto s=world();auto e=draw(s,circuit,seed);baseline.push_back({e.reward,e.trophy,e.quantity});}
 auto privateDoc=doc;privateDoc["cards"][0].erase("acquisition");install(privateDoc);
 unsigned at=0;for(unsigned circuit=0;circuit<3;++circuit)for(unsigned seed=0;seed<256;++seed){auto s=world();const auto e=draw(s,circuit,seed);assert((baseline[at++]==std::array<unsigned,3>{e.reward,e.trophy,e.quantity}));}
 reset();auto all=doc;
 for(auto [name,tier,circuit]:{std::tuple{"invitational",10,"invitational"},std::tuple{"masters",8,"masters"}}){auto c=all["cards"][0];c["id"]=name;c["tier"]=tier;c["acquisition"]["tournamentPrizes"]={circuit};all["cards"].push_back(c);}
 install(all);auto id=expansionCardID("prizes:champion");
 assert(tournamentTrophy(cards[cardIndex(id)],1)&&tournamentTrophy(cards[cardIndex(id)],2));
 for(unsigned circuit:{0u,3u,100u})assert(!tournamentTrophy(cards[cardIndex(id)],circuit));
 const auto invite=expansionCardID("prizes:invitational"),masters=expansionCardID("prizes:masters");
 assert(tournamentTrophy(cards[cardIndex(invite)],1)&&!tournamentTrophy(cards[cardIndex(invite)],2));
 assert(!tournamentTrophy(cards[cardIndex(masters)],1)&&tournamentTrophy(cards[cardIndex(masters)],2));
 at=0;unsigned selected=0;
 for(unsigned circuit=0;circuit<3;++circuit)for(unsigned seed=0;seed<256;++seed){auto s=world();const auto e=draw(s,circuit,seed);const auto b=baseline[at++];assert(e.reward==b[0]&&e.quantity==b[2]);selected+=e.trophy>=expansionCardBase;if(!circuit)assert(!e.trophy&&e.reward!=Trophy);}
 assert(selected>0);
 // Reservations and every owner/finish exclude the card from fresh draws.
 for(unsigned circuit:{1u,2u})for(bool foil:{false,true})for(unsigned owner=0;owner<8;++owner){
  auto s=world(id);const auto copy=id|(foil?foilFlag:0);
  switch(owner){
   case 0:s.player[copy]=1;break;case 1:s.opponents[123][copy]=1;break;
   case 2:s.merchants[456].stock[copy]=1;break;case 3:s.merchants[456].resale[copy]=1;break;
   case 4:s.contract.escrow[0][copy]=1;break;case 5:s.contract.credits[1][copy]=1;break;
   case 6:s.tournaments.trophies.insert(id);break;case 7:s.tournaments.events.emplace_back().trophy=copy;break;
  }
  const auto e=draw(s,circuit);assert(e.reward==TournamentPacks&&e.quantity==circuit+1&&!e.trophy);
  assert(!grantExpansionCard(s,"prizes:champion",1,!foil));
 }
 auto reserved=world(id);auto first=draw(reserved,1);assert(first.reward==Trophy&&first.trophy==id&&reserved.tournaments.trophies.contains(id));
 assert(draw(reserved,2).reward==TournamentPacks);assert(!grantExpansionCard(reserved,"prizes:champion",1,true));
 // Player and NPC awards use the same announced identity and pay exactly once.
 for(bool player:{false,true}){
  auto s=announced(id,1);auto& e=s.tournaments.events[0];finish(e,player);
  const auto base=e.bracket[14];const int npc=player?-1:opponentIndex(base);const auto key=player?0:opponentLedgerKey(base,opponents[npc].reference);
  if(!player){opponentPools()[base]=personalPool(npc);opponentPools()[base][id]=1;assert(!count(tournamentStock(s,base),id));}
  awardTournament(s,e);assert(e.awarded&&count(player?s.player:s.opponents.at(key),id)==1);
  if(player)assert(count(s.discovered,id)==1&&s.goldCredit==int(e.purse));
  else{bool played=false;for(unsigned seed=0;seed<256;++seed){auto h=opponentHand(s.opponents.at(key),npc,seed);played|=std::find(h.begin(),h.end(),id)!=h.end();}assert(played);}
  const auto wire=encodeCollection(s);awardTournament(s,e);assert(wire==encodeCollection(s));CollectionSave loaded;assert(decodeCollection(wire,loaded));assert(count(player?loaded.player:loaded.opponents.at(key),id)==1);
  opponentPools().clear();
 }
 // M'aiq can play an expansion trophy, including outside his usual VI–VII range.
 const int maiq=opponentIndex(0x954BF);auto s=world();Tournament e;e.reward=Trophy;e.quantity=1;e.trophy=masters;e.bracket[14]=opponents[maiq].base;
 awardTournament(s,e);const auto maiqKey=opponentLedgerKey(opponents[maiq].base,opponents[maiq].reference);assert(e.awarded);
 assert(count(playableOpponentStock(s.opponents.at(maiqKey),maiq),masters)==1);
 bool played=false;for(unsigned seed=0;seed<256;++seed){auto h=opponentHand(s.opponents.at(maiqKey),maiq,seed);played|=std::find(h.begin(),h.end(),masters)!=h.end();}assert(played);
 // An estate transfers its actual copy, retains foil and cannot duplicate stakes.
 for(unsigned held=0;held<3;++held){auto estate=world(id);estate.tournaments.trophies.insert(id);estate.deadPlayers.insert(opponents[0].base);const auto key=opponentLedgerKey(opponents[0].base,opponents[0].reference);estate.opponents[key][id|foilFlag]=1;
  if(held==1)estate.contract.escrow[1][id]=1;if(held==2)estate.contract.credits[0][id]=1;
  auto recovered=draw(estate,1);if(held)assert(recovered.reward==TournamentPacks&&count(estate.opponents[key],id|foilFlag)==1);
  else{assert(recovered.trophy==(id|foilFlag)&&count(estate.opponents[key],id|foilFlag)==0);recovered.bracket[14]=tournamentPlayer;awardTournament(estate,recovered);assert(count(estate.player,id|foilFlag)==1);}
 }
 // Saved prizes and reservations follow stable keys after registry reordering.
 s=announced(id,2);e=s.tournaments.events[0];s.tournaments.events[0].joined=true;s.tournaments.events[0].bracket[0]=tournamentPlayer;
 auto wire=encodeCollection(s);reset();install(manifest("earlier"));install(all);const auto moved=expansionCardID("prizes:champion");assert(moved!=id);
 CollectionSave loaded;assert(decodeCollection(wire,loaded));assert(loaded.tournaments.events[0].trophy==moved&&loaded.tournaments.trophies.contains(moved));
 const auto unchanged=encodeCollection(loaded);refreshTournamentRewards(loaded);assert(encodeCollection(loaded)==unchanged);
 // Removing a set after entry preserves the promised reward in the missing-card
 // ledger. Victory can finish; reinstalling restores the player's/NPC's card.
 wire=encodeCollection(loaded);reset();assert(decodeCollection(wire,loaded));const auto missing=loaded.tournaments.events[0].trophy;assert(cardIndex(missing)<0&&storedCard(loaded,missing));
 for(bool player:{false,true}){auto absent=loaded;auto& event=absent.tournaments.events[0];if(!player){event.joined=false;event.bracket[0]=e.bracket[0];}finish(event,player);awardTournament(absent,event);assert(event.awarded);const auto key=player?0:opponentLedgerKey(event.bracket[14],opponents[opponentIndex(event.bracket[14])].reference);
  auto saved=encodeCollection(absent);install(all);CollectionSave restored;assert(decodeCollection(saved,restored));id=expansionCardID("prizes:champion");assert(count(player?restored.player:restored.opponents.at(key),id)==1);assert(!grantExpansionCard(restored,"prizes:champion",1,true));reset();
 }
 // Unentered offers can be refreshed if their expansion is gone.
 loaded.tournaments.events[0].joined=false;loaded.tournaments.events[0].bracket[0]=e.bracket[0];refreshTournamentRewards(loaded);assert(loaded.tournaments.events[0].reward!=Trophy&&loaded.tournaments.events[0].trophy==0);
 // The reservation ledger supports the expansion limit, beyond the old 1,024.
 s=world();for(auto name:{"many-a","many-b","many-c"}){auto many=manifest(name);const auto card=many["cards"][0];for(unsigned n=1;n<400;++n){auto c=card;c["id"]="card-"+std::to_string(n);many["cards"].push_back(c);}install(many);}
 for(const auto& card:cards)if(card.unique)s.tournaments.trophies.insert(card.form);
 assert(s.tournaments.trophies.size()>1024&&decodeCollection(encodeCollection(s),loaded));assert(loaded.tournaments.trophies==s.tournaments.trophies);
 std::cout<<"Expansion tournament prizes: schema, opt-in circuits, unchanged odds, world uniqueness, NPC ownership, estate recovery, save/removal/reinstall and reservation capacity passed\n";
}
