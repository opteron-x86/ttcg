#pragma once
#include "Collection.h"
#include <limits>
namespace ttcg {
inline constexpr unsigned shopRestockHours=48;
inline constexpr int shopCopyLimit=1000000, shopQuantityLimit=99;
inline bool specialistDealer(unsigned base){return base==0x1413A||base==0x1329A;} // Revyn Sadri, Sayma
inline int shopPrice(CardID id){
 const auto i=cardIndex(id);if(i<0)return 0;
 constexpr int base[]{0,25,45,75,125,200,325,500,800,1250,2000};
 const auto& c=cards[i];if(c.tier<1||c.tier>10)return 0;
 const std::string_view rarity=c.rarity;
 return base[c.tier]*(rarity=="Legendary"?18:rarity=="Epic"?9:rarity=="Rare"?3:1)*(isFoil(id)?3:1);
}
inline int shopSalePrice(CardID id){return shopPrice(id)/5;}
inline bool shopCard(CardID id){const int i=cardIndex(id);return i>=0&&!cards[i].pending;}
inline bool regionalShopCard(const Card& c,unsigned hold){
 constexpr std::array<std::string_view,11> first{"","Nord","Beast","Imperial","Spirit","Nord","Khajiit","High Elf","Dark Elf","Construct","Dark Elf"};
 constexpr std::array<std::string_view,11> second{"","Imperial","Spirit","Breton","Undead","Beast","Argonian","Daedra","Nord","Orc","Daedra"};
 return hold<first.size()&&hold>0&&(std::string_view(c.groups).find(first[hold])!=std::string_view::npos||std::string_view(c.groups).find(second[hold])!=std::string_view::npos);
}
inline bool refreshShop(MerchantStock& m,unsigned key,unsigned hour,unsigned hold,bool specialist){
 if(m.generation&&(hour<m.restockAt||m.goldCredit))return false;
 // Player-sold uniques are never generated and never expire.
 std::erase_if(m.resale,[](auto item){const int i=cardIndex(item.first);return item.second<=0||i<0||!cards[i].unique;});
 m.stock.clear();++m.generation;if(!m.generation)m.generation=1;
 m.restockAt=std::min(100000000u,hour+shopRestockHours);
 std::mt19937 rng(key^(m.generation*2654435761u)^0x43415244u);
 std::set<CardID> selected;
 for(int slot=0;slot<(specialist?14:10);++slot){
  const auto roll=rng()%1000;
  const int tier=specialist?(roll<10?1:roll<40?2:roll<100?3:roll<350?4:roll<650?5:roll<900?6:7):
   (roll<550?1:roll<800?2:roll<950?3:roll<980?4:roll<990?5:roll<997?6:7);
  const auto r=rng()%100;const std::string_view rarity=specialist?(r<35?"Common":r<80?"Rare":"Epic"):(r<70?"Common":r<95?"Rare":"Epic");
  std::vector<CardID> pool;
  for(const auto& c:cards)if(c.available&&!c.pending&&!c.unique&&c.tier==tier&&std::string_view(c.rarity)==rarity&&!selected.contains(c.form)){
   pool.push_back(c.form);if(regionalShopCard(c,hold))pool.push_back(c.form);
  }
  if(pool.empty())continue;
  const auto id=pool[rng()%pool.size()];selected.insert(id);
  m.stock[id|(rng()%1000<(specialist?20:5)?foilFlag:0)]=1;
 }
 return true;
}
inline int shopCount(const MerchantStock& m,CardID id){return count(m.stock,id)+count(m.resale,id);}
inline int shopSurplus(const CollectionSave& s,CardID id){return !shopCard(id)||isFoil(id)||cards[cardIndex(id)].unique?0:std::max(0,count(s.player,id)-std::max(2,deckCopiesNeeded(s,id)));}
enum ShopWarning:unsigned{ShopUnique=1,ShopFoil=2,ShopDeck=4,ShopLastHand=8};
inline int shopLastHandAt(const CollectionSave& s,CardID id){
 const int playable=playableCount(s.player),copies=count(s.player,baseCardID(id))+count(s.player,baseCardID(id)|foilFlag);
 const int needed=5-(playable-std::min(copies,deckLimit(id)));
 return playable>=5&&needed>0?copies-needed+1:0;
}
inline unsigned shopWarnings(const CollectionSave& s,CardID id,int quantity){
 if(!shopCard(id)||quantity<1)return 0;
 unsigned result=(cards[cardIndex(id)].unique?ShopUnique:0u)|(isFoil(id)?ShopFoil:0u);
 if(count(s.player,id)-quantity<deckCopiesNeeded(s,id))result|=ShopDeck;
 const int lastHand=shopLastHandAt(s,id);if(lastHand&&quantity>=lastHand)result|=ShopLastHand;
 return result;
}
struct ShopVisit {Stock buyback;}; // Cleared on leaving or loading a save.
enum class ShopAction{Buy,Sell,Buyback};
enum class ShopResult{Done,Invalid,Unavailable,PlayerGold,MerchantGold,Confirm,Pending};
inline const char* shopMessage(ShopResult r){
 switch(r){case ShopResult::Done:return "";case ShopResult::PlayerGold:return "You don't have enough gold.";case ShopResult::MerchantGold:return "The merchant doesn't have enough gold.";
 case ShopResult::Confirm:return "Confirm this sale first.";case ShopResult::Pending:return "Payment is pending. Try again.";
 case ShopResult::Unavailable:return "That card is no longer available.";default:return "That trade is unavailable.";}
}
template<class Bank> bool recoverShop(CollectionSave& s,MerchantStock& m,Bank& bank){
 if(s.goldCredit>0)s.goldCredit-=std::clamp(bank.change(0,s.goldCredit),0,s.goldCredit);
 if(m.goldCredit>0)m.goldCredit-=std::clamp(bank.change(1,m.goldCredit),0,m.goldCredit);
 return !s.goldCredit&&!m.goldCredit;
}
template<class Bank> ShopResult tradeCard(CollectionSave& s,MerchantStock& m,ShopVisit& visit,Bank& bank,ShopAction action,CardID id,int quantity,bool confirmed=false){
 if(s.contract.pending()||s.goldCredit||m.goldCredit)return ShopResult::Pending;
 if(!shopCard(id)||quantity<1||quantity>shopQuantityLimit)return ShopResult::Invalid;
 const bool selling=action==ShopAction::Sell,undo=action==ShopAction::Buyback;
 const int unit=selling||undo?shopSalePrice(id):shopPrice(id),total=unit*quantity;
 if(unit<=0||total>shopCopyLimit)return ShopResult::Invalid;
 if(selling){
  if(count(s.player,id)<quantity||shopCount(m,id)>shopCopyLimit-quantity)return ShopResult::Unavailable;
  if(!confirmed&&shopWarnings(s,id,quantity))return ShopResult::Confirm;
 }else if((undo?std::min(count(visit.buyback,id),count(m.resale,id)):shopCount(m,id))<quantity||count(s.player,id)>shopCopyLimit-quantity)return ShopResult::Unavailable;
 const int payer=selling?1:0,payee=1-payer;
 if(bank.count(payee)>std::numeric_limits<int>::max()-total)return ShopResult::Invalid;
 if(bank.count(payer)<total)return selling?ShopResult::MerchantGold:ShopResult::PlayerGold;
 // Reserve payment before transferring a virtual copy. A failed partial debit
 // is refunded; any undelivered gold is saved and retried without another card.
 const int paid=std::clamp(-bank.change(payer,-total),0,total);
 auto& refund=payer==0?s.goldCredit:m.goldCredit;
 if(paid!=total){refund+=paid-std::clamp(bank.change(payer,paid),0,paid);return ShopResult::Pending;}
 if(selling){s.player[id]-=quantity;m.resale[id]+=quantity;visit.buyback[id]+=quantity;retainOwnedDecks(s);}
 else{
  const int returned=std::min(quantity,count(m.resale,id));m.resale[id]-=returned;m.stock[id]-=quantity-returned;
  visit.buyback[id]=std::max(0,count(visit.buyback,id)-returned);s.player[id]+=quantity;s.discovered[id]=s.discovered[baseCardID(id)]=1;
 }
 auto& credit=payee==0?s.goldCredit:m.goldCredit;
 credit+=total-std::clamp(bank.change(payee,total),0,total);
 return ShopResult::Done;
}
}
