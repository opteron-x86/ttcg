'use strict';
// Presentation only: native commands remain authoritative for decks and matches.
let boardDrag=-1,tableScore=null,inspectionReturn=null;
function ownershipCorners(){const corners=document.createElement('span');corners.className='ownership-corners';corners.setAttribute('aria-hidden','true');return corners;}
const tableRuleHelp=Object.fromEntries([...TRADE_RULES,{title:'Casual',copy:'Games with children have no card trades or gold wagers.'}].map(rule=>[rule.title,rule.copy]));
function renderRuleChips(root,flags,trade,casual=false){
 const labels=[!(flags&(16|64))?'Hidden hands':'',...rulesText(flags).split(' · '),casual?'Casual':trade===undefined?'':tradeRuleText(trade)].filter(Boolean),key=labels.join('|');if(root.dataset.rules===key)return;root.dataset.rules=key;root.replaceChildren();
 for(const label of labels){const chip=document.createElement('span');chip.className='rule-chip';chip.textContent=label;chip.tabIndex=0;chip.dataset.help=RULE_PAGES.find(page=>page.title===label)?.copy||tableRuleHelp[label]||label;root.append(chip);}
}
function updateTableMode(){
 const lesson=state?.screen==='lesson',lobby=state?.phase==='ready'&&(!state.screen||state.screen==='lobby'),match=!lesson&&state?.phase!=='ready';
 $('table').classList.toggle('table-lesson',lesson);
 $('table').classList.toggle('table-lobby',lobby);$('table').classList.toggle('table-match',match);
}
$('lobby-edit').onclick=()=>openLobbyAlbum('decks');
function renderLobbyCards(){
 const hand=$('selected-deck');hand.replaceChildren();
 for(let h=0;h<5;h++){
  const slot=document.createElement('button'),id=draftDeck[h];slot.className='deck-slot';slot.id=`deck-${h}`;slot.dataset.slot=h;
  if(id<0){slot.classList.add('empty','needs-card');slot.textContent='+';slot.setAttribute('aria-label',`Add card to slot ${h+1}`);}
  else{slot.append(cardElement(id,0));slot.setAttribute('aria-label',`Inspect ${state.cards[id].name}`);}
  slot.onclick=()=>id<0?openLobbyAlbum('decks',h):inspectTableCard(id,0);
  slot.oncontextmenu=e=>{e.preventDefault();if(id>=0)inspectTableCard(id,0);};hand.append(slot);
 }
}
function tableStakes(){const c=state?.collection;return c?.wager>0?`${c.wager} gold`:'';}
function updateTableScores(score){
 for(let p=0;p<2;p++){
   const el=$(p?'rival-score':'player-score');el.textContent=score[p];
   if(tableScore&&score[p]!==tableScore[p]&&!matchMedia('(prefers-reduced-motion: reduce)').matches){
     const delta=document.createElement('span');delta.className='score-change';const diff=score[p]-tableScore[p];delta.textContent=(diff>0?'+':'−')+Math.abs(diff);el.parentElement.append(delta);
     const anim=delta.animate([{opacity:0,transform:'translateY(6px)'},{opacity:1,offset:.2},{opacity:0,transform:'translateY(-20px)'}],{duration:800,easing:'ease-out'});anim.finished.then(()=>delta.remove(),()=>delta.remove());
   }
 }tableScore=[...score];
}
function inspectTableCard(id,owner=0,modifier=0){
 const overlay=activeOverlay();
 if(id===null||id<0||!state?.cards[id]||busy||sent||(overlay&&!(overlay===$('reward')&&['selecting','opponent'].includes(trade?.phase))))return;
 clearBoardGhosts();closeSelectMenu();inspectionReturn=document.activeElement;
 const c=state.cards[id];$('table-inspect-art').replaceChildren(createCardArtViewer(id));$('table-inspect-name').textContent=c.name;
 $('table-inspect-meta').textContent=[`Tier ${['','I','II','III','IV','V','VI','VII','VIII','IX','X'][c.tier]}`,c.rarity,c.foil?'Foil':''].filter(Boolean).join(' · ');
 $('table-inspect-types').textContent=(c.groups||[]).join(' · ');
 const affinities=$('table-inspect-affinities');affinities.replaceChildren();for(const name of c.affinities||[]){const span=document.createElement('span');span.append(affinityIcon(name),document.createTextNode(name));affinities.append(span);}if(!affinities.children.length)affinities.textContent='Neutral';
 const ranks=$('table-inspect-ranks');ranks.replaceChildren();
 for(const [i,side] of ['Top','Right','Bottom','Left'].entries()){const item=document.createElement('span');item.textContent=['↑','→','↓','←'][i]+' '+(c.sides[i]===10?'A':c.sides[i]);item.setAttribute('aria-label',`${side}: ${c.sides[i]}`);ranks.append(item);}
 $('table-inspect-modifier').textContent=modifier?`All sides ${modifier>0?'+':'−'}${Math.abs(modifier)}`:'';
 $('table-inspection').hidden=false;$('table-inspect-close').focus();
}
function closeTableInspection(focus=true){if($('table-inspection').hidden)return false;$('table-inspection').hidden=true;if(focus&&inspectionReturn?.isConnected)inspectionReturn.focus();inspectionReturn=null;return true;}
$('table-inspect-close').onclick=()=>closeTableInspection();
$('table-inspect-flip').onclick=()=>{const r=$('table-inspect-art').firstElementChild?.rotation;if(r){r.x=0;r.y+=180;r.update();}};
$('table-inspect-reset').onclick=()=>{const r=$('table-inspect-art').firstElementChild?.rotation;if(r){r.x=r.y=0;r.update();}};
$('table-inspection').onclick=e=>{if(e.target===$('table-inspection'))closeTableInspection();};
$('table-inspect').onclick=()=>{if(selected>=0)inspectTableCard(state.hands[0][selected],0,state.handModifiers?.[0]?.[selected]||0);};
document.addEventListener('keydown',e=>{
 if(!state||['INPUT','SELECT','TEXTAREA'].includes(e.target.tagName))return;
 if(e.key.toLowerCase()==='i'&&!e.repeat){
   if(activeOverlay()===$('reward')){inspectTradeCard();e.preventDefault();return;}
   if(activeOverlay())return;
   const el=document.activeElement;
   if(el?.id.startsWith('deck-'))inspectTableCard(draftDeck[Number(el.dataset.slot)],0);
   else if(el?.id.startsWith('square-')){const i=Number(el.id.slice(7)),cell=displayBoard[i];if(cell)inspectTableCard(cell.card,cell.owner,state.modifiers?.[i]||0);else $('table-inspect').click();}
   else if(el?.id.startsWith('hand-')){const [,p,h]=el.id.split('-').map(Number);inspectTableCard(state.hands[p][h],p,state.handModifiers?.[p]?.[h]||0);}
   else $('table-inspect').click();e.preventDefault();
 }
});
function clearBoardGhosts(){document.querySelectorAll('.placement-ghost').forEach(e=>e.remove());}
function showBoardGhost(index){
 clearBoardGhosts();if(selected<0||busy||sent||state?.phase!=='match'||state.turn!==0||state.thinking||state.settling||displayBoard[index]||!$('table-inspection').hidden)return;
 const id=state.hands[0][selected];if(id===null||id<0)return;
 const ghost=document.createElement('div');ghost.className='placement-ghost';ghost.setAttribute('aria-hidden','true');ghost.append(cardElement(id,0));$('square-'+index).append(ghost);
}
function startBoardDrag(h){
 if(!playableHand(h))return;
 boardDrag=h;selected=h;
 document.querySelectorAll('#player-hand .hand-slot').forEach((el,i)=>el.classList.toggle('selected',i===h));
 displayBoard.forEach((c,i)=>{if(!c){$('square-'+i).disabled=false;$('square-'+i).classList.add('available');}});$('table-inspect').disabled=false;
}
function clearBoardDrag(){boardDrag=-1;clearBoardGhosts();}
function cancelTableDrag(){const active=!!tablePointer?.active;clearTablePointer();return active;}
function resetTablePresentation(){closeMatchRules(false);hideRuleTooltip();clearTablePointer();closeTableInspection(false);clearBoardDrag();tableScore=null;}

// Match the album's in-page pointer drag path; CEF never needs a desktop drag.
let tablePointer=null,tableSuppressClick=false;
function clearTablePointer(){
 const pointer=tablePointer;tablePointer=null;
 if(pointer?.active)tableSuppressClick=true;
 pointer?.ghost?.remove();
 if(pointer?.source.hasPointerCapture?.(pointer.id))pointer.source.releasePointerCapture(pointer.id);
 clearBoardDrag();
}
$('table').addEventListener('pointerdown',event=>{
 tableSuppressClick=false;
 if(event.button!==0||document.querySelector('.modal-shade:not([hidden])'))return;
 const source=event.target.closest('[data-table-drag]');if(!source||source.disabled)return;
 clearTablePointer();tablePointer={id:event.pointerId,source,type:source.dataset.tableDrag,value:Number(source.dataset.dragValue),session:state.session,x:event.clientX,y:event.clientY,active:false,ghost:null};
});
window.addEventListener('pointermove',event=>{
 const pointer=tablePointer;if(!pointer||pointer.id!==event.pointerId)return;
 if(pointer.session!==state?.session||!pointer.source.isConnected){clearTablePointer();return;}
 if(!pointer.active&&Math.hypot(event.clientX-pointer.x,event.clientY-pointer.y)<6)return;
 event.preventDefault();
 if(!pointer.active){
   pointer.active=true;pointer.source.setPointerCapture(pointer.id);
   startBoardDrag(pointer.value);
   const rect=pointer.source.getBoundingClientRect(),ghost=document.createElement('div');ghost.className='table-drag-ghost';ghost.setAttribute('aria-hidden','true');
   ghost.append(foilMaterial.clone(pointer.source.querySelector('.card')));
   Object.assign(ghost.style,{width:pointer.source.offsetWidth+'px',height:pointer.source.offsetHeight+'px',transform:`scale(${rect.width/pointer.source.offsetWidth})`});
   pointer.offsetX=pointer.x-rect.x;pointer.offsetY=pointer.y-rect.y;pointer.ghost=ghost;document.body.append(ghost);
 }
 Object.assign(pointer.ghost.style,{left:(event.clientX-pointer.offsetX)+'px',top:(event.clientY-pointer.offsetY)+'px'});
 clearBoardGhosts();
 const target=document.elementFromPoint(event.clientX,event.clientY)?.closest('#board .square');
 if(!target)return;
 showBoardGhost(Number(target.id.slice(7)));
});
window.addEventListener('pointerup',event=>{
 const pointer=tablePointer;if(!pointer||event.pointerId!==pointer.id)return;
 if(!pointer.active){clearTablePointer();return;}
 event.preventDefault();
 const target=document.elementFromPoint(event.clientX,event.clientY)?.closest('#board .square');
 clearTablePointer();if(!target||pointer.session!==state?.session)return;
 requestPlacement(pointer.value,Number(target.id.slice(7)));
});
$('table').addEventListener('click',event=>{if(tableSuppressClick&&event.target.closest('#challenge,#game')){tableSuppressClick=false;event.preventDefault();event.stopImmediatePropagation();}},true);
window.addEventListener('pointercancel',clearTablePointer);
window.addEventListener('blur',clearTablePointer);
