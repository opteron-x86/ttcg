'use strict';
// Values are persisted in TTCG.ini. Keep the order stable when adding fonts.
const appearanceFonts=[
 {name:'Default',family:''},
 {name:'Georgia',family:'Georgia,"Times New Roman",serif'},
 {name:'Times New Roman',family:'"Times New Roman","Liberation Serif",serif'},
 {name:'Arial',family:'Arial,Helvetica,"Liberation Sans",sans-serif'},
 {name:'Verdana',family:'Verdana,"DejaVu Sans",sans-serif'}
];
const appearanceFontFields=[
 {id:'interface-font',key:'interfaceFont'},
 {id:'card-name-font',key:'cardNameFont'},
 {id:'card-value-font',key:'cardValueFont'}
];
const cardFaceRanges=[
 {id:'rank-shadow-opacity',key:'rankShadowOpacity',fallback:100,format:v=>v+'%'},
 {id:'rank-shadow-blur',key:'rankShadowBlur',fallback:2,format:v=>String(v)},
 {id:'rank-shadow-offset',key:'rankShadowOffset',fallback:1,format:v=>String(v)},
 {id:'rank-stroke-weight',key:'rankStrokeWeight',fallback:10,format:v=>(v/10).toFixed(1)}
];
// Drafts are only held while dragging; host acknowledgement (or failure) owns
// the saved values. Rendering an unrelated state update must not move a slider.
const cardFaceDrafts=new Map();
function faceValue(config,key,fallback){return cardFaceDrafts.get(key)??config[key]??fallback;}
function applyCardFaceSettings(){
 const config=state?.settings||{},root=document.documentElement;
 for(const field of appearanceFontFields){
  const family=appearanceFonts[config[field.key]||0]?.family;
  if(family)root.style.setProperty('--'+field.id,family);else root.style.removeProperty('--'+field.id);
 }
 root.dataset.rankLayout=config.rankLayout===1?'classic':'edges';
 root.dataset.cardNames=config.showCardNames===false?'hidden':'shown';
 root.dataset.nameplate=['band','fade','print'][config.nameplateStyle||0]||'band';
 root.dataset.nameCase=['small-caps','title','upper'][config.nameCase||0]||'small-caps';
 const strength=faceValue(config,'rankShadowOpacity',100)/100;
 // em units keep a consistent finish from small hand cards to large previews.
 const blur=faceValue(config,'rankShadowBlur',2)/32,offset=faceValue(config,'rankShadowOffset',1)/32;
 root.style.setProperty('--rank-shadow',`0 ${offset}em ${blur}em rgba(5,9,7,${.93*strength}),0 0 ${blur*2}em rgba(0,0,0,${.67*strength})`);
 root.style.setProperty('--rank-stroke',config.rankStroke?`${faceValue(config,'rankStrokeWeight',10)/320}em`:'0px');
}
function renderCardFaceSettings(){
 if(!state||$('settings-panel').hidden)return;
 const config=state.settings||{},locked=Boolean(config.capturing||bindingPending)||state.phase!=='ready';
 for(const field of appearanceFontFields){
  const control=$(field.id);control.value=String(appearanceFonts[config[field.key]]?config[field.key]:0);
  control.disabled=locked||field.key==='cardNameFont'&&config.showCardNames===false;control._refresh();
 }
 for(const [index,name] of ['edges','classic'].entries()){
  const button=$('card-face-'+name);button.disabled=locked;button.setAttribute('aria-pressed',String((config.rankLayout===1?1:0)===index));
 }
 for(const [id,key,fallback] of [['card-face-names','showCardNames',true],['rank-stroke','rankStroke',false]]){
  const control=$(id),on=config[key]??fallback;control.disabled=locked;control.setAttribute('aria-checked',String(on));control.lastElementChild.textContent=on?'On':'Off';
 }
 for(const [id,key] of [['card-nameplate','nameplateStyle'],['card-name-case','nameCase']]){
  const control=$(id);control.value=String(config[key]||0);control.disabled=locked||config.showCardNames===false;control._refresh();
 }
 for(const field of cardFaceRanges){
  const value=faceValue(config,field.key,field.fallback),control=$(field.id);
  control.value=String(value);control.disabled=locked||field.key==='rankStrokeWeight'&&!config.rankStroke;
  $(field.id+'-value').textContent=field.format(value);control.setAttribute('aria-valuetext',field.format(value));
 }
 $('rank-finish-reset').disabled=locked;
}
for(const field of appearanceFontFields){
 const control=$(field.id);
 control.replaceChildren(...appearanceFonts.map((font,i)=>new Option(font.name,String(i))));control._refresh();
 control.onchange=()=>{control.disabled=true;control._refresh();send('settings',`${field.id} ${control.value}`);};
}
for(const [index,name] of ['edges','classic'].entries())$('card-face-'+name).onclick=()=>{
 if($('card-face-'+name).disabled)return;
 for(const option of ['edges','classic'])$('card-face-'+option).disabled=true;
 send('settings',`card-ranks ${index}`);
};
for(const [id,key,command,fallback] of [['card-face-names','showCardNames','card-names',true],['rank-stroke','rankStroke','rank-stroke',false]])$(id).onclick=()=>{
 const control=$(id);if(control.disabled)return;control.disabled=true;send('settings',`${command} ${(state.settings?.[key]??fallback)?0:1}`);
};
for(const [id,command] of [['card-nameplate','nameplate'],['card-name-case','name-case']])$(id).onchange=()=>{
 const control=$(id);control.disabled=true;control._refresh();send('settings',`${command} ${control.value}`);
};
for(const field of cardFaceRanges){
 const input=$(field.id);
 input.oninput=()=>{cardFaceDrafts.set(field.key,Number(input.value));applyCardFaceSettings();$(field.id+'-value').textContent=field.format(Number(input.value));input.setAttribute('aria-valuetext',field.format(Number(input.value)));};
 input.onchange=()=>{const value=Number(input.value);cardFaceDrafts.delete(field.key);send('settings',`${field.id} ${value}`);};
}
$('rank-finish-reset').onclick=()=>{if($('rank-finish-reset').disabled)return;$('rank-finish-reset').disabled=true;send('settings','rank-finish-reset 0');};
