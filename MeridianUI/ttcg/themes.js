'use strict';
// Indices are persisted in TTCG.ini; keep this order stable.
const interfaceThemes=[
 {id:'tessera',name:'Tessera',surface:'felt'},
 {id:'charcoal',name:'Charcoal',surface:'slate'},
 {id:'oxblood',name:'Oxblood',surface:'leather'},
 {id:'parchment',name:'Parchment',surface:'parchment'},
 {id:'umber',name:'Umber',surface:'walnut'}
];
const boardSurfaces=[
 {id:'theme',name:'Match theme'},
 {id:'plain',name:'Plain'},
 {id:'slate',name:'Slate'},
 {id:'felt',name:'Green felt'},
 {id:'parchment',name:'Parchment'},
 {id:'leather',name:'Dark leather'},
 {id:'walnut',name:'Walnut'}
];
const themeFields=[
 {id:'interface-theme',command:'theme',key:'theme',choices:interfaceThemes},
 {id:'board-surface',command:'board-surface',key:'boardSurface',choices:boardSurfaces}
];
let boardTextureDraft=null;
function applyThemeSettings(){
 const config=state?.settings||{},root=document.documentElement;
 const theme=interfaceThemes[config.theme]||interfaceThemes[0];
 const surface=(boardSurfaces[config.boardSurface]||boardSurfaces[0]).id;
 root.dataset.theme=theme.id;root.classList.toggle('alternate-theme',theme.id!=='tessera');
 root.dataset.surface=surface==='theme'?theme.surface:surface;
 root.style.setProperty('--texture-strength',String((boardTextureDraft??config.boardTexture??10)/100));
}
function renderThemeSettings(){
 if(!state||$('settings-panel').hidden)return;
 const config=state.settings||{},locked=Boolean(config.capturing||bindingPending)||state.phase!=='ready';
 for(const field of themeFields){
  const control=$(field.id);control.value=String(field.choices[config[field.key]]?config[field.key]:0);
  control.disabled=locked;control._refresh();
 }
 const input=$('board-texture'),value=boardTextureDraft??config.boardTexture??10;
 input.value=String(value);input.disabled=locked||document.documentElement.dataset.surface==='plain';
 $('board-texture-value').textContent=value+'%';input.setAttribute('aria-valuetext',value+'%');
}
for(const field of themeFields){
 const control=$(field.id);
 control.replaceChildren(...field.choices.map((choice,i)=>new Option(choice.name,String(i))));control._refresh();
 control.onchange=()=>{control.disabled=true;control._refresh();send('settings',`${field.command} ${control.value}`);};
}
$('board-texture').oninput=()=>{
 boardTextureDraft=Number($('board-texture').value);applyThemeSettings();
 $('board-texture-value').textContent=boardTextureDraft+'%';$('board-texture').setAttribute('aria-valuetext',boardTextureDraft+'%');
};
$('board-texture').onchange=()=>{const value=Number($('board-texture').value);boardTextureDraft=null;send('settings',`board-texture ${value}`);};
const themeCorners=document.createElement('div');themeCorners.id='theme-corners';themeCorners.setAttribute('aria-hidden','true');
themeCorners.innerHTML=Array.from({length:4},()=>'<svg viewBox="0 0 80 80"><path d="M1 32V1h31 M5 20V5h15"/></svg>').join('');
$('table').append(themeCorners);
