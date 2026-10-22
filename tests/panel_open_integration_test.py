"""Run the production dialogue handoff and actor gates against a small game host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/Plugin.cpp').read_text()
collection = (root / 'include/NativeCollection.h').read_text()
def section(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]
actor_gate = section(collection, 'enum class ActorCardBlock', 'inline bool resumingWith')
finish = section(source, 'void finishPanelRequest(', 'void requestPanel(')
cancel = section(source, 'void cancelPanelRequest()', '\nstd::string albumSection')
adapters = '\n'.join([
    section(source, 'void beginPapyrus(', 'void receiveCommand('),
    section(source, 'void tournamentEntryPapyrus(', 'bool expansionLoadedPapyrus('),
])
code = r"""
#include "PanelOpenRequest.h"
#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
namespace ttcg {
 enum class NpcSyncWork { ActorState,Humanoid,Hostility,Scene };
 struct NpcSyncWorkScope {explicit NpcSyncWorkScope(NpcSyncWork){}};
 template<class F> auto measureNpcSync(NpcSyncWork,F f){return f();}
}
namespace RE {
 struct Actor;
 struct ActorHandle {
  Actor* ptr=nullptr;
  struct Pointer {Actor* ptr;Actor* get(){return ptr;}Actor* operator->(){return ptr;}explicit operator bool()const{return ptr;}};
  Pointer get(){return {ptr};}
  explicit operator bool()const{return ptr;}
  bool operator==(const ActorHandle&)const=default;
 };
 struct Position {float x=0;float GetDistance(Position p){return x>p.x?x-p.x:p.x-x;}};
 struct Actor {
  bool humanoid=true,dead=false,disabled=false,combat=false,hostile=false,scene=false,child=false,childReady=true;
  int cell=1;Position pos;
  bool IsHumanoid(){return humanoid;}bool IsDead(){return dead;}bool IsDisabled(){return disabled;}
  bool IsInCombat(){return combat;}bool IsHostileToActor(Actor*){return hostile;}bool GetCurrentScene(){return scene;}
  int GetParentCell(){return cell;}Position GetPosition(){return pos;}
  ActorHandle GetHandle(){return {this};}const char* GetName(){return "NPC";}
 };
 struct PlayerCharacter {static Actor* GetSingleton();};
 struct MainMenu {static constexpr int MENU_NAME=1;};
 struct LoadingMenu {static constexpr int MENU_NAME=2;};
 struct DialogueMenu {static constexpr int MENU_NAME=3;};
 struct UI {
  bool main=false,loading=false,dialogue=false,paused=false;
  static UI* GetSingleton();
  bool IsMenuOpen(int name){return name==1?main:name==2?loading:dialogue;}
  bool GameIsPaused(){return paused;}
 };
 struct MenuTopicManager {ActorHandle speaker;static MenuTopicManager* GetSingleton();};
 struct StaticFunctionTag{};
 namespace SendHUDMessage {inline std::vector<std::string> messages;void ShowHUDMessage(const char* text){messages.emplace_back(text);}}
}
RE::Actor player,npc,other;
RE::UI menus;
RE::MenuTopicManager topics;
RE::Actor* RE::PlayerCharacter::GetSingleton(){return &player;}
RE::UI* RE::UI::GetSingleton(){return &menus;}
RE::MenuTopicManager* RE::MenuTopicManager::GetSingleton(){return &topics;}
namespace campaign {
 struct {bool gamesEnabled=true;}interfaceSettings;
 bool available(){return true;}
 bool isChild(RE::Actor* actor){return actor->child;}
 bool childDialogueAvailable(RE::Actor* actor){return actor->childReady;}
 ACTOR_GATE
}
namespace collectionIntegration {inline int publications=0;void publish(){++publications;}}
namespace subject {
 namespace log {template<class... A> void debug(A&&...){}}
 struct API {bool focus=false;bool IsValid(int){return true;}bool HasAnyActiveFocus(){return focus;}}host,*api=&host;
 bool visible=false,uiReady=true;
 int view=1,opens=0;
 std::uint64_t now=0;
 std::uint64_t musicTime(){return now;}
 ttcg::PanelOpenRequest panelOpenRequest;
 RE::ActorHandle pending;
 std::string pendingScreen,panelWaitReason,openedScreen;
 std::vector<std::string> failures;
 CANCEL
 void logPanelFailure(RE::Actor*,const std::string&,std::uint64_t,const char* reason){failures.emplace_back(reason);}
 void showPanel(RE::Actor* actor,const std::string& screen){assert(campaign::actorReadyForCards(actor));visible=true;++opens;openedScreen=screen;}
 FINISH
 // Queue routing is recorded without Papyrus or real actor/task lifetimes.
 std::string routed;RE::Actor* routedActor=nullptr;
 void queuePanelRequest(RE::Actor* actor,std::string screen){routedActor=actor;routed=screen;}
 ADAPTERS
 void reset(){cancelPanelRequest();player={};npc={};other={};menus={};topics={};host={};visible=false;uiReady=true;opens=0;now=0;failures.clear();RE::SendHUDMessage::messages.clear();campaign::interfaceSettings.gamesEnabled=true;collectionIntegration::publications=0;}
 std::uint64_t start(std::string screen="lobby"){pending=npc.GetHandle();pendingScreen=screen;return panelOpenRequest.begin(now);}
 void tick(std::uint64_t token,std::uint64_t at){now=at;finishPanelRequest(token);}
}
int main(){
 using namespace subject;
 reset();tick(panelOpenRequest.token(),0);assert(failures.empty()&&opens==0); // An unrelated dialogue close is inert.
 for(auto screen:{"lobby","shop","tournaments","registration"}){
  reset();menus.dialogue=true;topics.speaker=npc.GetHandle();const auto token=start(screen);
  tick(token,0);menus.dialogue=false;host.focus=true;tick(token,100);assert(opens==0);
  host.focus=false;tick(token,200);tick(token,300);assert(opens==1&&openedScreen==screen);
  tick(token,400);assert(opens==1&&collectionIntegration::publications==1&&failures.empty());
 }
 reset();const auto afterClose=start();tick(afterClose,0);tick(afterClose,100);assert(opens==1);
 reset();npc.scene=true;const auto scene=start();tick(scene,0);tick(scene,7000);assert(opens==0&&failures.empty());
 npc.scene=false;tick(scene,7100);tick(scene,7200);assert(opens==1);
 reset();npc.scene=true;const auto busy=start();tick(busy,0);tick(busy,8000);assert(opens==0&&failures.size()==1&&failures[0]=="NPC scene"&&RE::SendHUDMessage::messages.size()==1);
 npc.scene=false;tick(busy,9000);assert(opens==0&&failures.size()==1);
 for(int block=0;block<9;++block){
  reset();const auto token=start();tick(token,0);
  if(block==0)npc.humanoid=false;
  if(block==1)npc.dead=true;
  if(block==2)npc.disabled=true;
  if(block==3)npc.combat=true;
  if(block==4)npc.hostile=true;
  if(block==5){npc.child=true;npc.childReady=false;}
  if(block==6)player.combat=true;
  if(block==7)npc.cell=2;
  if(block==8)npc.pos.x=601;
  tick(token,100);assert(opens==0&&failures.size()==1);
  npc={};player={};tick(token,200);assert(opens==0); // A denied request cannot reopen itself.
 }
 for(int cancel=0;cancel<5;++cancel){
  reset();const auto token=start();tick(token,0);
  if(cancel==0)menus.loading=true;
  if(cancel==1)menus.main=true;
  if(cancel==2)campaign::interfaceSettings.gamesEnabled=false;
  if(cancel==3){menus.dialogue=true;topics.speaker=other.GetHandle();}
  if(cancel==4)cancelPanelRequest();
  tick(token,100);assert(opens==0);
  menus={};campaign::interfaceSettings.gamesEnabled=true;tick(token,200);assert(opens==0);
 }
 // All spoken options route through the same handoff; no early readiness gate
 // is allowed to discard a challenge before the transient scene clears.
 reset();npc.scene=true;
 beginPapyrus(nullptr,&npc);assert(routed=="lobby"&&routedActor==&npc);
 shopPapyrus(nullptr,&npc);assert(routed=="shop");
 tournamentPapyrus(nullptr,&npc);assert(routed=="tournaments");
 tournamentEntryPapyrus(nullptr,&npc);assert(routed=="registration");
 std::cout<<"Production handoff: play/shop/tournament routing, dialogue/focus/scene delays, actor gates, timeout, load, disable and conversation cancellation passed\n";
}
"""
code = code.replace('ACTOR_GATE', actor_gate).replace('CANCEL', cancel).replace('FINISH', finish).replace('ADAPTERS', adapters)
with tempfile.TemporaryDirectory(prefix='tessera-panel-') as d:
    cpp, exe = Path(d) / 'handoff.cpp', Path(d) / 'handoff'
    cpp.write_text(code)
    subprocess.run(['c++', '-std=c++20', '-O2', '-I'+str(root/'include'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
