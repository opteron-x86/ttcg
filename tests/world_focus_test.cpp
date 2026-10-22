#include "WorldFocus.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Host {
 bool owns=false,paused=false,deny=false,releaseQueued=false;
 int pauseCount=2; // Independent game menus already own two pause requests.
 std::vector<std::string> calls;
 bool HasFocus(unsigned) const {return owns;}
 bool Focus(unsigned,bool pause) {
   calls.push_back(pause?"pause":"unpause");
   if(deny)return false;
   if(owns)return true; // An existing focus does not change its pause mode.
   owns=true;paused=pause;if(paused)++pauseCount;return true;
 }
 void Unfocus(unsigned) {
   calls.push_back("release");
   releaseQueued=true;
 }
 void flushUI() {
   if(!releaseQueued)return;
   if(owns&&paused)--pauseCount;
   owns=false;paused=false;releaseQueued=false;
 }
};

int main() {
 using ttcg::shouldPauseWorld;
 const ttcg::WorldPauseSettings defaults;
 assert(defaults.album&&defaults.shops&&defaults.setup&&defaults.matches);
 for(bool album:{false,true})for(bool shops:{false,true})for(bool setup:{false,true})for(bool matches:{false,true}){
   const ttcg::WorldPauseSettings settings{album,shops,setup,matches};
   assert(shouldPauseWorld(settings,"album",false)==album);
   assert(shouldPauseWorld(settings,"shop",false)==shops);
   assert(shouldPauseWorld(settings,"lobby",false)==setup);
   assert(shouldPauseWorld(settings,"lesson",false));
   assert(shouldPauseWorld(settings,"unknown",false));
   for(const auto* screen:{"album","lobby","shop","lesson"})assert(shouldPauseWorld(settings,screen,true)==matches);
 }
 Host host;ttcg::WorldFocus focus;
 using enum ttcg::FocusUpdate;
 assert(!focus.unpaused());
 assert(focus.apply(host,1u,true)==Ready&&host.pauseCount==3);
 assert(focus.apply(host,1u,true)==Ready&&host.calls.size()==1); // Stable publications do not refocus.
 assert(focus.apply(host,1u,false)==Releasing&&focus.switching()&&host.pauseCount==3);
 assert((host.calls==std::vector<std::string>{"pause","release"}));
 assert(!focus.complete(host,1u)&&focus.switching()); // Cannot reclaim before the queued release.
 assert(focus.apply(host,1u,false)==Waiting&&host.calls.size()==2);
 host.flushUI();assert(!host.HasFocus(1u)&&focus.switching()); // Ignore this expected menu close.
 assert(focus.complete(host,1u)&&focus.unpaused()&&!focus.switching()&&host.pauseCount==2);
 assert((host.calls==std::vector<std::string>{"pause","release","unpause"}));
 assert(focus.apply(host,1u,false)==Ready&&host.calls.size()==3);
 // Setup, album (including deck editing), shops and matches follow their own
 // preference on every transition, without retaining another screen's pause.
 ttcg::WorldPauseSettings settings{false,true,true,false};
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"lobby",false))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&host.pauseCount==3);
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"album",false))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&focus.unpaused()&&host.pauseCount==2);
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"lobby",false))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&host.pauseCount==3);
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"lobby",true))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&focus.unpaused()&&host.pauseCount==2);
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"shop",false))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&host.pauseCount==3);
 settings.shops=false;assert(focus.apply(host,1u,shouldPauseWorld(settings,"shop",false))==Releasing);
 host.flushUI();assert(focus.complete(host,1u)&&focus.unpaused()&&host.pauseCount==2);
 auto count=host.calls.size();
 assert(focus.apply(host,1u,shouldPauseWorld(settings,"album",false))==Ready&&host.calls.size()==count);
 host.Unfocus(1u);host.flushUI();count=host.calls.size();
 assert(focus.apply(host,1u,true)==Lost&&host.calls.size()==count); // Never steal lost focus.
 focus.reset();assert(!focus.unpaused());
 assert(focus.apply(host,1u,true)==Ready&&host.pauseCount==3);
 assert(focus.apply(host,1u,false)==Releasing);host.flushUI();
 host.deny=true;assert(!focus.complete(host,1u)&&!focus.unpaused()&&!focus.switching()&&!host.owns&&host.pauseCount==2);
 focus.reset();host.deny=false;
 assert(focus.apply(host,1u,false)==Ready&&focus.unpaused());
 assert(focus.apply(host,1u,true)==Releasing); // Closing/loading during a pending switch cancels it.
 focus.reset();host.flushUI();count=host.calls.size();
 assert(!focus.complete(host,1u)&&host.calls.size()==count&&host.pauseCount==2&&!focus.unpaused());
 std::cout<<"World focus: all 16 pause configurations, album/setup/shop/match transitions, immediate switches, pause ownership, focus loss and failure passed\n";
}
