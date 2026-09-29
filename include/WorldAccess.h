#pragma once
#include "Campaign.h"
namespace ttcg {
struct WorldAccess {unsigned dibella=0,derkeethus=0;bool wuunferthJailed=false,gulumQuest=false;unsigned windStone=0,baldor=0,talvas=0;bool orcWelcome=false,laidToRest=false,mineReopened=false;};
inline void refreshWorldAccess(CollectionSave& s,const WorldAccess& a){
 s.accessBlocked.clear();auto block=[&](std::uint32_t base,bool blocked){if(blocked)s.accessBlocked.insert(base);};
 block(0x1E765,a.dibella<200);block(0x1403E,a.derkeethus<200);block(0x14146,a.wuunferthJailed);block(0x13284,a.gulumQuest);
 for(auto base:{0xDB018FA9u,0xDB018FA5u,0xDB018FB5u,0xDB018FAEu,0xDB018FC5u,0xDB017934u})block(base,a.windStone<70);
 block(0xDB018FB5,a.baldor<30);block(0xDB017777,a.talvas<300);
 for(auto base:{0x13B81u,0x19959u})block(base,!a.orcWelcome);
 block(0x135F0,!a.laidToRest);
 for(auto base:{0xDB018F15u,0xDB018F14u})block(base,!a.mineReopened);
 for(auto base:{0xDB018FBDu,0xDB018F99u})block(base,a.windStone<70);
}
}
