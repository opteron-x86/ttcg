'use strict';
let demoKey='',demoFrame=null,demoSelected=false,demoBusy=false,demoGeneration=0;
const demoAnimations=new Set(),demoTimers=new Map();
function lessonCardName(d){return state.cards[d.hands[0][d.hand]]?.name||'the highlighted card';}
function lessonRank(d,square,side){return state.cards[d.board[square]?.card]?.sides[side];}
function basicLessonPrompt(d){
 const rank=(q,s)=>lessonRank(d,q,s),choose=`Choose ${lessonCardName(d)}.`;
 return [
  choose,
  'Take turns filling the board. Watch Opponent place a card.',
  `Diagonal cards do not capture each other. ${choose}`,
  `${rank(3,1)} beats ${rank(4,3)}. The captured card turns blue and counts for you.`,
  `${rank(1,2)} against ${rank(4,0)}: equal numbers do not capture. ${choose}`,
  'Only touching sides are compared. Watch Opponent’s next move.',
  `${rank(5,0)} beats ${rank(2,2)}, and ${rank(5,3)} beats ${rank(4,1)}. One placement can capture two cards. ${choose}`,
  `Your ${rank(8,0)} cannot beat their ${rank(5,2)}. Strong sides help defend a card.`,
  `Opponent captures with ${rank(6,0)} against ${rank(3,2)}. ${choose}`,
  'Five each—a draw. The unplayed card counts too. More than five wins.'
 ][d.step]||'';
}
function comboLessonPrompt(d,plus){
 if(!d.step)return `${plus?'Make two touching pairs add to the same total.':'Match two touching sides to capture with Same.'} Choose ${lessonCardName(d)}.`;
 const a=lessonRank(d,0,1),b=lessonRank(d,1,3),c=lessonRank(d,0,2),e=lessonRank(d,3,0);
 return (plus?`${a} + ${b} and ${c} + ${e} both make ${a+b}. Plus`:`${a} matches ${b}, and ${c} matches ${e}. Same`)+' captures both cards, then a captured card takes its neighbour for Combo.';
}
function resetLesson(){
 demoGeneration++;
 for(const [timer,resolve] of demoTimers){clearTimeout(timer);resolve();}demoTimers.clear();
 for(const a of demoAnimations)a.cancel();demoAnimations.clear();
 demoKey='';demoFrame=null;demoSelected=false;demoBusy=false;
 $('guided-lesson').hidden=true;$('demo-callout').hidden=true;
}
function demoPause(ms){return new Promise(resolve=>{const t=setTimeout(()=>{demoTimers.delete(t);resolve();},ms);demoTimers.set(t,resolve);});}
function demoAnimate(el,frames,options){const a=el.animate(frames,options);demoAnimations.add(a);return a;}
function demoScore(board){
 const score=demoFrame.hands.map(h=>h.filter(id=>id!==null).length);
 for(const cell of board)if(cell)score[cell.owner]++;
 $('demo-player-score').textContent=score[0];$('demo-rival-score').textContent=score[1];
}
function demoControls(focus=false){
 const d=demoFrame;if(!d)return;
 const placement=d.hand>=0,finished=d.step===(d.total||9);
 $('demo-player-nameplate').classList.toggle('active',!demoBusy&&!finished&&placement);
 $('demo-rival-nameplate').classList.toggle('active',!demoBusy&&!finished&&!placement);
 for(let h=0;h<5;h++){
   const el=$(`demo-hand-0-${h}`);el.disabled=demoBusy||h!==d.hand;
   el.classList.toggle('demo-target',placement&&h===d.hand&&!demoBusy);
   el.classList.toggle('selected',h===d.hand&&demoSelected);
   el.setAttribute('aria-pressed',String(h===d.hand&&demoSelected));
 }
 for(let i=0;i<9;i++){
   const el=$(`demo-square-${i}`);el.disabled=demoBusy||!demoSelected||i!==d.square;
   el.classList.toggle('demo-target',demoSelected&&i===d.square&&!demoBusy);
 }
 $('demo-prompt').textContent=demoBusy?'':lessonPrompt(d,demoSelected);
 $('demo-next').hidden=placement||d.step===(d.total||9);$('demo-next').disabled=demoBusy;
 $('demo-replay').hidden=d.step!==(d.total||9);$('demo-replay').disabled=demoBusy;
 $('demo-play').textContent=d.workshopRule?'Back to game':d.sameLesson?'Continue':'Play Opponent';$('demo-next').textContent=d.workshopRule?'Opponent’s turn':d.sameLesson?'Continue':"Opponent's turn";$('demo-play').disabled=demoBusy;$('demo-play').className=d.step===(d.total||9)?'primary':'quiet';
 if(focus&&!demoBusy){$(placement?(demoSelected?`demo-square-${d.square}`:`demo-hand-0-${d.hand}`):d.step===(d.total||9)?'demo-play':'demo-next').focus({preventScroll:true});}
}
function demoSend(action){if(demoBusy||!demoFrame)return;demoBusy=true;demoControls();send('lesson',action);}
function renderLesson(){
 for(const id of ['challenge','album','game','music','settings-open'])$(id).hidden=true;
 $('guided-lesson').hidden=false;
 renderRuleChips($('demo-rules'),state.lesson.rules);
 $('rules-label').textContent='';
 $('demo-rival-name').textContent='Opponent';
 const d=state.lesson,key=`${state.session}:${d.step}`;
 if(key===demoKey)return;
 const previous=demoFrame,animate=previous&&d.step===previous.step+1;
 const source=animate?$(`demo-hand-${1-(d.step%2)}-${d.lastHand}`)?.getBoundingClientRect():null;
 demoKey=key;demoFrame=d;demoSelected=false;demoBusy=Boolean(animate);
 for(let p=0;p<2;p++){
   const hand=$(p?'demo-rival-hand':'demo-player-hand');hand.replaceChildren();
   d.hands[p].forEach((id,h)=>{const el=document.createElement('button');el.id=`demo-hand-${p}-${h}`;el.className='demo-card';el.disabled=true;
     if(id!==null){el.append(cardElement(id,p,d.handModifiers?.[p]?.[h]||0));el.setAttribute('aria-label',description(id));}else{el.classList.add('spent');el.setAttribute('aria-label','Played');}
     el.onclick=()=>{if(demoBusy||h!==demoFrame.hand||p!==0)return;demoSelected=true;demoControls(true);};hand.append(el);
   });
 }
 const board=d.board.map((cell,i)=>cell?{...cell,owner:animate&&cell.flip?previous.board[i].owner:cell.owner}:null);
 $('demo-board').replaceChildren();
 board.forEach((cell,i)=>{const el=document.createElement('button');el.id=`demo-square-${i}`;el.className='demo-square';el.disabled=true;
   el.setAttribute('aria-label',`Row ${Math.floor(i/3)+1}, column ${i%3+1}${cell?': '+description(cell.card):''}`);
   if(cell){const card=cardElement(cell.card,cell.owner,d.modifiers?.[i]||0);card.append(ownershipCorners());el.append(card);}
   else if(d.tiles?.[i]){const marker=document.createElement('span');marker.className='tile-affinity';marker.append(affinityIcon(affinityNames[Math.log2(d.tiles[i])]));el.append(marker);}
   el.onclick=()=>{if(demoSelected&&i===demoFrame.square)demoSend(`place ${demoFrame.hand} ${i}`);};$('demo-board').append(el);
 });
 demoScore(board);demoControls(!animate);
 if(animate)void animateLesson(d,board,source,demoGeneration);
 else markDemoRanks(d);
}
function markDemoRanks(d){
 for(const [square,side] of d.comparisons)$(`demo-square-${square}`).querySelector(`.rank.${['top','right','bottom','left'][side]}`)?.classList.add('demo-rank');
}
async function animateLesson(d,board,source,token){
 const reduced=matchMedia('(prefers-reduced-motion:reduce)').matches;
 if(!reduced&&source){
   const el=$(`demo-square-${d.lastSquare}`).firstElementChild,target=el.getBoundingClientRect(),scale=target.width/el.offsetWidth;
   demoAnimate(el,[{transformOrigin:'top left',transform:`translate(${(source.x-target.x)/scale}px,${(source.y-target.y)/scale}px) scale(${source.width/target.width},${source.height/target.height})`,opacity:.7},{transformOrigin:'top left',transform:'none',opacity:1}],{duration:360,easing:'ease-out'});
   await demoPause(370);if(token!==demoGeneration)return;
 }
 markDemoRanks(d);
 if(d.comparisons.length){await demoPause(reduced?0:650);if(token!==demoGeneration)return;}
 const stages=d.stages?.length?d.stages:[{rule:0,highlights:0,captures:d.board.reduce((mask,c,i)=>mask|(c?.flip?1<<i:0),0)}];
 let comboShown=false;
 for(const stage of stages){
   const captures=Array.from({length:9},(_,i)=>i).filter(i=>stage.captures&(1<<i));
   if(stage.rule){
     const label=ruleStageLabel(stage.rule);
     for(let i=0;i<9;i++)if(stage.highlights&(1<<i))$(`demo-square-${i}`).classList.add('demo-rule-glow');
     if(label!=='Combo'||!comboShown){
       const overlay=$('demo-callout');const text=document.createElement('span');text.className='announcement-text';text.textContent=label;overlay.replaceChildren(text);overlay.dataset.tone=label.toLowerCase();overlay.hidden=false;overlay.classList.remove('running');void overlay.offsetWidth;overlay.classList.add('running');
       send('rule',label.toLowerCase());
       await demoPause(reduced?150:850);if(token!==demoGeneration)return;overlay.hidden=true;
     }
     if(label==='Combo')comboShown=true;
   }
   if(!reduced)for(const i of captures)demoAnimate($(`demo-square-${i}`).firstElementChild,[{transform:'scaleX(1)'},{transform:'scaleX(.03)'},{transform:'scaleX(1)'}],{duration:520,easing:'ease-in-out'});
   if(captures.length){await demoPause(reduced?0:260);if(token!==demoGeneration)return;}
   for(const i of captures){board[i]={...d.board[i]};const el=$(`demo-square-${i}`).firstElementChild;el.classList.toggle('owner-0',board[i].owner===0);el.classList.toggle('owner-1',board[i].owner===1);}
   demoScore(board);
   if(captures.length){await demoPause(reduced?0:260);if(token!==demoGeneration)return;}
   document.querySelectorAll('.demo-rule-glow').forEach(el=>el.classList.remove('demo-rule-glow'));
 }
 for(const a of demoAnimations)a.cancel();demoAnimations.clear();demoBusy=false;demoControls(true);
}
$('demo-next').onclick=()=>demoSend('next');
$('demo-play').onclick=()=>demoSend('play');
$('demo-replay').onclick=()=>demoSend('replay');

function lessonPrompt(d,selected=false){
 if(selected)return 'Place it in the highlighted square.';
 if(d.caption)return d.caption;
 if(d.workshopRule)return workshopPrompt(d,selected);
 if(d.plusLesson)return comboLessonPrompt(d,true);
 if(d.sameLesson)return comboLessonPrompt(d,false);
 return basicLessonPrompt(d);
}
function workshopPrompt(d,selected=demoSelected){
 if(d.workshopRule===4){if(selected)return 'Place it in the highlighted square.';return ['Place Barded Guar. Under Reverse, lower touching numbers win.','Watch the opponent play against the Guar.','The lower touching rank captures the Guar. Equal ranks still do not capture.'][d.step];}

 if(d.riftTour){if(selected)return 'Place it in the highlighted square.';
  return ["Three opposing cards are shown after the deal. Two stay hidden. Choose the highlighted card.","Watch the opponent’s placement. A hidden card is revealed when played.","Your opponent also sees only three of your starting cards. Choose the highlighted card.","Keep track of which cards have been played.","We’ll set up two touching pairs for Plus. Choose the highlighted card.","Watch the opponent complete the setup.","Your opponent has not seen this card. Place Mistveil Enchanter in the corner.","The matching sums trigger Plus. The captured cards can then take their neighbours for Combo.","One square remains. Choose your last card.","Under Direct trade, you keep the cards you control—even in a draw. This lesson uses borrowed cards."][d.step];
 }

 if(d.workshopRule===16){if(d.step===0)return 'Open reveals both hands. Choose Piercing Twilight.';return selected?'Place it in the highlighted square.':basicLessonPrompt(d);}
 if(d.workshopRule===2147483648){if(d.step===0)return "The opposing hand stays hidden until its cards are played. Choose Piercing Twilight.";return selected?"Place it in the highlighted square.":basicLessonPrompt(d);}
 if(d.affinityTour){if(selected)return "Place it in the highlighted square.";return ["Place Barded Guar on the Endurance square.","Its matching affinity gives every side +1. Watch the next placement.","The Guar defended with its bonus. Place the neutral card on the Agility square.","Neutral cards have no matching affinity: every side loses 1. Watch the opponent’s Guar.","Their Guar also loses 1 on Agility. Place your next card on the marked square.","A matching affinity gives +1. Blank squares leave every card unchanged."][d.step];}
 if(d.workshopRule===64){if(d.step===0)return 'Three opposing cards are visible. The other two stay hidden until played. Choose Piercing Twilight.';return selected?'Place it in the highlighted square.':basicLessonPrompt(d);}
 if(selected)return 'Place it in the highlighted square.';
 if(d.step===0)return d.workshopRule===32?'Place Barded Guar on the Endurance square.':'Place Barded Guar to bring Beast to three cards.';
 if(d.step===1)return d.workshopRule===32?'A matching affinity gives +1 to every side. Watch the next placement.':d.workshopRule===128?'Beast reached three. All Beast cards gain +1, on both sides. Watch the next placement.':'Beast reached three. All Beast cards lose 1, on both sides. Watch the next placement.';
 return d.workshopRule===256?'The Scamp’s 2 beats the Guar’s reduced 1. Beast keeps −1 for the rest of this match.':d.workshopRule===128?'The Guar’s raised 3 defends against 2. Beast keeps +1 for the rest of this match.':'The Guar’s matching affinity raises 2 to 3, defending against the Scamp’s 2. A mismatch gives −1; blank squares change nothing.';
}
