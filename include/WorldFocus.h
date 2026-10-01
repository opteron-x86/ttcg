#pragma once
#include <optional>
#include <string_view>

namespace ttcg {
struct WorldPauseSettings {
 bool album=true,shops=true,setup=true,matches=true;
};
inline bool shouldPauseWorld(const WorldPauseSettings& settings,std::string_view screen,bool active) {
 if(active)return settings.matches;
 if(screen=="album")return settings.album;
 if(screen=="shop")return settings.shops;
 if(screen=="lobby")return settings.setup;
 return true;
}

// Meridian's AlreadyFocused result does not change the existing pause mode.
// Unfocus queues a UI task. Reclaim only after that task has run; an immediate
// TryFocus would keep the old mode, then lose focus when the release arrives.
enum class FocusUpdate { Ready, Releasing, Waiting, Lost };
class WorldFocus {
 std::optional<bool> paused;
 std::optional<bool> requested;
public:
 template<class Host,class View> FocusUpdate apply(Host& host,View view,bool pause) {
   if(requested){requested=pause;return FocusUpdate::Waiting;}
   if(paused) {
     // Do not steal focus back if another interface has taken it.
     if(!host.HasFocus(view))return FocusUpdate::Lost;
     if(*paused==pause)return FocusUpdate::Ready;
     requested=pause;host.Unfocus(view);return FocusUpdate::Releasing;
   }
   if(!host.Focus(view,pause)){reset();return FocusUpdate::Lost;}
   paused=pause;return FocusUpdate::Ready;
 }
 template<class Host,class View> bool complete(Host& host,View view) {
   if(!requested||host.HasFocus(view))return false;
   if(!host.Focus(view,*requested)){reset();return false;}
   paused=requested;requested.reset();return true;
 }
 bool unpaused() const {return paused.has_value()&&!*paused;}
 bool switching() const {return requested.has_value();}
 void reset(){paused.reset();requested.reset();}
};
}
