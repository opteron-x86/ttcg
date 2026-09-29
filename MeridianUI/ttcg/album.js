'use strict';
let albumTab='cards',collectionInspectedArt='',albumTarget=-1,albumActiveDeck=null;
const albumViews=Object.fromEntries(['cards','decks'].map(key=>[key,{query:'',filter:'owned',sort:'tier',direction:'asc',tier:'0',group:'',affinity:'',rarity:'',finish:'',scroll:0}]));
const albumFilterKeys=['tier','group','affinity','rarity','finish'];
let albumViewKey='',albumSession='',albumDeck=[],albumSavedDeck='',inspected=-1,seenReveal=0,albumConfirmation=null,albumVisible=[];
const PACK_ID=-2;
const TOURNAMENT_PACK_ID=-3;
function albumPacks(){return [
 {id:PACK_ID,element:'album-card-pack',label:'Card pack',count:Number(state.collection.packs?.[0]||0),command:['open-pack']},
 {id:TOURNAMENT_PACK_ID,element:'album-tournament-pack',label:'Tournament pack',count:Number(state.collection.packs?.[1]||0),command:['tournament','pack 0']}
];}
let albumRoute=null;
function openLobbyAlbum(tab='cards',slot=-1){
 if(state?.phase!=='ready'||state.screen==='album')return;
 albumRoute={tab,slot};send('browse','album');
}
let rulesRenderedKey='',rulesTopic='play',rulesDemoGen=0,rulesDemoTimer=0,rulesFrameIndex=0,rulesSelected=false,rulesBusy=false;
function albumView(){return albumTab==='decks'?'decks':'cards';}
function rememberAlbumFilters(){
 const prefs=albumViews[albumViewKey||albumView()];
 prefs.query=$('album-search').value;for(const key of ['filter','sort','direction',...albumFilterKeys])prefs[key]=$('album-'+key).value;
}
function resetAlbum(){albumRoute=null;finishAlbumRename(false,false);stopRulesDemo();clearAlbumDrag();albumSession='';albumViewKey='';albumTarget=-1;inspected=-1;seenReveal=0;closeAlbumModal(false);closeAlbumPopover(false);$('album').hidden=true;}
function closeAlbumModal(restore=true){
 const wasOpen=!$('album-modal').hidden;$('album-modal').hidden=true;albumConfirmation=null;albumArtRotation=null;$('album-modal').classList.remove('inspecting');$('inspect-navigation').hidden=true;$('inspect-tools').hidden=true;
 if(restore&&wasOpen)$('inspect-card').focus({preventScroll:true});
}
function closeAlbumPopover(restore=true){
 if(!$('album-filter-panel').hidden){$('album-filter-panel').hidden=true;$('album-filters-toggle').setAttribute('aria-expanded','false');if(restore)$('album-filters-toggle').focus();return true;}
 return false;
}
function setAlbumTab(tab){
 if(!['cards','decks','opponents','tournaments','rules'].includes(tab)||state?.screen!=='album')return;
 stopRulesDemo();clearAlbumDrag();closeSelectMenu();closeAlbumPopover(false);albumTab=tab;renderAlbum();
 $('album-'+(tab==='opponents'?'opponents':tab)+'-tab').focus({preventScroll:true});
}
function openAlbumDeckForCard(id){
 setAlbumTab('decks');
 if(!albumVisible.includes(id)){
  $('album-search').value='';$('album-filter').value='owned';$('album-filter')._refresh();
  for(const key of albumFilterKeys){$('album-'+key).value=key==='tier'?'0':'';$('album-'+key)._refresh();}
  albumFiltersChanged();
 }
 inspected=id;renderAlbumInspection();
}
function saveAlbumDeck(){send('deck',Array.from({length:5},(_,h)=>albumDeck[h]??-1).join(' '));}
function albumExtras(id){if(state.cards[id].foil)return 0;return Math.max(0,state.collection.owned[id]-Math.max(1,protectedDeckCopies(id)));}
function albumKnown(id){return state.revealCards===true||state.collection.known?.[id]||state.collection.owned[id]>0;}
function albumCard(id,thumbnail=true){if(albumKnown(id))return cardElement(id,0,0,thumbnail);const back=document.createElement('div');back.className='unknown-card';back.textContent='?';return back;}
function refreshAlbumGroups(){
 const select=$('album-group'),groups=[...new Set(state.cards.flatMap(card=>card.groups||[]))].sort();
 const signature=JSON.stringify(groups);if(select.dataset.groups===signature)return;
 const selected=select.value;
 select.replaceChildren(new Option('All types',''),...groups.map(group=>new Option(group,group)));
 select.value=groups.includes(selected)?selected:'';select.dataset.groups=signature;select._refresh();
}
function renderAlbum(){
 if(albumPointer){albumClickAfter=performance.now()+200;clearAlbumDrag();}
 const focus=document.activeElement?.id,c=state.collection;
 refreshAlbumGroups();
 const route=albumRoute;if(route){albumTab=route.tab;albumRoute=null;}
 const tab=albumTab;
 const entering=albumSession!==String(state.session);
 if(entering){albumSession=String(state.session);albumDeck=deckSlots(c.deck);albumSavedDeck=JSON.stringify(c.deck);inspected=state.cards.findIndex(card=>card.art+Boolean(card.foil)===collectionInspectedArt);closeAlbumModal(false);closeAlbumPopover(false);}
 if(albumActiveDeck!==c.activeDeck){albumActiveDeck=c.activeDeck;albumTarget=-1;}
 if(route)albumTarget=route.slot;
 if(albumSavedDeck!==JSON.stringify(c.deck)){albumDeck=deckSlots(c.deck);albumSavedDeck=JSON.stringify(c.deck);}
 $('album').dataset.view=tab;
 $('album-tabs').hidden=false;
 for(const key of ['cards','decks','opponents','tournaments','rules'])$('album-'+key+'-tab').setAttribute('aria-pressed',String(tab===key));
 renderTournamentBadge();
 $('album-count').hidden=tab!=='cards';
 const discovered=identityTotal(id=>c.known?.[id]||c.owned[id]>0);
 $('album-count').textContent=`${identityTotal(id=>c.owned[id]>0)} owned · ${discovered}/${identityTotal(()=>true)} discovered`;
 $('album-back').hidden=!(state.matchContext??Boolean(state.opponent));
 $('album-tournaments').hidden=tab!=='tournaments';$('opponents-panel').hidden=tab!=='opponents';$('album-rules').hidden=tab!=='rules';$('album-workspace').hidden=tab==='opponents'||tab==='rules'||tab==='tournaments';
 $('album-status').textContent=c.notice||'';$('album-status').hidden=!c.notice;
 if(tab==='tournaments'){renderTournaments();renderAlbumReveal();return;}
 if(tab==='opponents'){renderOpponents();return;}
 if(tab==='rules'){renderAlbumRules();return;}
 const key=albumView();
 if(albumViewKey!==key||entering){
  albumViewKey=key;const prefs=albumViews[key];$('album-search').value=prefs.query;
  for(const prop of ['filter','sort','direction',...albumFilterKeys]){$('album-'+prop).value=prefs[prop];$('album-'+prop)._refresh();}
 }
 const editing=tab==='decks';
 $('saved-decks-panel').hidden=!editing;$('album-deck-editor').hidden=!editing;
 renderDeckTools('album');if(editing)renderAlbumDeck();
 $('album-filter').disabled=false;$('album-filter')._refresh();$('album-filter').nextElementSibling.hidden=false;
 const query=$('album-search').value.trim().toLowerCase(),filter=$('album-filter').value,tier=Number($('album-tier').value),group=$('album-group').value,affinity=$('album-affinity').value,rarity=$('album-rarity').value;
 albumVisible=state.cards.map((_,id)=>id).filter(id=>{
  const card=state.cards[id],n=c.owned[id];
  if(!albumKnown(id)||tier&&tier!==card.tier)return false;
  const finish=$('album-finish').value;if(finish&&(finish==='foil')!==Boolean(card.foil))return false;
  if(query&&![card.name,...(card.groups||[]),...(card.subtypes||[]),...(card.affinities||[]),card.rarity].join(' ').toLowerCase().includes(query))return false;
  if(group&&!card.groups?.includes(group)||rarity&&rarity!==card.rarity)return false;
  if(affinity&&(affinity==='Neutral'?card.affinities?.length!==0:!card.affinities?.includes(affinity)))return false;
  return !((filter==='owned'&&!n)||(filter==='missing'&&n)||(filter==='extras'&&!albumExtras(id)));
 });
 const rarityOrder={Common:0,Rare:1,Epic:2,Legendary:3},sort=$('album-sort').value,direction=$('album-direction').value==='desc'?-1:1;
 albumVisible.sort((a,b)=>{const x=state.cards[a],y=state.cards[b];return direction*((sort==='tier'?x.tier-y.tier:sort==='rarity'?rarityOrder[x.rarity]-rarityOrder[y.rarity]:sort==='quantity'?(c.owned[a]-c.owned[b]):0)||x.name.localeCompare(y.name)||a-b);});
 const grid=$('album-grid');grid.replaceChildren();
 const availablePacks=tab==='cards'?albumPacks().filter(pack=>pack.count>0):[];
 const packs=$('album-pack-shelf');packs.replaceChildren();packs.hidden=!availablePacks.length;
 for(const pack of availablePacks){
  const button=document.createElement('button');button.id=pack.element;button.className='pack-tile';
  const art=document.createElement('span');art.className='pack-art';button.append(art);
  const label=document.createElement('span');label.className='pack-label';label.textContent=pack.label+'s';button.append(label);
  const count=document.createElement('span');count.className='owned-count';count.textContent=`×${pack.count}`;button.append(count);
  button.setAttribute('aria-label',`${pack.label}s: ${pack.count}`);
  button.onclick=()=>{if(performance.now()<albumClickAfter)return;inspected=pack.id;renderAlbumInspection();};
  packs.append(button);
 }
 for(const id of albumVisible){
  const card=state.cards[id],n=c.owned[id],button=document.createElement('button');button.id=`album-card-${id}`;button.className='collection-card';
  button.append(albumCard(id));button.setAttribute('aria-label',`${card.foil?'Foil · ':''}${card.name}, ${n} owned`);
  const count=document.createElement('span');count.className='owned-count';count.textContent=`×${n}`;button.append(count);
  const used=editing?albumDeck.filter(x=>x===id).length:0;
  if(used){const mark=document.createElement('span');mark.className='in-deck-count';mark.textContent=`Deck · ${used}`;button.append(mark);}
  button.onclick=()=>{if(performance.now()<albumClickAfter)return;inspected=id;renderAlbumInspection();};
  button.ondblclick=()=>{if(albumTab!=='decks')openAlbumDeckForCard(id);addAlbumCard(id);};
  button.dataset.cardDrag=id;button.dataset.draggable=String(editing&&canDragAlbumCard(id));button.draggable=false;grid.append(button);
 }
 if(!albumVisible.length){const empty=document.createElement('div');empty.className='collection-empty';empty.textContent=query||tier||group||affinity||rarity?'No matching cards':filter==='missing'?'No missing cards':filter==='extras'?'No extra cards':'No cards';grid.append(empty);}
 grid.scrollTop=albumViews[key].scroll;
 if(!availablePacks.some(pack=>pack.id===inspected)&&!albumVisible.includes(inspected))inspected=availablePacks[0]?.id??albumVisible[0]??-1;
 renderAlbumFilterSummary();renderAlbumInspection();
 if(!activeOverlay()){const target=focus?$(focus):null;if(target&&!target.disabled&&target.getClientRects().length)target.focus({preventScroll:true});else (grid.querySelector('button')||$('album-search')).focus({preventScroll:true});}
 renderAlbumReveal();
}
function renderAlbumReveal(){
 const c=state.collection;
 if(c.revealed?.length&&c.revealID!==seenReveal){
  seenReveal=c.revealID;albumConfirmation=null;closeSelectMenu();closeAlbumPopover(false);$('inspect-navigation').hidden=true;$('inspect-tools').hidden=true;$('album-modal').classList.remove('inspecting');
  $('album-modal-title').textContent=c.revealed.length===8?'Starter cards':c.revealKind==='tournament'?'Tournament pack':'Tessera pack';
  const cards=$('pack-reveal');cards.dataset.count=String(c.revealed.length);cards.replaceChildren();c.revealed.forEach((id,index)=>{const slot=document.createElement('div');slot.className='pack-card';slot.style.setProperty('--reveal-delay',`${index*90}ms`);slot.append(cardElement(id,0));cards.append(slot);});
  $('album-confirm').textContent='Continue';$('album-cancel').hidden=true;$('album-modal').hidden=false;$('album-confirm').focus();
 }
}
function renderAlbumDeck(){
 $('album-deck-label').textContent=`${deckCount(albumDeck)}/5`;
 const deck=$('album-deck');deck.replaceChildren();
 for(let h=0;h<5;h++){
  const button=document.createElement('button'),id=albumDeck[h];button.className='deck-slot';button.id=`album-deck-${h}`;
  if(id<0){button.classList.add('empty','needs-card');button.textContent='+';}
  else{button.append(cardElement(id,0));button.dataset.deckDrag=h;button.draggable=false;}
  button.setAttribute('aria-label',`Slot ${h+1}${id>=0?': '+state.cards[id].name:': empty'}`);button.setAttribute('aria-pressed',String(albumTarget===h));
  button.onclick=()=>{if(performance.now()<albumClickAfter)return;albumTarget=albumTarget===h?-1:h;renderAlbumDeck();renderAlbumInspection();$(button.id)?.focus({preventScroll:true});};
  const position=document.createElement('span');position.className='deck-position';position.textContent=h+1;position.setAttribute('aria-hidden','true');button.append(position);deck.append(button);
 }
 const selected=albumTarget>=0&&albumDeck[albumTarget]>=0;
 $('album-move-left').disabled=!selected||albumTarget<=0;$('album-move-right').disabled=!selected||albumTarget>=4;$('album-remove').disabled=!selected;
 const counts=prop=>{const tally=new Map();for(const id of albumDeck)if(id>=0){const values=state.cards[id][prop]||[];for(const value of values.length?values:prop==='affinities'?['Neutral']:[])tally.set(value,(tally.get(value)||0)+1);}return [...tally].sort((a,b)=>b[1]-a[1]||a[0].localeCompare(b[0]));};
 const composition=$('deck-composition');composition.replaceChildren();
 for(const prop of ['groups','affinities']){
  const row=document.createElement('span');row.className='deck-'+prop;row.setAttribute('aria-label',prop==='groups'?'Card types':'Affinities');
  for(const [name,n] of counts(prop)){
   const item=document.createElement('span');item.className='deck-composition-item';item.title=`${name}: ${n}`;item.setAttribute('aria-label',`${name}: ${n}`);
   if(prop==='affinities'&&name!=='Neutral')item.append(affinityIcon(name));
   else{const label=document.createElement('span');label.textContent=name;item.append(label);}
   const count=document.createElement('span');count.className='composition-count';count.textContent=n;item.append(count);row.append(item);
  }
  composition.append(row);
 }
 $('album-clear').disabled=deckCount(albumDeck)===0;
}
function renderAlbumFilterSummary(){
 const chips=$('album-filter-chips');chips.replaceChildren();
 for(const key of albumFilterKeys){const select=$('album-'+key);if(select.value===''||select.value==='0')continue;const button=document.createElement('button');button.className='filter-chip';button.textContent=select.selectedOptions[0].textContent+' ×';button.setAttribute('aria-label','Remove '+select.selectedOptions[0].textContent+' filter');button.onclick=()=>{select.value=key==='tier'?'0':'';select._refresh();albumFiltersChanged();};chips.append(button);}
 $('album-results').textContent=`${albumVisible.length} ${albumVisible.length===1?'card':'cards'}`;
 $('album-filters-clear').hidden=!chips.children.length&&!$('album-search').value;
 $('album-filters-toggle').textContent='Filters'+(chips.children.length?` · ${chips.children.length}`:'');
}
function renderAlbumInspection(){
 const c=state.collection,editing=albumTab==='decks';
 const pack=albumTab==='cards'?albumPacks().find(pack=>pack.id===inspected):null;
 document.querySelectorAll('#album-grid button,#album-pack-shelf button').forEach(button=>button.classList.toggle('inspected',button.id===(pack?pack.element:`album-card-${inspected}`)));
 if(inspected>=0)collectionInspectedArt=state.cards[inspected].art+Boolean(state.cards[inspected].foil);
 const preview=$('inspect-card');preview.replaceChildren();
 preview.closest('.album-inspector').classList.toggle('inspecting-pack',Boolean(pack));
 $('inspect-add').hidden=Boolean(pack)||!editing;
 $('inspect-reason').textContent='';
 if(pack){const art=document.createElement('span');art.className='pack-art';preview.append(art);}
 else if(inspected>=0)preview.append(albumCard(inspected,false));
 if(pack){
  const n=pack.count;
  $('inspect-count').textContent=pack.label;$('inspect-card').disabled=true;
  const metadata=$('inspect-metadata');metadata.replaceChildren();
  $('inspect-open-pack').hidden=n<1;$('inspect-open-pack').disabled=n<1;
  $('inspect-reason').textContent='';
  return;
 }
 $('inspect-open-pack').hidden=true;
 const valid=inspected>=0&&albumKnown(inspected),owned=valid?c.owned[inspected]:0;
 $('inspect-count').textContent=valid?`${owned} owned · Tier ${['','I','II','III','IV','V','VI','VII','VIII','IX','X'][state.cards[inspected].tier]}`:'';$('inspect-card').disabled=!valid;
 const metadata=$('inspect-metadata');metadata.replaceChildren();
 renderCardMetadata(metadata,valid?state.cards[inspected]:null);
 $('inspect-add').hidden=!editing;
 const target=editing?albumTarget:-1,full=deckCount(albumDeck)===5,canAdd=valid&&canAddAlbumCard(inspected,target);
 const choose=valid&&owned>0&&full&&target<0;
 $('inspect-add').hidden=!editing||choose;
 $('inspect-add').textContent=target>=0&&albumDeck[target]>=0?`Replace ${state.cards[albumDeck[target]].name}`:'Add to deck';
 $('inspect-add').disabled=!(canAdd||choose);$('inspect-add').title=valid?(state.cards[inspected].unique?'One per deck':'Two per deck'):'';
 $('inspect-reason').textContent=!editing||!valid?'':!owned?'Not owned':choose?'Select a card in your deck to replace.':!canAdd?'Already at limit':'';
}
function canAddAlbumCard(id,index=-1){
 if(state?.screen!=='album'||!Number.isInteger(id)||id<0||id>=state.cards.length)return false;
 const replace=index>=0&&(deckCount(albumDeck)===5||index===albumTarget),used=albumDeck.filter((card,h)=>card===id&&(!replace||h!==index)).length;
 return (deckCount(albumDeck)<5||replace)&&state.collection.owned[id]>used&&(state.cards[id].unique?1:2)>albumDeck.filter((card,h)=>card>=0&&cardIdentity(card)===cardIdentity(id)&&(!replace||h!==index)).length;
}
function canDragAlbumCard(id){return canAddAlbumCard(id,albumTarget)||albumDeck.some((_,h)=>canAddAlbumCard(id,h));}
function addAlbumCard(id,index=albumTarget>=0?albumTarget:albumDeck.indexOf(-1)){
 if(!canAddAlbumCard(id,index))return;
 if(index>=0&&(deckCount(albumDeck)===5||index===albumTarget))albumDeck[index]=id;else insertDeckCard(albumDeck,id,index);
 albumTarget=-1;saveAlbumDeck();renderAlbum();
}
function moveAlbumCard(from,to){
 if(state?.screen!=='album'||from<0||from>=5||albumDeck[from]<0||to<0||to>4||from===to)return;
 const [card]=albumDeck.splice(from,1);albumDeck.splice(to,0,card);albumTarget=to;saveAlbumDeck();renderAlbum();$(`album-deck-${to}`)?.focus({preventScroll:true});
}
let albumDrag=null,albumPointer=null,albumClickAfter=0;
function clearAlbumDrag() {
 if(albumDrag)albumClickAfter=performance.now()+200;
 albumPointer?.ghost?.remove();
 const pointer=albumPointer;albumPointer=null;albumDrag=null;
 if(pointer?.source.hasPointerCapture?.(pointer.id)) pointer.source.releasePointerCapture(pointer.id);
 document.querySelectorAll('.deck-drop,.deck-dragging').forEach(el=>el.classList.remove('deck-drop','deck-dragging'));
}
// Keep drag handling inside the page. CEF does not need to start a desktop drag.
function validAlbumDrop(index=-1) {
 return albumDrag&&state?.screen==='album'&&albumDrag.session===state.session&&(albumDrag.type==='deck'||canAddAlbumCard(albumDrag.value,index));
}
$('album').addEventListener('pointerdown',event=>{
 if(event.button!==0||state?.screen!=='album'||albumTab!=='decks'||!$('album-modal').hidden) return;
 const source=event.target.closest('[data-card-drag],[data-deck-drag]');if(!source)return;
 const type=source.dataset.deckDrag!==undefined?'deck':'card',value=Number(type==='deck'?source.dataset.deckDrag:source.dataset.cardDrag);
 if(type==='card'&&!canDragAlbumCard(value))return;
 clearAlbumDrag();albumPointer={id:event.pointerId,source,type,value,session:state.session,x:event.clientX,y:event.clientY,ghost:null};
});
window.addEventListener('pointermove',event=>{
 const pointer=albumPointer;if(!pointer||event.pointerId!==pointer.id)return;
 if(pointer.session!==state?.session) {clearAlbumDrag();return;}
 if(!albumDrag&&Math.hypot(event.clientX-pointer.x,event.clientY-pointer.y)<6)return;
 event.preventDefault();
 if(!albumDrag) {
   albumDrag={type:pointer.type,value:pointer.value,session:pointer.session};
   pointer.source.setPointerCapture(pointer.id);
   const rect=pointer.source.getBoundingClientRect(),ghost=pointer.source.cloneNode(true);
   ghost.removeAttribute('id');ghost.querySelectorAll('[id]').forEach(el=>el.removeAttribute('id'));
   ghost.classList.add('album-drag-ghost');ghost.setAttribute('aria-hidden','true');
   Object.assign(ghost.style,{width:`${pointer.source.offsetWidth}px`,height:`${pointer.source.offsetHeight}px`,transform:`scale(${rect.width/pointer.source.offsetWidth})`});
   pointer.offsetX=pointer.x-rect.x;pointer.offsetY=pointer.y-rect.y;pointer.ghost=ghost;document.body.append(ghost);
   pointer.source.classList.add('deck-dragging');
 }
 Object.assign(pointer.ghost.style,{left:`${event.clientX-pointer.offsetX}px`,top:`${event.clientY-pointer.offsetY}px`});
 document.querySelectorAll('.deck-drop').forEach(el=>el.classList.remove('deck-drop'));
 const target=document.elementFromPoint(event.clientX,event.clientY)?.closest('#album-deck .deck-slot');
 if(target&&validAlbumDrop(Number(target.id.slice(11))))target.classList.add('deck-drop');
});
window.addEventListener('pointerup',event=>{
 if(!albumPointer||event.pointerId!==albumPointer.id)return;
 if(!albumDrag){clearAlbumDrag();return;}
 event.preventDefault();albumClickAfter=performance.now()+200;
 const target=document.elementFromPoint(event.clientX,event.clientY)?.closest('#album-deck .deck-slot');
 const drag=target&&validAlbumDrop(Number(target.id.slice(11)))?albumDrag:null;clearAlbumDrag();
 if(!target||!drag)return;
 const h=Number(target.id.slice(11));
 if(drag.type==='deck')moveAlbumCard(drag.value,h);else addAlbumCard(drag.value,h);
});
window.addEventListener('pointercancel',clearAlbumDrag);
window.addEventListener('blur',clearAlbumDrag);
window.addEventListener('keydown',event=>{
 if(state?.screen==='album'&&event.shiftKey&&event.target.id?.startsWith('album-deck-')&&['ArrowLeft','ArrowRight'].includes(event.key)) {
   event.preventDefault();const from=Number(event.target.id.slice(11));moveAlbumCard(from,from+(event.key==='ArrowRight'?1:-1));
 }
});
const RULE_PAGES=[
 {id:'play',title:'How to play',demo:'play',copy:'Each player has five cards. Take turns placing one card on the 3×3 board. A card captures an adjacent opponent when its touching rank is higher. The tenth, unplayed card still counts, so the total is always ten. More than five wins.'},
 {id:'open',title:'Open',demo:'open',copy:'Both hands stay visible after the deal.'},
 {id:'threeopen',title:'Three Open',demo:'threeopen',copy:'Three cards in each hand are shown. The other two stay hidden until played. Cannot combine with Open.'},
 {id:'hidden',title:'Hidden hands',demo:'hidden',copy:'With neither Open nor Three Open, unplayed opposing cards stay hidden. Played cards are revealed.'},
 {id:'same',title:'Same',demo:'same',copy:'If two or more touching ranks equal the adjacent cards, those enemy cards are captured. Combo then flips any lower adjacent ranks. Same uses printed ranks.'},
 {id:'plus',title:'Plus',demo:'plus',copy:'If two or more touching pairs add to the same total, those enemy cards are captured. Combo then flips any lower adjacent ranks. Plus uses printed ranks.'},
 {id:'reverse',title:'Reverse',demo:'reverse',copy:'Lower ranks capture higher ones. Equal ranks do not capture. Same and Plus still trigger from their matching pairs; Combo follows Reverse.'},
 {id:'affinity',title:'Affinity',demo:'affinity',copy:'At the start of the match, one to five board squares receive affinity markers. Placing a card that matches a marker’s affinity raises all four sides by 1. A mismatch, including Neutral, lowers them by 1. Unmarked squares are unchanged.'},
 {id:'legion',title:'Legion',demo:'legion',copy:'When a play makes three or more cards of one creature group, choose that group. Those cards gain +1 on every side. Cannot combine with Decimation.'},
 {id:'decimation',title:'Decimation',demo:'decimation',copy:'When a play makes three or more cards of one creature group, choose that group. Those cards lose 1 on every side. Cannot combine with Legion.'},
 {id:'order',title:'Order',demo:'order',copy:'Play your cards in deck order. You still choose where to place each card. After a Sudden Death redeal, your original cards come first in their saved order, followed by acquired cards in their original order. Cannot combine with Chaos.'},
 {id:'chaos',title:'Chaos',demo:'chaos',copy:'At the start of each turn, one remaining card is chosen at random. You choose its square. This applies to both players. Cannot combine with Order.'},
 {id:'fallenace',title:'Fallen Ace',demo:'fallenace',copy:'A printed 1 captures a printed A, and A cannot capture 1. With Reverse, A captures 1 instead. These two printed ranks override affinity and creature bonuses when they meet; all other comparisons use their adjusted values.'},
 {id:'suddenDeath',title:'Sudden Death',demo:'suddenDeath',copy:'A draw starts another board with the same rules. Each player receives the five cards they control, including the unplayed card. Known cards stay known. Board affinities, creature bonuses and the starting player reset. After five redeals, another draw ends the game. Trades and wagers settle only once, at the end.'},
 {id:'samewall',title:'Same Wall',demo:'samewall',copy:'The edge of the board counts as A for Same. Match a printed A to the wall and another printed rank to an adjacent enemy to capture it and start Combo. Requires Same. Walls do not count toward Plus.'},
 {id:'swap',title:'Swap',demo:'swap',copy:'One random card from each hand changes sides before the first turn. The swap happens once, even with Sudden Death. Your deck stays unchanged. Trades use the original hands; Direct awards the cards each player controls at the end.'},
 {id:'trade',title:'Trade',demo:'',copy:'None, One, Diff, Direct or All decides which cards change hands after a staked game. Casual games never trade cards or gold.'}
];
function stopRulesDemo(){rulesRenderedKey='';rulesDemoGen++;rulesBusy=false;if(rulesDemoTimer){clearTimeout(rulesDemoTimer);rulesDemoTimer=0;}}
function rulesFrames(){return typeof ruleDemos==='object'?ruleDemos[rulesTopic]:null;}
function rulesVisibilityOnly(){return ['open','threeopen','hidden'].includes(rulesTopic);}
function renderRulesDemoFrame(frame,previous=null){
 const staticDisplay=rulesVisibilityOnly();
 $('rules-demo').classList.toggle('visibility-example',staticDisplay);
 $('rules-demo-caption').hidden=$('rules-demo-controls').hidden=staticDisplay;
 const board=$('rules-demo-board');board.replaceChildren();
 (frame.board||[]).forEach((cell,i)=>{
  const el=document.createElement('button');el.className='demo-square';
  el.setAttribute('aria-label',`Row ${Math.floor(i/3)+1}, column ${i%3+1}${cell?': '+description(cell.card):''}`);
  if(cell){const owner=previous&&cell.flip?previous.board[i].owner:cell.owner;const card=cardElement(cell.card,owner,frame.modifiers?.[i]||0);card.append(ownershipCorners());el.append(card);}
  else if(frame.tiles?.[i]){const marker=document.createElement('span');marker.className='tile-affinity';marker.append(affinityIcon(affinityNames[Math.log2(frame.tiles[i])]));el.append(marker);}
  el.disabled=staticDisplay||rulesBusy||!rulesSelected||i!==frame.square;
  el.classList.toggle('demo-target',!el.disabled);
  el.onclick=()=>advanceRulesDemo();board.append(el);
 });
 for(let p=0;p<2;p++){
  const hand=$(p?'rules-rival-hand':'rules-player-hand');hand.replaceChildren();
  frame.hands[p].forEach((id,h)=>{
   const el=document.createElement('button');el.className='rules-hand-card';el.style.setProperty('--hand-index',h);
   if(id!==null){el.append(cardElement(id,p,frame.handModifiers?.[p]?.[h]||0));el.setAttribute('aria-label',description(id));}else{el.classList.add('spent');el.setAttribute('aria-label','Played');}
   el.disabled=staticDisplay||rulesBusy||p!==0||h!==frame.hand;
   el.classList.toggle('demo-target',!el.disabled);el.classList.toggle('selected',!el.disabled&&rulesSelected);
   el.classList.toggle('swap-highlight',frame.swapped?.[p]===h);
   el.onclick=()=>{rulesSelected=true;renderRulesDemoFrame(frame);board.children[frame.square]?.focus({preventScroll:true});};hand.append(el);
  });
  $(p?'rules-rival-score':'rules-player-score').textContent=(previous||frame).score[p];
 }
 for(const [square,side] of frame.comparisons||[])board.children[square]?.querySelector('.rank.'+['top','right','bottom','left'][side])?.classList.add('demo-rank');
 $('rules-demo-caption').textContent=rulesBusy?'Watch the touching sides.':lessonPrompt(frame,rulesSelected);
 $('rules-demo-position').textContent=`${rulesFrameIndex+1} / ${rulesFrames().length}`;
 $('rules-demo-back').disabled=staticDisplay||rulesBusy||rulesFrameIndex===0;
 $('rules-demo-next').hidden=frame.hand>=0||rulesFrameIndex===rulesFrames().length-1;
 $('rules-demo-next').disabled=staticDisplay||rulesBusy;$('rules-demo-replay').disabled=staticDisplay||rulesBusy;
}
function advanceRulesDemo(){
 const frames=rulesFrames();if(rulesVisibilityOnly()||rulesBusy||!frames||rulesFrameIndex>=frames.length-1)return;
 const previous=frames[rulesFrameIndex],frame=frames[++rulesFrameIndex];rulesSelected=false;rulesBusy=true;
 renderRulesDemoFrame(frame,previous);
 const gen=rulesDemoGen,reduced=matchMedia('(prefers-reduced-motion:reduce)').matches;
 const stages=frame.stages?.length?frame.stages:[{captures:0}],board=$('rules-demo-board');let index=0;
 const finish=()=>{if(gen!==rulesDemoGen)return;rulesBusy=false;renderRulesDemoFrame(frame);const target=frame.hand>=0?$('rules-player-hand').children[frame.hand]:!$('rules-demo-next').hidden?$('rules-demo-next'):$('rules-demo-replay');target.focus({preventScroll:true});};
 const tick=()=>{
  if(gen!==rulesDemoGen)return;
  if(index>=stages.length){finish();return;}
  const stage=stages[index++];
  if(stage.rule)$('rules-demo-caption').textContent=ruleStageLabel(stage.rule);
  for(let i=0;i<9;i++)if(stage.captures&(1<<i)){
   const card=board.children[i].firstElementChild;card.classList.toggle('owner-0',frame.board[i].owner===0);card.classList.toggle('owner-1',frame.board[i].owner===1);
   if(!reduced)card.animate([{transform:'scaleX(.05)'},{transform:'scaleX(1)'}],{duration:360,easing:'ease-out'});
  }
  rulesDemoTimer=setTimeout(tick,reduced?0:900);
 };
 rulesDemoTimer=setTimeout(tick,reduced?0:650);
}
function playRulesDemo(key){
 stopRulesDemo();rulesFrameIndex=0;rulesSelected=false;
 const frames=typeof ruleDemos==='object'?ruleDemos[key]:null;
 $('rules-demo').hidden=!frames?.length;if(frames?.length)renderRulesDemoFrame(frames[0]);
 rulesRenderedKey=`${state.session}:${key}`;
}
$('rules-demo-next').onclick=advanceRulesDemo;
$('rules-demo-back').onclick=()=>{if(rulesBusy||rulesFrameIndex<1)return;rulesSelected=false;renderRulesDemoFrame(rulesFrames()[--rulesFrameIndex]);};
$('rules-demo-replay').onclick=()=>playRulesDemo(rulesTopic);
function renderAlbumRules(){
 const c=state.collection||{},hold=c.regions?.find(r=>r.id===c.holdID);
 const flags=hold?.rules??c.fixedRules??0,trade=tradeRuleText(hold?.trade);
 $('album-rules-hold').hidden=!hold;
 $('album-rules-hold').textContent=hold?`${hold.name} · ${rulesText(flags)||'Hidden hands'} · ${trade}`:'';
 const key=`${state.session}:${rulesTopic}`;if(rulesRenderedKey===key)return;
 renderChapterNavigation($('rules-list'),RULE_PAGES,rulesTopic,id=>{rulesTopic=id;renderAlbumRules();},'rules-topic-');
 const page=RULE_PAGES.find(p=>p.id===rulesTopic)||RULE_PAGES[0];
 $('rules-title').textContent=page.title;$('rules-copy').textContent=page.copy;
 if(page.demo)playRulesDemo(page.demo);else{$('rules-demo').hidden=true;stopRulesDemo();}
 rulesRenderedKey=key;
}
for(const tab of ['cards','decks','opponents','tournaments','rules'])$('album-'+tab+'-tab').onclick=()=>setAlbumTab(tab);
$('library-open').onclick=()=>state.collection?.tournamentID?openTournamentBoard(state.collection.tournamentID):openLobbyAlbum('cards');$('album-back').onclick=()=>send('browse','lobby');
$('album-clear').onclick=()=>{closeAlbumPopover();albumDeck=Array(5).fill(-1);albumTarget=-1;saveAlbumDeck();renderAlbum();};
$('album-remove').onclick=()=>{if(albumTarget<0)return;const h=albumTarget;albumDeck[h]=-1;saveAlbumDeck();renderAlbum();$('album-deck-'+h).focus();};
$('album-move-left').onclick=()=>moveAlbumCard(albumTarget,albumTarget-1);$('album-move-right').onclick=()=>moveAlbumCard(albumTarget,albumTarget+1);
$('inspect-add').onclick=()=>{
 const id=inspected;if(id<0||$('inspect-add').disabled)return;
 if(albumTab!=='decks')openAlbumDeckForCard(id);
 if(deckCount(albumDeck)===5&&albumTarget<0){$('album-deck-0').focus();return;}
 addAlbumCard(id);
};
function albumFiltersChanged(){rememberAlbumFilters();albumViews[albumView()].scroll=0;renderAlbum();}
$('album-search').addEventListener('input',albumFiltersChanged);
for(const key of ['filter','sort','direction',...albumFilterKeys])$('album-'+key).addEventListener('change',albumFiltersChanged);
$('album-grid').addEventListener('scroll',()=>{if(albumViewKey)albumViews[albumViewKey].scroll=$('album-grid').scrollTop;});
$('album-filters-toggle').onclick=()=>{const opening=$('album-filter-panel').hidden;closeSelectMenu();closeAlbumPopover(false);$('album-filter-panel').hidden=!opening;$('album-filters-toggle').setAttribute('aria-expanded',String(opening));if(opening)$('album-tier-trigger').focus();};
$('album-filters-done').onclick=()=>closeAlbumPopover();
$('album-filters-clear').onclick=()=>{$('album-search').value='';for(const key of albumFilterKeys){$('album-'+key).value=key==='tier'?'0':'';$('album-'+key)._refresh();}albumFiltersChanged();$('album-search').focus();};
document.addEventListener('pointerdown',event=>{
 if(!$('album-filter-panel').hidden&&!event.target.closest('#album-filter-panel,#album-filters-toggle'))closeAlbumPopover(false);
});
$('inspect-open-pack').onclick=()=>{const pack=albumPacks().find(pack=>pack.id===inspected);if(albumTab==='cards'&&pack?.count>0){$('inspect-open-pack').disabled=true;send(...pack.command);}};
$('album-confirm').onclick=()=>{const action=albumConfirmation;closeAlbumModal();action?.();};$('album-cancel').onclick=()=>closeAlbumModal();
// The album viewer shows the print and finish without gameplay overlays.
let albumArtRotation=null;
function createCardArtViewer(id){
 const scene=document.createElement('div');scene.className='art-scene zoom-card';scene.tabIndex=0;scene.setAttribute('role','group');
 scene.setAttribute('aria-label',state.cards[id].name+'. Drag or use arrow keys to rotate.');
 const turn=document.createElement('div');turn.className='art-turn';
 const front=cardElement(id,0);front.className='art-front'+(state.cards[id].foil?' foil':'');
 for(const child of [...front.children])if(!child.classList.contains('portrait'))child.remove();
 const back=document.createElement('div');back.className='art-back';
 turn.append(front,back);scene.append(turn);
 const rotation={x:0,y:0,pointer:null,update(){
  turn.style.transform=`rotateX(${this.x}deg) rotateY(${this.y}deg)`;
  const lightX=50+Math.sin(this.y*Math.PI/180)*45,lightY=38+Math.sin(this.x*Math.PI/180)*35;
  front.style.setProperty('--light-x',lightX+'%');front.style.setProperty('--light-y',lightY+'%');
  front.style.setProperty('--film-x',(50+Math.sin(this.y*Math.PI/360)*48)+'%');front.style.setProperty('--film-y',(50+Math.sin(this.x*Math.PI/360)*48)+'%');
  front.style.setProperty('--grain-x',(this.y%160)+'px');
  scene.dataset.side=Math.cos(this.x*Math.PI/180)*Math.cos(this.y*Math.PI/180)<0?'back':'front';
 }};
 scene.rotation=rotation;rotation.update();
 scene.onpointerdown=e=>{if(e.button!==0)return;e.preventDefault();scene.focus({preventScroll:true});scene.setPointerCapture(e.pointerId);rotation.pointer={id:e.pointerId,x:e.clientX,y:e.clientY,rx:rotation.x,ry:rotation.y};scene.classList.add('rotating');};
 scene.onpointermove=e=>{const p=rotation.pointer;if(!p||e.pointerId!==p.id)return;const scale=scene.getBoundingClientRect().width/scene.offsetWidth;rotation.y=p.ry+(e.clientX-p.x)/scale*.65;rotation.x=p.rx-(e.clientY-p.y)/scale*.65;rotation.update();};
 const release=()=>{rotation.pointer=null;scene.classList.remove('rotating');};
 scene.onpointerup=e=>{if(scene.hasPointerCapture(e.pointerId))scene.releasePointerCapture(e.pointerId);release();};scene.onpointercancel=release;scene.onlostpointercapture=release;
 scene.onkeydown=e=>{if(!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Home'].includes(e.key))return;e.preventDefault();e.stopPropagation();if(e.key==='Home'){rotation.x=0;rotation.y=0;}else if(e.key==='ArrowLeft')rotation.y-=15;else if(e.key==='ArrowRight')rotation.y+=15;else if(e.key==='ArrowUp')rotation.x+=15;else rotation.x-=15;rotation.update();};
 return scene;
}
$('inspect-flip').onclick=()=>{if(!albumArtRotation)return;albumArtRotation.x=0;albumArtRotation.y+=180;albumArtRotation.update();};
$('inspect-reset').onclick=()=>{if(!albumArtRotation)return;albumArtRotation.x=0;albumArtRotation.y=0;albumArtRotation.update();};

function showAlbumInspection(){
 if(inspected<0||!albumKnown(inspected))return;
 albumConfirmation=null;closeSelectMenu();closeAlbumPopover(false);$('album-modal').classList.add('inspecting');$('album-modal-title').textContent=state.cards[inspected].name;
 const slot=createCardArtViewer(inspected);albumArtRotation=slot.rotation;$('pack-reveal').replaceChildren(slot);$('inspect-tools').hidden=false;
 $('inspect-navigation').hidden=false;const index=albumVisible.indexOf(inspected);$('inspect-position').textContent=`${index+1} / ${albumVisible.length}`;$('inspect-previous').disabled=index<=0;$('inspect-next').disabled=index>=albumVisible.length-1;
 $('album-confirm').textContent='Close';$('album-cancel').hidden=true;$('album-modal').hidden=false;
}
function stepAlbumInspection(direction){const i=albumVisible.indexOf(inspected)+direction;if(i<0||i>=albumVisible.length)return;inspected=albumVisible[i];renderAlbumInspection();showAlbumInspection();}
$('inspect-card').onclick=()=>{showAlbumInspection();if(!$('album-modal').hidden)$('album-confirm').focus();};
$('inspect-previous').onclick=()=>stepAlbumInspection(-1);$('inspect-next').onclick=()=>stepAlbumInspection(1);
