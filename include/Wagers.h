#pragma once
#include <algorithm>
#include <array>
namespace ttcg {
inline constexpr int maximumWager=2000,maximumBettingGold=1000000000;
inline constexpr std::array<int,11> wagerAmounts{0,5,10,25,50,100,200,250,500,1000,2000};
inline bool offeredWager(int amount){return std::find(wagerAmounts.begin(),wagerAmounts.end(),amount)!=wagerAmounts.end();}
struct BettingPurse {int gold=0;unsigned restockAt=0;};
inline void refreshBettingPurse(BettingPurse& purse,int cap,unsigned hour){
 if(cap<0||cap>maximumWager||hour>100000000)return;
 if(!purse.restockAt||hour>=purse.restockAt){
  purse.gold=std::max(purse.gold,cap*3);
  purse.restockAt=hour+72;
 }
}
inline int changeBettingPurse(BettingPurse& purse,int amount){
 const int moved=std::clamp(amount,-purse.gold,maximumBettingGold-purse.gold);
 purse.gold+=moved;return moved;
}
}
