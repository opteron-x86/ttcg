'use strict';
const defaultCardBack={id:'mosaic',name:'Mosaic',art:'backs/mosaic.png',custom:false};
const cardBackCacheEpoch=Date.now();
let cardBackCatalogKey='',cardBackImageKey='',cardBackImageError='';
function cardBackUrl(path){return `${artUrl(path)}?back=${cardBackCacheEpoch}-${state?.cardBackVersion||0}`;}
function updateCardBackImage(){
 const path=state.cardBack||defaultCardBack.art,url=cardBackUrl(path);
 if(url===cardBackImageKey)return;
 cardBackImageKey=url;cardBackImageError='';
 const image=new Image();
 image.onload=()=>{
  if(cardBackImageKey!==url)return;
  document.documentElement.style.setProperty('--card-back-image',`url("${url}")`);
 };
 image.onerror=()=>{
  if(cardBackImageKey!==url)return;
  document.documentElement.style.setProperty('--card-back-image',`url("${cardBackUrl(defaultCardBack.art)}")`);
  cardBackImageError='This image couldn’t be opened. Using Mosaic.';renderCardBackSettings();
 };
 image.src=url;
}
function cardBackSettingsLocked(){return state?.phase!=='ready'||Boolean(bindings().capturing||bindingPending);}
function renderCardBackSettings(){
 if(!state||$('settings-panel').hidden)return;
 const choices=state.cardBacks||[defaultCardBack],grid=$('card-back-choices');
 const key=JSON.stringify([state.cardBackVersion||0,choices]);
 if(key!==cardBackCatalogKey){
  cardBackCatalogKey=key;
  const focus=document.activeElement?.dataset.back,scroll=$('settings-panel').querySelector('.settings-body').scrollTop;
  grid.replaceChildren();
  for(const back of choices){
   const button=document.createElement('button');button.id='card-back-choice-'+encodeURIComponent(back.id);button.className='card-back-choice';button.dataset.back=back.id;button.dataset.loaded='pending';
   const portrait=document.createElement('span');portrait.className='card-back-thumb';
   const image=document.createElement('img');image.alt='';image.draggable=false;
   image.onload=()=>{button.dataset.loaded='true';renderCardBackSettings();};
   image.onerror=()=>{button.dataset.loaded='false';renderCardBackSettings();};
   image.src=cardBackUrl(back.art);portrait.append(image);
   const label=document.createElement('span');label.className='card-back-label';
   const name=document.createElement('span');name.className='card-back-name';name.textContent=back.name;
   const detail=document.createElement('span');detail.className='card-back-detail';label.append(name,detail);
   button.append(portrait,label);
   button.onclick=()=>{
    if(button.disabled||cardBackSettingsLocked())return;
    if(state.cardBackID!==back.id||state.cardBackMissing)send('cardback',`select ${back.id}`);
   };
   grid.append(button);
  }
  const focused=[...grid.children].find(button=>button.dataset.back===focus);
  focused?.focus({preventScroll:true});$('settings-panel').querySelector('.settings-body').scrollTop=scroll;
 }
 const locked=cardBackSettingsLocked();
 for(const button of grid.children){
  const back=choices.find(b=>b.id===button.dataset.back),failed=button.dataset.loaded==='false',selected=back.id===(state.cardBackID||'mosaic');
  button.disabled=locked||button.dataset.loaded!=='true';
  button.setAttribute('aria-pressed',String(selected));
  button.setAttribute('aria-label',back.name+(failed?' — image unavailable':''));
  button.querySelector('.card-back-detail').textContent=failed?'Unavailable':back.custom?'Custom':'';
 }
 $('card-back-refresh').disabled=locked;
 $('card-back-status').textContent=state.settings?.cardBackNotice||cardBackImageError||(state.cardBackMissing?'Saved card back not found. Using Mosaic.':'');
}
$('card-back-refresh').onclick=()=>{if(!cardBackSettingsLocked())send('cardback','refresh');};
