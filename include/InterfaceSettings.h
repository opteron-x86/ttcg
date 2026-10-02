#pragma once
#include <array>
#include <string_view>

namespace ttcg {
struct InterfaceSettings {
 bool gamesEnabled=true,showCardNames=true;
 unsigned rankLayout=0; // 0: edges; 1: classic diamond.
 template<class Save> bool set(std::string_view key,int value,Save save) {
  if(value!=0&&value!=1)return false;
  const char* section=key=="games"?"Gameplay":"Appearance";
  const char* setting=key=="games"?"Enabled":key=="card-names"?"ShowCardNames":key=="card-ranks"?"RankLayout":nullptr;
  if(!setting||!save(section,setting,value))return false;
  if(key=="games")gamesEnabled=value!=0;
  else if(key=="card-names")showCardNames=value!=0;
  else rankLayout=unsigned(value);
  return true;
 }
};
inline bool validHotkey(unsigned key) {
 if(key>255)return false;
 for(auto reserved:{1u,29u,42u,54u,56u,157u,184u,219u,220u})if(key==reserved)return false;
 return true;
}
struct HotkeyBindings {
 unsigned collection=65,challenge=66,toggleGames=0;
 struct Field {std::string_view name,label;const wchar_t* ini;unsigned HotkeyBindings::* member;unsigned defaultKey;};
 static constexpr std::array fields{
  Field{"collection","Album",L"CollectionKey",&HotkeyBindings::collection,65},
  Field{"challenge","Challenge",L"ChallengeKey",&HotkeyBindings::challenge,66},
  Field{"toggleGames","Enable / disable games",L"ToggleGamesKey",&HotkeyBindings::toggleGames,0}
 };
 std::string_view conflict(std::string_view name,unsigned key) const {
  for(const auto& f:fields)if(key&&f.name!=name&&this->*f.member==key)return f.label;
  return {};
 }
 void normalize() {
  for(std::size_t i=0;i<fields.size();++i){
   auto& value=this->*fields[i].member;
   if(!validHotkey(value))value=0;
   for(std::size_t j=0;j<i;++j)if(value==this->*fields[j].member)value=0;
  }
 }
};
}
