"""Keep Skyrim cursor activation separate from the desktop cursor and focus."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def section(source, start, end):
    begin = source.index(start)
    return source[begin:source.index(end, begin)]


host = (ROOT / 'native/skse/Host.cpp').read_text()
menu = (ROOT / 'native/skse/Menu.cpp').read_text()
code = r'''
#include "Session.h"
#include "Interaction.h"
#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <atomic>
using HWND=int;
HWND foreground=1;
HWND GetForegroundWindow(){return foreground;}
namespace REX::W32 {
 int count=-3,calls=0;
 int ShowCursor(bool visible){++calls;return count+=visible?1:-1;}
}
namespace SKSE {
 struct Tasks {
  std::vector<std::function<void()>> pending;
  void AddTask(std::function<void()> task){pending.push_back(std::move(task));}
  void flush(){auto jobs=std::move(pending);pending.clear();for(auto& task:jobs)task();}
 };
 Tasks* GetTaskInterface(){static Tasks tasks;return &tasks;}
 namespace log {template<class... T>void info(const char*,T...) {}}
}
namespace RE {
 enum class UI_MENU_FLAGS {kPausesGame=1,kUsesCursor=2,kUpdateUsesCursor=4,kUsesMenuContext=8,kCustomRendering=16,kDisablePauseMenu=32,kDontHideCursorWhenTopmost=64};
 enum class UI_MESSAGE_TYPE {kUpdate,kShow,kHide,kForceHide,kScaleformEvent,kUserEvent};
 enum class UI_MESSAGE_RESULTS {kHandled,kPassOn};
 struct UIMessage {UI_MESSAGE_TYPE value;const UI_MESSAGE_TYPE* type=&value;};
 struct UserEvents {enum class INPUT_CONTEXT_ID {kMenuMode=1};};
 struct Flags {
  unsigned bits=0;
  template<class... T>void set(T... flags){((bits|=unsigned(flags)),...);}
  void reset(UI_MENU_FLAGS flag){bits&=~unsigned(flag);}
  bool has(UI_MENU_FLAGS flag)const{return (bits&unsigned(flag))!=0;}
 };
 struct IMenu {
  Flags menuFlags,inputContext;int depthPriority=0,refreshes=0;bool open=false;
  virtual ~IMenu()=default;
  virtual void PostDisplay(){}
  virtual UI_MESSAGE_RESULTS ProcessMessage(UIMessage&){return UI_MESSAGE_RESULTS::kPassOn;}
  virtual void RefreshPlatform(){++refreshes;}
  bool UsesMenuContext()const{return menuFlags.has(UI_MENU_FLAGS::kUsesMenuContext);}
 };
 struct UIBlurManager {
  int blurCount=0;
  static UIBlurManager* GetSingleton(){static UIBlurManager manager;return &manager;}
  void IncrementBlurCount(){++blurCount;}
  void DecrementBlurCount(){--blurCount;}
 };
 struct MenuCursor {
  struct Runtime {int showCursorCount=-3;} data;
  Runtime& GetRuntimeData(){return data;}
  void SetCursorVisibility(bool visible);
  static MenuCursor* GetSingleton(){static MenuCursor cursor;return &cursor;}
 };
 CURSOR_VISIBILITY
 struct CursorMenu {static constexpr auto MENU_NAME="Cursor Menu";};
 struct HUDMenu {static constexpr auto MENU_NAME="HUD Menu";};
 struct Console {static constexpr auto MENU_NAME="Console";};
 struct MainMenu {static constexpr auto MENU_NAME="Main Menu";};
 struct LoadingMenu {static constexpr auto MENU_NAME="Loading Menu";};
 struct DialogueMenu {static constexpr auto MENU_NAME="Dialogue Menu";};
 struct UI {
  bool gameCursorVisible=false;
  std::map<std::string,std::shared_ptr<IMenu>> menus;
  std::vector<std::shared_ptr<IMenu>> menuStack;
  static UI* GetSingleton(){static UI ui;return &ui;}
  auto GetMenu(const char* name){auto it=menus.find(name);return it==menus.end()?std::shared_ptr<IMenu>{}:it->second;}
  bool IsMenuOpen(const char* name){auto menu=GetMenu(name);return menu&&menu->open;}
 };
}
namespace RE {
 enum class BSEventNotifyControl {kContinue};
 template<class T>struct BSTEventSource {};
 struct MenuOpenCloseEvent {std::string menuName;bool opening;};
 struct UIMessageQueue {
  std::vector<std::pair<std::string,UI_MESSAGE_TYPE>> messages;
  static UIMessageQueue* GetSingleton(){static UIMessageQueue queue;return &queue;}
  void AddMessage(const char* menu,UI_MESSAGE_TYPE type,void*){messages.emplace_back(menu,type);}
  void flush(){auto* ui=UI::GetSingleton();for(auto& [name,type]:messages){
   auto& menu=ui->menus[name];if(!menu)menu=std::make_shared<IMenu>();
   if(type==UI_MESSAGE_TYPE::kShow)menu->open=true;
   if(type==UI_MESSAGE_TYPE::kHide||type==UI_MESSAGE_TYPE::kForceHide)menu->open=false;
   if(name==CursorMenu::MENU_NAME){
    // Model the game-rendered pointer separately: no ShowCursor call is needed.
    bool owner=false;for(auto& active:ui->menuStack)owner|=active->open&&active->menuFlags.has(UI_MENU_FLAGS::kUsesCursor);
    ui->gameCursorVisible=menu->open&&owner;
   }
  }messages.clear();}
 };
}
namespace preview::skyrim {
constexpr char menuName[]="TesseraMenu";
FOREIGN_MENU
struct Host {
 struct State {Session session;std::atomic<HWND> window{1};bool ownsFocus=true,clearInput=false,closeQueued=false;std::atomic<bool> wantsText{false},cursorRefreshQueued{false};std::uint64_t cursorLogToken=0;unsigned cursorLogCount=0;} storage;
 State* state=&storage;bool pause=true;int interruptions=0;
 static Host& get(){static Host host;return host;}
 bool pauses()const{return pause;}
 void shown();
 void hidden(){storage.session.hidden();}
 void draw();
 void discardInput(){}
 void syncTextInput(std::uint64_t){}
 Interaction interaction()const;
 void refreshCursor();
 void queueClose(const char*){++interruptions;}
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*,RE::BSTEventSource<RE::MenuOpenCloseEvent>*);
};
HOST_INTERACTION
HOST_SHOWN
HOST_CURSOR
HOST_MENUS
HOST_DRAW_FOCUS
NATIVE_MENU
}
int main(){
 using namespace preview::skyrim;
 auto& host=Host::get();auto* ui=RE::UI::GetSingleton();
 auto* tasks=SKSE::GetTaskInterface();auto* queue=RE::UIMessageQueue::GetSingleton();auto* cursor=RE::MenuCursor::GetSingleton();
 const auto flush=[&]{tasks->flush();queue->flush();};
 const auto onlyGameCursor=[&]{
  assert(ui->gameCursorVisible);
  assert(REX::W32::count==-3&&cursor->data.showCursorCount==-3&&REX::W32::calls==0);
 };
 // The scene stays blurred exactly as long as a menu instance exists.
 {Menu brief;assert(RE::UIBlurManager::GetSingleton()->blurCount==1);}
 assert(RE::UIBlurManager::GetSingleton()->blurCount==0);
 auto menu=std::make_shared<Menu>();ui->menus[menuName]=menu;
 assert(menu->menuFlags.has(RE::UI_MENU_FLAGS::kUsesCursor));
 assert(RE::UIBlurManager::GetSingleton()->blurCount==1);
 assert(menu->menuFlags.has(RE::UI_MENU_FLAGS::kDontHideCursorWhenTopmost));
 auto cursorMenu=std::make_shared<RE::IMenu>();ui->menus[RE::CursorMenu::MENU_NAME]=cursorMenu;
 RE::UIMessage show{RE::UI_MESSAGE_TYPE::kShow};
 // Cold open: enqueue the game cursor only after Tessera reaches the menu stack.
 // Its sprite must become visible while the desktop pointer stays hidden.
 host.storage.session.open();menu->ProcessMessage(show);
 assert(host.storage.session.active()&&tasks->pending.size()==1&&queue->messages.empty()&&!ui->gameCursorVisible);
 menu->open=true;ui->menuStack={menu};flush();onlyGameCursor();
 const auto token=host.storage.session.token();assert(host.interaction()==Interaction::active);
 // A negative OS visibility count is normal. It must not trigger restoration
 // on every render frame or introduce a second cursor.
 for(int frame=0;frame<60;++frame)host.draw();
 assert(tasks->pending.empty()&&queue->messages.empty());onlyGameCursor();
 // Controller platform callbacks keep Skyrim's cursor available without
 // touching the OS pointer or switching to the base Scaleform hide policy.
 menu->RefreshPlatform();menu->RefreshPlatform();assert(tasks->pending.size()==1);flush();
 assert(menu->refreshes==0&&menu->menuFlags.has(RE::UI_MENU_FLAGS::kUsesCursor));onlyGameCursor();
 auto console=std::make_shared<RE::IMenu>();console->menuFlags.set(RE::UI_MENU_FLAGS::kUsesMenuContext,RE::UI_MENU_FLAGS::kUsesCursor);
 ui->menus[RE::Console::MENU_NAME]=console;
 for(bool open:{false,true}){
  console->open=open;if(open)ui->menuStack.push_back(console);
  foreground=2;assert(host.interaction()==Interaction::suspended&&host.storage.session.accepts(token));
  // The desktop may display its own cursor outside Skyrim. Tessera must neither
  // show nor hide it while the player is in another application.
  REX::W32::count=cursor->data.showCursorCount=0;host.draw();menu->RefreshPlatform();flush();
  assert(REX::W32::count==0&&REX::W32::calls==0);
  foreground=1;REX::W32::count=cursor->data.showCursorCount=-3;
  assert(host.interaction()==(open?Interaction::suspended:Interaction::active));
  if(open)ui->menuStack.pop_back();
 }
 // Console closure can hide the sprite even when Cursor Menu remains open.
 // Update the game menu on return; don't equate that sprite with ShowCursor.
 console->open=false;ui->gameCursorVisible=false;
 RE::MenuOpenCloseEvent consoleClosed{RE::Console::MENU_NAME,false};
 host.ProcessEvent(&consoleClosed,nullptr);flush();onlyGameCursor();
 // Recover when the game cursor menu itself disappears, deduplicating frames.
 cursorMenu->open=false;ui->gameCursorVisible=false;host.draw();host.draw();
 assert(tasks->pending.size()==1);flush();onlyGameCursor();
 auto other=std::make_shared<RE::IMenu>();other->menuFlags.set(RE::UI_MENU_FLAGS::kUsesMenuContext);ui->menuStack.push_back(other);
 assert(host.interaction()==Interaction::lost);host.refreshCursor();flush();assert(queue->messages.empty()&&REX::W32::calls==0);
 ui->menuStack.pop_back();menu->open=false;assert(host.interaction()==Interaction::lost);menu->open=true;
 // Stale work must not reopen either cursor after close/load, including while
 // another application is displaying its desktop pointer.
 host.refreshCursor();host.storage.session.close();host.hidden();menu->open=false;
 ui->gameCursorVisible=false;cursorMenu->open=false;REX::W32::count=cursor->data.showCursorCount=0;flush();
 assert(!ui->gameCursorVisible&&!cursorMenu->open&&REX::W32::count==0&&REX::W32::calls==0&&host.interaction()==Interaction::hidden);
 REX::W32::count=cursor->data.showCursorCount=-3;
 host.storage.session.open();menu->ProcessMessage(show);menu->open=true;flush();onlyGameCursor();
 std::cout<<"Native cursor ownership: game sprite only, desktop untouched, no repeated recovery, deferred open, console, controller callbacks, Alt-Tab and stale work passed\n";
}
'''
code = code.replace('FOREIGN_MENU', section(host, 'bool foreignMenu(', '\n}\nstruct Host::State'))
cursor = (ROOT / 'build/deps/commonlib/src/RE/M/MenuCursor.cpp').read_text()
code = code.replace('CURSOR_VISIBILITY', section(cursor, '\tvoid MenuCursor::SetCursorVisibility(', '\n}'))
code = code.replace('HOST_SHOWN', section(host, 'void Host::shown()', '\nvoid Host::refreshCursor'))
code = code.replace('HOST_CURSOR', section(host, 'void Host::refreshCursor()', '\nvoid Host::releaseTextInput'))
code = code.replace('HOST_MENUS', section(host, 'RE::BSEventNotifyControl Host::ProcessEvent(const RE::MenuOpenCloseEvent*', '\n}\n}').rstrip()+'\n}')
code = code.replace('HOST_INTERACTION', section(host, 'Interaction Host::interaction()', '\nvoid Host::discardInput'))
code = code.replace('HOST_DRAW_FOCUS', section(host, 'void Host::draw() {', '    ContextGuard contextGuard;')+'}\n')
code = code.replace('NATIVE_MENU', section(menu, 'class Menu final', '\n}\nvoid registerMenu'))
with tempfile.TemporaryDirectory(prefix='tessera-native-input-') as work:
    cpp, binary = Path(work) / 'input.cpp', Path(work) / 'input'
    cpp.write_text(code)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-O2', '-I'+str(ROOT/'native/skse'), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
