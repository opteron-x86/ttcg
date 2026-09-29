#pragma once
#include "Presentation.h"
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#pragma push_macro("NOMINMAX")
#pragma push_macro("NOGDI")
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#undef NOGDI
#include "MeridianUIAPI/InputDllLoader.h"
#pragma pop_macro("NOGDI")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
namespace ttcg::ui {
using View=std::uint64_t;
using Ready=void(*)(View);
using Listener=void(*)(const char*);
inline constexpr const char* name="Meridian UI";
inline constexpr const char* focusMenu="MeridianUI_FocusMenu";
class Host {
 Meridian::UI::View::IViewAPI* meridian{};
 Meridian::UI::Input::IInputAPI* input{};
 bool controller=false;
public:
 bool acquire() {
   Meridian::UI::Settings settings{};
   meridian=Meridian::UI::View::Query(&settings,"TTCG");
   if(meridian) input=Meridian::UI::Input::Query(&settings,"TTCG");
   return meridian!=nullptr;
 }
 View CreateView(Ready ready) {
   Meridian::UI::View::ViewCreateInfo info{};
   info.ownerName="ttcg"; info.viewName="match"; info.startUrl="mod://ttcg/index.html";
   info.initiallyVisible=false; info.frameRate=60; info.onDOMReady=ready;
   return meridian->CreateView(&info);
 }
 bool RegisterJSListener(View view,const char* name,Listener listener) {
   return meridian->RegisterListener(view,name,listener);
 }
 void InteropCall(View view,const char* function,const char* payload) {
   // Both names and payloads are string-escaped; JSON remains data, never code.
   const auto script="window["+quote(function)+"]?.("+quote(payload)+");";
   if(!meridian->ExecuteJavaScript(view,script.c_str())) SKSE::log::warn("Meridian rejected JS call: {}",function);
 }
 bool IsValid(View view) const {
   return meridian&&view&&meridian->IsValid(view);
 }
 void Show(View view) {
   meridian->Show(view);
 }
 void Hide(View view) {
   meridian->Hide(view);
 }
 bool Focus(View view,bool pause) {
   auto result=meridian->TryFocus(view,pause?Meridian::UI::View::FocusMode::PauseGame:Meridian::UI::View::FocusMode::Unpaused);
   SKSE::log::info("Meridian focus result: {}",static_cast<unsigned>(result));
   return result==Meridian::UI::View::FocusResult::Granted||result==Meridian::UI::View::FocusResult::AlreadyFocused;
 }
 bool HasFocus(View view) const {
   return meridian->HasFocus(view);
 }
 bool HasAnyActiveFocus() const {
   return meridian->HasAnyFocus();
 }
 void Unfocus(View view) {
   meridian->Unfocus(view);
 }
 void EnableController(View view) {
   if(input) {
     Meridian::UI::Input::ViewInputConfig config{}; config.enabled=1; config.allowCursor=1;
     auto result=input->ConfigureView(view,&config);
     controller=result==Meridian::UI::Input::Result::Ok;
     SKSE::log::info("Meridian controller result: {}",static_cast<unsigned>(result));
     if(controller) InteropCall(view,"ttcgInstallMeridianInput","");
   }
 }
 bool HandlesController() const {
   return controller;
 }
};
inline Host host;
inline Host* Acquire() { return host.acquire()?&host:nullptr; }
}
