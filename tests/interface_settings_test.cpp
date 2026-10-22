#include "InterfaceSettings.h"
#include "WorldIdentity.h"
#include <cassert>
#include <string>
#include <iostream>
int main(){
 ttcg::InterfaceSettings settings;
 assert(settings.gamesEnabled&&settings.showCardNames&&settings.rankLayout==0);
 std::string stored;int calls=0;
 auto write=[&](const char* section,const char* key,int value){++calls;stored=std::string(section)+"/"+key+"="+std::to_string(value);return true;};
 assert(settings.set("games",0,write)&&!settings.gamesEnabled&&stored=="Gameplay/Enabled=0");
 assert(settings.set("card-ranks",1,write)&&settings.rankLayout==1&&stored=="Appearance/RankLayout=1");
 assert(settings.set("card-names",0,write)&&!settings.showCardNames&&stored=="Appearance/ShowCardNames=0");
 auto failed=[](const char*,const char*,int){return false;};
 assert(!settings.set("games",1,failed)&&!settings.gamesEnabled);
 assert(!settings.set("card-ranks",0,failed)&&settings.rankLayout==1);
 assert(!settings.set("card-names",1,failed)&&!settings.showCardNames);
 for(auto key:{"games","card-ranks","card-names","invalid"})for(int value:{-1,2,100})assert(!settings.set(key,value,write));
 assert(!settings.set("invalid",1,write)&&calls==3);
 assert(settings.set("games",1,write)&&settings.gamesEnabled);
 assert(settings.residents==1);
 for(int value:{0,1,2})assert(settings.set("residents",value,write)&&settings.residents==unsigned(value)&&stored=="Gameplay/Residents="+std::to_string(value));
 assert(!settings.set("residents",0,failed)&&settings.residents==2);
 for(int value:{-1,3,100})assert(!settings.set("residents",value,write)&&settings.residents==2);
 // Every persisted field uses the same bounds for INI loading and UI writes.
 ttcg::InterfaceSettings defaults;
 for(const auto& f:ttcg::InterfaceSettings::fields){
  assert(defaults.get(f)==f.defaultValue);
  assert(settings.set(f.command,f.maximum,write)&&settings.get(f)==f.maximum);
  assert(!settings.set(f.command,0,failed)&&settings.get(f)==f.maximum);
  assert(!settings.set(f.command,-1,write)&&!settings.set(f.command,f.maximum+1,write));
 }
 settings.load([](const char*,const char*,int){return -1;});
 for(const auto& f:ttcg::InterfaceSettings::fields)assert(settings.get(f)==f.defaultValue);
 settings.load([](const char*,const char*,int){return 0x7fffffff;});
 for(const auto& f:ttcg::InterfaceSettings::fields)assert(settings.get(f)==f.defaultValue);
 assert(settings.set("rank-shadow-opacity",0,write)&&settings.rankShadowOpacity==0);
 assert(settings.set("rank-stroke",1,write)&&settings.rankStroke);
 assert(settings.set("rank-stroke-weight",25,write)&&settings.rankStrokeWeight==25);
 assert(settings.set("nameplate",2,write)&&settings.set("name-case",2,write));
 assert(settings.set("interface-font",3,write)&&settings.interfaceFont==3&&stored=="Appearance/InterfaceFont=3");
 assert(settings.set("card-name-font",2,write)&&settings.cardNameFont==2&&stored=="Appearance/CardNameFont=2");
 assert(settings.set("card-value-font",4,write)&&settings.cardValueFont==4&&stored=="Appearance/CardValueFont=4");
 assert(defaults.theme==0&&defaults.boardSurface==0&&defaults.boardTexture==10);
 assert(settings.set("theme",2,write)&&settings.theme==2&&stored=="Appearance/Theme=2");
 assert(settings.set("board-surface",6,write)&&settings.boardSurface==6&&stored=="Appearance/BoardSurface=6");
 assert(settings.set("board-texture",37,write)&&settings.boardTexture==37&&stored=="Appearance/BoardTexture=37");
 assert(!settings.set("theme",1,failed)&&settings.theme==2);
 for(unsigned theme=0;theme<ttcg::interfaceThemes.size();++theme){
  auto roundTrip=settings;
  assert(roundTrip.set("theme",theme,write)&&roundTrip.theme==theme);
  roundTrip.load([&](const char*,const char* key,int fallback){return std::string_view(key)=="Theme"?int(theme):fallback;});
  assert(roundTrip.theme==theme);
 }
 // Existing theme IDs remain stable across upgrades.
 assert(ttcg::interfaceThemes[2].id=="oxblood"&&ttcg::interfaceThemes[4].id=="umber");
 assert(ttcg::interfaceThemes[5].id=="oathvein"&&ttcg::interfaceThemes[6].id=="veldun"&&ttcg::interfaceThemes[7].id=="norden");
 assert(!settings.set("theme",ttcg::interfaceThemeCount,write));
 assert(settings.resetRankFinish(write));
 assert(settings.rankShadowOpacity==100&&!settings.rankStroke&&settings.rankStrokeWeight==10);
 assert(settings.nameplateStyle==2&&settings.nameCase==2); // Reset only affects values.
 assert(settings.interfaceFont==3&&settings.cardNameFont==2&&settings.cardValueFont==4);
 assert(settings.theme==2&&settings.boardSurface==6&&settings.boardTexture==37);
 assert(!settings.resetRankFinish(failed));
 assert(settings.set("match-art-treatment",3,write)&&settings.opponentTreatment==3);
 assert(settings.set("match-art-scope",1,write)&&settings.opponentScope==1);
 assert(settings.set("match-palette",4,write)&&settings.controlPalette==4);
 assert(settings.set("match-player-colour",0x123456,write)&&settings.playerColour==0x123456);
 assert(!settings.set("match-player-colour",0xabcdef,failed)&&settings.playerColour==0x123456);
 assert(!settings.set("match-opponent-colour",0x1000000,write));
 assert(settings.set("match-control-marker",1,write)&&settings.controlMarker);
 assert(!settings.resetMatchAppearance(failed)&&settings.controlMarker&&settings.opponentTreatment==3);
 assert(settings.resetMatchAppearance(write));
 for(const auto& f:ttcg::InterfaceSettings::fields)if(f.command.starts_with("match-"))assert(settings.get(f)==f.defaultValue);
 assert(settings.theme==2&&settings.boardTexture==37&&settings.nameplateStyle==2);
 settings.load([](const char*,const char* key,int fallback){return std::string_view(key)=="PlayerColour"?0x102030:fallback;});
 assert(settings.playerColour==0x102030&&settings.opponentColour==0xdf9585);
 assert(defaults.vanillaVoicesOnly);
 for(auto plugin:{"Skyrim.esm","Update.esm","DAWNGUARD.ESM","HearthFires.esm","Dragonborn.esm"})assert(ttcg::residentVoiceAllowed(plugin,false,true));
 for(auto plugin:{"CustomCompanion.esp","CustomVoices.esl","skyrim-voices.esm",""}){
  assert(!ttcg::residentVoiceAllowed(plugin,false,true));
  assert(ttcg::residentVoiceAllowed(plugin,true,true)); // Authored custom voices are explicitly supported.
  assert(ttcg::residentVoiceAllowed(plugin,false,false));
 }
 ttcg::HotkeyBindings keys;
 assert(keys.collection==65&&keys.challenge==66&&!keys.toggleGames);
 assert(keys.conflict("toggleGames",65)=="Album"&&keys.conflict("collection",66)=="Challenge");
 keys.toggleGames=64;assert(keys.conflict("challenge",64)=="Enable / disable games");
 assert(keys.conflict("toggleGames",64).empty()&&keys.conflict("collection",0).empty());
 keys.collection=keys.challenge=keys.toggleGames=65;keys.normalize();
 assert(keys.collection==65&&!keys.challenge&&!keys.toggleGames);
 keys={0,66,66};keys.normalize();assert(!keys.collection&&keys.challenge==66&&!keys.toggleGames);
 for(auto key:{1u,29u,42u,54u,56u,157u,184u,219u,220u,256u,unsigned(-1)}){
  assert(!ttcg::validHotkey(key));keys={key,0,0};keys.normalize();assert(!keys.collection);
 }
 std::cout<<"Interface settings: persisted appearance bounds, failures, reset, vanilla/authored voice policy and hotkeys passed\n";
}
