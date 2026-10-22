#include "NpcFactionTracking.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <latch>
#include <thread>
using namespace ttcg;

int main(){
 NpcFactionTracking tags;
 const auto first=tags.generation();
 // Serialization must not consume the recovery pass before the actor lists
 // are ready. Later refreshes must not repeat it.
 assert(!tags.beginDiscovery(first));tags.allowDiscovery();
 assert(tags.beginDiscovery(first));assert(!tags.beginDiscovery(first));

 // Recovery or an actor-load event can observe a stale tag, including an
 // actor carrying only the generic-dialogue faction. No profile is needed.
 assert(tags.observe(first,100));assert(tags.observe(first,100));
 assert(tags.observe(first,200));assert(!tags.observe(first,0));
 assert(tags.reconcile(first,{100})==std::vector<std::uint32_t>{200});
 assert(tags.reconcile(first,{100}).empty());

 // Registered actors are tracked when dialogue writes their markers. A
 // reference change or removed profile clears only the old reference.
 assert(tags.reconcile(first,{100,300}).empty());
 assert(tags.reconcile(first,{101,300})==std::vector<std::uint32_t>{100});
 assert(tags.reconcile(first,{101})==std::vector<std::uint32_t>{300});
 assert(tags.reconcile(first,{101}).empty());
 // Disabled/busy profiles remain registered (with availability rank zero).
 // Their markers must not be mistaken for a removed profile.
 assert(tags.reconcile(first,{101}).empty());

 // Forget unresolved references after cleanup; a later actor-load event
 // discovers persisted markers and retries without a full process-list scan.
 assert(tags.reconcile(first,{})==std::vector<std::uint32_t>{101});
 assert(tags.reconcile(first,{}).empty());
 assert(tags.observe(first,101));
 assert(tags.reconcile(first,{})==std::vector<std::uint32_t>{101});

 // A save switch invalidates queued observations and cleanup snapshots from
 // the previous character, even if both saves use the same loaded form IDs.
 assert(tags.observe(first,400));tags.reset();const auto next=tags.generation();
 assert(next!=first&&!tags.current(first)&&tags.current(next));
 assert(!tags.observe(first,400));assert(tags.reconcile(first,{400}).empty());
 assert(!tags.beginDiscovery(next));tags.allowDiscovery();assert(tags.beginDiscovery(next));
 assert(tags.reconcile(next,{}).empty());
 assert(tags.observe(next,400));assert(tags.reconcile(next,{400}).empty());
 assert(tags.reconcile(first,{}).empty());assert(tags.reconcile(next,{400}).empty());
 assert(tags.reconcile(next,{})==std::vector<std::uint32_t>{400});

 // Engine tasks can arrive from different threads. Recovery is claimed only
 // once, observations deduplicate, and an old task cannot resurrect old IDs.
 tags.reset();tags.allowDiscovery();const auto concurrent=tags.generation();
 std::atomic_uint claims{0};
 std::thread a([&]{claims+=tags.beginDiscovery(concurrent);for(unsigned n=1;n<=100;++n)assert(tags.observe(concurrent,n));});
 std::thread b([&]{claims+=tags.beginDiscovery(concurrent);for(unsigned n=1;n<=100;++n)assert(tags.observe(concurrent,n));});
 a.join();b.join();assert(claims==1);assert(tags.reconcile(concurrent,{}).size()==100);
 std::latch queued(1),resume(1);
 std::thread old([&]{queued.count_down();resume.wait();assert(!tags.observe(concurrent,999));assert(tags.reconcile(concurrent,{999}).empty());});
 queued.wait();tags.reset();const auto final=tags.generation();
 assert(tags.observe(final,1000));resume.count_down();old.join();
 assert(tags.reconcile(final,{})==std::vector<std::uint32_t>{1000});
 std::cout<<"NPC faction tracking: deferred recovery, stale tags, reference changes, reloads, unresolved actors and concurrent tasks passed\n";
}
