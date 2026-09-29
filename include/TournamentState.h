#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <map>
#include <vector>
namespace ttcg {
inline constexpr std::uint32_t tournamentPlayer=0x14;
enum TournamentReward : unsigned { Trophy=0,TournamentPacks=1,StandardPacks=2,FoilUpgrades=3,EpicCards=4 };
struct Tournament {
 unsigned id=0,hold=0,announced=0,starts=0,ends=0,rules=0,fee=0,purse=0;
 unsigned reward=0,trophy=0,quantity=0,seed=0,matchNode=0,replays=0;
 bool joined=false,eliminated=false,awarded=false,development=false;
 bool discovered=false,notified=false;
 // Eight entrants, four quarterfinal winners, two semifinal winners, champion.
 std::array<std::uint32_t,15> bracket{};
 // Invitation: 0 not required/ineligible, 1 pending, 2 with courier, 3 delivered.
 // Eligible courier events have no playable dates until delivery.
 unsigned circuit=0,invitation=0,announcement=0,lastNode=0,foilUpgrades=0;
 std::array<std::array<unsigned,2>,7> wins{};
 // Played victories recorded with RecordResults enabled, by player pairing.
 // A default advance or an unrecorded game cannot complete an earned pairing.
 std::array<unsigned,3> reputationWins{};
};
struct TournamentState {
 std::array<unsigned,10> nextRegular{};
 std::array<unsigned,2> nextCircuit{},circuitRotation{};
 // Retain the retired one-shot test token in format 26; new switches use the
 // actual unfinished events rather than a token that can strand a test.
 unsigned sequence=0,developmentToken=0;
 unsigned packCredit=0,standardPackCredit=0;
 std::map<unsigned,unsigned> npcGold;
 std::set<std::uint32_t> trophies;
 std::vector<Tournament> events;
};
inline void encodeTournaments(std::vector<std::uint32_t>& out,const TournamentState& s){
 for(const auto* dates:{&s.nextCircuit,&s.circuitRotation})out.insert(out.end(),dates->begin(),dates->end());
 out.insert(out.end(),s.nextRegular.begin(),s.nextRegular.end());
 for(auto v:{s.sequence,s.developmentToken,s.packCredit,s.standardPackCredit})out.push_back(v);
 out.push_back(s.npcGold.size());for(auto [id,n]:s.npcGold){out.push_back(id);out.push_back(n);}
 out.push_back(s.trophies.size());for(auto id:s.trophies)out.push_back(id);
 out.push_back(s.events.size());
 for(const auto& e:s.events){
  for(auto v:{e.id,e.hold,e.announced,e.starts,e.ends,e.rules,e.fee,e.purse,e.reward,e.trophy,e.quantity,e.seed,e.matchNode,e.replays})out.push_back(v);
  out.push_back(unsigned(e.joined)|(unsigned(e.eliminated)<<1)|(unsigned(e.awarded)<<2)|(unsigned(e.development)<<3)|(unsigned(e.discovered)<<4)|(unsigned(e.notified)<<5));
  out.insert(out.end(),e.bracket.begin(),e.bracket.end());
  for(auto v:{e.circuit,e.invitation,e.announcement,e.lastNode,e.foilUpgrades})out.push_back(v);
  for(const auto& pair:e.wins)for(auto v:pair)out.push_back(v);
  out.insert(out.end(),e.reputationWins.begin(),e.reputationWins.end());
 }
}
template<class Get,class ValidCard,class ValidRules> bool decodeTournaments(Get get,TournamentState& s,ValidCard card,ValidRules rules,unsigned version=27){
 if(version!=26&&version!=27&&version!=28)return false;
 for(auto* dates:{&s.nextCircuit,&s.circuitRotation})for(auto& v:*dates){v=get();if(v>100000000)return false;}
 for(auto& v:s.nextRegular){v=get();if(v>100000000)return false;}
 s.sequence=get();s.developmentToken=get();s.packCredit=get();s.standardPackCredit=get();
 if(s.sequence>1000000||s.packCredit>1000000||s.standardPackCredit>1000000)return false;
 auto n=get();if(n>1024)return false;for(unsigned i=0;i<n;++i){auto id=get(),amount=get();if(!id||amount>1000000||s.npcGold.contains(id))return false;s.npcGold[id]=amount;}
 n=get();if(n>1024)return false;for(unsigned i=0;i<n;++i){auto id=get();if(!card(id)||!s.trophies.insert(id).second)return false;}
 n=get();if(n>40)return false;std::set<unsigned> ids,reserved;unsigned entered=0;
 for(unsigned i=0;i<n;++i){
  Tournament e;e.id=get();e.hold=get();e.announced=get();e.starts=get();e.ends=get();e.rules=get();e.fee=get();e.purse=get();e.reward=get();e.trophy=get();e.quantity=get();e.seed=get();e.matchNode=get();e.replays=get();auto flags=get();
  e.joined=flags&1;e.eliminated=flags&2;e.awarded=flags&4;e.development=flags&8;e.discovered=flags&16;e.notified=flags&32;
  if(!e.id||!ids.insert(e.id).second||e.id>s.sequence||!e.hold||e.hold>10||e.ends>100000000||!rules(e.rules)||e.fee>10000||e.purse>100000||e.reward>EpicCards||(e.trophy&&!card(e.trophy))||(e.reward==Trophy&&!e.trophy)||(e.reward==EpicCards&&(e.trophy||!e.quantity||e.quantity>3))||e.quantity>10||flags>63||(e.matchNode&&e.matchNode!=8&&e.matchNode!=12&&e.matchNode!=14)||e.replays>1000000)return false;
  std::set<unsigned> entrants;
  for(unsigned j=0;j<15;++j){auto v=get();e.bracket[j]=v;if(j<8&&(!v||!entrants.insert(v).second))return false;if(j>=8&&v){const unsigned a=(j-8)*2;if(v!=e.bracket[a]&&v!=e.bracket[a+1])return false;}}
  if((e.joined!=(e.bracket[0]==tournamentPlayer))||(e.awarded&&!e.bracket[14])||(e.matchNode&&(e.bracket[e.matchNode]||!e.joined||e.eliminated)))return false;
  e.circuit=get();e.invitation=get();e.announcement=get();e.lastNode=get();e.foilUpgrades=get();
  if(e.foilUpgrades>e.quantity||(e.foilUpgrades&&(!e.awarded||e.reward!=3||e.bracket[14]!=tournamentPlayer||e.announcement!=3)))return false;
  if(e.circuit>2||e.invitation>3||(!e.circuit&&e.invitation)||e.announcement>4||(e.lastNode&&e.lastNode!=8&&e.lastNode!=12&&e.lastNode!=14))return false;
  const bool awaiting=e.circuit&&(e.invitation==1||e.invitation==2);
  if(awaiting){if(e.starts||e.ends||e.joined||e.discovered||e.notified||e.bracket[14])return false;}
  else if(e.starts<e.announced||e.ends!=e.starts+(e.circuit?7u:3u)*24)return false;
  if((e.joined&&!e.discovered)||(e.notified&&!e.discovered)||(e.circuit&&e.discovered&&e.invitation!=3))return false;
  if(e.joined&&!e.eliminated&&!e.bracket[14]&&++entered>1)return false;
  for(unsigned pair=0;pair<7;++pair){auto& w=e.wins[pair];w[0]=get();w[1]=get();if(w[0]>2||w[1]>2||(w[0]==2&&w[1]==2))return false;
   if(e.bracket[pair+8]&&w[e.bracket[pair+8]==e.bracket[pair*2]?0:1]!=2)return false;
   if(!e.bracket[pair+8]&&(w[0]==2||w[1]==2))return false;
  }
  constexpr std::array<unsigned,3> playerPairs{0,4,6};
  for(unsigned stage=0;stage<3;++stage){auto& wins=e.reputationWins[stage];wins=get();if(wins>e.wins[playerPairs[stage]][0]||(wins&&!e.joined))return false;}
  if(!e.awarded)for(unsigned j=0;j<8;++j)if(e.bracket[j]!=tournamentPlayer&&!reserved.insert(e.bracket[j]).second)return false;
  s.events.push_back(e);
 }
 return true;
}
}
