// Exercise the production discovery/patching algorithm with an in-memory form
// graph. Engine parsing and inventory reset behavior still need Skyrim testing.
#include <cassert>
#include <map>
#include <set>
#include <vector>
#include <cstdint>
#include <string_view>
#include <type_traits>
namespace SKSE::log {template<class... T>void debug(const char*,T...){} template<class... T>void info(const char*,T...){} template<class... T>void warn(const char*,T...){} template<class... T>void error(const char*,T...){} }
namespace RE {
struct TESForm {unsigned id=0;virtual ~TESForm()=default;static inline std::map<unsigned,TESForm*> forms;unsigned GetFormID(){return id;}template<class T>T* As(){return dynamic_cast<T*>(this);}template<class T>static T* LookupByID(unsigned id){return dynamic_cast<T*>(forms[id]);}};
struct TESBoundObject:TESForm{};
struct BGSKeyword:TESForm{};
struct BGSListForm:TESForm {std::vector<TESForm*> forms;bool HasForm(TESForm* f){for(auto p:forms)if(p==f)return true;return false;}};
struct TESLevItem:TESForm{};
struct TESObjectCONT:TESForm {std::map<TESForm*,int> objects;int GetObjectCount(TESForm* p){return objects[p];}bool AddObjectToContainer(TESForm* p,int n,void*){objects[p]+=n;return true;}};
struct TESObjectREFR:TESForm {
 TESForm* base{};TESForm* GetBaseObject(){return base;}
 std::map<TESBoundObject*,int> stock;
 bool inventoryInitialized=false;
 unsigned inventoryInitializations=0;
 template<class Filter>auto GetInventoryCounts(Filter filter,bool noInit=false){
  if(!noInit&&!inventoryInitialized){inventoryInitialized=true;++inventoryInitializations;}
  std::map<TESBoundObject*,int> result;
  if(inventoryInitialized)for(auto [object,count]:stock)if(filter(*object))result[object]=count;
  return result;
 }
};
struct TESFaction:TESForm {bool vendor=true;bool IsVendor(){return vendor;}struct Data {struct Values {bool notBuySell=false;} vendorValues;TESObjectREFR* merchantContainer{};BGSListForm* vendorSellBuyList{};} vendorData;};
struct TESNPC:TESForm {struct Rank {TESFaction* faction;int rank;};std::vector<Rank> factions;bool IsInFaction(TESFaction* f){for(auto r:factions)if(r.faction==f&&r.rank>=0)return true;return false;}};
struct Actor {std::set<TESFaction*> factions;bool IsInFaction(TESFaction* f){return factions.contains(f);}};
struct TESDataHandler {static TESDataHandler* GetSingleton(){static TESDataHandler d;return &d;}std::vector<TESFaction*> factions;std::vector<TESNPC*> npcs;template<class T>auto& GetFormArray(){if constexpr(std::is_same_v<T,TESNPC>)return npcs;else return factions;}template<class T>T* LookupForm(unsigned id,const char*){return TESForm::LookupByID<T>(id);}};
}
#include "VendorDistribution.h"
int main(int argc,char** argv){
 assert(argc==1||(argc==2&&std::string_view(argv[1])=="--no-pack-injection"));
 const bool distributePacks=argc==1;
 const int expectedPacks=distributePacks?1:0;
 using namespace RE;
 BGSKeyword categories[4],pack;unsigned ids[]={0x8F958,0x8F959,0x937A2,0x914E9};
 for(int i=0;i<4;++i)TESForm::forms[ids[i]]=&categories[i];
 BGSListForm exclusion,inclusion,food;for(auto& c:categories)inclusion.forms.push_back(&c);
 TESObjectCONT general,modded,inn,shared;TESObjectREFR refs[4];refs[0].base=&general;refs[1].base=&modded;refs[2].base=&inn;refs[3].base=&shared;
 TESFaction f[7];for(int i=0;i<7;++i)f[i].id=0xFE001800+i;
 auto setup=[&](int i,int ref,BGSListForm& list,bool blacklist){f[i].vendorData={ {blacklist},&refs[ref],&list};};
 setup(0,0,exclusion,true);setup(1,0,exclusion,true);setup(2,1,inclusion,false);setup(3,2,food,false);setup(4,3,exclusion,true);setup(5,3,food,false);setup(6,0,exclusion,true);f[6].vendor=false;
 for(auto& faction:f)TESDataHandler::GetSingleton()->factions.push_back(&faction);
 TESLevItem roll;
 TESForm::forms[0x880]=&roll;TESForm::forms[0x881]=&pack;
 TESFaction role,marker;TESForm::forms[0x51599]=&role;TESForm::forms[0x882]=&marker;
 TESNPC regular,modNPC,innNPC;regular.factions.push_back({&role,0});modNPC.factions.push_back({&f[2],0});innNPC.factions.push_back({&f[3],0});
 TESDataHandler::GetSingleton()->npcs={&regular,&modNPC,&innNPC};
 if(distributePacks)vendors::initialize("Fixture.esp");else vendors::initialize("Fixture.esp",false);
 assert(vendors::factions.size()==4);
 assert(general.GetObjectCount(&roll)==expectedPacks&&modded.GetObjectCount(&roll)==expectedPacks);
 assert(inn.GetObjectCount(&roll)==0&&shared.GetObjectCount(&roll)==0);
 assert(inclusion.HasForm(&pack)&&!food.HasForm(&pack));
 assert(modNPC.IsInFaction(&marker)&&!innNPC.IsInFaction(&marker));
 assert(vendors::tagAlbumSellers()==0); // Duplicate initialization is harmless.
 TESNPC later;later.factions.push_back({&f[0],0});TESDataHandler::GetSingleton()->npcs.push_back(&later);
 assert(vendors::tagAlbumSellers()==1&&later.IsInFaction(&marker));
 Actor actor;assert(!vendors::albumEligible(nullptr)&&!vendors::albumEligible(&actor));
 actor.factions.insert(&role);assert(vendors::albumEligible(&actor));
 actor.factions.clear();actor.factions.insert(&f[2]);assert(vendors::albumEligible(&actor));
 actor.factions.clear();actor.factions.insert(&f[3]);assert(!vendors::albumEligible(&actor));
 actor.factions.clear();actor.factions.insert(&marker);assert(!vendors::albumEligible(&actor)); // Callback revalidates real seller membership.
 TESBoundObject album,otherItem;
 unsigned removals=0;
 auto change=[&](TESObjectREFR* ref,int n){assert(n<0);assert(ref->inventoryInitialized);++removals;ref->stock[&album]+=n;return n;};
 // First installation and repeated reloads must not initialize merchant stock.
 for(unsigned reload=0;reload<3;++reload){
  assert(vendors::removeLegacyAlbums(&album,change)==0);
  for(const auto& ref:refs)assert(!ref.inventoryInitialized&&ref.inventoryInitializations==0);
 }
 assert(removals==0);
 // Existing saves retain their album migration, without touching other stock.
 for(unsigned n=0;n<4;++n){refs[n].inventoryInitialized=true;refs[n].stock[&album]=int(n)+1;refs[n].stock[&otherItem]=9;}
 assert(vendors::removeLegacyAlbums(&album,change)==2&&removals==2);
 assert(refs[0].stock[&album]==0&&refs[1].stock[&album]==0&&refs[2].stock[&album]==3&&refs[3].stock[&album]==4);
 assert(vendors::removeLegacyAlbums(&album,change)==0&&removals==2);
 for(const auto& ref:refs)assert(ref.stock.at(&otherItem)==9&&ref.inventoryInitializations==0);
 refs[0].stock[&album]=-1;refs[1].stock.erase(&album);
 assert(vendors::removeLegacyAlbums(&album,change)==0&&removals==2);
 refs[0].stock[&album]=2;
 assert(vendors::removeLegacyAlbums(&album,[](TESObjectREFR*,int){return 0;})==0&&refs[0].stock[&album]==2);
 assert(vendors::removeLegacyAlbums(nullptr,change)==0&&removals==2);
 vendors::albumContainers.push_back(nullptr);
 assert(vendors::removeLegacyAlbums(&album,change)==1&&removals==3);
 // Distribution is fixed at startup; later initialization cannot switch modes.
 vendors::initialize("Fixture.esp",!distributePacks);
 assert(vendors::factions.size()==4);
 assert(general.GetObjectCount(&roll)==expectedPacks&&modded.GetObjectCount(&roll)==expectedPacks);
 assert(inclusion.HasForm(&pack)&&modNPC.IsInFaction(&marker));
}
