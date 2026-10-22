#include "DropInMusic.h"
#include <cassert>
#include <iostream>
#include <map>

struct Audio {
 bool accepted=true,isPlaying=false;
 int requests=0,stops=0,suspends=0,restores=0,failures=0;
 bool play(const std::filesystem::path&) { ++requests; isPlaying=false; return accepted; }
 bool playing() { return isPlaying; }
 void stop() { ++stops; isPlaying=false; }
 void suspend() { ++suspends; }
 void restore() { ++restores; }
 void failed() { ++failures; }
};
int main(int argc,char** argv) {
 assert(argc==2);
 namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/"ttcg-drop-in-regression";
 fs::remove_all(root); fs::create_directories(root/"Nested");
 fs::copy_file(argv[1],root/"Track with spaces.XWM");
 fs::copy_file(argv[1],root/"Nested"/"duplicate.xwm");
 { std::ofstream bad(root/"broken.xwm"); bad<<"RIFFbroken"; }
 { std::ofstream unrelated(root/"notes.txt"); unrelated<<"Not a track"; }
 auto scan=ttcg::scanMusic(root);
 assert(scan.files.size()==1&&scan.duplicates==1&&scan.rejected.size()==1);
 fs::remove(root/"Track with spaces.XWM");
 assert(ttcg::scanMusic(root).files.size()==1); // Still finds nested duplicate.
 fs::remove(root/"Nested"/"duplicate.xwm");
 assert(ttcg::scanMusic(root).files.empty()); // Removed files disappear on rescan.
 assert(ttcg::scanMusic(root/"missing").files.empty());
 fs::remove_all(root);

 Audio audio; ttcg::FolderPlaylist player;
 player.stop(audio); assert(audio.restores==0&&audio.stops==0);
 assert(!player.start({},audio,0)&&!player.active());
 assert(player.start({"one.xwm","two.xwm"},audio,0));
 player.tick(audio,250); assert(audio.suspends==0); // Don't mute while loading.
 audio.isPlaying=true; player.tick(audio,500); assert(audio.suspends==1);
 player.tick(audio,1000); assert(audio.suspends==1); // No repeated pause.
 audio.isPlaying=false; player.tick(audio,2000); assert(audio.requests==2);
 audio.isPlaying=true; player.tick(audio,2250);
 audio.isPlaying=false; player.tick(audio,3000); assert(audio.requests==3); // Cycles.
 player.stop(audio); assert(!player.active()&&audio.restores==1);
 player.stop(audio); assert(audio.restores==1);
 player.tick(audio,9000); assert(audio.requests==3); // Stale timer cannot restart.

 Audio failed; failed.accepted=false; ttcg::FolderPlaylist bad;
 assert(!bad.start({"one.xwm","two.xwm"},failed,0));
 assert(failed.requests==2&&failed.restores==1&&!bad.active());
 Audio silent; ttcg::FolderPlaylist pending;
 assert(pending.start({"one.xwm","two.xwm"},silent,0));
 pending.tick(silent,2999); assert(silent.requests==1);
 pending.tick(silent,3000); assert(silent.requests==2);
 pending.tick(silent,6000); assert(!pending.active()&&silent.failures==2&&silent.restores==1&&silent.suspends==0);

 Audio missing; ttcg::FolderPlaylist removed;
 assert(removed.start({"one.xwm"},missing,0));
 missing.isPlaying=true; removed.tick(missing,250);
 missing.accepted=false; missing.isPlaying=false; removed.tick(missing,500);
 assert(!removed.active()&&missing.restores==1); // Removed next file restores music.
 std::cout<<"Drop-in music: folder scan, deduplication, bad files, asynchronous start, cycling, failure recovery and cancellation passed\n";
}
