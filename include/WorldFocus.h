#pragma once
#include <optional>
#include <string_view>

namespace ttcg {
inline bool shouldPauseWorld(bool pauseAlbum,std::string_view screen,bool matchContext,bool active) {
 return pauseAlbum||screen!="album"||matchContext||active;
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
