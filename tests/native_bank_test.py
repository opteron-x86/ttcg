"""Exercise the production form resolver and banks against a small game host."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def section(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


source = (Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'include/NativeCollection.h').read_text()
resolver = section(source, 'template<class T> T* resolveID(', 'inline unsigned tournamentVenue()')
bank = section(source, 'struct Bank {', 'inline bool available()')
shop = section((ROOT / 'include/NativeShop.h').read_text(), 'struct ShopBank {', 'inline bool openShop(')
# Use CommonLib's actual typed lookup. It checks the exact form tag, whereas
# TESForm::As permits Actor -> TESObjectREFR and other valid base conversions.
commonlib = (ROOT / 'build/deps/commonlib/include/RE/T/TESDataHandler.h').read_text()
typed_lookup = section(commonlib, '\ttemplate <class T>\n\tT* TESDataHandler::LookupForm(', '\ttemplate <class T>\n\tT* TESDataHandler::LookupFormRaw(')

code = r'''
#include "Campaign.h"
#include "NpcSyncTiming.h"
#include <cassert>
#include <iostream>
#include <map>
#include <string>
namespace RE {
 using FormID=std::uint32_t;
 enum class FormType { Form, Reference, ActorCharacter, Quest, BoundObject };
 struct TESForm {
  static constexpr auto FORMTYPE=FormType::Form;
  FormType type;unsigned id;
  TESForm(FormType t,unsigned i):type(t),id(i){}
  virtual ~TESForm()=default;
  bool Is(FormType t)const{return type==t;}
  template<class T>T* As(){return dynamic_cast<T*>(this);}
  static TESForm* LookupByID(unsigned);
  template<class T>static T* LookupByID(unsigned id){auto* f=LookupByID(id);return f?f->As<T>():nullptr;}
 };
 struct TESObjectREFR:TESForm {
  static constexpr auto FORMTYPE=FormType::Reference;
  unsigned ledger;int gold=1000;
  TESObjectREFR(unsigned id,unsigned key):TESForm(FORMTYPE,id),ledger(key){}
 };
 struct Actor:TESObjectREFR {
  static constexpr auto FORMTYPE=FormType::ActorCharacter;
  Actor(unsigned id,unsigned key):TESObjectREFR(id,key){type=FORMTYPE;}
 };
 struct TESQuest:TESForm {
  static constexpr auto FORMTYPE=FormType::Quest;
  explicit TESQuest(unsigned id):TESForm(FORMTYPE,id){}
 };
 struct TESBoundObject:TESForm {
  static constexpr auto FORMTYPE=FormType::BoundObject;
  TESBoundObject():TESForm(FORMTYPE,0xF){}
 };
 inline std::map<unsigned,TESForm*> forms;
 TESForm* TESForm::LookupByID(unsigned id){auto it=forms.find(id);return it==forms.end()?nullptr:it->second;}
 struct TESDataHandler {
  std::map<std::pair<std::string,unsigned>,TESForm*> files;
  static TESDataHandler* GetSingleton(){static TESDataHandler data;return &data;}
  TESForm* LookupForm(unsigned local,std::string_view name){auto it=files.find({ttcg::normalizedPlugin(name),local});return it==files.end()?nullptr:it->second;}
  template<class T>T* LookupForm(unsigned local,std::string_view name);
 };
 TYPED_LOOKUP
 inline Actor player{0x14,0};
 struct PlayerCharacter {static Actor* GetSingleton(){return &player;}};
}
namespace campaign {
 inline ttcg::CollectionSave saved;
 inline RE::TESBoundObject goldObject;
 inline RE::TESBoundObject* goldForm=&goldObject;
 unsigned ledgerKey(RE::TESObjectREFR* actor){return actor?actor->ledger:0;}
 int physicalCount(RE::TESObjectREFR* actor,RE::TESBoundObject*){return actor?actor->gold:0;}
 int physicalChange(RE::TESObjectREFR* actor,RE::TESBoundObject*,int amount){if(!actor)return 0;amount=std::max(amount,-actor->gold);actor->gold+=amount;return amount;}
 RESOLVER
 BANK
 SHOP
}
int main(){
 using namespace ttcg;
 auto& data=*RE::TESDataHandler::GetSingleton();
 Hand ours{},theirs{};for(int h=0;h<5;++h){ours[h]=cards[h].form;theirs[h]=cards[h+5].form|(h==0?foilFlag:0);}
 const auto bind=[&](RE::TESForm* form,const WorldForm& identity){RE::forms[form->id]=form;data.files[{normalizedPlugin(identity.plugin),identity.local}]=form;};
 const auto reset=[&](unsigned ledger){
  campaign::saved={};RE::player.gold=1000;
  for(auto id:ours)++campaign::saved.player[id];
  for(auto id:theirs)++campaign::saved.opponents[ledger][id];
  refreshBettingPurse(campaign::saved.bettingPurses[ledger],100,10);
 };
 for(const auto& identity:{WorldForm{"Skyrim.esm",0x13478},WorldForm{"Dragonborn.esm",0x17777},WorldForm{"BSHeartland.esm",0x7459C},WorldForm{"Residents.esl",0x801}}){
  const auto key=worldFormID(identity);
  RE::Actor npc{identity.plugin=="Skyrim.esm"?key:0x2707459Cu,key};bind(&npc,identity);
  assert(RE::TESForm::LookupByID<RE::TESObjectREFR>(npc.id)==&npc);
   // Actor records require a lookup with the matching form type.
  assert(data.LookupForm<RE::TESObjectREFR>(identity.local,normalizedPlugin(identity.plugin))==nullptr);
  assert(data.LookupForm<RE::Actor>(identity.local,normalizedPlugin(identity.plugin))==&npc);
  assert(campaign::resolveID<RE::Actor>(key)==&npc);
  assert(campaign::resolveID<RE::TESQuest>(key)==nullptr);
  for(int wager:{0,100})for(int winner:{-1,0,1}){
   reset(key);auto& saved=campaign::saved;
   // A completed game against somebody else must not select their account.
   saved.contract.opponent=0xDEAD;saved.contract.wagerLedger=0xDEAD;saved.bettingPurses[0xDEAD]={777,82};
   campaign::Bank bank(key);
   assert(validHand(theirs,saved.opponents.at(key)));
   assert(saved.contract.reserve(bank,{ours,theirs},TradeOne,wager,key));
   assert(bank.actors[1]==&npc&&bank.rival==key&&bank.wagerLedger()==key);
   assert(RE::player.gold==1000-wager&&bank.count(1,0)==300-wager&&npc.gold==1000);
   assert(saved.bettingPurses.at(0xDEAD).gold==777);
   CollectionSave restored;assert(decodeCollection(encodeCollection(saved),restored));saved=restored;
   campaign::Bank recovery(key);assert(recovery.wagerLedger()==key);
   assert(saved.contract.settleSelection(recovery,winner,winner<0?0:1));
   assert(RE::player.gold==1000+(winner==0?wager:winner==1?-wager:0));
   assert(recovery.count(1,0)==300+(winner==1?wager:winner==0?-wager:0));
   assert(count(saved.player,theirs[0])==(winner==0?1:0));
   assert(count(saved.opponents.at(key),ours[0])==(winner==1?1:0));
   assert(npc.gold==1000&&saved.bettingPurses.at(0xDEAD).gold==777);
  }
  assert(campaign::resolveID<RE::TESObjectREFR>(key)==&npc);
  // Pre-purse saves must still return physical gold, including modded actors.
  reset(key);auto& saved=campaign::saved;saved.contract.held=true;saved.contract.opponent=key;saved.contract.wager=100;
  saved.contract.hands={ours,theirs};saved.contract.escrow[0][0]=100;saved.contract.escrow[1][0]=100;
  RE::player.gold=npc.gold=900;
  campaign::Bank legacy(key);assert(legacy.wagerLedger()==0);
  assert(saved.contract.settleSelection(legacy,-1,0));assert(RE::player.gold==1000&&npc.gold==1000&&saved.bettingPurses.at(key).gold==300);
  // The merchant adapter shares the resolver and must support real references.
  const auto chestKey=worldFormID({identity.plugin,0x123});
  RE::TESObjectREFR chest{identity.plugin=="Skyrim.esm"?chestKey:0x28000123u,chestKey};bind(&chest,{identity.plugin,0x123});
  campaign::ShopBank shop(chest.ledger);assert(shop.actors[1]==&chest&&shop.count(1)==1000);
  assert(shop.change(1,-75)==-75&&chest.gold==925);
  // An in-progress game's virtual refund survives a removed actor reference.
  reset(key);campaign::Bank pending(key);assert(saved.contract.reserve(pending,{ours,theirs},TradeOne,100,key));
  RE::forms.erase(npc.id);data.files.erase({normalizedPlugin(identity.plugin),identity.local});
  campaign::Bank removed(key,key);assert(removed.actors[1]==nullptr&&removed.rival==key&&removed.wagerLedger()==key);
  assert(saved.contract.settleSelection(removed,-1,0));assert(RE::player.gold==1000&&saved.bettingPurses.at(key).gold==300);
 }
 const auto missing=worldFormID({"Removed.esm",0x801});
 campaign::saved={};campaign::saved.contract.wagerLedger=0xBEEF;
 campaign::saved.bettingPurses[0xBEEF]={800,82};
 campaign::Bank unavailable(missing);
 assert(unavailable.actors[1]==nullptr&&unavailable.rival==missing&&unavailable.wagerLedger()==missing&&unavailable.count(1,0)==0);
 assert(campaign::saved.bettingPurses.at(0xBEEF).gold==800);
 assert(campaign::resolveID<RE::TESObjectREFR>(0)==nullptr);
 assert(campaign::resolveID<RE::TESObjectREFR>(missing)==nullptr);
 std::cout<<"Native banks: Skyrim/DLC/mod/light actors, exact-type negative control, cards/foils/wagers, save/refund, legacy physical gold, merchant references and unrelated-account isolation passed\n";
}
'''
code = (code.replace('TYPED_LOOKUP', typed_lookup).replace('RESOLVER', resolver)
        .replace('BANK', bank).replace('SHOP', shop))
with tempfile.TemporaryDirectory(prefix='tessera-native-bank-') as work:
    cpp, binary = Path(work) / 'bank.cpp', Path(work) / 'bank'
    cpp.write_text(code)
    subprocess.run(['c++', '-std=c++20', '-O2', '-I'+str(ROOT/'include'), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
