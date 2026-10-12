#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>
#include <utility>

namespace ttcg {
enum class NpcSyncPhase {World,Access,Globals,Dialogue,Cleanup,Count};
enum class NpcSyncWork {Other,Forms,Identity,Integration,ActorState,Humanoid,Scene,Hostility,ChildConditions,Factions,Readiness,Restock,Cache,Count};
inline constexpr std::array<std::string_view,5> npcSyncPhaseNames{"world","access","globals","dialogue","cleanup"};
inline constexpr std::array<std::string_view,13> npcSyncWorkNames{"other","forms","identity","integration","actor state","humanoid","scene","hostility","child conditions","factions","readiness","restock","cache"};
struct NpcSyncTiming {
 using Clock=std::chrono::steady_clock;
 using Duration=Clock::duration;
 std::array<Duration,std::size_t(NpcSyncPhase::Count)> phases{};
 std::array<Duration,std::size_t(NpcSyncWork::Count)> work{};
 std::array<unsigned,std::size_t(NpcSyncWork::Count)> calls{};
 Duration total{};
 unsigned clockReads=0;
 static double ms(Duration duration){return std::chrono::duration<double,std::milli>(duration).count();}
};

// Only selected background/load refreshes activate a capture. Thread-local
// ownership keeps simultaneous engine tasks separate; no actor state is cached.
class NpcSyncCapture;
inline thread_local NpcSyncCapture* activeNpcSyncCapture=nullptr;
class NpcSyncCapture {
 NpcSyncCapture* previous_{};
 NpcSyncTiming::Clock::time_point started_{},segment_{};
 NpcSyncWork work_=NpcSyncWork::Other;
public:
 NpcSyncTiming* timing{};
 explicit NpcSyncCapture(NpcSyncTiming* target):timing(target){
  if(!timing)return;
  *timing={};previous_=activeNpcSyncCapture;activeNpcSyncCapture=this;
  started_=segment_=now();
 }
 NpcSyncCapture(const NpcSyncCapture&)=delete;
 NpcSyncCapture& operator=(const NpcSyncCapture&)=delete;
 ~NpcSyncCapture(){
  if(!timing)return;
  const auto end=now();timing->work[std::size_t(work_)]+=end-segment_;
  timing->total=end-started_;activeNpcSyncCapture=previous_;
 }
 NpcSyncTiming::Clock::time_point now(){++timing->clockReads;return NpcSyncTiming::Clock::now();}
 NpcSyncWork switchWork(NpcSyncWork next){
  const auto at=now();timing->work[std::size_t(work_)]+=at-segment_;
  const auto previous=work_;work_=next;segment_=at;return previous;
 }
};

// Work buckets are exclusive: nested form/actor checks are subtracted from
// their caller's bucket. Calls count instrumented scopes, not engine opcodes.
class NpcSyncWorkScope {
 NpcSyncCapture* capture_=activeNpcSyncCapture;
 NpcSyncWork previous_=NpcSyncWork::Other;
public:
 explicit NpcSyncWorkScope(NpcSyncWork work){
  if(capture_){++capture_->timing->calls[std::size_t(work)];previous_=capture_->switchWork(work);}
 }
 NpcSyncWorkScope(const NpcSyncWorkScope&)=delete;
 NpcSyncWorkScope& operator=(const NpcSyncWorkScope&)=delete;
 ~NpcSyncWorkScope(){if(capture_)capture_->switchWork(previous_);}
};
template<class F> decltype(auto) measureNpcSync(NpcSyncWork work,F&& function){
 const NpcSyncWorkScope timing(work);return std::forward<F>(function)();
}

// The five coarse phases do not nest. They describe where work happened;
// the work buckets above describe what it did. Do not add the two reports.
class NpcSyncPhaseScope {
 NpcSyncCapture* capture_=activeNpcSyncCapture;
 NpcSyncPhase phase_;
 NpcSyncTiming::Clock::time_point started_{};
public:
 explicit NpcSyncPhaseScope(NpcSyncPhase phase):phase_(phase){if(capture_)started_=capture_->now();}
 NpcSyncPhaseScope(const NpcSyncPhaseScope&)=delete;
 NpcSyncPhaseScope& operator=(const NpcSyncPhaseScope&)=delete;
 ~NpcSyncPhaseScope(){if(capture_)capture_->timing->phases[std::size_t(phase_)]+=capture_->now()-started_;}
};
}
