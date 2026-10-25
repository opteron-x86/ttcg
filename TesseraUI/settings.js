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
 renderCollectionRecovery();
}
let collectionRecoveryConfirmation=null;
function collectionRecoveryDisabled(action){
 return state?.screen!=='album'||state.phase!=='ready'||state.interruptedMatch||state.collection?.recoveryBlocked||(action==='reset'&&state.collection?.resetBlocked)||state.collection?.staked||state.collection?.paying||bindings().capturing||bindingPending;
}
function renderCollectionRecovery(){
 const missing=state.collection?.missingSets||[];
 $('collection-recovery').hidden=!missing.length;
 $('collection-missing-sets').textContent=`Missing expansions: ${missing.join(', ')}. Reinstall them to restore your cards, or remove their saved copies.`;
 $('collection-remove-missing').disabled=Boolean(collectionRecoveryDisabled());
 $('dev-clear-collection').disabled=Boolean(collectionRecoveryDisabled());
 $('dev-reset-tessera').disabled=Boolean(collectionRecoveryDisabled('reset'));
 $('collection-recovery-blocked').textContent=state.collection?.recoveryBlocked||'';
 $('development-clear-help').textContent=state.collection?.recoveryBlocked||'Start collecting again with an empty album. Your game progress is kept.';
 $('development-reset-help').textContent=state.collection?.resetBlocked||state.collection?.recoveryBlocked||'Start Tessera over with your album and starter cards. All Tessera progress is erased.';
}
function closeCollectionRecovery(restore=true){
 const pending=collectionRecoveryConfirmation;
 if($('collection-recovery-confirm').hidden)return false;
 $('collection-recovery-confirm').hidden=true;collectionRecoveryConfirmation=null;
 if(restore&&pending)$(pending.button).focus({preventScroll:true});return true;
}
function confirmCollectionRecovery(action,button){
 if(collectionRecoveryDisabled(action)||action!=='remove-missing'&&!developmentSettings()?.enabled)return;
 const clear=action==='clear',reset=action==='reset';collectionRecoveryConfirmation={action,button,session:state.session,revision:state.revision};
 $('collection-recovery-confirm-title').textContent=reset?'Reset Tessera for this character?':clear?'Clear your collection?':'Remove unavailable cards?';
 $('collection-recovery-confirm-copy').textContent=reset?
  'Erases your collection, decks, records, reputation, tournaments and regional rule changes. Resets NPC and merchant collections, cancels card exchanges, and removes carried packs and old invitations. Keeps your album and gives eight starter cards and a starter deck. Gold and interface preferences stay. Development overrides return to defaults for all characters. Restore an earlier save to undo.':clear?
  'Deletes every card and foil, including unavailable cards, clears discovery marks and empties all decks for this character. Your album, gold and game progress are kept. Automatic card grants and Reveal all cards will be turned off for all characters. Restore an earlier save to recover deleted cards.':
  'Removes unavailable cards and their discovery marks from this character, and clears their deck slots. Installed cards and game progress are kept. Reinstalling the expansions will no longer restore these copies. Keep an earlier save if you may want them back.';
 $('collection-recovery-confirm-action').textContent=reset?'Reset Tessera':clear?'Clear collection':'Remove cards';
 closeSelectMenu();$('collection-recovery-confirm').hidden=false;$('collection-recovery-cancel').focus();
}
$('dev-clear-collection').onclick=()=>confirmCollectionRecovery('clear','dev-clear-collection');
$('dev-reset-tessera').onclick=()=>confirmCollectionRecovery('reset','dev-reset-tessera');
$('collection-remove-missing').onclick=()=>confirmCollectionRecovery('remove-missing','collection-remove-missing');
$('collection-recovery-cancel').onclick=()=>closeCollectionRecovery();
$('collection-recovery-confirm-action').onclick=()=>{
 const pending=collectionRecoveryConfirmation;closeCollectionRecovery();
 if(!pending||pending.session!==state?.session||pending.revision!==state.revision||collectionRecoveryDisabled(pending.action)||pending.action!=='remove-missing'&&!developmentSettings()?.enabled)return;
 $('collection-remove-missing').disabled=$('dev-clear-collection').disabled=$('dev-reset-tessera').disabled=true;
 send('collection-recovery',`${pending.action} confirm`);
};
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
