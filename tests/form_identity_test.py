"""Run the production form identity against a small load order."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/NativeCollection.h').read_text()
begin = source.index('inline const RE::TESFile* identityFile(')
identity = source[begin:source.index('template <class T> T* resolveID', begin)]
code = r"""
#include "WorldIdentity.h"
#include <cassert>
#include <cstdint>
#include <vector>
namespace RE {
struct TESFile {
 const char* name;std::uint8_t compileIndex;std::uint16_t smallFileCompileIndex;
 std::string_view GetFilename() const {return name;}
};
// Skyrim, Update, a regular plugin, two light plugins, and the injecting plugin.
inline TESFile skyrim{"Skyrim.esm",0,0},update{"Update.esm",1,0},town{"Town.esp",0x2A,0},
               small{"Small.esl",0xFE,0x001},other{"Other.esl",0xFE,0x123},patch{"Patch.esp",0x03,0};
struct TESDataHandler {
 static TESDataHandler* GetSingleton(){static TESDataHandler data;return &data;}
 const TESFile* LookupLoadedModByIndex(std::uint8_t index){
  for(auto* file:{&skyrim,&update,&town,&patch})if(file->compileIndex==index)return file;
  return nullptr;
 }
 const TESFile* LookupLoadedLightModByIndex(std::uint16_t index){
  for(auto* file:{&small,&other})if(file->smallFileCompileIndex==index)return file;
  return nullptr;
 }
};
struct TESForm {
 std::uint32_t formID;const TESFile* file;
 const TESFile* GetFile(int) const {return file;}
 std::uint32_t GetFormID() const {return formID;}
 // CommonLib's own local ID, which the identity used before.
 std::uint32_t GetLocalFormID() const {
  if(!file)return formID;
  std::uint32_t index=file->compileIndex<<24;index+=file->smallFileCompileIndex<<12;return formID&~index;
 }
};
}
namespace ttcg {
enum class NpcSyncWork {Identity};
struct NpcSyncWorkScope {explicit NpcSyncWorkScope(NpcSyncWork){}};
}
namespace campaign {
IDENTITY
// What resolveID looks up for a saved identity.
std::uint32_t resolved(std::uint32_t id){
 const auto* master=ttcg::identityMaster(id);
 if(!master)return id;
 for(auto* file:{&RE::skyrim,&RE::update,&RE::town,&RE::small,&RE::other,&RE::patch})
  if(ttcg::normalizedPlugin(file->name)==ttcg::normalizedPlugin(master))
   return (std::uint32_t(file->compileIndex)<<24)+(std::uint32_t(file->smallFileCompileIndex)<<12)+ttcg::identityLocal(id);
 return 0;
}
std::uint32_t before(RE::TESForm& form){
 return form.file?ttcg::authoredIdentity(form.file->GetFilename(),form.GetLocalFormID(),form.GetFormID()):form.GetFormID();
}
}
int main(){
 using namespace campaign;
 // Ordinary records keep exactly the identity existing saves hold, and resolve to themselves.
 std::vector<RE::TESForm> ordinary{{0x00013BB9,&RE::skyrim},{0x01000ABC,&RE::update},{0x2A001234,&RE::town},
  {0xFE001801,&RE::small},{0xFE123FFF,&RE::other},{0x03000D62,&RE::patch},{0xFF000900,nullptr}};
 for(auto& form:ordinary){
  assert(persistentID(&form)==before(form));
  assert(resolved(persistentID(&form))==form.formID);
 }
 assert(persistentID(&ordinary[0])==0x13BB9&&persistentID(nullptr)==0);
 // Records Patch.esp injects into other plugins' spaces resolve to themselves too.
 // The old identity named Patch.esp and came back as a different form or none.
 std::vector<RE::TESForm> injected{{0x000ABCDE,&RE::patch},{0x010ABCDE,&RE::patch},
  {0x2A0ABCDE,&RE::patch},{0xFE001900,&RE::patch},{0xFE123900,&RE::patch}};
 for(auto& form:injected){
  assert(resolved(persistentID(&form))==form.formID);
  assert(resolved(before(form))!=form.formID);
 }
 // An injected record is the same thing as a record its target plugin defines.
 RE::TESForm native{0x000ABCDE,&RE::skyrim};
 assert(persistentID(&injected[0])==persistentID(&native)&&persistentID(&injected[0])==0xABCDE);
 // The identity does not depend on where the plugins sit in the load order.
 const auto town=persistentID(&injected[2]),light=persistentID(&injected[4]);
 RE::town.compileIndex=0x31;RE::other.smallFileCompileIndex=0x045;RE::patch.compileIndex=0x07;
 RE::TESForm movedTown{0x310ABCDE,&RE::patch},movedLight{0xFE045900,&RE::patch};
 assert(persistentID(&movedTown)==town&&persistentID(&movedLight)==light);
 // A space no loaded plugin owns has no identity, instead of a wrong one.
 RE::TESForm orphan{0x550ABCDE,&RE::patch};
 assert(persistentID(&orphan)==0);
}
""".replace('IDENTITY', identity)
with tempfile.TemporaryDirectory(prefix='tessera-identity-') as directory:
    cpp = Path(directory) / 'identity.cpp'
    exe = Path(directory) / 'identity'
    cpp.write_text(code)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-I', str(root / 'include'), str(cpp), '-o', str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True)
print('Form identity: ordinary records unchanged; injected records resolve to themselves in any load order')
