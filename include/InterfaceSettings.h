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
 unsigned theme=0,boardSurface=0,boardTexture=10,windowSize=90;
 unsigned rankShadowOpacity=100,rankShadowBlur=2,rankShadowOffset=1,rankStrokeWeight=10;
 bool controlMarker=false;
 unsigned opponentTreatment=0,opponentScope=2,opponentStrength=100;
 unsigned controlPalette=0,playerColour=0x87c4dd,opponentColour=0xdf9585;
 unsigned controlBorderStyle=0,controlBorderWidth=12,controlBorderOpacity=100;
 struct Field {
  std::string_view command,json;const char* section;const char* ini;
  std::variant<bool InterfaceSettings::*,unsigned InterfaceSettings::*> member;
  int maximum,defaultValue;
  bool valid(int value)const{return command=="window-size"?value>=60&&value<=100&&value%5==0:value>=0&&value<=maximum;}
  bool boolean()const{return std::holds_alternative<bool InterfaceSettings::*>(member);}
 };
 static constexpr std::array fields{
  Field{"window-size","windowSize","Appearance","WindowSize",&InterfaceSettings::windowSize,100,90},
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
  Field{"rank-stroke-weight","rankStrokeWeight","Appearance","RankStrokeWeight",&InterfaceSettings::rankStrokeWeight,30,10},
  Field{"match-art-treatment","opponentTreatment","Appearance","OpponentTreatment",&InterfaceSettings::opponentTreatment,3,0},
  Field{"match-art-scope","opponentScope","Appearance","OpponentScope",&InterfaceSettings::opponentScope,2,2},
  Field{"match-art-strength","opponentStrength","Appearance","OpponentStrength",&InterfaceSettings::opponentStrength,100,100},
  Field{"match-palette","controlPalette","Appearance","ControlPalette",&InterfaceSettings::controlPalette,4,0},
  Field{"match-player-colour","playerColour","Appearance","PlayerColour",&InterfaceSettings::playerColour,0xffffff,0x87c4dd},
  Field{"match-opponent-colour","opponentColour","Appearance","OpponentColour",&InterfaceSettings::opponentColour,0xffffff,0xdf9585},
  Field{"match-border-style","controlBorderStyle","Appearance","ControlBorderStyle",&InterfaceSettings::controlBorderStyle,3,0},
  Field{"match-border-width","controlBorderWidth","Appearance","ControlBorderWidth",&InterfaceSettings::controlBorderWidth,40,12},
  Field{"match-border-opacity","controlBorderOpacity","Appearance","ControlBorderOpacity",&InterfaceSettings::controlBorderOpacity,100,100},
  Field{"match-control-marker","controlMarker","Appearance","ControlMarker",&InterfaceSettings::controlMarker,1,0}
 };
 static const Field* find(std::string_view key){for(const auto& f:fields)if(f.command==key)return &f;return nullptr;}
 int get(const Field& field)const{return std::visit([&](auto member){return int(this->*member);},field.member);}
 void assign(const Field& field,int value){std::visit([&](auto member){this->*member=static_cast<std::remove_reference_t<decltype(this->*member)>>(value);},field.member);}
 template<class Read> void load(Read read){for(const auto& f:fields){const auto value=read(f.section,f.ini,f.defaultValue);assign(f,f.valid(value)?value:f.defaultValue);}}
 template<class Save> bool resetRankFinish(Save save){
  return resetGroup("rank-",save);
 }
 template<class Save> bool resetMatchAppearance(Save save){
  return resetGroup("match-",save);
 }
 template<class Save> bool resetGroup(std::string_view prefix,Save save){
  bool success=true;
  for(const auto& f:fields)if(f.command.starts_with(prefix)){
   if(save(f.section,f.ini,f.defaultValue))assign(f,f.defaultValue);else success=false;
  }
  return success;
 }
 template<class Save> bool set(std::string_view key,int value,Save save) {
  const auto* f=find(key);if(!f||!f->valid(value)||!save(f->section,f->ini,value))return false;
  assign(*f,value);return true;
 }
};
// Keep the original DirectInput code in the low byte so existing INIs load
// unchanged. Modifier bits and the primary key are persisted in one write.
enum HotkeyModifier : unsigned {
 HotkeyLShift=1u<<8,HotkeyRShift=1u<<9,HotkeyLCtrl=1u<<10,
 HotkeyRCtrl=1u<<11,HotkeyLAlt=1u<<12,HotkeyRAlt=1u<<13,
 HotkeyMeta=1u<<14 // Observed, but never accepted as a binding.
};
inline constexpr unsigned hotkeyModifierMask=0x3F00;
struct HotkeyModifierKey {unsigned scan,flag;};
inline constexpr std::array<HotkeyModifierKey,6> hotkeyModifierKeys{{
 {42,HotkeyLShift},{54,HotkeyRShift},{29,HotkeyLCtrl},
 {157,HotkeyRCtrl},{56,HotkeyLAlt},{184,HotkeyRAlt}
}};
template<class Pressed> unsigned heldHotkeyModifiers(Pressed pressed){
 unsigned modifiers=0;
 for(const auto& m:hotkeyModifierKeys)if(pressed(m.scan))modifiers|=m.flag;
 if(pressed(219)||pressed(220))modifiers|=HotkeyMeta;
 return modifiers;
}
inline bool validHotkey(unsigned binding) {
 if(binding&~(255u|hotkeyModifierMask))return false;
 const auto key=binding&255u;
 if(!key)return binding==0;
 for(auto reserved:{1u,29u,42u,54u,56u,157u,184u,219u,220u})if(key==reserved)return false;
 return true;
}
inline bool hotkeyMatches(unsigned binding,unsigned key,unsigned modifiers){
 return binding&&key<=255&&!(modifiers&~hotkeyModifierMask)&&validHotkey(binding)&&binding==(key|modifiers);
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
