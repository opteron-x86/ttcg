#include "InterfaceSettings.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace ttcg;
int main(int argc,char**){
 // The browser tests consume the same packed values used by the native input.
 if(argc>1){
  std::cout<<"{\"leftShiftF7\":"<<(HotkeyLShift|65)<<",\"rightShiftF7\":"<<(HotkeyRShift|65)
   <<",\"controlAltF8\":"<<(HotkeyLCtrl|HotkeyRAlt|66)<<",\"allModifiersF6\":"<<(hotkeyModifierMask|64)<<"}\n";return 0;
 }
 // Each of the 64 side-specific combinations matches only that combination.
 for(unsigned modifiers=0;modifiers<=hotkeyModifierMask;modifiers+=256){
  std::array<bool,256> pressed{};
  for(const auto& m:hotkeyModifierKeys)pressed[m.scan]=bool(modifiers&m.flag);
  const auto observed=heldHotkeyModifiers([&](unsigned key){return pressed[key];});
  assert(observed==modifiers);
  for(auto key:{18u,28u,57u,65u,66u,87u,156u,200u}){
   const auto binding=key|modifiers;assert(validHotkey(binding));
   // The one-integer INI round trip preserves both sides and multiple modifiers.
   assert(std::stoul(std::to_string(binding))==binding);
   assert(hotkeyMatches(binding,key,observed));
   for(unsigned other=0;other<=hotkeyModifierMask;other+=256)assert(hotkeyMatches(binding,key,other)==(other==modifiers));
   assert(!hotkeyMatches(binding,key+1,observed));
   assert(!hotkeyMatches(binding,256+key,observed));
   assert(!hotkeyMatches(binding,key,observed|HotkeyMeta));
  }
  for(auto key:{0u,1u,29u,42u,54u,56u,157u,184u,219u,220u}){
   const auto binding=key|modifiers;assert(validHotkey(binding)==(binding==0));
   assert(!hotkeyMatches(binding,key,observed));
  }
  for(auto meta:{219u,220u}){pressed[meta]=true;assert(heldHotkeyModifiers([&](unsigned key){return pressed[key];})==(modifiers|HotkeyMeta));pressed[meta]=false;}
 }
 // Sharing a primary key is fine; only identical full chords conflict.
 HotkeyBindings keys{65,65|HotkeyLShift,65|HotkeyRShift};keys.normalize();
 assert(keys.collection==65&&keys.challenge==(65|HotkeyLShift)&&keys.toggleGames==(65|HotkeyRShift));
 assert(keys.conflict("collection",65|HotkeyLCtrl).empty());
 assert(keys.conflict("collection",65|HotkeyLShift)=="Challenge");
 keys.toggleGames=keys.challenge;keys.normalize();assert(!keys.toggleGames&&keys.challenge==(65|HotkeyLShift));
 keys={HotkeyLShift,66,HotkeyMeta|65};keys.normalize();assert(!keys.collection&&keys.challenge==66&&!keys.toggleGames);
 assert(!validHotkey(65536|65)&&!validHotkey(unsigned(-1)));
 assert(!hotkeyMatches(65,65,1)&&!hotkeyMatches(0,0,0));
 std::cout<<"Hotkeys: all 64 modifier combinations, native keyboard state, exact matching, reserved keys, persistence and conflicts passed\n";
}
