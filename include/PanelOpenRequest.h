#pragma once
#include <atomic>
#include <cstdint>

namespace ttcg {
// One short-lived request, advanced on the game thread. Workers only read the
// generation; closing/loading/replacing a request invalidates their queued work.
class PanelOpenRequest {
public:
 enum class Step { Idle, Waiting, Open, Expired };
 static constexpr std::uint64_t retryMs=100,settleMs=100,timeoutMs=8000;
private:
 std::atomic<std::uint64_t> generation{0};
 std::uint64_t started=0,clearSince=0;
 bool waiting=false,clear=false;
public:
 std::uint64_t begin(std::uint64_t now) {
  started=now;clearSince=0;waiting=true;clear=false;return ++generation;
 }
 void cancel() {waiting=false;clear=false;++generation;}
 bool current(std::uint64_t token) const {return generation.load()==token;}
 // Only the game thread reads the request state; workers use current().
 bool active(std::uint64_t token) const {return waiting&&current(token);}
 std::uint64_t token() const {return generation.load();}
 std::uint64_t elapsed(std::uint64_t now) const {return now>=started?now-started:0;}
 Step poll(std::uint64_t token,std::uint64_t now,bool blocked) {
  if(!active(token))return Step::Idle;
  if(elapsed(now)>=timeoutMs){cancel();return Step::Expired;}
  if(blocked){clear=false;return Step::Waiting;}
  if(!clear){clear=true;clearSince=now;}
  if(now-clearSince<settleMs)return Step::Waiting;
  cancel();return Step::Open;
 }
};
}
