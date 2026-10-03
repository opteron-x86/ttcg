'use strict';
const defaultCardBack={id:'mosaic',name:'Mosaic',art:'backs/mosaic.png',custom:false};
const cardBackCacheEpoch=Date.now();
let cardBackCatalogKey='',cardBackImageKey='',cardBackImageError='',cardBackObjectURL='';
function cardBackUrl(path){return `${artUrl(path)}?back=${cardBackCacheEpoch}-${state?.cardBackVersion||0}`;}
function loadCardBack(image,path,failed=()=>{}){artImages.set(image,path,{version:`${cardBackCacheEpoch}-${state?.cardBackVersion||0}`,failed});}
function applyCardBackImage(image){
 // Large data URIs exceed Chromium's CSS custom-property value limit. Keep
 // one blob alive for the selected back, including cards created later.
 const previous=cardBackObjectURL;let source=image.src;cardBackObjectURL='';
 if(source.startsWith('data:image/')){
  const comma=source.indexOf(','),mime=source.slice(5,source.indexOf(';'));
  const bytes=Uint8Array.from(atob(source.slice(comma+1)),c=>c.charCodeAt(0));
  source=cardBackObjectURL=URL.createObjectURL(new Blob([bytes],{type:mime}));
 }
 document.documentElement.style.setProperty('--card-back-image',`url("${source}")`);
 if(previous)URL.revokeObjectURL(previous);
}
function updateCardBackImage(){
 const path=state.cardBack||defaultCardBack.art,url=cardBackUrl(path);
 if(url===cardBackImageKey)return;
 cardBackImageKey=url;cardBackImageError='';
 const image=new Image();
 image.onload=()=>{
  if(cardBackImageKey!==url)return;
  applyCardBackImage(image);
 };
 loadCardBack(image,path,()=>{
  if(cardBackImageKey!==url)return;
  const fallback=new Image();fallback.onload=()=>{if(cardBackImageKey===url)applyCardBackImage(fallback);};
  loadCardBack(fallback,defaultCardBack.art);
  cardBackImageError='This image couldn’t be opened. Using Mosaic.';renderCardBackSettings();
 });
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
   loadCardBack(image,back.art,()=>{button.dataset.loaded='false';renderCardBackSettings();});portrait.append(image);
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
