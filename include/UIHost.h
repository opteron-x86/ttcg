#pragma once
#include "Host.h"
namespace ttcg::ui {
using View=std::uint64_t;
using Listener=void(*)(const char*);
inline constexpr const char* name="Native UI";
inline constexpr const char* focusMenu=preview::skyrim::menuName;
class Host {
public:
 void initialize(Listener command,void(*interrupted)()) {
  auto& native=preview::skyrim::Host::get();native.initialize();
  native.connect([command](const std::string& data){command(data.c_str());},interrupted);
 }
 bool IsValid(View view)const{return view==1;}
 void State(View,const std::string& snapshot){preview::skyrim::Host::get().receive(snapshot);}
 void Reset(View){preview::skyrim::Host::get().clear();}
 void Escape(View){preview::skyrim::Host::get().escape();}
 void Show(View){}
 void Hide(View){preview::skyrim::Host::get().unfocus();}
 bool Focus(View,bool pause){return preview::skyrim::Host::get().focus(pause);}
 bool HasFocus(View)const{return preview::skyrim::Host::get().focused();}
 bool HasAnyActiveFocus()const{return preview::skyrim::Host::get().anyFocus();}
 void Unfocus(View){preview::skyrim::Host::get().unfocus();}
};
inline Host host;
inline Host* Acquire(){return &host;}
}
