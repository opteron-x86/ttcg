#include "reputation_support.h"
#include "WorldAccess.h"
#include "Development.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace ttcg;CollectionSave s;
 CollectionSave discovery;const int contact=opponentIndex(0x13BAA),merchant=opponentIndex(0x13BAB);
 willingToPlay(discovery,contact,3);assert(!progress(discovery,contact).known);
 replenishOpponent(discovery,contact,opponents[contact].reference,3);assert(!progress(discovery,contact).known);
 grantStarter(discovery);assert(!progress(discovery,0).known);
 acceptOpponentChallenge(discovery,contact);assert(progress(discovery,contact).known);

 auto master=opponentIndex(0x13BBF),expert=opponentIndex(0x1A696);
 assert(master>=0&&expert>=0);assert(unlockedOpponent(s,master));assert(unlockedOpponent(s,expert));assert(!s.starter);
 assert(challengeAcceptance(s,0)==0&&challengeAcceptance(s,master)==0);
 assert(!willingToPlay(s,contact,3));
 grantStarter(s);assert(challengeAcceptance(s,0)==100);
 auto result=willingToPlay(s,master,3);auto saved=encodeCollection(s);CollectionSave restored;assert(decodeCollection(saved,restored));
 for(unsigned hour=3;hour<24;++hour)assert(willingToPlay(restored,master,hour)==result);
 CollectionSave before;assert(willingToPlay(before,master,3)==result);
 earnReputation(s,1);
 assert(localReputation(s,1).famous());assert(!localReputation(s,2).famous());assert(challengeAcceptance(s,master)>challengeAcceptance(before,master));
 // Once qualified, daily willingness is persistent and reload-stable.
 result=willingToPlay(s,master,3);assert(decodeCollection(encodeCollection(s),restored));
 for(unsigned hour=3;hour<24;++hour)assert(willingToPlay(restored,master,hour)==result);
 CollectionSave again;assert(decodeCollection(encodeCollection(s),again));
 auto complete=encodeCollection(s);const auto unchanged=encodeCollection(again);
 for(std::size_t n=0;n<complete.size();++n){auto cut=complete;cut.resize(n);assert(!decodeCollection(cut,again)&&encodeCollection(again)==unchanged);}
 CollectionSave early;recordCampaignResult(early,expert,0,false,0);assert(!early.starter&&localReputation(early,1).wins==0);
 recordCampaignResult(early,expert,0,false,0);assert(localReputation(early,1).wins==0&&early.players.empty());
 again.accessBlocked.insert(opponents[master].base);assert(!unlockedOpponent(again,master)&&!willingToPlay(again,master,25));
 // Legacy membership bits cannot lock built-in opponents, including after
 // loading an older save; actual world/quest blocks still apply.
 DevelopmentSettings dev;dev.enabled=true;dev.unlockAllPlayers=true;
 s.collegeMember=s.guildMember=false;refreshWorldAccess(s,{});
 for(unsigned base:{0x1C193u,0x13B81u,0x19959u}){const auto i=opponentIndex(base);assert(i>=0&&unlockedOpponent(s,i)&&dev.unlockedPlayer(s,i));}
 for(int i=0;i<int(opponents.size());++i)if(opponents[i].hold==7)assert(!s.accessBlocked.contains(opponents[i].base));
 assert(decodeCollection(encodeCollection(s),restored));assert(!restored.collegeMember&&!restored.guildMember);
 assert(unlockedOpponent(restored,opponentIndex(0x1C193)));
 restored.accessBlocked.insert(0x1C193);assert(!unlockedOpponent(restored,opponentIndex(0x1C193))&&!dev.unlockedPlayer(restored,opponentIndex(0x1C193)));
 s.brandSheiJailed=true;assert(!unlockedOpponent(s,opponentIndex(0x1334F)));
 std::cout<<"Free play: Hulda album gate, local distinct-opponent reputation, daily decisions, reload persistence, Famous Master challenges and vanilla access restrictions passed\n";
}
