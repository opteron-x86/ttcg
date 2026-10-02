'use strict';
let state=null, selected=-1, sent=false, lastRevision='';
let displayBoard=[], busy=false, highlights=0, playbackToken=0;
const pauses=new Map(), flips=new Set();
function cancelPlayback() {
 playbackToken++;
 for(const [timer,resolve] of pauses) { clearTimeout(timer); resolve(); }
 pauses.clear();
 for(const animation of flips) animation.cancel();
 flips.clear(); busy=false; highlights=0;
 $('rule-callout').hidden=true;$('starter-draw').hidden=true;hideRuleTooltip();
 document.querySelectorAll('.first-turn').forEach(el=>el.classList.remove('first-turn'));
}
function pause(ms) {
 return new Promise(resolve=>{ const timer=setTimeout(()=>{ pauses.delete(timer); resolve(); },ms); pauses.set(timer,resolve); });
}
const $=id=>document.getElementById(id);
// Shared interaction scope for keyboard and Meridian navigation. Keep overlay
// priority here so a hidden control cannot receive focus behind another dialog.
function activeOverlay(){
 return ['shop-confirm','tournament-confirm','tournament-outcome','deck-dialog','settings-panel','album-modal','abolition-dialog','table-inspection','match-rules','group-choice','rule-request','post-match','forfeit','reward','album-filter-panel'].map($).find(el=>el&&!el.hidden)||null;
}
function activeInteractionScope(){return openSelectMenu?.list||activeOverlay()||$('table');}
function visibleControls(scope){return [...scope.querySelectorAll('button,input,select,[tabindex="0"]')].filter(el=>!el.disabled&&el.getClientRects().length);}
function renderChapterNavigation(container,entries,selected,onSelect,prefix){
 const scroll=container.scrollTop;container.replaceChildren();
 for(const entry of entries){
  const button=document.createElement('button');button.className='chapter-link';button.id=prefix+entry.id;button.textContent=entry.title||entry.name;
  if(entry.detail){const detail=document.createElement('span');detail.className='chapter-detail';detail.textContent=entry.detail;button.append(detail);}
  if(entry.unseen){button.classList.add('unseen');button.setAttribute('aria-label',`${entry.title||entry.name}. ${entry.detail||''}. New tournament`);}
  button.setAttribute('aria-pressed',String(selected===entry.id));
  button.onclick=()=>{onSelect(entry.id);$(prefix+entry.id)?.focus({preventScroll:true});};container.append(button);
 }
 container.scrollTop=scroll;
}
function renderCardMetadata(container,card){
 container.replaceChildren();if(!card)return;
 for(const [kind,value] of [['rarity',card.rarity+(card.unique?' · Unique':'')+(card.foil?' · Foil':'')],['groups',card.groups?.join(' / ')],['affinities',card.affinities?.join(' / ')||'Neutral']]){
  const line=document.createElement('div');line.className='inspect-'+kind;line.textContent=value||'';if(kind==='rarity')line.dataset.rarity=card.rarity;container.append(line);
 }
}
function appendFoilLayers(portrait){
 for(const name of ['film','grain','glare']){const layer=document.createElement('span');layer.className='foil-'+name;portrait.append(layer);}
 foilMaterial.attach(portrait.classList.contains('foil')?portrait:portrait.parentElement,'prismatic',portrait);
}
function resizeWindow() {
 const size=Math.max(60,Math.min(95,Number($('window-size').value)||90));
 document.documentElement.style.setProperty('--window-scale',Math.min(innerWidth/1400,innerHeight/920)*size/100);
 try { localStorage.setItem('ttcg.windowSize',String(size)); } catch { /* Storage may be disabled in the embedded view. */ }
}
try { const saved=Number(localStorage.getItem('ttcg.windowSize')); if(saved>=60&&saved<=96) $('window-size').value=String(Math.min(95,Math.round(saved/5)*5)); } catch {}
$('window-size').addEventListener('change',resizeWindow);
window.addEventListener('resize',resizeWindow);
resizeWindow();
buildSelectMenus();
function send(verb, args='') {
 if(!state||typeof window.ttcgCommand!=='function') return;
 window.ttcgCommand(`${verb} ${state.session} ${state.revision}${args?' '+args:''}`);
}
function tradeRuleText(rule) { return ['None','One','Diff','Direct','All'][rule]||'None'; }
const ruleDefinitions=[[16,'Open'],[64,'Three Open'],[1,'Same'],[8192,'Same Wall'],[2,'Plus'],[4,'Reverse'],[2048,'Fallen Ace'],[32,'Affinity'],[128,'Legion'],[256,'Decimation'],[512,'Order'],[1024,'Chaos'],[16384,'Swap'],[4096,'Sudden Death']];
function rulesText(flags) { return ruleDefinitions.filter(([bit])=>flags&bit).map(([,name])=>name).join(' · '); }
function playableHand(h){const id=state?.hands?.[0]?.[h];return Number.isInteger(id)&&id>=0&&(state.forcedHand==null||state.forcedHand<0||state.forcedHand===h);}
function ruleStageLabel(flags){if(flags&8)return 'Combo';return ruleDefinitions.filter(([bit])=>flags&bit).map(([,name])=>name).filter(name=>name!=='Same'||!(flags&8192)).join(' + ');}
function artUrl(path) { return 'art/'+path.split('/').map(encodeURIComponent).join('/'); }
function cardArtUrl(data,thumbnail=false) {
 return artUrl(thumbnail?(data.thumbnail||('thumbs/'+data.name+'.webp')):data.art);
}
function setCardImage(image,data,thumbnail=false) {
 image.src=cardArtUrl(data,thumbnail);
 image.onerror=()=>{image.onerror=null;if(thumbnail)image.src=cardArtUrl(data);};
}
function cardBackElement(owner=1) {
 const card=document.createElement('div');card.className=`card card-back owner-${owner}`;
 return card;
}
const affinityNames=['Strength','Intelligence','Willpower','Agility','Endurance'];
const affinityColors=['#e78871','#7fbce9','#e4ce80','#91c78b','#c2a0e4'];
// Set A: solid heraldic silhouettes with cream edges; no enclosing badge.
// Geometry uses a shared 100-unit canvas so it stays sharp in small hands and zoom.
const affinityArtwork=[
 // Strength: flexed arm, fist and bicep.
 `<path class="affinity-silhouette" d="M9 72C6 61 8 52 13 42L26 17Q28 13 34 10L47 4Q50 3 53 7L60 17Q63 22 59 25Q61 30 56 32Q52 34 48 30Q43 34 39 30L36 26Q31 40 35 51L36 60Q46 49 59 55Q69 40 83 49Q96 59 92 74L86 93Q79 88 74 80Q54 96 35 87L13 79Q8 78 9 72Z"/>
 <path class="affinity-detail" d="M49 12 56 23M44 17 49 29M36 26 39 22M36 60Q33 66 28 68M59 55Q65 59 69 65M38 76Q48 82 61 76M74 80 71 71"/>`,
 // Intelligence: an open volume, with fanned pages and a center fold.
 `<path class="affinity-silhouette" d="M7 22 14 21 14 16 21 17 21 11Q38 9 50 23Q62 9 79 11L79 17 86 16 86 21 93 22 97 87Q72 79 57 87Q50 93 43 87Q27 79 3 87Z"/>
 <path class="affinity-detail" d="M21 17 19 68Q37 64 50 77Q63 64 81 68L79 17M14 21 11 77Q34 70 50 81Q66 70 89 77L86 21M50 23 50 77"/>`,
 // Willpower: frontal lion, angular mane and broad negative-space facial cuts.
 `<path class="affinity-silhouette" d="M50 3 54 12Q65 7 73 10L66 17Q79 14 84 21Q88 27 82 34L94 49 84 46 94 65 84 59Q88 75 78 85L77 72 65 94 63 84 50 98 37 84 35 94 23 72 22 85Q12 75 16 59L6 65 16 46 6 49 18 34Q12 27 16 21Q21 14 34 17L27 10Q35 7 46 12Z"/>
 <path class="affinity-cutout" d="M20 23Q26 20 31 24L25 31Q18 27 20 23M80 23Q74 20 69 24L75 31Q82 27 80 23M50 25Q43 17 34 18L39 26 30 24 23 36 32 33 22 48 31 45 21 64 31 55 29 76 38 64 40 80 50 91 60 80 62 64 71 76 69 55 79 64 69 45 78 48 68 33 77 36 70 24 61 26 66 18Q57 17 50 25Z"/>
 <path fill="currentColor" d="M50 31 39 26 30 39 35 49 39 54 35 63Q36 69 46 69L50 74 54 69Q64 69 65 63L61 54 65 49 70 39 61 26Z"/>
 <path class="affinity-cutout" d="M33 39 43 42 44 49 38 46ZM67 39 57 42 56 49 62 46ZM43 55Q50 58 57 55L54 61 51 63 51 67 61 71 56 73 50 70 44 73 39 71 49 67 49 63 46 61Z"/>
 <path class="affinity-detail" d="M36 61 43 62M64 61 57 62"/>
 <path fill="#f2e3b8" d="M41 75Q50 70 59 75L57 82 54 79 50 85 46 79 43 82Z"/>`,
 // Agility: one diagonal arrow, with a broad point and cut feather edges.
 `<path class="affinity-silhouette" d="M93 7 80 43 74 34 41 68 40 81 32 90 30 76 25 82 25 92 16 98 15 83 3 81 10 74 20 74 13 70 22 62 35 61 67 28 58 23Z"/>
 <path class="affinity-detail" d="M19 81 72 28M26 68 18 68M33 76 33 68"/>`,
 // Endurance: wide anvil horn, narrow waist, and a planted foot.
 `<path class="affinity-silhouette" d="M4 29 37 29 37 24 96 24Q98 35 76 40L68 42Q59 49 65 60Q69 67 77 70L77 82 61 82Q60 77 52 77L41 77Q33 77 32 82L24 82 24 71Q40 66 40 55Q40 45 29 43Q10 39 4 29Z"/>`
];
const affinityPigments=['#b63c33','#3879b4','#d7a631','#55913c','#79459c'];
function affinityIcon(name) {
 const i=affinityNames.indexOf(name),icon=document.createElementNS('http://www.w3.org/2000/svg','svg');icon.setAttribute('viewBox','0 0 100 100');icon.setAttribute('role','img');icon.setAttribute('aria-label',name);icon.setAttribute('focusable','false');icon.classList.add('affinity-icon');icon.style.color=affinityPigments[i];
 // Only fixed, local artwork enters this SVG, never card metadata or user text.
 icon.innerHTML=affinityArtwork[i]||'';return icon;
}
const tierMetrics=new Map();
function tierNumeral(label) {
 // Center the visible glyphs, rather than the font's line box and side bearings.
 const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg'),text=document.createElementNS(ns,'text');
 if(!tierMetrics.has(label)) {
   const context=document.createElement('canvas').getContext('2d');context.font='16px Georgia,serif';
   const m=context.measureText(label);
   tierMetrics.set(label,{x:20+((m.actualBoundingBoxLeft??0)-(m.actualBoundingBoxRight??m.width))/2,y:20+((m.actualBoundingBoxAscent??11)-(m.actualBoundingBoxDescent??0))/2});
 }
 const metrics=tierMetrics.get(label);svg.setAttribute('viewBox','0 0 40 40');svg.setAttribute('aria-hidden','true');svg.setAttribute('focusable','false');
 text.setAttribute('x',metrics.x);text.setAttribute('y',metrics.y);text.textContent=label;svg.append(text);return svg;
}
function cardIdentity(id){return id<0?-1:state.cards[id].baseIndex??id;}
function identityCopies(deck,id){return deck.filter(x=>x>=0&&cardIdentity(x)===cardIdentity(id)).length;}
function identityTotal(predicate){return new Set(state.cards.flatMap((_,i)=>predicate(i)?[cardIdentity(i)]:[])).size;}
function cardElement(id,owner,modifier=0,thumbnail=false) {
 if(id===-2)return cardBackElement(owner);
 const data=state.cards[id], card=document.createElement('div'); card.className=`card card-face owner-${owner}`;
 const portrait=document.createElement('div'); portrait.className='portrait';
 const image=document.createElement('img'); setCardImage(image,data,thumbnail);if(thumbnail){image.loading='lazy';image.decoding='async';} image.style.objectPosition=data.focus||'50% 40%'; image.alt=''; image.draggable=false; portrait.append(image); card.append(portrait);
 if(data.foil){card.classList.add('foil');appendFoilLayers(portrait);}
 card.title=(data.foil?'Foil · ':'')+data.name;
 const name=document.createElement('span'); name.className='card-name'; name.textContent=data.name; card.append(name);
 card.dataset.rarity=data.rarity||'Common';
 const tier=document.createElement('span'),numeral=['','I','II','III','IV','V','VI','VII','VIII','IX','X'][data.tier]||String(data.tier); tier.className='tier'; tier.dataset.rarity=card.dataset.rarity; tier.append(tierNumeral(numeral)); tier.title=`Tier ${numeral} · ${card.dataset.rarity}`; tier.setAttribute('aria-label',`Tier ${data.tier}, ${card.dataset.rarity}`); card.append(tier);
 const affinity=document.createElement('span');affinity.className='card-affinities';affinity.title=(data.affinities||[]).join(' / ')||'Neutral';for(const name of data.affinities||[])affinity.append(affinityIcon(name));card.append(affinity);
 if(modifier){const change=document.createElement('span');change.className='affinity-modifier '+(modifier>0?'positive':'negative');change.textContent=(modifier>0?'+':'−')+Math.abs(modifier);change.title='All sides '+change.textContent;card.append(change);}
 ['top','right','bottom','left'].forEach((side,index)=>{ const rank=document.createElement('span'); rank.className=`rank ${side}`; rank.textContent=data.sides[index]===10?'A':data.sides[index];if(modifier)rank.title=`${data.sides[index]} ${modifier>0?'+':'−'} ${Math.abs(modifier)} = ${data.sides[index]+modifier}`; card.append(rank); });
 return card;
}
function description(id) { if(id===-2)return 'Hidden card'; const c=state.cards[id]; return `${c.foil?'Foil · ':''}${c.name}: ${c.sides.join(', ')}`; }
function render() {
 if(!state) return;
 applyCardFaceSettings();
 clearBoardGhosts();
 updateTableMode();
 const focus=document.activeElement?.id;
 $('settings-open').hidden=state.phase!=='ready';renderBindings();renderInterruptedMatch();
 $('music').setAttribute('aria-pressed',String(Boolean(state.musicEnabled)));
 $('music').disabled=!state.musicAvailable;
 $('music').title=state.musicAvailable?(state.musicEnabled?'Turn match music off':'Turn match music on'):'No match tracks installed';
 $('table').hidden=false; $('backdrop').hidden=false;
 renderCampaignState();
 $('guided-lesson').hidden=state.screen!=='lesson';
 if(state.screen==='lesson'){renderLesson();return;}
 if(renderRuleRequest())return;
 const shopping=state.phase==='ready'&&state.screen==='shop';
 $('shop').hidden=!shopping;
 if(shopping){$('album').hidden=true;$('challenge').hidden=true;$('game').hidden=true;$('rules-label').textContent='';$('music').hidden=true;$('leave').setAttribute('aria-label','Close card trading');$('leave').title='Close card trading (Esc)';renderShop();return;}
 const album=state.phase==='ready'&&state.screen==='album';
 const exitLabel=album?(state.matchContext??Boolean(state.opponent))?'Back to match setup':'Close album':state.phase==='ready'?'Leave match setup':'Leave match';
 $('leave').setAttribute('aria-label',exitLabel);$('leave').title=exitLabel+' (Esc)';
 $('music').hidden=Boolean(album)&&!state.matchContext;
 $('album').hidden=!album; $('challenge').hidden=album||state.phase!=='ready'; $('game').hidden=state.phase==='ready';
 if(album) { $('rules-label').textContent=''; renderAlbum(); return; }
 $('challenge-name').textContent=state.opponent; $('rival-name').textContent=state.opponent;
 $('rules-label').textContent=rulesText(state.rules);
 renderRuleChips($('board-rules'),state.rules,state.collection?.fixedTrade,state.collection?.practice);
 if(state.phase==='ready') { renderCollection(); if(!openSelectMenu&&!document.querySelector('.modal-shade:not([hidden])')) { if(focus&&$(focus)?.getClientRects().length&&!$(focus).disabled) $(focus).focus({preventScroll:true}); else document.querySelector('#selected-deck button')?.focus(); } return; }
 const score=state.hands.map(hand=>hand.filter(id=>id!==null).length);
 for(const cell of displayBoard) if(cell) score[cell.owner]++;
 updateTableScores(score);
 $('player-nameplate').classList.toggle('active',state.phase==='match'&&!state.opening&&state.turn===0);
 $('rival-nameplate').classList.toggle('active',state.phase==='match'&&!state.opening&&state.turn===1);
 $('board-stakes').textContent=tableStakes();
 $('group-status').textContent=state.activeGroup?`${state.activeGroup} ${state.groupModifier>0?'+1':'−1'}`:'';
 const yourTurn=state.phase==='match'&&state.turn===0&&!state.thinking&&!state.settling&&!busy&&!sent;
 if(!state.opening&&state.turn===0&&state.forcedHand>=0&&playableHand(state.forcedHand))selected=state.forcedHand;
 $('table-inspect').disabled=selected<0||busy;
 $('match-rules-open').disabled=busy||sent||state.settling||state.thinking;
 for(let p=0;p<2;p++) {
   const hand=$(p===0?'player-hand':'rival-hand'); hand.replaceChildren();
   state.hands[p].forEach((id,h)=>{
     const slot=document.createElement('button'); slot.id=`hand-${p}-${h}`; slot.className='hand-slot';
     slot.disabled=id===null||id===-2||busy;
     if(busy&&state.opening&&state.swapped?.[p]===h&&!$('rule-callout').hidden&&$('rule-callout').textContent==='Swap')slot.classList.add('swap-highlight');
     if(id===null) { slot.classList.add('spent'); slot.setAttribute('aria-label','Played'); }
     else { slot.append(cardElement(id,p,state.handModifiers?.[p]?.[h]||0)); slot.setAttribute('aria-label',description(id)); }
     slot.querySelector('.card-face')?.append(ownershipCorners());
     if(p===0&&h===selected&&id!==null) { slot.classList.add('selected'); slot.setAttribute('aria-pressed','true'); }
     slot.onclick=()=>{ if(p===1||!yourTurn||!playableHand(h)){inspectTableCard(id,p,state.handModifiers?.[p]?.[h]||0);return;} selected=h; render(); $('square-'+state.board.findIndex(x=>x===null))?.focus(); };
     slot.oncontextmenu=e=>{e.preventDefault();inspectTableCard(id,p,state.handModifiers?.[p]?.[h]||0);};
     slot.draggable=false;slot.dataset.placeable=String(p===0&&yourTurn&&playableHand(h));
     if(p===0&&yourTurn&&playableHand(h)){slot.dataset.tableDrag='hand';slot.dataset.dragValue=h;}
     hand.append(slot);
   });
 }
 displayBoard.forEach((value,index)=>{
   let square=$(`square-${index}`);
   if(!square) {
     square=document.createElement('button'); square.id=`square-${index}`; square.className='square'; square.dataset.position=index+1;
     $('board').append(square);
   }
   square.disabled=value?busy:!yourTurn||selected<0;
   const tile=state.tiles?.[index]||0;
   square.setAttribute('aria-label',`Row ${Math.floor(index/3)+1}, column ${index%3+1}${value?': '+description(value.card):''}${tile?', '+affinityNames[Math.log2(tile)]+' affinity':''}`);
   const cardKey=value?`${value.card}:${state.modifiers?.[index]||0}`:'empty';
   if(square.dataset.card!==cardKey) {
     square.replaceChildren(); square.dataset.card=cardKey;
     if(value) { const card=cardElement(value.card,value.owner,state.modifiers?.[index]||0);card.append(ownershipCorners());square.append(card); }
   }
   if(value) {
     square.firstElementChild.classList.toggle('owner-0',value.owner===0);
     square.firstElementChild.classList.toggle('owner-1',value.owner===1);
   }
   square.dataset.affinity=String(tile);
   let marker=square.querySelector('.tile-affinity');
   if(tile&&!value) {
     if(!marker){marker=document.createElement('span');marker.className='tile-affinity';square.append(marker);}
     const i=Math.log2(tile);marker.replaceChildren(affinityIcon(affinityNames[i]));
     square.style.setProperty('--affinity-color',affinityColors[i]);
   }else marker?.remove();
   square.classList.toggle('rule-highlight',Boolean(highlights&(1<<index)));
   square.classList.toggle('empty',value===null);
   square.classList.toggle('available',value===null&&yourTurn&&selected>=0);
   square.onclick=()=>{ if(value){inspectTableCard(value.card,value.owner,state.modifiers?.[index]||0);return;} if(sent||!yourTurn||selected<0) return; clearBoardGhosts();requestPlacement(selected,index); };
   square.oncontextmenu=e=>{e.preventDefault();if(value)inspectTableCard(value.card,value.owner,state.modifiers?.[index]||0);};
   square.onmouseenter=()=>showBoardGhost(index);square.onmouseleave=clearBoardGhosts;

 });
 renderReward();
 $('match-footer').hidden=state.phase==='result';
 $('turn').classList.toggle('opponent-turn',state.turn===1);
 $('turn').textContent=busy||state.settling||state.phase==='result'?'':(state.turn===0?(state.forcedHand>=0?'Place the highlighted card':'Your turn'):`${state.opponent}'s turn`);
 if(!$('match-rules').hidden||!$('table-inspection').hidden||openSelectMenu||!$('forfeit').hidden||!$('reward').hidden||!$('post-match').hidden) return;
 if(focus&&$(focus)&&!$(focus).disabled&&$(focus).getClientRects().length) $(focus).focus({preventScroll:true});
 else if(yourTurn) document.querySelector('#player-hand button[data-placeable="true"]')?.focus({preventScroll:true});
}
function resultLabel(score) { return score[0]===score[1]?'Draw':score[0]>score[1]?'You Win!':'You Lose'; }
async function announce(label,tone,duration,token,result=false) {
 const overlay=$('rule-callout'),text=document.createElement('span');
 text.className='announcement-text'; text.textContent=label;
 overlay.replaceChildren(text); overlay.dataset.tone=tone; overlay.dataset.result=String(result);
 overlay.style.setProperty('--announcement-duration',`${duration}ms`);
 overlay.hidden=false;
 await pause(duration);
 if(token===playbackToken) overlay.hidden=true;
}
async function playOpening(next) {
 const token=playbackToken;
 busy=true; displayBoard=next.board.map(cell=>cell?{...cell}:null); render();
 const hand=$(next.turn===0?'player-hand':'rival-hand');
 const name=$('game').querySelector(next.turn===0?'.player-name':'.rival-name');
 if(next.swapped?.[0]>=0&&!next.redeals){
   const swapped=next.swapped.map((h,p)=>$(`hand-${p}-${h}`));swapped.forEach(el=>el?.classList.add('swap-highlight'));
   await announce('Swap','rule',2200,token);if(token!==playbackToken)return;
   document.querySelectorAll('.swap-highlight').forEach(el=>el.classList.remove('swap-highlight'));
 }
 await drawStarter(next,token);if(token!==playbackToken)return;
 hand.classList.add('first-turn'); name.classList.add('first-turn');
 send('opening');
 await announce(next.turn===0?'You start':`${next.opponent} starts`,next.turn===0?'first-player':'first-rival',1100,token);
 if(token!==playbackToken) return;
 hand.classList.remove('first-turn'); name.classList.remove('first-turn');
 busy=false; render(); send('settled');
}
async function playCaptures(next) {
 const token=playbackToken;
 busy=true;
 displayBoard=next.board.map(cell=>cell?{...cell,owner:cell.flip?1-cell.owner:cell.owner}:null);
 render();
 let comboAnnounced=false;
 const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
 for(const stage of next.stages||[]) {
   if(token!==playbackToken) return;
   highlights=stage.highlights;
   render();
   if(stage.label&&(stage.label!=='Combo'||!comboAnnounced)) {
     if(stage.label==='Combo') comboAnnounced=true;
     for(const rule of stage.label.split(' / ')) send('rule',rule.toLowerCase().replaceAll(' ',''));
     await announce(stage.group?`${stage.label} · ${stage.group}`:stage.label,stage.label==='Combo'?'combo':stage.group?'group':'rule',760,token);
     if(token!==playbackToken) return;
   }
   // Flip a capture wave together. Keep later Combo waves in their native order.
   const captured=displayBoard.map((cell,i)=>cell&&(stage.captures&(1<<i))?i:-1).filter(i=>i>=0),animations=[];
   if(!reduced&&captured.length) {
     for(const i of captured) {
       const animation=$(`square-${i}`).firstElementChild.animate([
         {transform:'perspective(700px) rotateY(0deg)'},
         {transform:'perspective(700px) rotateY(90deg)',offset:.5},
         {transform:'perspective(700px) rotateY(0deg)'}
       ],{duration:400,easing:'ease-in-out'});
       flips.add(animation);animations.push(animation);
     }
     await pause(200);if(token!==playbackToken)return;
   }
   for(const i of captured){displayBoard[i]={...next.board[i]};highlights&=~(1<<i);}render();
   if(animations.length){await pause(200);if(token!==playbackToken)return;for(const animation of animations){animation.cancel();flips.delete(animation);}}
   highlights=0;
   render();
 }
 if(token!==playbackToken) return;
 displayBoard=next.board.map(cell=>cell?{...cell}:null);
 if(next.suddenDeath){
   render();await announce('Sudden Death · Redeal','draw',1600,token);
   if(token!==playbackToken)return;
 }
 if(next.phase==='result') {
   render();
   send('result');
   await announce(resultLabel(next.score),next.score[0]===next.score[1]?'draw':next.score[0]>next.score[1]?'win':'lose',1800,token,true);
   if(token!==playbackToken) return;
 }
 busy=false; render();
 send('settled');
}
window.ttcgReset=()=>{
 interruptedForfeit=false;$('interrupted-match').hidden=true;
 resetShop();
 resetTablePresentation();
 closeGroupChoice();
 resetLesson();
 ruleAnswerPending='';$('rule-request').hidden=true;$('table').classList.remove('rule-prompt');
 closeDeckDialog();closeAbolition();resetSettings();closeSelectMenu(); cancelPlayback(); state=null; lastRevision=''; selected=-1; sent=false; displayBoard=[];
 foilMotion.stop(); $('board').replaceChildren(); $('table').hidden=true; $('backdrop').hidden=true;
 $('forfeit').hidden=true;$('tournament-confirm').hidden=true;$('tournament-outcome').hidden=true;tournamentConfirmation=null;resetTournaments();resetTrade();resetAlbum();
};
window.ttcgState=raw=>{
 const next=typeof raw==='string'?JSON.parse(raw):raw;
 const key=`${next.session}:${next.revision}`;
 const changed=key!==lastRevision;
 if(next.screen!=='lesson'||next.session!==state?.session)resetLesson();
 clearTablePointer();
 if(changed) { closeMatchRules(false);hideRuleTooltip();closeShopConfirmation(false);closeAbolition(false);clearBoardDrag();closeTableInspection(false);closeGroupChoice();cancelPlayback(); selected=-1; sent=false; lastRevision=key; }
 if(!state||next.session!==state.session) { tableScore=null;resetTrade(); closeSelectMenu(); $('board').replaceChildren(); }
 if(next.albumSection==='tournaments'&&next.session!==state?.session&&next.screen==='album'){albumTab='tournaments';routeTournamentBoard(next.collection?.tournaments);}
 state=next;
 $('tournament-outcome').hidden=true;
 if(next.screen!=='album')albumRoute=null;
 updateCardBackImage();
 if(changed&&next.settling) { if(next.opening) void playOpening(next); else void playCaptures(next); return; }
 if(!busy) displayBoard=next.board.map(cell=>cell?{...cell}:null);
 render();
 // A large album can delay the paired browser key event until setup has rendered.
 if(escapeReturnPending){if(next.screen==='lobby')lastEscapeTime=performance.now();escapeReturnPending=false;}
};
function closeView() {
 if(closeShopConfirmation())return;
 if(closeTournamentConfirmation())return;
 if(closeTournamentOutcome())return;
 if(closeTableInspection())return;
 if(closeMatchRules())return;
 if(cancelTableDrag())return;
 if(closeDeckDialog())return;
 if(closeGroupChoice())return;
 if(closeAbolition())return;
 if(closeSettings())return;
 if(albumDrag) { clearAlbumDrag(); return; }
 if(!$('album-modal').hidden) { closeAlbumModal(); return; }
 if(closeAlbumPopover())return;
 if(trade&&trade.phase!=='done') return;
 interruptedForfeit=false;
 if(state?.collection?.tournamentID&&state.phase==='match'){$('forfeit-cost').textContent='This ends your tournament run.';$('forfeit').hidden=false;$('keep-playing').focus();return;}
 if(state?.collection?.staked&&state.phase!=='ready') {
   if(state.phase==='result') { renderReward(); return; }
   $('forfeit-cost').textContent=[(state.collection.trade)?`Lose ${(state.collection.trade||1)===1?'one card':'five cards'}`:'',state.collection.wager?`Lose ${state.collection.wager} gold`:''].filter(Boolean).join(' · ');
   $('forfeit').hidden=false; closeSelectMenu(); $('keep-playing').focus(); return;
 }
 if(state?.screen==='album'&&(state.matchContext??Boolean(state.opponent))){send('browse','lobby');return 'lobby';}
 send('close'); window.ttcgReset();
}
$('leave').onclick=closeView;
$('music').onclick=()=>send('music',state.musicEnabled?'0':'1');
$('start').onclick=()=>{ if($('start').disabled)return;if(deckCount(draftDeck)<5){openLobbyAlbum('decks',draftDeck.indexOf(-1));return;}send('start',`${state.collection.fixedRules??state.rules} ${state.collection.fixedTrade??0} ${$('wager').value} ${draftDeck.join(' ')}`); };
$('play-again').onclick=()=>{if(state.collection?.tournamentID){openTournamentBoard(state.collection.tournamentID);return;}if($('play-again').disabled||$('play-again').hidden||state.collection?.canPlay===false)return;$('play-again').disabled=true;$('post-leave').disabled=true;send('prepare');};
$('post-leave').onclick=closeView;
let lastEscapeTime=-Infinity,lastEscapeSource='',escapeReturnPending=false;
window.ttcgEscape=(source='native')=>{
 // Skyrim's native key event and Ultralight may report the same press.
 if(!state) return;
 if(albumDrag) { clearAlbumDrag(); return; }
 const now=performance.now();
 if(source!==lastEscapeSource&&now-lastEscapeTime<200) return;
 lastEscapeTime=now; lastEscapeSource=source;
 if(albumNameEdit){finishAlbumRename(false);return;}
 if(!state) return;
 if(!$('forfeit').hidden) { $('forfeit').hidden=true; $('leave').focus(); }
 else if(openSelectMenu) closeSelectMenu(true);
 else if(closeDeckDialog())return;
 else if(closeView()==='lobby')escapeReturnPending=true;
};
window.ttcgKey=key=>{
 if(!state) return;
 if(albumNameEdit&&key==='cancel'){window.ttcgEscape('gamepad');return;}
 if(albumNameEdit&&key==='confirm'){$('album-deck-name-input').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));return;}
 if(key==='cancel'&&closeTableInspection())return;
 if(key==='cancel'&&closeMatchRules())return;
 if(key==='cancel'&&cancelTableDrag())return;
 if(key==='cancel'&&closeDeckDialog())return;
 if(key==='cancel'&&closeGroupChoice())return;
 if(key==='cancel'&&closeAbolition())return;
 if(key==='cancel'&&closeSettings())return;
 if(key==='cancel'&&!$('album-modal').hidden) { closeAlbumModal(); return; }
 if(selectMenuKey(key)) return;
 if(key==='cancel'&&closeAlbumPopover())return;
 if(!$('album-modal').hidden&&$('album-modal').classList.contains('inspecting')&&['left','right','previous','next'].includes(key)){stepAlbumInspection(['left','previous'].includes(key)?-1:1);return;}
 if(state.screen==='album'&&!document.querySelector('.modal-shade:not([hidden])')&&$('album-filter-panel').hidden&&['previous','next'].includes(key)){
   const tabs=['cards','decks','opponents','tournaments','rules'];setAlbumTab(tabs[(tabs.indexOf(albumTab)+(key==='previous'?tabs.length-1:1))%tabs.length]);return;
 }
 if(key==='cancel') {
   if(!$('forfeit').hidden) { $('forfeit').hidden=true; $('leave').focus(); return; }
   if(selected>=0&&!(state.turn===0&&state.forcedHand>=0)) { selected=-1; render(); }
   else closeView();
   return;
 }
 if(key==='confirm') { document.activeElement?.click(); return; }
 const scope=activeInteractionScope();
 if(scope===$('table')&&document.activeElement?.id.startsWith('album-card-')&&['left','right','up','down'].includes(key)){
   const id=Number(document.activeElement.id.slice(11)),index=albumVisible.indexOf(id),columns=getComputedStyle($('album-grid')).gridTemplateColumns.split(' ').length;
   const next=index+({left:-1,right:1,up:-columns,down:columns}[key]);
   if(next>=0&&next<albumVisible.length){const target=$('album-card-'+albumVisible[next]);target.focus({preventScroll:true});target.scrollIntoView({block:'nearest',inline:'nearest'});}return;
 }
 const buttons=visibleControls(scope);
 if(!buttons.length) return;
 let current=buttons.indexOf(document.activeElement);
 if(document.activeElement?.tagName==='SELECT'&&(key==='left'||key==='right')) {
   const select=document.activeElement; select.selectedIndex=(select.selectedIndex+(key==='right'?1:select.options.length-1))%select.options.length; select.dispatchEvent(new Event('change')); return;
 }
 let step=(key==='previous'||key==='left'||key==='up')?-1:1;
 if(document.activeElement?.id.startsWith('square-')&&(key==='up'||key==='down')) step*=3;
 current=(current+step+buttons.length)%buttons.length; buttons[current].focus({preventScroll:true});
};
window.addEventListener('keydown',event=>{
 if(event.key==='Escape'||event.key==='Esc'||event.code==='Escape'||event.keyCode===27) { event.preventDefault(); if(!event.repeat) window.ttcgEscape('dom'); return; }
 const keys={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right',...(openSelectMenu?{Home:'home',End:'end'}:{})};
 if(event.target.tagName==='INPUT') return;
 if(event.shiftKey&&event.target.id?.startsWith('album-deck-')) return;
 if(keys[event.key]) { event.preventDefault(); window.ttcgKey(keys[event.key]); }
});

function deckSlots(deck) { return Array.from({length:5},(_,i)=>Number.isInteger(deck[i])&&deck[i]>=0?deck[i]:-1); }
function deckCount(deck) { return deck.filter(id=>id>=0).length; }
function insertDeckCard(deck,id,index=deck.indexOf(-1)) {
 if(index<0||index>4||!deck.includes(-1)||identityCopies(deck,id)>=(state.cards[id].unique?1:2))return;
 if(deck[index]<0){deck[index]=id;return;}
 let gap=deck.indexOf(-1,index);
 if(gap>=0) {for(let i=gap;i>index;--i)deck[i]=deck[i-1];}
 else {gap=deck.lastIndexOf(-1);for(let i=gap;i<index;++i)deck[i]=deck[i+1];}
 deck[index]=id;
}
let draftDeck=Array(5).fill(-1);
function renderCollection() {
 const c=state.collection;if(!c)return;
 draftDeck=deckSlots(c.deck);
 renderDeckTools('lobby');renderCultureTerms();updateTableMode();
 const fixedWager=c.fixedWager??-1,flags=c.fixedRules??state.rules;
 $('library-open').textContent=c.tournamentID?'View bracket':'Album';
 $('challenge-skill').textContent=[c.hold,c.tournamentID?'Tournament':c.skill].filter(Boolean).join(' · ');
 renderRuleChips($('normal-rules'),flags,c.fixedTrade,c.practice);
 $('rule-lesson').hidden=!c.ruleLesson;$('rule-lesson').textContent='Show '+['','Three Open','Legion','Decimation','Affinity'][c.ruleLesson||0];
 $('match-summary').hidden=!c.ruleLesson;
 $('wager-control').hidden=Boolean(c.practice)||fixedWager>=0||!c.maxWager;
 $('wager').disabled=!c.canStake;
 for(const option of $('wager').options)option.disabled=Number(option.value)>Math.min(c.gold,c.rivalGold,c.maxWager??0);
 if(c.practice)$('wager').value='0';else if(fixedWager>=0)$('wager').value=String(fixedWager);
 else if(!c.canStake||$('wager').selectedOptions[0]?.disabled)$('wager').value='0';
 $('wager')._refresh();
 const count=deckCount(draftDeck),incomplete=count<5;
 $('deck-label').textContent=`${count}/5`;
 $('fixed-wager').textContent=fixedWager>0?`Wager: ${fixedWager} gold`:'';
 renderLobbyCards();
 const shortage=fixedWager>Math.min(c.gold,c.rivalGold),pending=Boolean(c.ruleRequest||c.culture?.offerRule);
 $('start').textContent=incomplete?'Complete deck':'Play';
 $('start').disabled=pending||(!incomplete&&(c.canPlay===false||shortage));
 $('match-terms').textContent=c.notice||(!incomplete&&c.canPlay===false?c.playUnavailableReason||'Unavailable for a game.':shortage?'Not enough gold for this wager.':'');
}
// Presentation stays separate from escrow: a transfer completes only after the
// native inventory acknowledgement. Slot indices preserve duplicate copies.
let trade=null;
const tradeFlights=new Set();
function resetTrade() {
 for(const flight of tradeFlights) { flight.animation.cancel(); flight.clone.remove(); }
 tradeFlights.clear(); trade=null;
 $('game').classList.remove('trade-collected'); $('reward').hidden=true;$('post-match').hidden=true;$('play-again').disabled=false;$('post-leave').disabled=false;
}
async function flyCard(source,target,token,delay=0) {
 if(!source||!target||matchMedia('(prefers-reduced-motion: reduce)').matches) return;
 const from=source.rect,to=target.getBoundingClientRect();
 if(!from.width||!to.width) return;
 const clone=foilMaterial.clone(source.card);
 // Animate in viewport coordinates so the scaled table does not distort paths.
 const wrapper=document.createElement('div'); wrapper.className='trade-flight';
 Object.assign(wrapper.style,{left:`${from.x}px`,top:`${from.y}px`,width:'160px',height:'210px'});
 clone.removeAttribute('style'); wrapper.append(clone); document.body.append(wrapper);
 const animation=wrapper.animate([
   {transform:`translate(0,0) scale(${from.width/160},${from.height/210})`,opacity:1},
   {transform:`translate(${to.x-from.x}px,${to.y-from.y}px) scale(${to.width/160},${to.height/210})`,opacity:1}
 ],{duration:580,delay,easing:'cubic-bezier(.22,.7,.18,1)',fill:'both'});
 const flight={animation,clone:wrapper}; tradeFlights.add(flight);
 await pause(580+delay);
 animation.cancel(); wrapper.remove(); tradeFlights.delete(flight);
 if(token!==playbackToken) return;
}
function tradeSources() {
 const sources=[];
 document.querySelectorAll('#board .card,#player-hand .card,#rival-hand .card').forEach(card=>{
   const parent=card.parentElement;
   const id=parent.id.startsWith('square-')?displayBoard[Number(parent.id.slice(7))]?.card:state.hands[Number(parent.id.split('-')[1])][Number(parent.id.split('-')[2])];
   const p=Number(parent.id.split('-')[1]),h=Number(parent.id.split('-')[2]);
   const origin=parent.id.startsWith('square-')?displayBoard[Number(parent.id.slice(7))]?.origin:state.handOrigins?.[p]?.[h]??p*5+h;
   sources.push({id,origin,card:foilMaterial.clone(card),rect:card.getBoundingClientRect()});
 });
 return sources;
}
function updateTrade() {
 if(!trade) return;
 const c=state.collection,t=trade;
 $('reward').dataset.phase=t.phase;
 $('trade-gold').textContent=t.wager?(t.winner<0?`${t.wager} gold returned`:`${t.winner===0?'+':'−'}${t.wager} gold`):'';
 $('trade-gold').dataset.tone=t.winner===0?'win':t.winner===1?'lose':'draw';
 $('trade-status').textContent=c.notice||'';
 $('reward-title').textContent=t.phase==='done'?resultLabel(state.score):t.chooses&&t.winner>=0?(t.winner===0?(t.required===1?'Choose a card':`Choose ${t.required} cards · ${t.choices.size}/${t.required}`):`${state.opponent}'s choice`):resultLabel(state.score);
 for(const button of $('reward').querySelectorAll('.trade-hand button')) {
   const origin=Number(button.dataset.owner)*5+Number(button.dataset.slot);
   const chosen=t.kind===3?t.destinations[origin]!==Math.floor(origin/5):Number(button.dataset.owner)===1-t.winner&&t.choices.has(Number(button.dataset.slot));
   button.classList.toggle('selected',chosen);
   button.setAttribute('aria-pressed',String(chosen));
   button.disabled=!['selecting','opponent'].includes(t.phase);
 }
 $('trade-inspect').disabled=!['selecting','opponent'].includes(t.phase)||!t.inspectSlot;
 const action=$('take-card');
 action.hidden=t.phase==='done'||(!c.paying&&!(t.chooses&&t.winner===0));
 action.textContent=c.paying?'Retry':'Confirm';
 action.disabled=!(c.paying||(t.phase==='selecting'&&t.choices.size===t.required));
}
async function transferTrade(token) {
 const t=trade; if(!t||['reviewing','transferring','done'].includes(t.phase)) return;
 // A native payout failure must not be presented as a completed transfer.
 if(state.collection.staked) { updateTrade(); return; }
 t.phase='reviewing';updateTrade();
 while(!$('table-inspection').hidden){await pause(100);if(token!==playbackToken||trade!==t)return;}
 t.phase='transferring'; updateTrade();
 const destinations=t.kind===3?t.destinations:Array.from({length:10},(_,i)=>i/5|0);
 if(t.kind!==3&&t.winner>=0)for(const h of t.choices)destinations[(1-t.winner)*5+h]=t.winner;
 for(let origin=0;origin<10;++origin)if(destinations[origin]!==Math.floor(origin/5)){
   const p=Math.floor(origin/5),h=origin%5,receiver=destinations[origin];
   const slot=document.querySelector(`.trade-hand button[data-owner="${p}"][data-slot="${h}"]`);
   const card=slot?.firstElementChild,target=$(`trade-owner-${receiver}`);
   if(card){
     const source={card:foilMaterial.clone(card),rect:card.getBoundingClientRect()};
     const received=cardElement(t.hands[p][h],receiver);received.style.setProperty('--stack-index',target.children.length);
     // The receiving stack keeps every transferred card visible, including both directions.
     target.append(received);target.classList.add('receiving');slot.classList.add('transferred');
     await flyCard(source,received,token);
     if(token!==playbackToken||trade!==t)return;
     target.classList.remove('receiving');target.classList.add('received');
   }
 }
 t.phase='done'; updateTrade();
 await pause(matchMedia('(prefers-reduced-motion: reduce)').matches?150:650);
 if(token!==playbackToken||trade!==t)return;
 showPostMatch();
}
async function gatherTrade(t,sources,token) {
 const flights=[];
 for(let p=0;p<2;p++) for(let h=0;h<5;h++) {
   const slot=document.querySelector(`.trade-hand button[data-owner="${p}"][data-slot="${h}"]`);
   const index=sources.findIndex(source=>source.origin>=0?source.origin===p*5+h:source.id===t.hands[p][h]);
   if(index<0) continue;
   slot.classList.add('gathering');
   flights.push(flyCard(sources.splice(index,1)[0],slot,token,(p*5+h)*45).then(()=>slot.classList.remove('gathering')));
 }
 $('game').classList.add('trade-collected');
 await Promise.all(flights);
 if(token!==playbackToken||trade!==t) return;
 if(t.chooses&&t.winner===0) {
   t.phase='selecting'; updateTrade(); $('prize-0')?.focus();
 } else {
   t.phase='opponent'; updateTrade();
   if((t.kind&&t.winner>=0)||t.kind===3) {
     if(t.kind!==3){const mask=state.collection.selectedMask??(1<<state.collection.choice);t.choices=new Set(Array.from({length:5},(_,h)=>h).filter(h=>mask&(1<<h)));}
     updateTrade(); await pause(1000);
     if(token!==playbackToken||trade!==t) return;
   }
   await transferTrade(token);
 }
}
function showPostMatch() {
 if(state.phase!=='result'||busy||state.settling||state.collection?.staked||state.collection?.paying)return;
 $('reward').hidden=true;
 if(state.collection?.tournamentID){showTournamentOutcome(activeTournament(),true);return;}
 const opening=$('post-match').hidden,canPlay=Boolean(state.collection?.tournamentID)||state.collection?.canPlay!==false;
 const replay=$('play-again'),leave=$('post-leave'),reason=$('post-match-reason');
 replay.textContent=state.collection?.tournamentID?'Tournament':'Play again';
 replay.hidden=!canPlay;reason.textContent=canPlay?'':state.collection?.playUnavailableReason||state.collection?.notice||'They are unavailable for another game.';reason.hidden=!reason.textContent;
 leave.classList.toggle('primary',!canPlay);
 if(!canPlay)leave.disabled=false;
 renderTournamentNotice();
 $('post-match').hidden=false;
 if(opening||(!canPlay&&document.activeElement===replay)){closeSelectMenu();(canPlay?replay:leave).focus();}
}
function renderReward() {
 const c=state.collection;
 if(state.phase==='result') $('forfeit').hidden=true;
 if(state.phase!=='result'||state.settling||busy)return;
 if(!(c?.trade||c?.wager)) {showPostMatch();return;}
 if(trade) {
   updateTrade();
   if(trade.phase==='claiming'&&!c.staked) void transferTrade(playbackToken);
   return;
 }
 const sources=tradeSources();
 const winner=state.score[0]===state.score[1]?-1:state.score[0]>state.score[1]?0:1;
 const kind=c.trade,required=c.required??(kind===1?1:kind===2?Math.min(5,Math.abs(state.score[0]-state.score[1])):kind===4?5:0);
 trade={phase:'gathering',winner,kind,chooses:kind===1||kind===2,required,wager:c.wager,choices:new Set(),destinations:c.transferOwners||Array.from({length:10},(_,i)=>i/5|0),hands:state.originalHands||state.fullHands||[c.deck,c.rivalDeck]};
 const t=trade;
 $('reward').hidden=false; closeSelectMenu(); $('forfeit').hidden=true;
 $('trade-rival-name').textContent=state.opponent;
 for(let p=0;p<2;p++) {
   const target=$(p===1?'reward-cards':'trade-player-cards'); target.replaceChildren();
   $(`trade-owner-${p}`).replaceChildren(); $(`trade-owner-${p}`).className='trade-received';
   t.hands[p].forEach((id,h)=>{
     const slot=document.createElement('button'); slot.id=p===1?`prize-${h}`:`trade-card-${h}`;
     slot.dataset.owner=p; slot.dataset.slot=h; slot.append(cardElement(id,p));
     slot.setAttribute('aria-label',state.cards[id].name);
     slot.onclick=()=>{ if(t.phase!=='selecting'||p!==1){inspectTradeCard(slot);return;} if(t.choices.has(h))t.choices.delete(h);else if(t.required===1)t.choices=new Set([h]);else if(t.choices.size<t.required)t.choices.add(h);updateTrade(); };
     slot.onfocus=slot.onmouseenter=()=>{t.inspectSlot=slot;$('trade-inspect').disabled=!['selecting','opponent'].includes(t.phase);};
     slot.oncontextmenu=e=>{e.preventDefault();inspectTradeCard(slot);};
     target.append(slot);
   });
 }
 updateTrade(); void gatherTrade(t,sources,playbackToken);
}
function inspectTradeCard(slot=document.activeElement?.closest('.trade-hand button')||trade?.inspectSlot){
 if(!trade||!slot||!['selecting','opponent'].includes(trade.phase))return;
 inspectTableCard(trade.hands[Number(slot.dataset.owner)][Number(slot.dataset.slot)],Number(slot.dataset.owner));
}
$('trade-inspect').onclick=()=>inspectTradeCard();
$('take-card').onclick=()=>{
 if(!trade||$('take-card').disabled) return;
 if(state.collection.paying) { trade.phase='claiming'; send('payout'); updateTrade(); return; }
 if(trade.phase==='selecting'&&trade.choices.size===trade.required) {
   trade.phase='claiming'; updateTrade(); if(trade.kind===1)send('claim',String([...trade.choices][0]));else send('claim-many',String([...trade.choices].reduce((mask,h)=>mask|(1<<h),0)));
 }
};
let interruptedForfeit=false;
function renderInterruptedMatch(){
 const saved=state?.interruptedMatch,show=Boolean(saved)&&state.phase==='ready'&&state.screen==='album';
 $('interrupted-match').hidden=!show;if(!show)return;
 $('interrupted-match-copy').textContent=saved.tournament?`${saved.finished?'Collect your rewards':'Resume your game'} at ${saved.venue}.`:
  `${saved.finished?'Finish your game':'Resume your game'} with ${saved.opponent}.`;
 $('interrupted-resume').hidden=!saved.canResume;
 $('interrupted-resume').textContent=saved.finished?'Collect rewards':'Resume game';
 $('interrupted-forfeit').hidden=Boolean(saved.finished);
}
$('interrupted-resume').onclick=()=>send('interrupted','resume');
$('interrupted-forfeit').onclick=()=>{
 if(!state?.interruptedMatch)return;
 interruptedForfeit=true;$('forfeit-cost').textContent=state.interruptedMatch.tournament?'This ends your tournament run.':'The game counts as a loss. Its trade rule and wager still apply.';
 $('forfeit').hidden=false;$('keep-playing').focus();
};
$('keep-playing').onclick=()=>{ $('forfeit').hidden=true;(interruptedForfeit?$('interrupted-forfeit'):$('leave')).focus();interruptedForfeit=false; };
$('confirm-forfeit').onclick=()=>{const saved=interruptedForfeit;interruptedForfeit=false;$('forfeit').hidden=true;send(saved?'interrupted':'forfeit',saved?'forfeit':'');};
$('wager').addEventListener('change',renderCollection);
window.addEventListener('keydown',event=>{
 if(event.key!=='Tab'||event.defaultPrevented)return;
 const scope=activeOverlay();if(!scope)return;
 const controls=visibleControls(scope);if(!controls.length)return;
 const index=controls.indexOf(document.activeElement);
 event.preventDefault();controls[(index+(event.shiftKey?controls.length-1:1))%controls.length].focus({preventScroll:true});
});

let groupPlacement=null;
function closeGroupChoice(){if(!groupPlacement)return false;groupPlacement=null;$('group-choice').hidden=true;return true;}
function requestPlacement(hand,square){
 if(!state||state.phase!=='match'||state.turn!==0||state.thinking||state.settling||busy||sent||document.querySelector('.modal-shade:not([hidden])')||!Number.isInteger(hand)||hand<0||hand>4||!playableHand(hand)||!Number.isInteger(square)||square<0||square>8||state.board[square])return;
 const id=state.hands[0][hand],groups=state.cards[id].groups||[];
 const candidates=state.activeGroup||!(state.rules&384)?[]:groups.filter(g=>1+state.board.filter(c=>c&&(state.cards[c.card].groups||[]).includes(g)).length>=3);
 if(candidates.length<=1){sent=true;render();send('play',`${hand} ${square}`);return;}
 groupPlacement={hand,square};const all=[...new Set(state.cards.flatMap(c=>c.groups||[]))].sort();
 $('group-options').replaceChildren();for(const group of candidates){const b=document.createElement('button');b.textContent=group;b.onclick=()=>{const move=groupPlacement;closeGroupChoice();sent=true;render();send('play',`${move.hand} ${move.square} ${all.indexOf(group)}`);};$('group-options').append(b);}
 $('group-choice').hidden=false;$('group-options').firstElementChild.focus();
}
$('group-cancel').onclick=()=>{closeGroupChoice();$('hand-0-'+selected)?.focus();};
$('rule-lesson').onclick=()=>send('rule-lesson');
