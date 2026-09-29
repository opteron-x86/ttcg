#pragma once
#include "Shop.h"
#include "Presentation.h"
namespace ttcg {
inline std::string shopJson(const CollectionSave& s,const MerchantStock& m,const ShopVisit& visit,std::string_view name,bool specialist,int gold,int merchantGold,std::string_view notice={}){
 std::string out="{\"name\":"+quote(name)+",\"specialist\":"+(specialist?"true":"false")+",\"gold\":"+std::to_string(gold)+",\"merchantGold\":"+std::to_string(merchantGold)+",\"pending\":"+(s.goldCredit||m.goldCredit?"true":"false")+",\"notice\":"+quote(notice)+",\"buy\":[";
 bool first=true;Stock offered=m.stock;for(auto [id,n]:m.resale)offered[id]+=n;
 for(auto [id,n]:offered)if(n>0&&shopCard(id)){
  if(!first)out+=',';first=false;
  out+="{\"id\":"+std::to_string(displayIndex(id))+",\"count\":"+std::to_string(n)+",\"price\":"+std::to_string(shopPrice(id))+",\"buyback\":"+std::to_string(std::min(count(visit.buyback,id),count(m.resale,id)))+",\"salePrice\":"+std::to_string(shopSalePrice(id))+"}";
 }
 out+="],\"sell\":[";first=true;
 for(auto [id,n]:s.player)if(n>0&&shopCard(id)){
  if(!first)out+=',';first=false;
  out+="{\"id\":"+std::to_string(displayIndex(id))+",\"count\":"+std::to_string(n)+",\"price\":"+std::to_string(shopSalePrice(id))+",\"surplus\":"+std::to_string(shopSurplus(s,id))+",\"reserved\":"+std::to_string(deckCopiesNeeded(s,id))+",\"warnings\":"+std::to_string(shopWarnings(s,id,1))+",\"lastHandAt\":"+std::to_string(shopLastHandAt(s,id))+"}";
 }
 return out+"]}";
}
}
