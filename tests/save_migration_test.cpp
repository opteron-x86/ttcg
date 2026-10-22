#include "Campaign.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(){
 // Development-era saves containing retired identities are deliberately unsupported.
 // Reject the entire payload without partially loading player state.
 ttcg::CollectionSave current;ttcg::grantStarter(current);
 const auto before=ttcg::encodeCollection(current);
 for(const char* path:{"tests/fixtures/collection-v20.txt","tests/fixtures/collection-v20-pending.txt"}){
  std::ifstream input(path);assert(input);std::vector<std::uint32_t> wire;
  for(std::uint32_t n;input>>n;)wire.push_back(n);
  assert(!ttcg::decodeCollection(wire,current,20));assert(ttcg::encodeCollection(current)==before);
 }
 ttcg::CollectionSave loaded;assert(ttcg::decodeCollection(before,loaded));
 assert(ttcg::encodeCollection(loaded)==before&&ttcg::validHand(loaded.deck,loaded.player));
 // Decode collections with an empty expansion dictionary.
 auto v30=before;assert(v30.back()==0);v30.pop_back();v30.resize(v30.size()-2);assert(v30.back()==0);v30.pop_back();assert(ttcg::decodeCollection(v30,loaded,30));
 auto v29=v30;assert(v29.front()==0);v29.erase(v29.begin());v29.resize(v29.size()-3);assert(ttcg::decodeCollection(v29,loaded,29));
 auto v28=v29;assert(v28.front()==0);v28.erase(v28.begin());assert(ttcg::decodeCollection(v28,loaded,28)&&ttcg::validHand(loaded.deck,loaded.player));
 // Preserve collections and courier events; initialize regional visits.
 std::ifstream old("tests/fixtures/collection-v26-courier.txt");assert(old);std::vector<std::uint32_t> wire;for(std::uint32_t n;old>>n;)wire.push_back(n);
 assert(ttcg::decodeCollection(wire,loaded,26));assert(loaded.starter&&loaded.tournaments.events.size()==2);
 assert(loaded.tournaments.events[0].invitation==1&&loaded.tournaments.events[0].announced==32);
 assert(loaded.culture.carriedHold==2&&loaded.culture.carriedRules==(ttcg::Open|ttcg::Same)&&!loaded.culture.visitHold&&loaded.culture.asked.empty());
 assert(loaded.culture.regions.at(8).abolitionAfter==300&&!loaded.culture.regions.at(8).offer.status);
 assert(ttcg::decodeCollection(ttcg::encodeCollection(loaded),current));assert(ttcg::encodeCollection(current)==ttcg::encodeCollection(loaded));
 // Restore saved progress and cooldowns from the fixed fixture.
 std::ifstream prior("tests/fixtures/collection-v31-progress.txt");assert(prior);wire.clear();for(std::uint32_t n;prior>>n;)wire.push_back(n);
 assert(ttcg::decodeCollection(wire,loaded,31));
 const auto base=0x13BBFu;
 const auto ma=std::find_if(ttcg::opponents.begin(),ttcg::opponents.end(),[](auto& p){return !p.dynamic&&p.traveller;});
 assert(ma!=ttcg::opponents.end());const int maiq=ma-ttcg::opponents.begin();
 assert(loaded.starter&&ttcg::validHand(loaded.deck,loaded.player)&&loaded.goldCredit==23);
 assert(loaded.players.at(base).known&&loaded.players.at(base).competitive.wins==3&&loaded.players.at(base).competitive.losses==2);
 assert(loaded.reputation.at(base).quarters==24&&loaded.invitations.at(base).accepted);
 assert(loaded.holds.at(1).rules==(ttcg::Open|ttcg::Same)&&loaded.holds.at(1).trade==ttcg::TradeAll);
 assert(!ttcg::encounterReady(loaded,maiq,999)&&ttcg::encounterReady(loaded,maiq,1000));
 const auto cleaned=ttcg::encodeCollection(loaded);assert(cleaned.size()+6==wire.size()+3+3*loaded.culture.regions.size());
 assert(ttcg::decodeCollection(cleaned,current)&&ttcg::encodeCollection(current)==cleaned);
 // Corrupt data in a discarded field must still reject atomically.
 const std::array<std::uint32_t,3> oldFlags{1,base,65};
 auto at=std::search(wire.begin(),wire.end(),oldFlags.begin(),oldFlags.end());assert(at!=wire.end());
 const auto index=at-wire.begin();auto corrupt=wire;corrupt[index+2]=128;
 assert(!ttcg::decodeCollection(corrupt,current,31)&&ttcg::encodeCollection(current)==cleaned);
 corrupt=wire;corrupt[index]=2;corrupt.insert(corrupt.begin()+index+3,{base,65});
 assert(!ttcg::decodeCollection(corrupt,current,31)&&ttcg::encodeCollection(current)==cleaned);
 std::cout<<"Saves: current and older formats load, retired data is discarded, M'aiq cooldown and reputation preserved, corrupt payloads rejected atomically\n";
}
