#pragma once
#include <algorithm>
#include <utility>
#include <type_traits>

namespace ttcg {
// These helpers are shared with the regression tests; the game supplies its
// music type/manager and the native removal event as the finish callback.
template<class Music>
bool hasMusicTracks(const Music* music) {
 return music&&!music->tracks.empty()&&std::all_of(music->tracks.begin(),music->tracks.end(),[](auto* track){ return track!=nullptr; });
}
template<class Music>
bool hasCurrentMusicTrack(const Music* music) {
 return music&&music->currentTrackIndex<music->tracks.size()&&music->tracks[music->currentTrackIndex]!=nullptr;
}
// A paused menu can leave the location music playing after a playlist add.
// Own only a pause we performed, and never restart a different/stopped track.
template<class Music>
class SuspendedMusic {
 using TrackPointer=decltype(std::declval<Music>().tracks[0]);
 using Track=std::remove_reference_t<TrackPointer>;
 Music* owner{};
 Track track{};
public:
 template<class Playing,class Pause>
 bool suspend(Music* music,Music* excluded,Playing playing,Pause pause) {
   if(owner||!music||music==excluded||!hasCurrentMusicTrack(music)||!playing(music)) return false;
   auto selected=music->tracks[music->currentTrackIndex];
   if(!pause(music)) return false;
   owner=music; track=selected; return true;
 }
 template<class Relevant,class Paused,class Resume>
 bool restore(Relevant relevant,Paused paused,Resume resume) {
   auto previous=owner; auto selected=track;
   owner=nullptr; track=nullptr;
   if(!previous||!relevant(previous)||!hasCurrentMusicTrack(previous)||previous->tracks[previous->currentTrackIndex]!=selected||!paused(previous)) return false;
   resume(previous); return true;
 }
};
template<class Music,class Manager,class Finish>
void stopOwnedMusic(Music* music,Manager* manager,bool& added,Finish finish) {
 if(!added) return; // Startup, repeated closes and post-load cleanup own nothing.
 if(!music) { added=false; return; }
 if(!manager) return;
 if(hasCurrentMusicTrack(music)) {
   added=false; // Clear before dispatch: removal may cause another close callback.
   finish(music);
   return;
 }
 // Engine Fixes' DoFinish hook indexes the current track without a bounds
 // check. A queued type may not have selected a track yet. Cancel only our
 // pending queue entry in that case, without invoking DoFinish on it.
 if(manager->current==music) return; // Invalid active state: retain ownership for a later retry.
 auto& queue=manager->musicQueue;
 for(auto it=queue.begin();it!=queue.end();) {
   if(*it==music) it=queue.erase(it);
   else ++it;
 }
 added=false;
}
}
