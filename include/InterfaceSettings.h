#pragma once
#include <array>
#include <string_view>
#include <variant>
#include <type_traits>

namespace ttcg {
struct InterfaceSettings {
 bool gamesEnabled=true,showCardNames=true,vanillaVoicesOnly=true,rankStroke=false;
 unsigned residents=1,rankLayout=0,nameplateStyle=0,nameCase=0;
 unsigned interfaceFont=0,cardNameFont=0,cardValueFont=0;
 unsigned theme=0,boardSurface=0,boardTexture=10;
 unsigned rankShadowOpacity=100,rankShadowBlur=2,rankShadowOffset=1,rankStrokeWeight=10;
 struct Field {
  std::string_view command,json;const char* section;const char* ini;
  std::variant<bool InterfaceSettings::*,unsigned InterfaceSettings::*> member;
  int maximum,defaultValue;
  bool valid(int value)const{return value>=0&&value<=maximum;}
  bool boolean()const{return std::holds_alternative<bool InterfaceSettings::*>(member);}
 };
 static constexpr std::array fields{
  Field{"games","gamesEnabled","Gameplay","Enabled",&InterfaceSettings::gamesEnabled,1,1},
  Field{"residents","residents","Gameplay","Residents",&InterfaceSettings::residents,2,1},
  Field{"vanilla-voices","vanillaVoicesOnly","Gameplay","VanillaVoicesOnly",&InterfaceSettings::vanillaVoicesOnly,1,1},
  Field{"card-names","showCardNames","Appearance","ShowCardNames",&InterfaceSettings::showCardNames,1,1},
  Field{"card-ranks","rankLayout","Appearance","RankLayout",&InterfaceSettings::rankLayout,1,0},
  Field{"nameplate","nameplateStyle","Appearance","NameplateStyle",&InterfaceSettings::nameplateStyle,2,0},
  Field{"name-case","nameCase","Appearance","NameCase",&InterfaceSettings::nameCase,2,0},
  Field{"interface-font","interfaceFont","Appearance","InterfaceFont",&InterfaceSettings::interfaceFont,4,0},
  Field{"card-name-font","cardNameFont","Appearance","CardNameFont",&InterfaceSettings::cardNameFont,4,0},
  Field{"card-value-font","cardValueFont","Appearance","CardValueFont",&InterfaceSettings::cardValueFont,4,0},
  Field{"theme","theme","Appearance","Theme",&InterfaceSettings::theme,4,0},
  Field{"board-surface","boardSurface","Appearance","BoardSurface",&InterfaceSettings::boardSurface,6,0},
  Field{"board-texture","boardTexture","Appearance","BoardTexture",&InterfaceSettings::boardTexture,100,10},
  Field{"rank-shadow-opacity","rankShadowOpacity","Appearance","RankShadowOpacity",&InterfaceSettings::rankShadowOpacity,100,100},
  Field{"rank-shadow-blur","rankShadowBlur","Appearance","RankShadowBlur",&InterfaceSettings::rankShadowBlur,8,2},
  Field{"rank-shadow-offset","rankShadowOffset","Appearance","RankShadowOffset",&InterfaceSettings::rankShadowOffset,6,1},
  Field{"rank-stroke","rankStroke","Appearance","RankStroke",&InterfaceSettings::rankStroke,1,0},
  // Tenths of a pixel at a 32px value size. Scales with the card's lettering.
  Field{"rank-stroke-weight","rankStrokeWeight","Appearance","RankStrokeWeight",&InterfaceSettings::rankStrokeWeight,30,10}
 };
 static const Field* find(std::string_view key){for(const auto& f:fields)if(f.command==key)return &f;return nullptr;}
 int get(const Field& field)const{return std::visit([&](auto member){return int(this->*member);},field.member);}
 void assign(const Field& field,int value){std::visit([&](auto member){this->*member=static_cast<std::remove_reference_t<decltype(this->*member)>>(value);},field.member);}
 template<class Read> void load(Read read){for(const auto& f:fields){const auto value=read(f.section,f.ini,f.defaultValue);assign(f,f.valid(value)?value:f.defaultValue);}}
 template<class Save> bool resetRankFinish(Save save){
  bool success=true;
  for(const auto& f:fields)if(f.command.starts_with("rank-")){
   if(save(f.section,f.ini,f.defaultValue))assign(f,f.defaultValue);else success=false;
  }
  return success;
 }
 template<class Save> bool set(std::string_view key,int value,Save save) {
  const auto* f=find(key);if(!f||!f->valid(value)||!save(f->section,f->ini,value))return false;
  assign(*f,value);return true;
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
