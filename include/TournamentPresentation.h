#pragma once
#include "Tournaments.h"
#include "Presentation.h"
#include "GameDate.h"
namespace ttcg {
inline bool tournamentRewardAllowsCommand(const CollectionSave& s,std::string_view command){
 // The final win awards foil choices before its capture animation finishes.
 // Keep presentation acknowledgements available so that win can settle and
 // open its reward screen; card acquisition and other play remain blocked.
 return !pendingTournamentFoils(s)||command=="tournament"||command=="close"||
  command=="opening"||command=="rule"||command=="result"||command=="settled"||command=="music";
}
inline std::string tournamentPrize(const Tournament& e){
 if(e.reward==0){auto i=cardIndex(e.trophy);return i>=0?cards[i].name:"Trophy card";}
 if(e.reward==EpicCards)return e.circuit?std::to_string(e.quantity)+" Epic foils":"Epic card";
 if(e.reward==1)return std::to_string(e.quantity)+(e.quantity==1?" tournament pack":" tournament packs");
 if(e.reward==2)return std::to_string(e.quantity)+" standard packs";
 return std::to_string(e.quantity)+(e.quantity==1?" foil upgrade":" foil upgrades");
}
inline std::string tournamentsJson(const CollectionSave& s,unsigned hour,unsigned venue,unsigned registration=0,GameDate date={},std::string_view player="You",unsigned currentHold=0){
 std::string out="{\"hour\":"+std::to_string(hour)+",\"venue\":"+std::to_string(venue)+",\"currentHold\":"+std::to_string(currentHold)+",\"events\":[";
 bool first=true;for(auto it=s.tournaments.events.rbegin();it!=s.tournaments.events.rend();++it){const auto& e=*it;if(!tournamentVisible(e,hour))continue;if(!first)out+=',';first=false;
 const auto node=tournamentNode(e);const bool atVenue=venue==e.hold;
 out+=std::format("{{\"id\":{},\"hold\":{},\"name\":{},\"venue\":{},\"host\":{},\"announced\":{},\"starts\":{},\"ends\":{},\"rules\":{},\"fee\":{},\"purse\":{},\"reward\":{},\"prize\":{},\"trophy\":{},\"joined\":{},\"eliminated\":{},\"champion\":{},\"atVenue\":{},\"round\":{},\"development\":{}",e.id,e.hold,quote(holdName(e.hold)),quote(tournamentVenues[e.hold-1].name),quote(tournamentVenues[e.hold-1].host),e.announced,e.starts,e.ends,e.rules,e.fee,e.purse,e.reward,quote(tournamentPrize(e)),displayIndex(e.trophy),e.joined?"true":"false",e.eliminated?"true":"false",e.bracket[14],atVenue?"true":"false",node==8?1:node==12?2:node==14?3:0,e.development?"true":"false");
 out+=",\"foilUpgrades\":"+std::to_string(e.foilUpgrades);
 out+=",\"prizeCards\":[";bool firstCard=true;for(auto id:tournamentPrizeCards(e)){if(!firstCard)out+=',';firstCard=false;out+=std::to_string(displayIndex(id));}out+=']';
 out+=std::format(",\"unseen\":{},\"circuit\":{},\"circuitName\":{},\"canEnter\":{},\"announcement\":{},\"lastNode\":{},\"startsLabel\":{},\"endsLabel\":{},\"wins\":[",!e.notified&&tournamentOpen(e,hour)?"true":"false",e.circuit,quote(tournamentCircuitName(e.circuit)),atVenue&&registration==e.hold&&tournamentEntryAvailable(e,hour)&&!tournamentEntryActive(s)?"true":"false",e.announcement,e.lastNode,quote(gameDateText(shiftGameDate(date,int(e.starts)-int(hour)))),quote(gameDateText(shiftGameDate(date,int(e.ends)-int(hour)))));
 for(unsigned n=0;n<7;++n){if(n)out+=',';out+=std::format("[{},{}]",e.wins[n][0],e.wins[n][1]);}out+="],\"bracket\":[";
 const unsigned visible=e.bracket[14]?15:node?node:8;
 for(unsigned n=0;n<15;++n){
  if(n)out+=',';
  // The first NPC is the reserve if the invitation is declined. Show the
  // player's available place until the event closes, without entering them.
  const bool reserved=n==0&&!e.joined&&!e.bracket[14]&&tournamentEntryAvailable(e,hour);
  const auto base=!reserved&&n<visible?e.bracket[n]:0;const auto i=opponentIndex(base);
  out+="{\"id\":"+std::to_string(base)+",\"name\":"+quote(base==tournamentPlayer?std::string(player):i>=0?opponents[i].name:"")+",\"reserved\":"+(reserved?"true":"false")+"}";
 }out+="]}";
 }return out+std::format("],\"activeEntry\":{},\"notification\":{}}}",tournamentEntryActive(s)?"true":"false",tournamentNotice(s,currentHold,hour));
}
}
