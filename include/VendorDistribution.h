#pragma once
#include <algorithm>
#include <set>
#include <vector>
namespace vendors {
inline std::vector<RE::TESFaction*> factions;
inline std::vector<RE::TESObjectREFR*> albumContainers;
inline RE::TESFaction* generalGoodsRole{};
inline RE::TESFaction* albumSeller{};
// The vanilla role is a static dialogue fallback; mod service factions are
// discovered from their buying categories and receive one dialogue marker.
inline bool albumEligible(RE::Actor* actor){
 if(!actor)return false;
 if(generalGoodsRole&&actor->IsInFaction(generalGoodsRole))return true;
 for(auto faction:factions)if(actor->IsInFaction(faction))return true;
 return false;
}
inline unsigned tagAlbumSellers(){
 if(!albumSeller)return 0;
 unsigned count=0;
 for(auto npc:RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()){
  if(!npc||npc->IsInFaction(albumSeller))continue;
  for(auto faction:factions)if(npc->IsInFaction(faction)){
   npc->factions.push_back({albumSeller,0});++count;break;
  }
 }
 return count;
}
// Remove stock injected by versions 0.23.1/0.23.2 when loading an existing save.
// Albums are now sold exclusively by dialogue; never remove the player's book.
template<class Count,class Change> unsigned removeLegacyAlbums(Count count,Change change){
 unsigned changed=0;
 for(auto container:albumContainers){const int have=count(container);if(have>0&&change(container,-have))++changed;}
 return changed;
}
// Classify service factions by their actual buying categories, including mod
// factions. General goods must accept weapons, armor, books and household goods.
// This excludes food-only inns and specialist shops without naming any NPC.
inline bool generalGoods(RE::TESFaction* faction) {
 if(!faction||!faction->IsVendor())return false;
 const auto& data=faction->vendorData;
 if(!data.merchantContainer||!data.vendorSellBuyList)return false;
 for(auto id:{0x8F958u,0x8F959u,0x937A2u,0x914E9u}){
  auto keyword=RE::TESForm::LookupByID<RE::BGSKeyword>(id);
  if(!keyword)return false;
  const bool listed=data.vendorSellBuyList->HasForm(keyword);
  if(data.vendorValues.notBuySell?listed:!listed)return false;
 }
 return true;
}
inline bool eligible(RE::Actor* actor){
 if(!actor)return false;
 for(auto faction:factions)if(actor->IsInFaction(faction))return true;
 return false;
}
inline void initialize(const char* plugin){
 static bool initialized=false;if(initialized)return;
 auto data=RE::TESDataHandler::GetSingleton();
 generalGoodsRole=RE::TESForm::LookupByID<RE::TESFaction>(0x51599);
 albumSeller=data->LookupForm<RE::TESFaction>(0x882,plugin);
 auto roll=data->LookupForm<RE::TESLevItem>(0x880,plugin);
 auto keyword=data->LookupForm<RE::BGSKeyword>(0x881,plugin);
 if(!roll||!keyword){SKSE::log::error("Missing vendor pack records");return;}
 std::set<RE::TESObjectCONT*> generalContainers,otherContainers;
 std::set<RE::TESObjectREFR*> generalReferences,otherReferences;
 for(auto faction:data->GetFormArray<RE::TESFaction>()){
  if(!faction||!faction->IsVendor())continue;
  const bool general=generalGoods(faction);
  auto ref=faction->vendorData.merchantContainer;
  auto object=ref?ref->GetBaseObject():nullptr;
  auto base=object?object->As<RE::TESObjectCONT>():nullptr;
  if(general){factions.push_back(faction);generalReferences.insert(ref);if(base)generalContainers.insert(base);}
  else {if(ref)otherReferences.insert(ref);if(base)otherContainers.insert(base);}
 }
 for(auto ref:generalReferences)if(!otherReferences.contains(ref))albumContainers.push_back(ref);
 for(auto faction:factions){
  // Inclusion lists need the pack category; blacklist vendors already accept it.
  auto& v=faction->vendorData;
  if(!v.vendorValues.notBuySell&&!v.vendorSellBuyList->HasForm(keyword))v.vendorSellBuyList->forms.push_back(keyword);
  SKSE::log::info("General-goods faction: {:08X}",faction->GetFormID());
 }
 unsigned count=0;
 for(auto base:generalContainers){
  // Base inventories are shared by all references. Avoid stocking an inn or
  // specialist when a mod reuses its chest base for a general merchant.
  if(otherContainers.contains(base)){SKSE::log::warn("Shared vendor container {:08X}: pack injection skipped",base->GetFormID());continue;}
  if(base->GetObjectCount(roll)==0&&base->AddObjectToContainer(roll,1,nullptr))++count;
 }
 const auto tagged=tagAlbumSellers();
 SKSE::log::info("Album dialogue: general-goods role {}, marker {}, tagged {} NPC bases",generalGoodsRole!=nullptr,albumSeller!=nullptr,tagged);
 initialized=true;
 SKSE::log::info("Album sellers: {} service factions; packs added to {} container bases",factions.size(),count);
}
}
