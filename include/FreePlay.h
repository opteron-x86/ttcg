#pragma once
#include "Campaign.h"
namespace ttcg {
inline unsigned challengeAcceptance(const CollectionSave& s,int i){
 if(!s.starter||!unlockedOpponent(s,i)||!challengeReputationMet(s,i)||!opponentHasCards(s,i))return 0;
 if(opponents[i].base==0x13BA3||opponents[i].traveller)return 100;
 constexpr unsigned chances[]={95,90,80,60,30,12};
 const auto rep=localReputation(s,opponents[i].hold);
 return std::min(98u,chances[std::min(5u,opponents[i].level)]+std::min(35u,rep.quarters/2)+(rep.famous()?35u:0u));
}
inline unsigned invitationHash(unsigned value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);}
inline bool willingToPlay(CollectionSave& s,int i,unsigned hour){
 if(!s.starter||!unlockedOpponent(s,i)||!challengeReputationMet(s,i)||!opponentHasCards(s,i)||!encounterReady(s,i,hour))return false;
 const auto& p=opponents[i];if(p.base==0x13BA3||p.traveller)return true;
 auto& invitation=s.invitations[p.base];
 if(!invitation.until||hour>=invitation.until){
  invitation.until=(hour/24+1)*24;
  // A reload before the first request produces the same daily roll too.
  invitation.accepted=invitationHash((p.dynamic?identitySeed(worldKey(p.base)):p.base)^invitationHash(hour/24)^0x54544347u)%100<challengeAcceptance(s,i);
 }
 return invitation.accepted;
}
}
