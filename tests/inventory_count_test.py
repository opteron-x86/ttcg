"""Run the production inventory counters against a reference that records being set up."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/NativeCollection.h').read_text()
begin = source.index('inline int physicalCount(')
counters = source[begin:source.index('inline bool authoredKey(', begin)]
shop = (root / 'include/NativeShop.h').read_text()
begin = shop.index('struct ShopBank {')
bank = shop[begin:shop.index('bool openShop(', begin)]
code = r"""
#include <algorithm>
#include <array>
#include <cassert>
#include <map>
namespace RE {
enum class ITEM_REMOVE_REASON {kRemove};
struct TESBoundObject {int id;};
// A reference whose inventory exists only once something sets it up, as a merchant
// chest's does. Setting it up rolls its leveled lists: here, 300 gold appears.
struct TESObjectREFR {
 std::map<TESBoundObject*,int> stock;
 TESBoundObject* leveledGold{};
 bool ready=false;unsigned setups=0;
 void setUp(){if(!ready){ready=true;++setups;if(leveledGold)stock[leveledGold]+=300;}}
 template<class Filter>std::map<TESBoundObject*,int> GetInventoryCounts(Filter filter,bool noInit=false){
  if(!noInit)setUp();
  std::map<TESBoundObject*,int> result;
  if(ready)for(auto [object,count]:stock)if(filter(*object))result[object]=count;
  return result;
 }
 // The game sets an inventory up itself when something is put in or taken out.
 void AddObjectToContainer(TESBoundObject* object,void*,int count,void*){setUp();stock[object]+=count;}
 void RemoveItem(TESBoundObject* object,int count,ITEM_REMOVE_REASON,void*,void*){setUp();stock[object]-=count;}
};
inline TESObjectREFR player;
struct PlayerCharacter {static TESObjectREFR* GetSingleton(){return &player;}};
}
namespace campaign {
inline RE::TESBoundObject gold{0xF},letter{0xF80};
inline auto* goldForm=&gold;
inline RE::TESObjectREFR chest;
template<class T>T* resolveID(unsigned){return &chest;}
COUNTERS
BANK
}
int main(){
 using namespace campaign;
 RE::TESObjectREFR courier;
 courier.leveledGold=&gold;
 // Looking for a letter in a container nobody has opened finds none, and leaves it unopened.
 for(int poll=0;poll<50;++poll)assert(physicalCount(&courier,&letter)==0);
 assert(!courier.ready&&courier.setups==0);
 assert(physicalCount(nullptr,&letter)==0&&physicalCount(&courier,nullptr)==0&&physicalChange(&courier,&letter,0)==0);
 // Putting something in is the game's doing; the change is still reported exactly.
 assert(physicalChange(&courier,&letter,2)==2&&courier.setups==1&&physicalCount(&courier,&letter)==2);
 assert(physicalChange(&courier,&letter,-5)==-2&&physicalCount(&courier,&letter)==0);
 // A resident who was never loaded is paid without their purse being counted first.
 RE::TESObjectREFR resident;
 resident.leveledGold=&gold;
 assert(physicalChange(&resident,&gold,400)>=400&&resident.stock[&gold]==700&&resident.setups==1);
 // A merchant the player is trading with has real gold: its chest is set up, once.
 chest.leveledGold=&gold;
 RE::player.ready=true;
 RE::player.stock[&gold]=1000;
 ShopBank trade(1);
 assert(trade.count(1)==300&&chest.setups==1);
 assert(trade.change(1,50)==50&&trade.change(0,-50)==-50&&trade.count(1)==350&&trade.count(0)==950);
 assert(chest.setups==1&&RE::player.setups==0);
}
""".replace('COUNTERS', counters).replace('BANK', bank)
with tempfile.TemporaryDirectory(prefix='tessera-count-') as directory:
    cpp = Path(directory) / 'count.cpp'
    exe = Path(directory) / 'count'
    cpp.write_text(code)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Inventory counts: unopened containers stay unopened; a merchant in trade is set up once')
