#pragma once
#include <cstdint>
#include <optional>

namespace ttcg {
// Only time work that blocks the table. A player's turn has no deadline.
class PresentationWait {
 struct Pending { std::uint64_t session,revision,since; unsigned phase; };
 std::optional<Pending> pending;
public:
 static constexpr std::uint64_t timeout=30000;
 void reset(){pending.reset();}
 bool expired(std::uint64_t session,std::uint64_t revision,unsigned phase,std::uint64_t now){
  if(!phase){reset();return false;}
  if(!pending||pending->session!=session||pending->revision!=revision||pending->phase!=phase){
   pending=Pending{session,revision,now,phase};return false;
  }
  return now>=pending->since&&now-pending->since>=timeout;
 }
};
}
