'use strict';
// KeyboardEvent.code describes physical keys, matching Skyrim's DirectInput bindings.
const scanCodes={Escape:1,Minus:12,Equal:13,Backspace:14,Tab:15,BracketLeft:26,BracketRight:27,Enter:28,ControlLeft:29,Semicolon:39,Quote:40,Backquote:41,ShiftLeft:42,Backslash:43,Comma:51,Period:52,Slash:53,ShiftRight:54,NumpadMultiply:55,AltLeft:56,Space:57,CapsLock:58,NumLock:69,ScrollLock:70,Numpad7:71,Numpad8:72,Numpad9:73,NumpadSubtract:74,Numpad4:75,Numpad5:76,Numpad6:77,NumpadAdd:78,Numpad1:79,Numpad2:80,Numpad3:81,Numpad0:82,NumpadDecimal:83,IntlBackslash:86,F11:87,F12:88,NumpadEnter:156,ControlRight:157,NumpadDivide:181,PrintScreen:183,AltRight:184,Pause:197,Home:199,ArrowUp:200,PageUp:201,ArrowLeft:203,ArrowRight:205,End:207,ArrowDown:208,PageDown:209,Insert:210,Delete:211,MetaLeft:219,MetaRight:220,ContextMenu:221};
[...'1234567890'].forEach((c,i)=>scanCodes['Digit'+c]=i+2);
for(const [letters,start] of [['QWERTYUIOP',16],['ASDFGHJKL',30],['ZXCVBNM',44]])[...letters].forEach((c,i)=>scanCodes['Key'+c]=i+start);
for(let i=1;i<=10;i++)scanCodes['F'+i]=58+i;
const scanNames=Object.fromEntries(Object.entries(scanCodes).map(([key,value])=>[value,key.replace(/^Key|^Digit/,'').replace('Numpad','Num ').replace('Arrow','')]));
Object.assign(scanNames,{0:'Unbound',12:'−',13:'=',26:'[',27:']',39:';',40:"'",41:'`',43:'\\',51:',',52:'.',53:'/',57:'Space',183:'Print Screen',201:'Page Up',209:'Page Down'});
let bindingPending='',bindingKeyHeld=false;
function bindings() {return state?.settings||{collection:65,challenge:66,capturing:'',notice:''};}
function renderBindings() {
 if(!state)return;
 const config=bindings(),capture=config.capturing||bindingPending;
 if(config.capturing)bindingPending='';
 for(const field of ['collection','challenge']) {
   $('bind-'+field).textContent=capture===field?'Press a key':scanNames[config[field]]||`Key ${config[field]}`;
   $('bind-'+field).classList.toggle('binding-active',capture===field);
   $('unbind-'+field).disabled=!config[field]||Boolean(capture);
   $('reset-'+field).disabled=Boolean(capture);
   $('bind-'+field).disabled=Boolean(capture)&&capture!==field;
 }
 $('binding-cancel').hidden=!capture;
 $('binding-status').textContent=config.notice|| (capture?'Esc to cancel':'');
 const pauseAlbum=config.pauseWorldInAlbum!==false,control=$('pause-world-in-album');
 control.disabled=Boolean(capture)||state.phase!=='ready';
 control.setAttribute('aria-checked',String(pauseAlbum));control.lastElementChild.textContent=pauseAlbum?'On':'Off';
 $('interface-status').textContent=config.interfaceNotice||'';$('interface-status').hidden=!config.interfaceNotice;
 renderDevelopmentSettings();
 renderCardBackSettings();
}
function resetSettings() {bindingPending='';bindingKeyHeld=false;$('settings-panel').hidden=true;}
function closeSettings() {
 if($('settings-panel').hidden)return false;
 bindingPending='';send('binding','cancel');$('settings-panel').hidden=true;$('settings-open').focus();return true;
}
$('settings-open').onclick=()=>{if(state?.phase!=='ready')return;closeSelectMenu();$('settings-panel').hidden=false;renderBindings();$('bind-collection').focus();};
$('settings-panel').querySelector('.settings-body').addEventListener('focusin',event=>event.target.scrollIntoView({block:'nearest',inline:'nearest'}));
$('settings-done').onclick=closeSettings;
$('binding-cancel').onclick=()=>{bindingPending='';send('binding','cancel');};
$('pause-world-in-album').onclick=()=>{
 const control=$('pause-world-in-album');if(control.disabled)return;
 control.disabled=true;send('settings',`pause-album ${bindings().pauseWorldInAlbum===false?1:0}`);
};
for(const field of ['collection','challenge']) {
 $('bind-'+field).onclick=()=>{bindingPending=field;send('binding',`begin ${field}`);renderBindings();};
 $('unbind-'+field).onclick=()=>send('binding',`unbind ${field}`);
 $('reset-'+field).onclick=()=>send('binding',`reset ${field}`);
}
window.addEventListener('keydown',event=>{
 if(!state)return;
 const capture=bindings().capturing||bindingPending;
 if(capture||bindingKeyHeld) {
   event.preventDefault();event.stopImmediatePropagation();
   if(event.repeat||bindingKeyHeld)return;
   bindingKeyHeld=true;
   if(event.code==='Escape') {bindingPending='';send('binding','cancel');return;}
   const code=scanCodes[event.code];
   if(code===undefined||[29,42,54,56,157,184,219,220].includes(code))return;
   bindingPending='';send('binding',`set ${capture} ${code}`);return;
 }
 if(!$('settings-panel').hidden||event.target.matches('input,textarea')||event.ctrlKey||event.altKey||event.metaKey)return;
 const code=scanCodes[event.code],config=bindings();
 if(code&&(code===config.collection||code===config.challenge)) {
   event.preventDefault();event.stopImmediatePropagation();if(!event.repeat)window.ttcgEscape('dom');
 }
},true);
window.addEventListener('keyup',()=>{bindingKeyHeld=false;},true);
