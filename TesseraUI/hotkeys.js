'use strict';
// KeyboardEvent.code describes physical keys, matching Skyrim's DirectInput bindings.
const scanCodes={Escape:1,Minus:12,Equal:13,Backspace:14,Tab:15,BracketLeft:26,BracketRight:27,Enter:28,ControlLeft:29,Semicolon:39,Quote:40,Backquote:41,ShiftLeft:42,Backslash:43,Comma:51,Period:52,Slash:53,ShiftRight:54,NumpadMultiply:55,AltLeft:56,Space:57,CapsLock:58,NumLock:69,ScrollLock:70,Numpad7:71,Numpad8:72,Numpad9:73,NumpadSubtract:74,Numpad4:75,Numpad5:76,Numpad6:77,NumpadAdd:78,Numpad1:79,Numpad2:80,Numpad3:81,Numpad0:82,NumpadDecimal:83,IntlBackslash:86,F11:87,F12:88,NumpadEnter:156,ControlRight:157,NumpadDivide:181,PrintScreen:183,AltRight:184,Pause:197,Home:199,ArrowUp:200,PageUp:201,ArrowLeft:203,ArrowRight:205,End:207,ArrowDown:208,PageDown:209,Insert:210,Delete:211,MetaLeft:219,MetaRight:220,ContextMenu:221};
[...'1234567890'].forEach((c,i)=>scanCodes['Digit'+c]=i+2);
for(const [letters,start] of [['QWERTYUIOP',16],['ASDFGHJKL',30],['ZXCVBNM',44]])[...letters].forEach((c,i)=>scanCodes['Key'+c]=i+start);
for(let i=1;i<=10;i++)scanCodes['F'+i]=58+i;
const scanNames=Object.fromEntries(Object.entries(scanCodes).map(([key,value])=>[value,key.replace(/^Key|^Digit/,'').replace('Numpad','Num ').replace('Arrow','')]));
Object.assign(scanNames,{0:'Unbound',12:'−',13:'=',26:'[',27:']',39:';',40:"'",41:'`',43:'\\',51:',',52:'.',53:'/',57:'Space',183:'Print Screen',201:'Page Up',209:'Page Down'});
// Same packed representation as InterfaceSettings.h. Keep each side distinct.
const hotkeyModifierKeys=[['ControlLeft',1024,'LCtrl'],['ControlRight',2048,'RCtrl'],['ShiftLeft',256,'LShift'],['ShiftRight',512,'RShift'],['AltLeft',4096,'LAlt'],['AltRight',8192,'RAlt']];
const hotkeyModifierGroups=[['ctrlKey',3072],['shiftKey',768],['altKey',12288]];
let bindingPending='',bindingKeyHeld='',heldModifiers=0;
function applyHotkeyState(config){
 if(Number.isInteger(config?.heldModifiers))heldModifiers=config.heldModifiers&0x3F00;
}
function hotkeyName(binding){
 if(!binding)return 'Unbound';
 const names=hotkeyModifierKeys.filter(([,bit])=>binding&bit).map(([, ,name])=>name);
 const key=binding&255;names.push(key?(scanNames[key]||`Key ${key}`):'…');return names.join(' + ');
}
function trackHotkeyModifiers(event,pressed){
 for(const [property,mask] of hotkeyModifierGroups)if(!event[property])heldModifiers&=~mask;
 const modifier=hotkeyModifierKeys.find(([code])=>code===event.code);
 if(modifier)heldModifiers=pressed?heldModifiers|modifier[1]:heldModifiers&~modifier[1];
 return Boolean(modifier);
}
function eventHotkey(event){
 const code=scanCodes[event.code];
 if(!code||code===1||[29,42,54,56,157,184,219,220].includes(code)||event.metaKey)return 0;
 // A modifier held before browser focus is seeded by the native snapshot.
 // If its side is unknown (e.g. after Alt-Tab), don't silently bind the wrong key.
 if(hotkeyModifierGroups.some(([property,mask])=>Boolean(event[property])!==Boolean(heldModifiers&mask)))return 0;
 return code|heldModifiers;
}
function renderBindingCapture(){
 const capture=bindings().capturing||bindingPending;
 if(capture)$('bind-'+capture).textContent=heldModifiers?hotkeyName(heldModifiers):'Press a key';
}
const bindingFields=['collection','challenge','toggleGames'];
const worldPauseOptions=[
 ['pause-world-in-album','pauseWorldInAlbum','pause-album'],
 ['pause-world-in-shops','pauseWorldInShops','pause-shop'],
 ['pause-world-in-match-setup','pauseWorldInMatchSetup','pause-setup'],
 ['pause-world-during-matches','pauseWorldDuringMatches','pause-match']
];
function bindings() {return state?.settings||{collection:65,challenge:66,capturing:'',notice:''};}
function renderBindings() {
 if(!state)return;
 const config=bindings(),capture=config.capturing||bindingPending;
 if(config.capturing)bindingPending='';
 for(const field of bindingFields) {
   $('bind-'+field).textContent=capture===field?'Press a key':hotkeyName(config[field]||0);
   $('bind-'+field).classList.toggle('binding-active',capture===field);
   $('unbind-'+field).disabled=!config[field]||Boolean(capture);
   $('reset-'+field).disabled=Boolean(capture);
   $('bind-'+field).disabled=Boolean(capture)&&capture!==field;
 }
 renderBindingCapture();
 const enabled=config.gamesEnabled!==false,control=$('games-enabled');
 control.disabled=Boolean(capture)||state.phase!=='ready';
 control.setAttribute('aria-checked',String(enabled));control.lastElementChild.textContent=enabled?'On':'Off';
 $('resident-mode').value=String(config.residents??1);$('resident-mode').disabled=Boolean(capture)||state.phase!=='ready';
 $('resident-mode')._refresh();
 const voices=$('vanilla-voices-only'),vanilla=config.vanillaVoicesOnly!==false;voices.disabled=Boolean(capture)||state.phase!=='ready';voices.setAttribute('aria-checked',String(vanilla));voices.lastElementChild.textContent=vanilla?'On':'Off';
 $('resident-candidate').textContent=config.residentCandidate||'Face a nearby resident to add them.';
 $('resident-add').disabled=!config.canAddResident||Boolean(capture)||state.phase!=='ready';
 $('binding-cancel').hidden=!capture;
 $('binding-status').textContent=config.notice|| (capture?'Esc to cancel':'');
 for(const [id,key] of worldPauseOptions){
  const paused=config[key]!==false,control=$(id);
  control.disabled=Boolean(capture)||state.phase!=='ready';
  control.setAttribute('aria-checked',String(paused));control.lastElementChild.textContent=paused?'On':'Off';
 }
 $('interface-status').textContent=config.interfaceNotice||'';$('interface-status').hidden=!config.interfaceNotice;
 renderDevelopmentSettings();
 renderCardBackSettings();
 renderCardFaceSettings();
 renderThemeSettings();
}
function resetSettings() {bindingPending='';bindingKeyHeld='';heldModifiers=0;cardFaceDrafts.clear();boardTextureDraft=null;$('settings-panel').hidden=true;}
// Settings is an album page. Escape only consumes a pending key capture;
// otherwise closing or changing tabs follows normal album navigation.
function closeSettings() {
 if(!bindingPending&&!bindings().capturing)return false;
 bindingPending='';bindingKeyHeld='';send('binding','cancel');return true;
}
$('vanilla-voices-only').onclick=()=>{const control=$('vanilla-voices-only');if(control.disabled)return;control.disabled=true;send('settings',`vanilla-voices ${bindings().vanillaVoicesOnly===false?1:0}`);};
$('games-enabled').onclick=()=>{
 const control=$('games-enabled');if(control.disabled)return;
 control.disabled=true;send('settings',`games ${bindings().gamesEnabled===false?1:0}`);
};
$('resident-mode').onchange=()=>{const select=$('resident-mode');select.disabled=true;select._refresh();send('settings',`residents ${select.value}`);};
$('resident-add').onclick=()=>{if($('resident-add').disabled)return;$('resident-add').disabled=true;send('npc-add');};
$('binding-cancel').onclick=()=>{bindingPending='';send('binding','cancel');};
for(const [id,key,command] of worldPauseOptions)$(id).onclick=()=>{
 const control=$(id);if(control.disabled)return;
 control.disabled=true;send('settings',`${command} ${bindings()[key]===false?1:0}`);
};
for(const field of bindingFields) {
 $('bind-'+field).onclick=()=>{bindingKeyHeld='';bindingPending=field;send('binding',`begin ${field}`);renderBindings();};
 $('unbind-'+field).onclick=()=>send('binding',`unbind ${field}`);
 $('reset-'+field).onclick=()=>send('binding',`reset ${field}`);
}
window.addEventListener('keydown',event=>{
 const modifier=trackHotkeyModifiers(event,true);
 if(!state)return;
 const capture=bindings().capturing||bindingPending;
 if(capture||bindingKeyHeld) {
   event.preventDefault();event.stopImmediatePropagation();
   if(modifier){renderBindingCapture();return;}
   if(event.repeat||bindingKeyHeld)return;
   if(event.code==='Escape') {bindingKeyHeld=event.code;bindingPending='';send('binding','cancel');return;}
   const chord=eventHotkey(event);
   if(!chord)return;
   bindingKeyHeld=event.code;bindingPending='';send('binding',`set ${capture} ${chord}`);return;
 }
 if(!$('settings-panel').hidden||event.target.matches('input,textarea,select')||event.target.isContentEditable)return;
 const chord=eventHotkey(event),config=bindings();
 if(chord&&chord===config.toggleGames){
   event.preventDefault();event.stopImmediatePropagation();
   if(!event.repeat)send('settings',`games ${config.gamesEnabled===false?1:0}`);return;
 }
 if(chord&&(chord===config.collection||config.gamesEnabled!==false&&chord===config.challenge)) {
   event.preventDefault();event.stopImmediatePropagation();if(!event.repeat)window.ttcgEscape('dom');
 }
},true);
window.addEventListener('keyup',event=>{
 trackHotkeyModifiers(event,false);
 if(event.code===bindingKeyHeld)bindingKeyHeld='';
 renderBindingCapture();
},true);
window.addEventListener('blur',()=>{heldModifiers=0;bindingKeyHeld='';renderBindingCapture();});
