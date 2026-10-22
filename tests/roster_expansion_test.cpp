#include "FreePlay.h"
#include "Tournaments.h"
#include "RuleCulture.h"
#include "WorldAccess.h"
#include "WorldIdentity.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace ttcg;
int main(){
 assert(opponents.size()==320);
 const unsigned masters[]={3,2,3,2,2,3,2,3,3,3};
 std::set<CardID> signatures;
 for(const auto& p:opponents)if(p.level==5){
  assert(p.signature&&count(personalPool(opponentIndex(p.base)),p.signature)==1);
  assert(signatures.insert(p.signature).second);
 }
 CollectionSave baseline;grantStarter(baseline);refreshWorldAccess(baseline,{});
 for(int i=0;i<int(opponents.size());++i)if(std::string_view(opponents[i].master)=="HearthFires.esm")progress(baseline,i).unavailable=true;
 for(unsigned hold=1;hold<=10;++hold){
  std::vector<int> experts;unsigned countMasters=0;
  for(int i=0;i<int(opponents.size());++i)if(opponents[i].hold==hold){countMasters+=opponents[i].level==5;if(opponents[i].level==4&&unlockedOpponent(baseline,i))experts.push_back(i);}
  assert(countMasters==masters[hold-1]);assert(experts.size()>=3);
  // Losing any one accessible Expert must not strand a player below Famous.
  // No College/Guild membership, rescued NPCs or Master victories are used.
  for(int absent:experts){
   auto s=baseline;progress(s,absent).unavailable=true;s.deadPlayers.insert(opponents[absent].base);
   for(unsigned turn=0;turn<300&&localReputation(s,hold).rank<Famous;++turn){
    int best=-1;unsigned score=0;
    for(int i=0;i<int(opponents.size());++i){const auto& p=opponents[i];if(p.hold!=hold||p.level==5||!unlockedOpponent(s,i)||!challengeReputationMet(s,i))continue;
     const auto award=reputationAward(s,i);if(!award)continue;const auto value=award+(!reputationWins(s,p.base)?1000:0);
     if(value>score){score=value;best=i;}
    }
    assert(best>=0);recordCampaignResult(s,best,0,true,Open,true);
   }
   if(localReputation(s,hold).rank<Famous){const auto r=localReputation(s,hold);std::cerr<<holdName(hold)<<" without "<<opponents[absent].name<<": rank="<<r.rank<<" people="<<r.players<<" strong="<<r.strongWins<<" experts="<<r.expertWins<<" points="<<r.quarters/4<<"\n";}
   assert(localReputation(s,hold).rank>=Famous);
  }
 }
 const int maiq=opponentIndex(0x954BF);const auto& p=opponents[maiq];
 assert(p.hold==0&&p.traveller&&p.level==5);
 CollectionSave s;grantStarter(s);assert(challengeReputationMet(s,maiq)&&willingToPlay(s,maiq,1));
 assert(!cultureOpponent(s,maiq)&&tournamentEntrant(s,maiq));
 const auto& signature=cards[cardIndex(p.signature)];
 assert(signature.tier==9&&signature.unique&&signature.affinityMask==0&&std::string_view(signature.groups)=="[\"Khajiit\"]");
 assert(std::string_view(signature.rarity)=="Legendary"&&signature.available&&!tournamentTrophy(signature,2));
 auto first=opponentTerms(s,maiq);assert(first.trade==TradeOne);
 CollectionSave loaded;assert(decodeCollection(encodeCollection(s),loaded));assert(opponentTerms(loaded,maiq).rules==first.rules);
 std::set<unsigned> seen;unsigned mid=0;
 for(unsigned n=0;n<10000;++n){progress(s,maiq).competitive.wins=n;auto terms=opponentTerms(s,maiq);assert(validRules(terms.rules)&&!(terms.rules&Reverse));seen.insert(terms.rules);mid+=std::popcount(terms.rules)==2||std::popcount(terms.rules)==3;}
 assert(seen.size()>100&&mid>6500&&mid<7500);

 for(unsigned circuit=0;circuit<3;++circuit){
  unsigned guests=0,eligible=0;
  for(unsigned sample=0;sample<1000;++sample){
   CollectionSave events;grantStarter(events);events.tournaments.sequence=sample*7;
   auto id=newTournament(events,1+sample%10,100,true,circuit);assert(id);
   auto* event=tournament(events,id);const bool appears=std::find(event->bracket.begin(),event->bracket.begin()+8,p.base)!=event->bracket.begin()+8;
   eligible+=!(event->rules&Reverse);guests+=appears;
   if(appears){assert(!(event->rules&Reverse)&&event->bracket[0]!=p.base);assert(tournamentReserved(events,p.base));
    auto id2=newTournament(events,1+(sample+1)%10,100,true,circuit);if(id2){const auto* other=tournament(events,id2);assert(std::find(other->bracket.begin(),other->bracket.begin()+8,p.base)==other->bracket.begin()+8);}
   }
  }
  assert(guests>eligible/40&&guests<eligible/12);
 }
 {
  CollectionSave prizes;grantStarter(prizes);Tournament event;event.reward=Trophy;event.quantity=1;event.circuit=2;event.bracket[14]=p.base;
  for(const auto& c:cards)if(tournamentTrophy(c,2)){event.trophy=c.form;break;}
  assert(event.trophy);awardTournament(prizes,event);assert(event.awarded);
  const auto key=opponentLedgerKey(p.base,p.reference);assert(count(prizes.opponents.at(key),event.trophy)==1);
  CollectionSave restored;assert(decodeCollection(encodeCollection(prizes),restored));
  bool trophyDrawn=false;
  for(unsigned seed=0;seed<256;++seed){const auto hand=opponentHand(restored.opponents.at(key),maiq,seed);trophyDrawn|=std::find(hand.begin(),hand.end(),event.trophy)!=hand.end();}
  assert(trophyDrawn); // His real tournament prize survives saving and can be staked later.
 }
 s={};grantStarter(s);replenishOpponent(s,maiq,p.reference,1);auto& stock=s.opponents[p.reference];
 const CardID copy=count(stock,p.signature)?p.signature:p.signature|foilFlag;
 assert(count(stock,copy)==1);
 for(auto [id,n]:stock)assert(!n||cards[cardIndex(id)].tier==6||cards[cardIndex(id)].tier==7||baseCardID(id)==p.signature);
 // Other ordinary tiers and other signature uniques stay out; tournament trophies may enter.
 for(const auto& c:cards)if(c.tier<6||c.tier>7)if(c.form!=p.signature)stock[c.form]=2;
 bool drawn=false,absent=false;
 for(unsigned seed=0;seed<256;++seed){const auto h=opponentHand(stock,maiq,seed);assert(validHand(h,stock));bool found=false;for(auto id:h){found|=baseCardID(id)==p.signature;const int tier=cards[cardIndex(id)].tier;assert(tier==6||tier==7||baseCardID(id)==p.signature||(cards[cardIndex(id)].unique&&!cards[cardIndex(id)].available&&!cards[cardIndex(id)].pending));}drawn|=found;absent|=!found;}
 assert(drawn&&absent);
 stock[copy]=0;s.player[copy]=1;recordCampaignResult(s,maiq,0,true,first.rules,true);finishTravellerEncounter(s,maiq,10);
 assert(widerRenown(s)==32);for(unsigned hold=1;hold<=10;++hold)assert(localReputation(s,hold).wins==0);
 assert(!willingToPlay(s,maiq,177)&&willingToPlay(s,maiq,178));
 assert(decodeCollection(encodeCollection(s),loaded));assert(!encounterReady(loaded,maiq,177)&&encounterReady(loaded,maiq,178));
 for(auto& [id,n]:loaded.opponents[p.reference])if(cards[cardIndex(id)].tier==6||cards[cardIndex(id)].tier==7)n=0;
 replenishOpponent(loaded,maiq,p.reference,178);assert(opponentHasCards(loaded,maiq));
 assert(!count(loaded.opponents[p.reference],p.signature)&&!count(loaded.opponents[p.reference],p.signature|foilFlag));
 recordCampaignResult(loaded,maiq,0,true,0,true);assert(widerRenown(loaded)==32); // First win only.
 assert(authoredIdentity("HearthFires.esm",0x5215,0x03005215)==0xFB005215);
 assert(authoredIdentity("HearthFires.esm",0x5215,0x06005215)==0xFB005215);
 assert(std::string_view(identityMaster(0xFB005215))=="HearthFires.esm");
 assert(authoredIdentity("Dragonborn.esm",0x18290,0x04018290)==0xDB018290);
 std::cout<<"Roster: 26 local Masters, death-resilient Expert progression in all holds, travelling M'aiq rules/stock/unique/cooldown/renown and DLC identities passed\n";
}
