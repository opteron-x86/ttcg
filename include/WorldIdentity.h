#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <algorithm>
namespace ttcg {
// Virtual world identities never contain a load-order index. Native callers
// normalize every non-vanilla form before putting it in a Tessera ledger.
inline constexpr std::uint32_t worldIdentityBase=0xE0000000u,worldIdentityLimit=0xE0010000u;
struct WorldForm {std::string plugin;std::uint32_t local=0;bool operator==(const WorldForm&)const=default;};
inline auto& worldForms(){static std::map<std::uint32_t,WorldForm> forms;return forms;}
inline std::string normalizedPlugin(std::string_view name){std::string s(name);for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
inline bool vanillaPlugin(std::string_view filename){
 const auto name=normalizedPlugin(filename);
 return name=="skyrim.esm"||name=="update.esm"||name=="dawnguard.esm"||name=="hearthfires.esm"||name=="dragonborn.esm";
}
inline bool residentVoiceAllowed(std::string_view voicePlugin,bool authored,bool vanillaOnly){
 return authored||!vanillaOnly||vanillaPlugin(voicePlugin);
}
inline bool validWorldForm(const WorldForm& f){
 if(f.plugin.empty()||f.plugin.size()>240||!f.local||f.local>0xFFFFFF)return false;
 for(unsigned char c:f.plugin)if(c<32||c==127||c=='/'||c=='\\'||c==':')return false;
 const auto s=normalizedPlugin(f.plugin);return s.ends_with(".esm")||s.ends_with(".esp")||s.ends_with(".esl");
}
inline bool virtualWorldID(std::uint32_t id){return id>=worldIdentityBase&&id<worldIdentityLimit;}
inline std::uint32_t worldFormID(WorldForm f){
 f.plugin=normalizedPlugin(f.plugin);if(!validWorldForm(f))return 0;
 if(f.plugin=="skyrim.esm")return f.local;
 if(f.plugin=="dragonborn.esm")return 0xDB000000u|f.local;
 if(f.plugin=="hearthfires.esm")return 0xFB000000u|f.local;
 for(const auto& [id,entry]:worldForms())if(entry==f)return id;
 if(worldForms().size()>=worldIdentityLimit-worldIdentityBase)return 0;
 const auto id=worldIdentityBase+static_cast<std::uint32_t>(worldForms().size());worldForms().emplace(id,std::move(f));return id;
}
inline const char* identityMaster(std::uint32_t id){
 if(auto it=worldForms().find(id);it!=worldForms().end())return it->second.plugin.c_str();
 switch(id>>24){case 0xDB:return "Dragonborn.esm";case 0xFB:return "HearthFires.esm";default:return nullptr;}
}
inline std::uint32_t identityLocal(std::uint32_t id){auto it=worldForms().find(id);return it==worldForms().end()?id&0xFFFFFFu:it->second.local;}
inline std::uint32_t authoredIdentity(std::string_view master,std::uint32_t local,std::uint32_t loaded){return master.empty()?loaded:worldFormID({std::string(master),local});}
inline std::string worldKey(std::uint32_t id){const auto* plugin=identityMaster(id);return normalizedPlugin(plugin?plugin:"Skyrim.esm")+":"+std::to_string(identityLocal(id));}
inline std::uint32_t identitySeed(std::string_view key){std::uint32_t hash=2166136261u;for(unsigned char c:key){hash^=c;hash*=16777619u;}return hash;}
}
