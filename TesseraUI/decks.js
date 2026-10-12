'use strict';
let deckDialogAction='',deckDialogReturn='';
let albumNameEdit=null;
function validDeckName(name){return Boolean(name)&&new TextEncoder().encode(name).length<=48&&!/[\x00-\x1f\x7f]/.test(name);}
function beginAlbumRename(){
 if(state?.phase!=='ready')return;
 const current=state.collection.savedDecks?.find(d=>d.id===state.collection.activeDeck);if(!current)return;
 albumNameEdit={id:current.id,name:current.name};
 const input=$('album-deck-name-input');input.value=current.name;input.setCustomValidity('');input.hidden=false;$('album-deck-rename').hidden=true;input.focus();input.select();
}
function finishAlbumRename(save,restore=true){
 if(!albumNameEdit)return;
 const current=albumNameEdit,input=$('album-deck-name-input'),name=input.value.trim();albumNameEdit=null;
 input.hidden=true;$('album-deck-rename').hidden=false;
 if(save&&validDeckName(name)&&name!==current.name)send('deck-manage',`rename ${current.id} ${name}`);
 if(restore)$('album-deck-rename').focus({preventScroll:true});
}
function protectedDeckCopies(id) {
 return state.collection.deckReserved?.[id]??state.collection.deck.filter(x=>x===id).length;
}
function renderDeckTools(prefix) {
 const c=state.collection,tools=$(prefix+'-deck-tools'),decks=c.savedDecks||[];
 tools.hidden=!decks.length||(prefix==='album'&&(state.screen!=='album'||albumTab!=='decks'));
 if(tools.hidden)return;
 const select=$(prefix+'-deck-select'),signature=JSON.stringify(decks.map(d=>[d.id,d.name,d.cards]));
 if(select.dataset.decks!==signature){
   select.dataset.decks=signature;select.replaceChildren();
   for(const d of decks){const option=document.createElement('option');option.value=d.id;option.textContent=d.name;select.append(option);}
 }
 select.value=String(c.activeDeck);select._refresh();
 if(prefix==='album'){
   $('album-deck-new').disabled=$('album-deck-copy').disabled=decks.length>=20;
   $('album-deck-delete').disabled=decks.length<=1;
   if(albumNameEdit&&albumNameEdit.id!==c.activeDeck)finishAlbumRename(false,false);
   $('album-deck-name').textContent=decks.find(d=>d.id===c.activeDeck)?.name||'Deck';
   const list=$('saved-decks-list'),scroll=list.scrollTop;list.replaceChildren();
   for(const d of decks){
     const button=document.createElement('button');button.className='saved-deck-row';button.id='saved-deck-'+d.id;button.setAttribute('aria-pressed',String(d.id===c.activeDeck));
     const name=document.createElement('span');name.className='saved-deck-name';name.textContent=d.name;button.append(name);
     const hand=d.id===c.activeDeck?albumDeck:d.cards,preview=document.createElement('span');preview.className='saved-deck-preview';preview.setAttribute('aria-hidden','true');
     for(const id of hand){const slot=document.createElement('span');if(id>=0&&state.cards[id]){const img=document.createElement('img');setCardImage(img,state.cards[id],true);img.alt='';img.loading='lazy';img.draggable=false;slot.append(img);if(state.cards[id].foil){slot.classList.add('foil','portrait');appendFoilLayers(slot);}}preview.append(slot);}
     const count=document.createElement('small');count.textContent=`${deckCount(hand)}/5`;button.append(preview,count);
     button.onclick=()=>{albumTarget=-1;closeAlbumPopover(false);send('deck-manage',`select ${d.id}`);};list.append(button);
   }
   list.scrollTop=scroll;
 }
}
function closeDeckDialog() {
 if($('deck-dialog').hidden)return false;
 $('deck-dialog').hidden=true;deckDialogAction='';$(deckDialogReturn)?.focus();return true;
}
function openDeckDialog(action) {
 if(state?.phase!=='ready')return;
 const decks=state.collection.savedDecks||[],current=decks.find(d=>d.id===state.collection.activeDeck);
 if(!current)return;
 closeSelectMenu();closeAlbumPopover(false);deckDialogAction=action;deckDialogReturn='album-deck-'+action;
 $('deck-dialog-title').textContent={new:'New deck',copy:'Duplicate deck',rename:'Rename deck',delete:'Delete deck?'}[action];
 $('deck-submit').textContent={new:'Create',copy:'Create',rename:'Save',delete:'Delete'}[action];
 $('deck-name').hidden=$('deck-name-label').hidden=action==='delete';
 $('deck-dialog-message').hidden=action!=='delete';$('deck-dialog-message').textContent=current.name;
 let n=1;while(decks.some(d=>d.name===`Deck ${n}`))n++;
 $('deck-name').value=action==='rename'?current.name:action==='copy'?current.name.slice(0,22)+' copy':`Deck ${n}`;
 $('deck-dialog').hidden=false;checkDeckName();
 $(action==='delete'?'deck-cancel':'deck-name').focus();if(action!=='delete')$('deck-name').select();
}
function checkDeckName() {
 const name=$('deck-name').value.trim();
 $('deck-submit').disabled=deckDialogAction!=='delete'&&!validDeckName(name);
}
$('album-deck-name-input').onblur=()=>finishAlbumRename(true,false);
$('album-deck-name-input').oninput=()=>{$('album-deck-name-input').setCustomValidity('');};
$('album-deck-name-input').onkeydown=event=>{
 if(!['Enter','Escape'].includes(event.key))return;
 event.preventDefault();event.stopPropagation();
 if(event.key==='Enter'&&!validDeckName(event.target.value.trim())){event.target.setCustomValidity(event.target.value.trim()?'Choose a shorter deck name.':'Enter a deck name.');event.target.reportValidity();return;}
 if(event.key==='Escape')window.ttcgEscape('dom');else finishAlbumRename(true);
};
for(const prefix of ['lobby','album']) {
 $(prefix+'-deck-select').onchange=event=>send('deck-manage',`select ${event.target.value}`);

}
for(const action of ['new','copy','delete'])$('album-deck-'+action).onclick=()=>openDeckDialog(action);
$('album-deck-rename').onclick=beginAlbumRename;
$('deck-name').oninput=checkDeckName;
$('deck-cancel').onclick=closeDeckDialog;
$('deck-form').onsubmit=event=>{
 event.preventDefault();if(!deckDialogAction||$('deck-submit').disabled)return;
 const action=deckDialogAction,id=state.collection.activeDeck,name=$('deck-name').value.trim();
 closeDeckDialog();send('deck-manage',`${action} ${id}${action==='delete'?'':' '+name}`);
};
