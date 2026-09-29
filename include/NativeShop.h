#pragma once
#include "ShopPresentation.h"
#include <sstream>
namespace campaign {
struct ShopContext {
 unsigned key=0;bool specialist=false;
 ttcg::ShopVisit visit;
 std::string notice;
};
inline ShopContext shop;
inline RE::TESObjectREFR* merchantContainer(RE::Actor* actor){
 if(!actor||!vendors::albumEligible(actor))return nullptr;
 for(auto faction:vendors::factions)if(actor->IsInFaction(faction)&&faction->vendorData.merchantContainer)return faction->vendorData.merchantContainer;
 // The vanilla general-goods role also covers service factions whose stock
 // categories have been changed by another mod.
 for(auto faction:RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESFaction>())
  if(faction&&faction->IsVendor()&&actor->IsInFaction(faction)&&faction->vendorData.merchantContainer)return faction->vendorData.merchantContainer;
 return nullptr;
}
struct ShopBank {
 std::array<RE::TESObjectREFR*,2> actors;
 explicit ShopBank(unsigned key):actors{RE::PlayerCharacter::GetSingleton(),resolveID<RE::TESObjectREFR>(key)}{}
 int count(int side){return physicalCount(actors[side],goldForm);}
 int change(int side,int amount){return physicalChange(actors[side],goldForm,amount);}
};
inline bool openShop(RE::Actor* actor){
 shop={};auto container=merchantContainer(actor);
 if(!available()||!hasAlbum()||!container||saved.contract.pending()||ttcg::pendingTournamentFoils(saved))return false;
 const auto key=persistentID(container);if(!key||(!saved.merchants.contains(key)&&saved.merchants.size()>=1024))return false;
 shop.key=key;shop.specialist=ttcg::specialistDealer(persistentID(actor->GetActorBase()));
 auto& m=saved.merchants[key];ShopBank bank(key);ttcg::recoverShop(saved,m,bank);
 ttcg::refreshShop(m,key,gameHour(),actorHold(actor),shop.specialist);
 if(saved.goldCredit||m.goldCredit)shop.notice=ttcg::shopMessage(ttcg::ShopResult::Pending);
 return true;
}
inline bool shopAvailable(RE::Actor* actor){return shop.key&&available()&&hasAlbum()&&persistentID(merchantContainer(actor))==shop.key&&saved.merchants.contains(shop.key);}
inline std::string shopJson(RE::Actor* actor){
 if(!shop.key||!saved.merchants.contains(shop.key))return "null";
 ShopBank bank(shop.key);return ttcg::shopJson(saved,saved.merchants.at(shop.key),shop.visit,actor?actor->GetName():"Merchant",shop.specialist,bank.count(0),bank.count(1),shop.notice);
}
inline void closeShop(){
 if(shop.key&&saved.merchants.contains(shop.key)){ShopBank bank(shop.key);ttcg::recoverShop(saved,saved.merchants.at(shop.key),bank);}
 shop={};
}
inline bool shopCommand(RE::Actor* actor,std::istringstream& input){
 std::string action,extra;int index=-1,quantity=0,confirm=0;
 if(!(input>>action>>index>>quantity>>confirm)||(input>>extra)||!shopAvailable(actor)||index<0||index>=int(ttcg::cards.size()*2)||confirm<0||confirm>1)return false;
 if(action!="buy"&&action!="sell"&&action!="buyback")return false;
 auto& m=saved.merchants.at(shop.key);ShopBank bank(shop.key);
 // Recovered payments are published before accepting a new transaction.
 if(saved.goldCredit||m.goldCredit){ttcg::recoverShop(saved,m,bank);shop.notice=saved.goldCredit||m.goldCredit?ttcg::shopMessage(ttcg::ShopResult::Pending):"Payment completed. You can trade again.";return true;}
 const auto result=ttcg::tradeCard(saved,m,shop.visit,bank,action=="sell"?ttcg::ShopAction::Sell:action=="buyback"?ttcg::ShopAction::Buyback:ttcg::ShopAction::Buy,ttcg::displayCardID(index),quantity,confirm);
 shop.notice=ttcg::shopMessage(result);
 if(result==ttcg::ShopResult::Done){
  shop.notice=(action=="sell"?"Sold ":action=="buyback"?"Bought back ":"Bought ")+std::string(ttcg::cards[index%ttcg::cards.size()].name);
  if(quantity>1)shop.notice+=" ×"+std::to_string(quantity);
  shop.notice+='.';if(saved.goldCredit||m.goldCredit)shop.notice+=" Payment is pending.";
 }
 return true;
}
}
