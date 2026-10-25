"""Exercise the production reset inventory cleanup with a small engine host."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/NativeCollection.h').read_text()
begin = source.index('inline bool removeResetItems()')
cleanup = source[begin:source.index('inline void acknowledgeTournamentCourier', begin)]
code = r'''
#include <array>
#include <cassert>
#include <map>
#include <string_view>
namespace RE {
struct TESBoundObject {unsigned id;};
struct TESObjectBOOK:TESBoundObject {};
struct TESObjectREFR {std::map<unsigned,int> items;};
inline TESObjectREFR player,container,courier;
struct PlayerCharacter {static TESObjectREFR* GetSingleton(){return &player;}};
struct TESDataHandler {
 std::map<unsigned,TESObjectBOOK> books;
 static TESDataHandler* GetSingleton(){static TESDataHandler data;return &data;}
 template<class T>T* LookupForm(unsigned id,std::string_view){auto it=books.find(id);return it==books.end()?nullptr:&it->second;}
};
}
namespace campaign {
inline const char* plugin="Tessera TCG.esp";
inline RE::TESBoundObject pack{0x840},tournamentPack{0xFD6},album{0x870},gold{0xF};
inline auto* packForm=&pack;inline auto* tournamentPackForm=&tournamentPack;
inline unsigned reject=0;
template<class T>T* resolveID(unsigned id){return id==0x39FB9?&RE::container:id==0x39FB7?&RE::courier:nullptr;}
int physicalCount(RE::TESObjectREFR* owner,RE::TESBoundObject* item){return owner->items[item->id];}
int physicalChange(RE::TESObjectREFR* owner,RE::TESBoundObject* item,int amount){if(item->id==reject)return 0;assert(amount==-owner->items[item->id]);owner->items[item->id]+=amount;return amount;}
CLEANUP
}
int main(){
 using namespace campaign;
 auto seed=[](){
  RE::player.items={{pack.id,4},{tournamentPack.id,2},{album.id,1},{gold.id,1234},{0x777,3}};
  RE::container.items={{0x777,1}};RE::courier.items={{0x777,2}};
  for(unsigned id=0xF80;id<0xF80+30;++id){RE::TESDataHandler::GetSingleton()->books[id]={{id}};RE::player.items[id]=3;RE::container.items[id]=2;RE::courier.items[id]=1;}
 };
 seed();assert(removeResetItems());
 assert(RE::player.items[pack.id]==0&&RE::player.items[tournamentPack.id]==0);
 assert(RE::player.items[album.id]==1&&RE::player.items[gold.id]==1234&&RE::player.items[0x777]==3);
 assert(RE::container.items[0x777]==1&&RE::courier.items[0x777]==2);
 for(unsigned id=0xF80;id<0xF80+30;++id)assert(!RE::player.items[id]&&!RE::container.items[id]&&!RE::courier.items[id]);
 assert(removeResetItems());
 seed();reject=pack.id;assert(!removeResetItems());assert(RE::player.items[pack.id]==4&&RE::player.items[album.id]==1&&RE::player.items[gold.id]==1234);
}
'''.replace('CLEANUP', cleanup)
with tempfile.TemporaryDirectory(prefix='tessera-reset-') as directory:
    cpp = Path(directory) / 'reset.cpp'
    exe = Path(directory) / 'reset'
    cpp.write_text(code)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Reset inventory: carried packs and all invitation copies removed; album, gold and unrelated items preserved; failed removals reported')
