'use strict';
let journalHold=null,journalPlayer='';
const playerKey=p=>String(p.base??`${p.holdID||1}:${p.name}`);
const gamesPlayed=p=>p.friendlyOnly?0:(p.wins||0)+(p.losses||0)+(p.draws||0);
function setJournalHold(id){journalHold=Number(id);journalPlayer='';$('journal-search').value='';renderOpponents();$('journal-chapter-'+journalHold)?.focus({preventScroll:true});}
function renderOpponents(){
 const c=state.collection,regions=c.regions||[],select=$('opponent-hold');
 if(journalHold===null)journalHold=regions.some(r=>r.id==c.holdID)?Number(c.holdID):regions[0]?.id??0;
 if(journalHold&&!regions.some(r=>r.id===journalHold))journalHold=0;
 const options=[{id:0,name:'All holds'},...regions];
 const regionKey=JSON.stringify(options.map(r=>[r.id,r.name]));
 if(select.dataset.regions!==regionKey){select.replaceChildren(...options.map(r=>new Option(r.name,String(r.id))));select.dataset.regions=regionKey;}
 select.value=String(journalHold);select._refresh();
 renderChapterNavigation($('journal-chapters'),options,journalHold,setJournalHold,'journal-chapter-');
 const region=regions.find(r=>r.id===journalHold),hold=journalHold;
 $('journal-title').textContent=region?.name||'Known players';
 $('hold-rules').textContent=region&&!region.travellers?`${rulesText(region.rules)||'No special rules · Hidden hands'} · ${tradeRuleText(region.trade)}`:'';
 $('progress-step').textContent=region&&!region.travellers?`${region.reputationTitle||'Unknown'} · Wins: ${region.wins||0}`:'';$('progress-step').hidden=!region||Boolean(region.travellers);

 const query=$('journal-search').value.trim().toLowerCase(),filter=$('journal-filter').value;
 const players=(c.roster||[]).filter(p=>(!hold||(p.holdID||1)===hold)&&(!query||[p.name,p.location,p.skill].join(' ').toLowerCase().includes(query))&&(filter==='all'||filter==='played'&&gamesPlayed(p)>0||filter==='unplayed'&&!gamesPlayed(p)));
 players.sort((a,b)=>a.name.localeCompare(b.name)||playerKey(a).localeCompare(playerKey(b)));
 if(!players.some(p=>playerKey(p)===journalPlayer))journalPlayer=players.length?playerKey(players[0]):'';
 const list=$('opponents-list'),listScroll=list.scrollTop;list.replaceChildren();
 for(const p of players){
  const row=document.createElement('button');row.className='opponent-row';row.dataset.player=playerKey(p);row.setAttribute('aria-pressed',String(journalPlayer===playerKey(p)));
  const identity=document.createElement('span'),name=document.createElement('span'),place=document.createElement('span');
  name.className='opponent-name';name.textContent=p.name;place.className='opponent-place';place.textContent=hold?p.location:[p.location,p.hold].filter((v,i,a)=>v&&a.indexOf(v)===i).join(' · ');identity.append(name,place);
  const record=document.createElement('span');record.className='opponent-record';record.textContent=p.friendlyOnly?'':p.unlocked===false?'Unavailable':gamesPlayed(p)?`${p.wins||0} W · ${p.losses||0} L · ${p.draws||0} D`:'Not played';
  const skill=document.createElement('span');skill.className='opponent-skill';skill.textContent=p.skill;record.prepend(skill);
  row.append(identity,record);if(!p.friendlyOnly&&p.wins)row.classList.add('defeated');
  row.onclick=()=>{journalPlayer=playerKey(p);for(const button of list.querySelectorAll('button'))button.setAttribute('aria-pressed',String(button===row));renderJournalPlayer(p);};list.append(row);
 }
 if(!players.length){const empty=document.createElement('p');empty.className='journal-empty';empty.textContent=query||filter!=='all'?'No matching players':'No known players';list.append(empty);}
 list.scrollTop=listScroll;$('journal-count').textContent=`${players.length} ${players.length===1?'player':'players'}`;
 renderJournalPlayer(players.find(p=>playerKey(p)===journalPlayer));
}
function renderJournalPlayer(p){
 const panel=$('journal-player');panel.replaceChildren();panel.hidden=!p;if(!p)return;
 const name=document.createElement('h3');name.textContent=p.name;panel.append(name);
 const detail=(label,value)=>{const row=document.createElement('div');row.className='journal-detail';const term=document.createElement('span');term.textContent=label;const text=document.createElement('span');text.textContent=value;row.append(term,text);panel.append(row);};
 detail('Location',p.location);if(!p.traveller)detail('Hold',p.hold||'Whiterun');detail('Skill',p.skill);
 if(p.unlocked===false)detail('Challenge','Unavailable');
 const record=(w,l,d)=>`${w||0} W · ${l||0} L · ${d||0} D`;
 if(!p.friendlyOnly)detail('Record',record(p.wins,p.losses,p.draws));
}
$('opponent-hold').addEventListener('change',()=>setJournalHold($('opponent-hold').value));
$('journal-search').addEventListener('input',renderOpponents);$('journal-filter').addEventListener('change',renderOpponents);
