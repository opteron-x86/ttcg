#include "TournamentPresentation.h"
#include <cassert>
#include <iostream>
using namespace ttcg;

unsigned champion(CollectionSave& s,unsigned reward,unsigned quantity,unsigned hour,unsigned circuit=0){
 const auto id=newTournament(s,1,hour,false,circuit);auto* e=tournament(s,id);assert(e);
 if(circuit){e->invitation=3;e->starts=hour;e->ends=hour+168;}
 e->reward=reward;e->quantity=quantity;e->trophy=0;e->joined=e->discovered=e->notified=true;e->bracket[0]=tournamentPlayer;
 for(unsigned node=8;node<15;++node){e->bracket[node]=e->bracket[(node-8)*2];e->wins[node-8][0]=2;}
 e->announcement=3;e->lastNode=14;awardTournament(s,*e);return id;
}
int main(){
 // Major Gods are Masters trophies. Lesser Gods stay pending, and neither
 // group may leak into ordinary packs or Invitational prizes.
 const auto godGroup=std::find(creatureGroups.begin(),creatureGroups.end(),"God")-creatureGroups.begin();
 unsigned readyGods=0,pendingGods=0;
 for(std::size_t i=0;i<cards.size();++i)if(cardGroups[i]&(1ull<<godGroup)){
  const auto& c=cards[i];assert(c.unique&&!c.available&&std::string_view(c.rarity)=="Legendary");
  assert(!tournamentTrophy(c,0)&&!tournamentTrophy(c,1));
  if(c.pending){++pendingGods;assert(c.tier==9&&!tournamentTrophy(c,2));continue;}
  ++readyGods;assert(c.tier==10&&tournamentTrophy(c,2));
  CollectionSave pool;for(const auto& other:cards)if(other.unique)pool.player[other.form|foilFlag]=1;
  pool.player.erase(c.form|foilFlag);
  Tournament e;e.circuit=2;std::mt19937 rng(0);assert(rng()%4==0);rng.seed(0);
  selectTournamentReward(pool,e,rng);assert(e.reward==Trophy&&e.trophy==c.form);
  // The same unique cannot be offered again after it has been reserved.
  rng.seed(0);selectTournamentReward(pool,e,rng);assert(!e.trophy&&e.reward==TournamentPacks&&e.quantity==3);
 }
 assert(readyGods==26&&pendingGods==5);
 // Every circuit uses four reward categories, with the class-specific tier,
 // quantity and finish. Fresh stock prevents reservations skewing this sample.
 std::mt19937 random(719);unsigned regularFoils=0;std::array<unsigned,4> tiers{};
 for(unsigned circuit=0;circuit<3;++circuit){
  std::array<unsigned,5> frequency{};
  for(unsigned n=0;n<5000;++n){
   CollectionSave pool;Tournament offer;offer.circuit=circuit;offer.seed=n;
   selectTournamentReward(pool,offer,random);++frequency[offer.reward];
   if(offer.reward==Trophy){assert(circuit);const auto& card=cards[cardIndex(offer.trophy)];assert(card.unique&&std::string_view(card.rarity)=="Legendary"&&!card.pending);assert(circuit==1?card.tier==8:card.tier>=9&&card.tier<=10);}
   if(offer.reward==TournamentPacks)assert(offer.quantity==circuit+1);
   if(offer.reward==StandardPacks)assert(!circuit&&offer.quantity==3);
   if(offer.reward==FoilUpgrades)assert(offer.quantity==(circuit?circuit==1?3:5:1));
   if(offer.reward==EpicCards){assert(offer.quantity==circuit+1&&!offer.trophy);const auto prize=tournamentPrizeCards(offer);assert(prize.size()==offer.quantity&&prize==tournamentPrizeCards(offer));std::set<CardID> distinct;
    for(auto id:prize){const auto& card=cards[cardIndex(id)];assert(distinct.insert(baseCardID(id)).second&&card.available&&!card.unique&&!card.pending&&std::string_view(card.rarity)=="Epic"&&card.tier>=4&&card.tier<=7);if(circuit)assert(isFoil(id));else{regularFoils+=isFoil(id);++tiers[card.tier-4];}}
   }
  }
  for(unsigned reward=0;reward<5;++reward)if(reward==(circuit?StandardPacks:Trophy))assert(!frequency[reward]);else assert(frequency[reward]>1100&&frequency[reward]<1400);
 }
 assert(regularFoils>90&&regularFoils<170);for(auto n:tiers)assert(n>230&&n<410);
 // Exhausted unique pools fall back to the correct number of tournament packs.
 for(unsigned circuit:{1u,2u})for(unsigned seed=0;seed<50;++seed){CollectionSave pool;for(const auto& c:cards)if(c.unique)pool.player[c.form]=1;Tournament e;e.circuit=circuit;std::mt19937 rng(seed);const auto category=rng()%4;rng.seed(seed);selectTournamentReward(pool,e,rng);assert(!e.trophy);if(category==0)assert(e.reward==TournamentPacks&&e.quantity==circuit+1);}
 for(unsigned circuit=0;circuit<3;++circuit){
  CollectionSave award;grantStarter(award);award.collegeMember=award.guildMember=award.erandurReady=award.valdrRescued=true;
  auto id=champion(award,EpicCards,circuit+1,100,circuit);auto* e=tournament(award,id);const auto prize=tournamentPrizeCards(*e);
  assert(e->awarded);for(auto card:prize)assert(count(award.player,card)>0&&count(award.discovered,card)>0);
  auto wire=encodeCollection(award);CollectionSave loaded;assert(decodeCollection(wire,loaded)&&tournamentPrizeCards(*tournament(loaded,id))==prize);awardTournament(award,*e);assert(wire==encodeCollection(award));
  assert(tournamentsJson(award,101,1).find("\"prizeCards\":[")!=std::string::npos);
  // A full target stack defers the entire card award, without partial copies.
  e->awarded=false;award.goldCredit=0;award.player[prize.back()]=1000000;auto before=encodeCollection(award);awardTournament(award,*e);assert(before==encodeCollection(award));
  // NPC champions receive exactly the same announced card copies and finishes.
  e->bracket[14]=e->bracket[1];auto key=opponentLedgerKey(e->bracket[1],opponents[opponentIndex(e->bracket[1])].reference);award.opponents[key]={};awardTournament(award,*e);assert(e->awarded);for(auto card:prize)assert(count(award.opponents[key],card)==1);
 }
 // Updating an old unentered development offer retains its courier and field.
 CollectionSave prior;auto offerID=newTournament(prior,1,100,true,1);auto* offer=tournament(prior,offerID);offer->reward=StandardPacks;offer->quantity=5;offer->trophy=0;const auto bracket=offer->bracket;
 refreshTournamentRewards(prior);assert(offer->id==offerID&&offer->invitation==1&&offer->bracket==bracket&&offer->reward!=StandardPacks);const auto refreshed=encodeCollection(prior);refreshTournamentRewards(prior);assert(refreshed==encodeCollection(prior));
 CollectionSave s;grantStarter(s);std::array<int,2> inventory{};
 auto packEvent=champion(s,1,2,100);assert(s.tournaments.packCredit==2);
 const auto awarded=encodeCollection(s);awardTournament(s,*tournament(s,packEvent));assert(awarded==encodeCollection(s));
 // A partial/failed engine delivery retains only the undelivered part. Once
 // items reach inventory, the award ledger cannot replace dropped/sold packs.
 deliverTournamentPacks(s,[&](unsigned kind,int amount){assert(kind==1&&amount==2);++inventory[kind];return 1;});assert(s.tournaments.packCredit==1);
 deliverTournamentPacks(s,[](unsigned,int){return 0;});assert(s.tournaments.packCredit==1);
 deliverTournamentPacks(s,[&](unsigned kind,int amount){inventory[kind]+=amount;return amount;});assert(inventory[1]==2&&!s.tournaments.packCredit);
 inventory[1]=0;deliverTournamentPacks(s,[](unsigned,int){assert(false);return 0;});
 const auto seed=s.economyRandom;const auto owned=s.player;
 auto consume=[&]{if(!inventory[1])return false;--inventory[1];return true;};
 assert(openTournamentPack(s,consume).empty()&&s.player==owned&&s.economyRandom==seed);
 inventory[1]=2;auto opened=openTournamentPack(s,consume);assert(opened.size()==5&&inventory[1]==1&&s.economyRandom!=seed&&cardCount(s.player)==cardCount(owned)+5);
 const auto first=tournamentPack(tournamentHash(s.economyRandom))[0];const auto oldCount=count(s.player,first);s.player[first]=1000000;
 const auto full=encodeCollection(s);assert(openTournamentPack(s,[]{assert(false);return true;}).empty());assert(encodeCollection(s)==full&&inventory[1]==1);s.player[first]=oldCount;
 // Foils belong to one completed event and are usable only in its outstanding
 // victory reward flow. They never accumulate in an account-wide balance.
 s.goldCredit=0;const auto foilEvent=champion(s,3,2,200);auto* e=tournament(s,foilEvent);assert(e->foilUpgrades==2);
 // Native records the award before the browser finishes the last capture.
 // The pending reward must allow playback to finish without admitting play,
 // browsing or card acquisition before the conversions are consumed.
 for(auto verb:{"opening","rule","result","settled","music","tournament","close"})assert(tournamentRewardAllowsCommand(s,verb));
 for(auto verb:{"play","start","prepare","browse","open-pack","development","deck","deck-manage"})assert(!tournamentRewardAllowsCommand(s,verb));
 auto card=s.deck[0];const auto total=cardCount(s.player);assert(!upgradeTournamentFoil(s,packEvent,card));assert(!upgradeTournamentFoil(s,foilEvent,card|foilFlag));
 assert(upgradeTournamentFoil(s,foilEvent,card));assert(cardCount(s.player)==total&&e->foilUpgrades==1&&validHand(s.deck,s.player));
 assert(openTournamentPack(s,[]{assert(false);return true;}).empty());assert(inventory[1]==1);
 // The native load handler explicitly forfeits pending rewards. A saved event
 // still round-trips before that policy is applied; malformed credits fail.
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(tournament(loaded,foilEvent)->foilUpgrades==1);
 forfeitTournamentFoils(loaded);assert(!pendingTournamentFoils(loaded));assert(!tournament(loaded,foilEvent)->announcement);
 assert(!upgradeTournamentFoil(loaded,foilEvent,s.deck[1]));
 auto corrupt=s;tournament(corrupt,foilEvent)->foilUpgrades=3;assert(!decodeCollection(encodeCollection(corrupt),loaded));
 forfeitTournamentFoils(s);assert(!e->foilUpgrades&&!e->announcement);auto future=cards.back().form;++s.player[future];assert(!upgradeTournamentFoil(s,foilEvent,future));
 awardTournament(s,*e);assert(!e->foilUpgrades);assert(openTournamentPack(s,consume).size()==5&&inventory[1]==0);
 s.goldCredit=0;const auto all=champion(s,3,2,300);
 for(unsigned n=0;n<2;++n){auto it=std::find_if(s.player.begin(),s.player.end(),[](auto p){return p.second>0&&!isFoil(p.first);});assert(it!=s.player.end());assert(upgradeTournamentFoil(s,all,it->first));}
 assert(!tournament(s,all)->foilUpgrades&&!pendingTournamentFoils(s));assert(!upgradeTournamentFoil(s,all,future));
 for(auto verb:{"play","start","prepare","browse","open-pack","development","deck","deck-manage"})assert(tournamentRewardAllowsCommand(s,verb));
 // An award deferred by the gold cap must not resurrect forfeited conversions.
 auto deferred=s;const auto delayed=newTournament(deferred,2,400);auto* d=tournament(deferred,delayed);
 d->reward=3;d->quantity=2;d->joined=d->discovered=d->notified=true;d->bracket[0]=tournamentPlayer;
 for(unsigned node=8;node<15;++node){d->bracket[node]=d->bracket[(node-8)*2];d->wins[node-8][0]=2;}d->announcement=3;deferred.goldCredit=1000000;
 awardTournament(deferred,*d);assert(!d->awarded);forfeitTournamentFoils(deferred);deferred.goldCredit=0;awardTournament(deferred,*d);assert(d->awarded&&!d->foilUpgrades);
 std::cout<<"Tournament rewards: physical pack delivery/consumption, failure atomicity, event-bound foil conversion, deck repair, expiry, reload and deferred-award forfeiture passed\n";
}
