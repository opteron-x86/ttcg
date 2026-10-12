#pragma once
#include "Match.h"
#include "AlbumView.h"
#include <cctype>
#include <string>
#include <string_view>
#include <numeric>
namespace preview {
inline std::string lower(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
inline int findCard(std::string_view name){for(int i=0;i<int(ttcg::cards.size());++i)if(name==ttcg::cards[i].name)return i;return 0;}
struct Filter {
 std::string text,set; int affinity=-1; int group=-1,tier=0,rarity=0,ownership=0,sort=0,finish=0;
 bool operator==(const Filter&)const=default;
 bool matches(const CardView& c,bool foil=false,bool live=false)const{
  const char* rarities[]{"All rarities","Common","Rare","Epic","Legendary"};
  if(group>=0&&!(c.groups&(1ull<<group)))return false;
  if(tier&&c.tier!=tier)return false;
  if(rarity&&std::string_view(c.rarity)!=rarities[rarity])return false;
  if(live){
   if(ownership==0&&c.owned[foil?1:0]<=0)return false;
   if(ownership==1&&!c.known[foil?1:0]&&c.owned[foil?1:0]<=0)return false;
   if(!c.available&&!c.known[foil?1:0]&&c.owned[foil?1:0]<=0)return false;
   if((finish==1&&foil)||(finish==2&&!foil))return false;
  }
  return text.empty()||lower(c.name).find(lower(text))!=std::string::npos;
 }
 std::vector<int> results(const std::vector<CardView>& cards,bool live=false)const{
  std::vector<int> out;const int n=int(cards.size());
  for(int i=0;i<n*(live?2:1);++i)if(matches(cards[i%n],i>=n,live))out.push_back(i);
  std::stable_sort(out.begin(),out.end(),[&](int a,int b){const auto& x=cards[a%n];const auto& y=cards[b%n];
   if(sort==2&&x.name!=y.name)return x.name<y.name;
   if(sort==3&&x.owned[a/n]!=y.owned[b/n])return x.owned[a/n]>y.owned[b/n];
   if(x.tier!=y.tier)return sort==1?x.tier>y.tier:x.tier<y.tier;
   if(x.name!=y.name)return x.name<y.name;return a<b;
  });return out;
 }
 std::vector<int> results()const{return results(baseCatalog());}
};
}
