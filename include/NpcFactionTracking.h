#pragma once
#include <cstdint>
#include <mutex>
#include <set>
#include <vector>

namespace ttcg {
// Transient loaded reference IDs, never saved identities or actor pointers.
// Faction state is rediscovered once after reset and on actor-load events.
class NpcFactionTracking {
 mutable std::mutex mutex_;
 std::uint64_t generation_=0;
 bool discoveryPending_=true;
 bool discoveryAllowed_=false;
 std::set<std::uint32_t> tagged_;
public:
 void reset(){
  std::scoped_lock lock(mutex_);++generation_;tagged_.clear();discoveryPending_=true;discoveryAllowed_=false;
 }
 // Serialization can refresh dialogue before Skyrim has finished loading its
 // actor lists. The post-load/new-game task opens recovery for the new world.
 void allowDiscovery(){std::scoped_lock lock(mutex_);discoveryAllowed_=true;}
 std::uint64_t generation()const{std::scoped_lock lock(mutex_);return generation_;}
 bool current(std::uint64_t generation)const{std::scoped_lock lock(mutex_);return generation==generation_;}
 bool beginDiscovery(std::uint64_t generation){
  std::scoped_lock lock(mutex_);
  if(generation!=generation_||!discoveryAllowed_||!discoveryPending_)return false;
  discoveryPending_=false;return true;
 }
 bool observe(std::uint64_t generation,std::uint32_t actor){
  std::scoped_lock lock(mutex_);
  if(generation!=generation_||!actor)return false;
  tagged_.insert(actor);return true;
 }
 std::vector<std::uint32_t> reconcile(std::uint64_t generation,const std::set<std::uint32_t>& registered){
  std::scoped_lock lock(mutex_);
  if(generation!=generation_)return {};
  std::vector<std::uint32_t> removed;
  for(auto it=tagged_.begin();it!=tagged_.end();){
   if(registered.contains(*it)){++it;continue;}
   removed.push_back(*it);it=tagged_.erase(it);
  }
  tagged_.insert(registered.begin(),registered.end());
  return removed;
 }
};
}
