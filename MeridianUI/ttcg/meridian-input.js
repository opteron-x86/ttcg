'use strict';
// Meridian injects its helper before the native DOM-ready callback. The native
// host calls this after configuring Input/1.
let meridianInputInstalled=false;
window.ttcgInstallMeridianInput=()=>{
 const input=window.MeridianInput;
 if(meridianInputInstalled||input?.version!==1) return;
 meridianInputInstalled=true;
 const root=document.getElementById('table');

 input.attachNavigation({
   root, wrap:false,
   initialFocus:()=>document.activeElement,
   getCandidates:()=>visibleControls(activeInteractionScope()),
   onBack:()=>window.ttcgKey('cancel')
 });
 input.onAction(event=>{
   if(!state) return true;
   if(event.phase!=='press'&&event.phase!=='repeat') return false;
   const key={up:'up',down:'down',left:'left',right:'right',accept:'confirm',cancel:'cancel',previousTab:'previous',nextTab:'next'}[event.action];
   if(!key) return false;
   if(openSelectMenu) {
     // Cursor acceptance is delivered as a real CEF click by Meridian.
     if(key==='confirm'&&input.getState().mode==='cursor') return false;
     selectMenuKey(key); return true;
   }
   if(key==='cancel'||key==='previous'||key==='next') { window.ttcgKey(key); return true; }
   // Meridian handles spatial navigation, right-stick scroll and cursor mode.
   return false;
 });
};
