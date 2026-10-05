'use strict';
let settingsSection='appearance',settingsNavKey='',settingsPreviewID=-1,settingsPreviewBack=false;
const settingsSections=[{id:'appearance',title:'Appearance'},{id:'controls',title:'Controls'},{id:'gameplay',title:'Gameplay'},{id:'developer',title:'Developer'}];
function setSettingsSection(section){
 if(!settingsSections.some(s=>s.id===section)||section==='developer'&&!developmentSettings()?.enabled)return;
 closeSettings();closeSelectMenu();settingsSection=section;renderSettings();
 $('settings-panel').querySelector('.settings-body').scrollTop=0;
}
function renderWindowSettings(){
 const size=Number($('window-size').value),full=size===100;
 const toggle=$('settings-fullscreen');toggle.setAttribute('aria-checked',String(full));toggle.lastElementChild.textContent=full?'On':'Off';
 const select=$('settings-window-size');select.disabled=full;select.value=String(full?windowedSize:size);select._refresh?.();
}
function renderSettings(){
 if(!state||$('settings-panel').hidden)return;
 const entries=settingsSections.filter(s=>s.id!=='developer'||developmentSettings()?.enabled);
 if(!entries.some(s=>s.id===settingsSection))settingsSection='appearance';
 const key=entries.map(s=>s.id).join(',')+settingsSection;
 if(key!==settingsNavKey){settingsNavKey=key;renderChapterNavigation($('settings-nav'),entries,settingsSection,setSettingsSection,'settings-section-');}
 for(const s of settingsSections)$('settings-'+s.id).hidden=s.id!==settingsSection;
 $('settings-title').textContent=entries.find(s=>s.id===settingsSection).title;
 $('settings-preview').hidden=settingsSection!=='appearance';
 renderWindowSettings();renderBindings();renderSettingsPreview();
}
function settingsPreviewCards(){
 return state.cards.map((_,i)=>i).filter(i=>albumKnown(i)&&!state.cards[i].foil);
}
function renderSettingsPreview(){
 if(!state||$('settings-panel').hidden||settingsSection!=='appearance')return;
 const ids=settingsPreviewCards();
 if(!ids.includes(settingsPreviewID))settingsPreviewID=state.collection?.deck?.find(id=>ids.includes(id))??ids[0]??-1;
 const mount=$('settings-preview-card'),key=`${state.session}:${settingsPreviewID}:${settingsPreviewBack}`;
 if(mount.dataset.card!==key){
  mount.dataset.card=key;mount.replaceChildren();
  if(settingsPreviewBack||settingsPreviewID<0){const back=document.createElement('div');back.className='card card-back';mount.append(back);}
  else mount.append(cardElement(settingsPreviewID,0,0,true));
 }
 $('settings-preview-name').textContent=settingsPreviewID>=0?state.cards[settingsPreviewID].name:'Card back';
 $('settings-preview-turn').textContent=settingsPreviewBack?'Show front':'Show back';
 $('settings-preview-turn').disabled=settingsPreviewID<0;
 for(const direction of ['previous','next'])$('settings-preview-'+direction).disabled=ids.length<2;
}
for(const [name,step] of [['previous',-1],['next',1]])$('settings-preview-'+name).onclick=()=>{
 const ids=settingsPreviewCards();if(ids.length<2)return;
 settingsPreviewID=ids[(ids.indexOf(settingsPreviewID)+step+ids.length)%ids.length];settingsPreviewBack=false;renderSettingsPreview();
};
$('settings-preview-turn').onclick=()=>{settingsPreviewBack=!settingsPreviewBack;renderSettingsPreview();};
$('settings-fullscreen').onclick=()=>{
 $('window-size').value=String(Number($('window-size').value)===100?windowedSize:100);
 resizeWindow();$('window-size')._refresh();renderWindowSettings();
};
$('settings-window-size').onchange=()=>{$('window-size').value=$('settings-window-size').value;resizeWindow();$('window-size')._refresh();};
// Keep keyboard/controller focus visible without scrolling the entire album.
$('settings-panel').querySelector('.settings-body').addEventListener('focusin',event=>event.target.scrollIntoView({block:'nearest',inline:'nearest'}));
function settingsRangeKey(key){
 const input=document.activeElement;if(!input?.matches('#settings-panel input[type="range"]')||input.disabled||!['left','right'].includes(key))return false;
 if(key==='left')input.stepDown();else input.stepUp();
 input.dispatchEvent(new Event('input',{bubbles:true}));input.dispatchEvent(new Event('change',{bubbles:true}));return true;
}
