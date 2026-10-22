#include "MusicPlayback.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
struct Track { int finishes=0; };
struct Music { std::vector<Track*> tracks; std::uint32_t currentTrackIndex=0; int status=1,pauses=0,resumes=0; };
struct Manager { std::vector<Music*> musicQueue; Music* current=nullptr; };
int main() {
 Track track,ambientTrack;
 Music empty,playlist{{&track}},ambient{{&ambientTrack}};
 Manager manager{{&ambient},&ambient};
 bool added=false; int removals=0;
 auto finish=[&](Music* music) {
   // Reproduce the Engine Fixes hook's unchecked track access: the helper
   // must never dispatch removal unless this index and pointer are valid.
   assert(ttcg::hasCurrentMusicTrack(music));
   ++music->tracks.at(music->currentTrackIndex)->finishes; ++removals;
   std::erase(manager.musicQueue,music);
   if(manager.current==music) manager.current=&ambient;
 };
 // The reported startup crash: an empty MUSC exists, but was never added.
 ttcg::stopOwnedMusic(&empty,&manager,added,finish);
 assert(removals==0&&manager.current==&ambient&&manager.musicQueue.size()==1);
 // A populated but untouched playlist must not be stopped on startup either.
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish); assert(removals==0);
 assert(!ttcg::hasMusicTracks(&empty)&&ttcg::hasMusicTracks(&playlist));
 Music broken{{nullptr}}; assert(!ttcg::hasMusicTracks(&broken));
 // A normal toggle-off stops only TTCG; repeated close/load callbacks do nothing.
 added=true; manager.current=&playlist; manager.musicQueue.push_back(&playlist);
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish);
 assert(!added&&removals==1&&track.finishes==1&&ambientTrack.finishes==0&&manager.current==&ambient);
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish); assert(removals==1);
 // Turn off before the queued playlist has selected its first track.
 playlist.currentTrackIndex=UINT32_MAX; added=true; manager.musicQueue.push_back(&playlist);
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish);
 assert(!added&&removals==1&&manager.musicQueue==std::vector<Music*>{&ambient});
 // Empty or unresolved queued playlists must never reach the finish hook.
 for(auto* music:{&empty,&broken}) {
   added=true; manager.musicQueue.push_back(music);
   ttcg::stopOwnedMusic(music,&manager,added,finish);
   assert(!added&&removals==1&&manager.musicQueue.size()==1);
 }
 // Keep an invalid active state for retry rather than dereferencing it.
 added=true; manager.current=&playlist;
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish); assert(added&&removals==1);
 playlist.currentTrackIndex=0;
 ttcg::stopOwnedMusic(&playlist,&manager,added,finish); assert(!added&&removals==2);
 // Manager teardown cannot dispatch events; missing forms relinquish ownership.
 added=true; ttcg::stopOwnedMusic(&playlist,static_cast<Manager*>(nullptr),added,finish); assert(added);
 ttcg::stopOwnedMusic(static_cast<Music*>(nullptr),&manager,added,finish); assert(!added);
 // A paused menu keeps the old music current while TTCG plays alongside it.
 // Explicitly suspend it and resume exactly once when TTCG is released.
 ttcg::SuspendedMusic<Music> suspended;
 auto playing=[](Music* m){ return m->status==1; };
 auto pause=[](Music* m){ ++m->pauses; m->status=2; return true; };
 auto paused=[](Music* m){ return m->status==2; };
 auto resume=[](Music* m){ ++m->resumes; m->status=1; };
 manager.current=&ambient;
 auto relevant=[&](Music* m){ return manager.current==m; };
 assert(!suspended.restore(relevant,paused,resume)); // Startup owns no pause.
 assert(suspended.suspend(&ambient,&playlist,playing,pause));
 assert(!suspended.suspend(&ambient,&playlist,playing,pause)); // Rematch/repeated on.
 assert(ambient.pauses==1&&ambient.status==2&&playlist.pauses==0);
 assert(suspended.restore(relevant,paused,resume));
 assert(!suspended.restore(relevant,paused,resume));
 assert(ambient.resumes==1&&ambient.status==1);
 assert(!suspended.suspend(&playlist,&playlist,playing,pause));
 assert(!suspended.suspend(&empty,&playlist,playing,pause));
 assert(!suspended.suspend(&broken,&playlist,playing,pause));
 ambient.status=2; // Another system paused it first; leave it alone.
 assert(!suspended.suspend(&ambient,&playlist,playing,pause));
 assert(!suspended.restore(relevant,paused,resume));
 ambient.status=1;
 assert(suspended.suspend(&ambient,&playlist,playing,pause));
 manager.current=&playlist; // Location changed: never restart the old music.
 assert(!suspended.restore(relevant,paused,resume));
 manager.current=&ambient; ambient.status=1;
 assert(suspended.suspend(&ambient,&playlist,playing,pause));
 ambient.tracks[0]=&track; // Track replaced under the same music type.
 assert(!suspended.restore(relevant,paused,resume));
 ambient.tracks[0]=&ambientTrack; ambient.status=1;
 assert(suspended.suspend(&ambient,&playlist,playing,pause));
 ambient.status=4; // Finished by the engine while owned.
 assert(!suspended.restore(relevant,paused,resume));
 assert(ambient.resumes==1);
 std::cout<<"Music playback regression: guarded cleanup and owned location pause/resume passed\n";
}
