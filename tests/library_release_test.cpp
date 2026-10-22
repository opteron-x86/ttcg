#include "Development.h"
#include "WorldAccess.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;
int main(){
 assert(!cards.empty()&&!creatureGroups.empty()&&!opponents.empty());
 std::set<CardID> ids;unsigned reserved=0;CardID unique=0;
 for(const auto& c:cards){assert(ids.insert(c.form).second);if(c.unique)unique=c.form;if(!c.available){++reserved;assert(c.pending||(c.unique&&c.tier>=8&&c.tier<=10));}else if(c.tier==7)assert(c.unique||c.available);}
 assert(reserved>0&&unique);
 CollectionSave s;DevelopmentSettings dev;dev.enabled=dev.addAllCards=true;
 assert(dev.grantCards(s)&&cardCount(s.player)==cards.size());auto granted=s.player;assert(!dev.grantCards(s)&&s.player==granted);
 s.player[unique]=4;s.player[cards[0].form]=4;
 Hand valid{unique,cards[0].form,cards[0].form,cards[1].form,cards[2].form};assert(validHand(valid,s.player));
 auto bad=valid;bad[4]=unique;assert(!validHand(bad,s.player));bad=valid;bad[4]=cards[0].form;assert(!validHand(bad,s.player));
 retainOwnedDeck(bad,s.player);assert(!bad[4]);
 // Reserved cards stay outside every normal acquisition source, even at full reputation.
 for(int i=1;i<int(opponents.size());++i)recordCampaignResult(s,i,0,true,0,true);
 s.collegeMember=s.guildMember=true;
 for(int i=0;i<int(opponents.size());++i){
  for(auto id:opponents[i].collection)if(id)assert(cards[cardIndex(id)].available);
 }
 for(unsigned seed=0;seed<1000;++seed){auto pack=packCards(seed);assert(pack.size()==5);for(auto id:pack)assert(cards[cardIndex(id)].available);}
 s.deck=valid;saveActiveDeck(s);auto wire=encodeCollection(s);CollectionSave loaded;assert(decodeCollection(wire,loaded)&&encodeCollection(loaded)==wire);
 // Every consolidated group activates without colliding with another group.
 for(int g=0;g<int(creatureGroups.size());++g){
  auto it=std::find_if(cardGroups.begin(),cardGroups.end(),[g](auto mask){return mask==(1ull<<g);});assert(it!=cardGroups.end());int card=int(it-cardGroups.begin());
  for(unsigned rule:{Legion,Decimation}){Match m(9,rule);m.turn=0;m.hands[0][0]=card;m.board[0]={card,0};m.board[8]={card,1};m.placed=2;
   auto choices=m.groupChoices({0,4});assert(choices==std::vector<int>{g});auto result=m.play({0,4});assert(result.legal&&m.activeGroup==g);assert(m.groupModifier(card)==(rule==Legion?1:-1));
   auto low=std::find_if(cardGroups.begin(),cardGroups.end(),[g](auto mask){return mask==(1ull<<((g+1)%creatureGroups.size()));});if(low!=cardGroups.end())assert(m.groupModifier(int(low-cardGroups.begin()))==0);
  }
 }
 CollectionSave world;refreshWorldAccess(world,{});assert(!unlockedOpponent(world,opponentIndex(0x1E765)));refreshWorldAccess(world,{200,200,false,false,70,30,300});assert(unlockedOpponent(world,opponentIndex(0x1E765)));
 for(auto base:{0x13BADu,0x13BA9u,0x13BAFu,0xFA123456u}){assert(opponentIndex(base)==-1);assert(validHand(selectHand(childStock(base),1),childStock(base)));}
 assert(world.players.empty());
 assert(world.reputation.empty());
 for(unsigned flags:{0u,3u,4u,32u,35u,36u})for(unsigned seed=0;seed<4;++seed){Match m(indices(selectHand(s.player,seed)),indices(selectHand(s.player,seed+90)),seed,flags);while(!m.finished())assert(m.play(chooseMove(m,1000,2)).legal);assert(m.score()[0]+m.score()[1]==10);}
 std::cout<<"Library: 703 identities, pending superior rosters, all acquisition routes, unique limits, 18 distinct groups, untracked child play, world access and expanded-pool games passed\n";
}
