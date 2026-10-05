#pragma once
#include "WorldIdentity.h"
#include <vector>
#include <set>
namespace ttcg {
inline constexpr unsigned maximumResidents=4096;
struct ResidentRecord {
 std::uint32_t reference=0,seed=0;
 unsigned source=0,hold=0,level=0; // source: automatic, manual, authored
 bool selected=false,enabled=true,traveller=false;
 std::string name,location,theme,definition;
 std::vector<std::uint32_t> starting;
};
struct ResidentState {
 std::uint32_t seed=0;
 std::map<std::uint32_t,ResidentRecord> profiles;
 std::set<std::string> grants;
};
inline void writeIdentityText(std::vector<std::uint32_t>& out,std::string_view value){out.push_back(static_cast<unsigned>(value.size()));for(unsigned char c:value)out.push_back(c);}
template<class Get> bool readIdentityText(Get get,std::string& value,unsigned limit){auto size=get();if(size>limit)return false;for(unsigned i=0;i<size;++i){auto c=get();if(c>255||c<32||c==127)return false;value+=char(c);}return true;}
inline void encodeResidents(std::vector<std::uint32_t>& out,const ResidentState& state){
 out.push_back(state.seed);out.push_back(state.profiles.size());
 for(const auto& [base,p]:state.profiles){for(auto v:{base,p.reference,p.seed,p.source,p.hold,p.level,unsigned(p.selected),unsigned(p.enabled),unsigned(p.traveller)})out.push_back(v);
  for(const auto* s:{&p.name,&p.location,&p.theme,&p.definition})writeIdentityText(out,*s);
  out.push_back(p.starting.size());out.insert(out.end(),p.starting.begin(),p.starting.end());
 }
 out.push_back(state.grants.size());for(const auto& key:state.grants)writeIdentityText(out,key);
}
template<class Get,class Card> bool decodeResidents(Get get,Card card,ResidentState& state){
 state.seed=get();auto n=get();if(n>maximumResidents)return false;
 for(unsigned i=0;i<n;++i){const auto base=get();if(!base||state.profiles.contains(base))return false;auto& p=state.profiles[base];
  p.reference=get();p.seed=get();p.source=get();p.hold=get();p.level=get();auto selected=get(),enabled=get(),traveller=get();
  if(!p.reference||p.source>2||p.hold>10||p.level>5||selected>1||enabled>1||traveller>1)return false;
  p.selected=selected;p.enabled=enabled;p.traveller=traveller;
  if(!readIdentityText(get,p.name,128)||p.name.empty()||!readIdentityText(get,p.location,128)||!readIdentityText(get,p.theme,32)||!readIdentityText(get,p.definition,129))return false;
  const auto count=get();if(count>128)return false;for(unsigned j=0;j<count;++j){auto id=card();if(!id)return false;p.starting.push_back(id);}
 }
 n=get();if(n>16384)return false;for(unsigned i=0;i<n;++i){std::string key;if(!readIdentityText(get,key,256)||key.empty()||!state.grants.insert(key).second)return false;}return true;
}
}
