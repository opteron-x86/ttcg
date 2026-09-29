'use strict';
let selectedTournament=0,tournamentConfirmation=null,tournamentHistory=false,postTournamentNotice=null;
let tournamentBoardTarget=0;
const tournamentNoticesSeen=new Set();
function renderTournamentPrizeCards(container,event){
 const ids=event.prizeCards||(event.trophy>=0?[event.trophy]:[]);container.replaceChildren();container.classList.toggle('multiple',ids.length>1);
 for(const id of ids){const card=state.cards[id];if(!card)continue;
  const figure=document.createElement('figure'),caption=document.createElement('figcaption');
  caption.textContent=card.name+(card.foil?' · Foil':'');figure.append(createCardArtViewer(id));if(event.reward!==0)figure.append(caption);container.append(figure);
 }
}
function resetTournaments(){selectedTournament=0;tournamentBoardTarget=0;tournamentHistory=false;postTournamentNotice=null;tournamentNoticesSeen.clear();}
function openTournamentBoard(id=0,action='board'){
 tournamentBoardTarget=id;send('tournament',action==='board'?action:`${action} ${id}`);
}
function routeTournamentBoard(data){
 const event=data?.events.find(e=>e.id===tournamentBoardTarget);
 selectedTournament=event?.id||0;tournamentHistory=Boolean(event?.champion);tournamentBoardTarget=0;
}
function tournamentUnseen(e){return e.unseen&&!tournamentNoticesSeen.has(e.id);}
function renderTournamentBadge(){
 const unseen=state.collection?.tournaments?.events.some(tournamentUnseen)||false;
 $('album-tournaments-tab').classList.toggle('unseen',unseen);
 $('album-tournaments-tab').setAttribute('aria-label',unseen?'Tournaments · New tournaments':'Tournaments');
}
function seeTournament(e){
 if(!e||!tournamentUnseen(e))return;
 tournamentNoticesSeen.add(e.id);renderTournamentBadge();queueMicrotask(()=>send('tournament',`seen ${e.id}`));
}
for(const history of [false,true])$('tournament-'+(history?'history':'available')).onclick=()=>{tournamentHistory=history;selectedTournament=0;renderTournaments();};
function renderTournamentNotice(){
 const data=state.collection?.tournaments;
 if(postTournamentNotice?.session!==state.session)postTournamentNotice=null;
 if(!postTournamentNotice){const event=data?.events.find(e=>e.id===data.notification);if(event&&tournamentUnseen(event)){postTournamentNotice={session:state.session,event};seeTournament(event);}}
 $('post-tournament').hidden=!postTournamentNotice;
 if(!postTournamentNotice)return;
 const e=postTournamentNotice.event;
 $('post-tournament-copy').textContent=`A Tessera tournament is open at ${e.venue}. Closes ${e.endsLabel}.`;
 $('post-tournament-view').onclick=()=>openTournamentBoard(e.id);
}
function tournamentStatus(e,hour){return e.champion?(e.champion===20?'Champion':e.eliminated?'Eliminated':'Complete'):e.eliminated?'Withdrawn':hour<e.ends?e.joined?'In progress':'Open for entry':'Closed';}
function closeTournamentConfirmation(){if($('tournament-confirm').hidden)return false;$('tournament-confirm').hidden=true;tournamentConfirmation=null;($('tournament-outcome').hidden?$('tournament-action'):$('tournament-outcome-next')).focus();return true;}
function confirmTournament(title,copy,action,label='Confirm'){tournamentConfirmation=action;$('tournament-confirm-title').textContent=title;$('tournament-confirm-copy').textContent=copy;$('tournament-confirm-copy').hidden=!copy;$('tournament-confirm-action').textContent=label;$('tournament-confirm').hidden=false;$('tournament-cancel').focus();}
$('tournament-cancel').onclick=closeTournamentConfirmation;
$('tournament-confirm-action').onclick=()=>{const action=tournamentConfirmation;closeTournamentConfirmation();if(typeof action==='function')action();else if(action)send('tournament',action);};
function renderTournaments(){
 const data=state.collection?.tournaments||{events:[],hour:0},allEvents=data.events;
 const announcement=allEvents.find(e=>e.announcement);
 if(announcement){selectedTournament=announcement.id;tournamentHistory=Boolean(announcement.champion);}
 const events=allEvents.filter(e=>tournamentHistory?e.joined&&Boolean(e.champion):!e.champion&&data.hour<e.ends);
 const priority=e=>e.joined&&!e.eliminated?0:e.canEnter?1:e.hold===data.currentHold?2:3;
 events.sort((a,b)=>tournamentHistory?b.ends-a.ends||b.id-a.id:priority(a)-priority(b)||a.ends-b.ends||a.id-b.id);
 $('tournament-available').setAttribute('aria-pressed',String(!tournamentHistory));$('tournament-history').setAttribute('aria-pressed',String(tournamentHistory));
 const owned=new Map();state.collection.owned.forEach((copies,id)=>owned.set(cardIdentity(id),(owned.get(cardIdentity(id))||0)+copies));
 const playable=[...owned].reduce((n,[id,copies])=>n+Math.min(copies,state.cards[id]?.unique?1:2),0);
 const e=events.find(e=>e.id===selectedTournament)||events[0];selectedTournament=e?.id||0;
 if(e)seeTournament(e);
 renderChapterNavigation($('tournament-list'),events.map(event=>({id:event.id,title:`${event.champion===20?'★ ':''}${event.name}${event.circuit===2?' · Masters':event.circuit===1?' · Invitational':''}${event.champion?'':event.joined?' · Entered':''}`,detail:event.champion?`Held ${event.startsLabel}`:`Closes ${event.endsLabel}`,unseen:tournamentUnseen(event)})),selectedTournament,id=>{selectedTournament=id;renderTournaments();},'tournament-event-');
 $('tournament-content').hidden=!e;$('tournament-title').textContent=e?e.circuit===2?`${e.name} · Tessera Masters`:`${e.name} ${e.circuit===1?'invitational':'tournament'}`:'Tournaments';$('tournament-venue').textContent=e?`${e.venue} · ${e.host}`:tournamentHistory?'Your completed tournaments will appear here.':'Visit an inn to find local tournaments. Invitations arrive by courier.';$('tournament-status').textContent=e?tournamentStatus(e,data.hour):'';
 if(e){
  renderRuleChips($('tournament-rules'),e.rules);$('tournament-dates').textContent=e.champion?`Held ${e.startsLabel}`:`Closes ${e.endsLabel}`;
  $('tournament-entry').textContent=e.fee?`${e.fee} gold entry`:'';
  $('tournament-prize-name').textContent=e.prize;$('tournament-prize-name').title=e.reward===1?'Five distinct cards, tiers III–VII. Common 30%, Rare 45%, Epic 25%; foil chance 5% per card.':'';$('tournament-purse').textContent=`${e.purse} gold`;
  $('tournament-prize-description').textContent=e.champion?`${e.bracket[14].name} won this tournament.`:e.reward===3?'Choose which cards to upgrade.':'';
  renderTournamentPrizeCards($('tournament-prize-art'),e);
  const bracket=$('tournament-bracket');bracket.replaceChildren();
  for(const [round,title,start,end] of [[1,'Quarterfinals',8,12],[2,'Semifinals',12,14],[3,'Final',14,15]]){
   const column=document.createElement('section'),heading=document.createElement('h3');heading.textContent=title;column.append(heading);column.classList.toggle('current-round',e.round===round);
   for(let n=start;n<end;n++){const pairing=document.createElement('div');pairing.className='tournament-pair';pairing.setAttribute('aria-label',title);
    for(let side=0;side<2;side++){const slot=e.bracket[(n-8)*2+side],row=document.createElement('div');row.className='tournament-entrant';if(slot.id===20)row.classList.add('you');if(slot.reserved)row.classList.add('reserved');else if(!slot.id)row.setAttribute('aria-label','Undecided');const wins=e.wins[n-8][side];if(slot.id&&wins===2)row.classList.add('winner');
     const name=document.createElement('span');name.textContent=slot.reserved?'Your place':slot.name;const score=document.createElement('span');score.className='tournament-wins';score.textContent=slot.id?String(wins):'';if(slot.id)score.setAttribute('aria-label',`${wins} wins`);row.append(name,score);pairing.append(row);
    }column.append(pairing);
   }bracket.append(column);
  }
  const open=!e.champion&&data.hour>=e.starts&&data.hour<e.ends,registration=open&&!e.joined,play=open&&e.joined&&!e.eliminated;
  const action=$('tournament-action');action.hidden=!registration&&!play;const roundName=['','quarterfinal','semifinal','final'][e.round],node=[0,8,12,14][e.round],games=node?e.wins[node-8].reduce((a,b)=>a+b,0):0;action.textContent=registration?'Enter tournament':games?'Continue '+roundName:'Play '+roundName;action.disabled=!e.atVenue||registration&&(!e.canEnter||state.collection.gold<e.fee||playable<5);
  action.onclick=()=>registration?confirmTournament(e.fee?`Enter for ${e.fee} gold?`:'Enter tournament?','',`enter ${e.id}`,'Enter'):send('tournament',`round ${e.id}`);
  $('tournament-withdraw').hidden=!e.joined||e.eliminated||Boolean(e.champion);$('tournament-withdraw').onclick=()=>confirmTournament('Withdraw from tournament?',e.fee?'Your entry fee will not be refunded.':'This ends your tournament run.',`withdraw ${e.id}`);
  $('tournament-edit').hidden=!e.joined||e.eliminated||Boolean(e.champion);$('tournament-edit').onclick=()=>setAlbumTab('decks');
  $('tournament-reason').textContent=registration&&data.activeEntry?'Finish your current tournament first.':registration&&!e.canEnter?`Speak to ${e.host} at ${e.venue} to enter.`:!e.atVenue&&!e.champion?`Return to ${e.venue} to play.`:registration&&state.collection.gold<e.fee?'Not enough gold for entry.':registration&&playable<5?'You need five playable cards to enter.':'';
  if(e.announcement)showTournamentOutcome(e);

 }
}

function activeTournament(){return state.collection?.tournaments?.events.find(e=>e.id===state.collection?.tournamentID);}
function leaveTournamentOutcome(e,action='ack'){
 if(!e)return;
 const leave=()=>{$('tournament-outcome').hidden=true;openTournamentBoard(e.id,action);};
 if(e.foilUpgrades)confirmTournament(`Leave ${e.foilUpgrades} foil upgrade${e.foilUpgrades===1?'':'s'} unused?`,'They will be lost.',leave,'Leave');
 else leave();
}
function closeTournamentOutcome(){
 if($('tournament-outcome').hidden)return false;
 leaveTournamentOutcome(state.collection?.tournaments?.events.find(e=>e.id===Number($('tournament-outcome').dataset.event)));return true;
}
function renderTournamentFoils(e){
 const panel=$('tournament-foil-reward');panel.hidden=!e.foilUpgrades;if(panel.hidden)return;
 $('tournament-foil-remaining').textContent=`${e.foilUpgrades} foil upgrade${e.foilUpgrades===1?'':'s'} remaining`;
 const select=$('tournament-foil-card'),selected=select.value;select.replaceChildren();
 const foilCopies=new Map(state.cards.flatMap((c,id)=>c.foil?[[cardIdentity(id),state.collection.owned[id]||0]]:[]));
 const candidates=state.cards.map((c,id)=>({c,id})).filter(({c,id})=>!c.foil&&state.collection.owned[id]>0&&(foilCopies.get(cardIdentity(id))||0)<1000000).sort((a,b)=>a.c.name.localeCompare(b.c.name));
 for(const {c,id} of candidates)select.append(new Option(c.name,String(id)));
 if([...select.options].some(o=>o.value===selected))select.value=selected;
 select.disabled=!select.options.length;select._refresh();$('tournament-upgrade').disabled=select.disabled;$('tournament-foil-empty').hidden=!select.disabled;
 const preview=()=>{const target=$('tournament-foil-preview');target.replaceChildren();if(select.value!=='')target.append(cardElement(Number(select.value),0));};
 select.onchange=preview;preview();
 $('tournament-upgrade').onclick=()=>{if(select.value==='')return;$('tournament-upgrade').disabled=true;send('tournament',`foil ${e.id} ${select.value}`);};
}
function showTournamentOutcome(e,fromResult=false){
 if(!e||(!fromResult&&!e.announcement))return;
 $('post-match').hidden=true;
 const modal=$('tournament-outcome'),opening=modal.hidden;modal.dataset.event=e.id;
 const champion=e.champion===20,stage=e.lastNode===8?'Quarterfinals':e.lastNode===12?'Semifinals':'Final';
 const node=e.lastNode||[0,8,12,14][e.round]||14,score=e.wins[node-8],rival=e.bracket[(node-8)*2+1].name;
 const result=state.score?.[0]===state.score?.[1]?'Draw':state.score?.[0]>state.score?.[1]?'Game won':'Game lost';
 $('tournament-outcome-title').textContent=champion?'Tournament won':e.announcement===1?'Through to the semifinals':e.announcement===2?'Through to the final':e.eliminated?'Tournament over':result;
 $('tournament-outcome-copy').textContent=champion?`${e.name} · ${e.circuitName}`:e.lastNode?`${stage} · ${score[0]}–${score[1]}${rival?' against '+rival:''}`:'You withdrew from the tournament.';
 const rewards=$('tournament-outcome-rewards');rewards.hidden=!champion;rewards.replaceChildren();
 if(champion){
  const gallery=document.createElement('div');gallery.className='tournament-reward-cards';renderTournamentPrizeCards(gallery,e);rewards.append(gallery);
  if(!e.foilUpgrades){const prize=document.createElement('p');prize.textContent=e.prize;rewards.append(prize);}
  const gold=document.createElement('p');gold.textContent=`${e.purse} gold`;rewards.append(gold);
 }
 renderTournamentFoils(e);
 const next=$('tournament-outcome-next');next.textContent=champion?'View tournament':e.eliminated?'View results':e.announcement===1?'Continue to semifinals':e.announcement===2?'Continue to final':result==='Draw'?'Replay game':'Next game';
 next.classList.toggle('primary',!e.foilUpgrades);next.classList.toggle('quiet',Boolean(e.foilUpgrades));$('tournament-upgrade').classList.add('primary');
 next.onclick=()=>{if(e.champion||e.eliminated)leaveTournamentOutcome(e);else{modal.hidden=true;send('tournament',`continue ${e.id}`);}};
 $('tournament-outcome-bracket').hidden=Boolean(e.champion)||e.eliminated;
 $('tournament-outcome-bracket').onclick=()=>{modal.hidden=true;openTournamentBoard(e.id,'ack');};
 $('tournament-outcome-leave').onclick=()=>leaveTournamentOutcome(e,'leave');
 modal.hidden=false;if(opening){closeSelectMenu();(e.foilUpgrades&&!$('tournament-foil-card').disabled?$('tournament-foil-card-trigger'):next).focus();}
}
