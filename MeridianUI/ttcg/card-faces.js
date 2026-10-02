'use strict';
function applyCardFaceSettings(){
 document.documentElement.dataset.rankLayout=state?.settings?.rankLayout===1?'classic':'edges';
 document.documentElement.dataset.cardNames=state?.settings?.showCardNames===false?'hidden':'shown';
}
function renderCardFaceSettings(){
 if(!state||$('settings-panel').hidden)return;
 const config=state.settings||{},locked=Boolean(config.capturing||bindingPending)||state.phase!=='ready';
 const cardID=state.collection?.deck?.find(id=>id>=0)??0;
 for(const [index,name] of ['edges','classic'].entries()){
  const button=$('card-face-'+name),preview=button.querySelector('.card-face-preview');
  button.disabled=locked;button.setAttribute('aria-pressed',String((config.rankLayout===1?1:0)===index));
  if(preview.dataset.card!==String(cardID)||!preview.firstChild){preview.replaceChildren(cardElement(cardID,0,0,true));preview.dataset.card=cardID;}
 }
 const names=$('card-face-names'),shown=config.showCardNames!==false;
 names.disabled=locked;names.setAttribute('aria-checked',String(shown));names.lastElementChild.textContent=shown?'On':'Off';
}
for(const [index,name] of ['edges','classic'].entries())$('card-face-'+name).onclick=()=>{
 if($('card-face-'+name).disabled)return;
 for(const option of ['edges','classic'])$('card-face-'+option).disabled=true;
 send('settings',`card-ranks ${index}`);
};
$('card-face-names').onclick=()=>{
 const control=$('card-face-names');if(control.disabled)return;
 control.disabled=true;send('settings',`card-names ${state.settings?.showCardNames===false?1:0}`);
};
