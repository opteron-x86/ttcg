'use strict';
const AUDIT = JSON.parse(document.getElementById('audit-data').textContent);
const cards = AUDIT.cards, byID = new Map(cards.map(c => [c.id, c]));
const roman = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X'];
const routeNames = {starter:'Starter grant', opponent:'NPC collection', merchant:'Card merchant', standard:'Standard pack', tournament:'Tournament pack', trophy:'Tournament trophy'};
const signals = {'no-route':'No acquisition route','small-group':'Group with ≤5 identities','shared-layout':'Shared side layout','stronger-layout':'Stronger peer layout','rarity-overlap':'Same layout, other rarity','pack-shortfall':'Sparse pack bucket','rank-policy':'Outside rank policy'};
const decisions = {review:'Needs review', keep:'Keep', adjust:'Adjust', retire:'Consider retiring'};
const grouping = AUDIT.grouping, grouped = new Map(grouping.rows.map(r => [r.id, r]));
const groupActions = {retain:'Group retained',regroup:'Group change',rework:'Tier / rarity review',review:'Boundary review',retire:'Retirement candidate'};
const storageKey = 'tessera-catalog-review-v1-' + AUDIT.fingerprint;
const $ = id => document.getElementById(id);
const state = {view:'overview', scope:'all', group:'', tier:'', rarity:'', signal:'', q:'', sort:'tier', page:0,
 proposedGroup:'',proposalAction:'changes',proposalTier:'',proposalSearch:'',proposalPage:0};
let reviews = new Map(), pinned = [], selected = null, filtered = [], storageWarning = '';
const pageSize = 30;

function el(tag, cls, text) {
 const n = document.createElement(tag);
 if (cls) n.className = cls;
 if (text !== undefined) n.textContent = text;
 return n;
}
function button(text, action, cls) {
 const b = el('button', cls, text); b.type = 'button'; b.addEventListener('click', action); return b;
}
function option(value, text) {const o = el('option', '', text); o.value = value; return o;}
function notify(text) {$('status').textContent = text; $('status').hidden = !text;}
function scopeCards() {
 return cards.filter(c => state.scope === 'all' || (state.scope === 'released' ? c.available :
     state.scope === 'pending' ? c.pending : state.scope === 'none' ? !c.routes.length&&!c.pending : c.routes.includes(state.scope)));
}
function saveLocation() {
 const params = new URLSearchParams();
 for (const key of ['view','scope','group','tier','rarity','signal','q','sort','proposedGroup','proposalAction','proposalTier','proposalSearch']) {
  if (state[key] || key === 'proposalAction') params.set(key, state[key]);
 }
 try {history.replaceState(null, '', '#' + params.toString());} catch (_) { /* File viewers may restrict history. */ }
}
function showView(view, scroll = false) {
 state.view = view;
 for (const n of document.querySelectorAll('.view')) n.hidden = n.id !== view;
 for (const b of document.querySelectorAll('[data-view]')) b.setAttribute('aria-pressed', String(b.dataset.view === view));
 $('scope').disabled = ['notebook','method','grouping'].includes(view);
 document.querySelector('.scope-label').hidden = view === 'grouping';
 if (view === 'overview') renderOverview();
 if (view === 'explorer') renderExplorer();
 if (view === 'notebook') renderNotebook();
 if (view === 'grouping') renderGrouping();
 saveLocation();
 if (scroll) document.querySelector('.view-bar').scrollIntoView({block:'start'});
}
function drill(filters = {}) {
 Object.assign(state, {group:'',tier:'',rarity:'',signal:'',q:'',sort:'tier',page:0}, filters);
 syncFilters(); showView('explorer', true);
}
function syncFilters() {
 for (const [id, key] of [['scope','scope'],['group','group'],['tier','tier'],['rarity','rarity'],['signal','signal'],['search','q'],['sort','sort']]) $(id).value = state[key];
}
function renderStats() {
 $('snapshot-id').textContent = AUDIT.fingerprint.slice(0,8);
 const entries = [[AUDIT.summary.cards, 'card identities'],[AUDIT.summary.groups, 'creature groups'],[AUDIT.summary.trophies, 'trophy candidates'],[AUDIT.summary.pending, 'unfinished superior cards']];
 $('stats').replaceChildren(...entries.map(([value, label]) => {
  const n = el('div','stat'); n.append(el('strong','',value),el('span','',label)); return n;
 }));
}
function renderOverview() {
 const scoped = scopeCards();
 $('scope-count').textContent = scoped.length + ' of ' + cards.length + ' cards in this scope';
 const findingData = [
  ['no-route', 'Acquisition gaps', 'No implemented route', scoped.filter(c => !c.routes.length&&!c.pending).length],
  ['small-group', 'Small creature groups', 'Cards in groups with ≤5 identities', scoped.filter(c => c.flags.includes('small-group')).length],
  ['rarity-overlap', 'Rarity overlaps', 'Same tier and layout, different rarity', scoped.filter(c => c.flags.includes('rarity-overlap')).length],
  ['pack-shortfall', 'Sparse pack buckets', 'Cards in pools that can run short', scoped.filter(c => c.flags.includes('pack-shortfall')).length],
 ];
 $('findings').replaceChildren(...findingData.map(([flag, title, note, count]) => {
  const n = button('', () => drill({signal:flag}), 'finding');
  n.append(el('strong', flag === 'no-route' && count ? 'warning':'',count),el('span','',title),el('small','',note)); return n;
 }));
 renderGroupMatrix(scoped); renderRarityMatrix(scoped); renderPacks(); renderCosts(scoped);
}
function heatButton(count, max, label, action) {
 const b = button(count || '—', action);
 b.dataset.empty = String(!count); b.setAttribute('aria-label', label + ': ' + count + ' cards');
 b.title = label + ': ' + count + ' cards';
 b.style.setProperty('--heat', count ? String(.08 + .40 * Math.sqrt(count / Math.max(1,max))) : '0');
 return b;
}
function tableWithHead(labels) {
 const table = el('table','heatmap'), head = el('thead'), row = el('tr'), body = el('tbody');
 for (const text of labels) {const th = el('th','',text); th.scope = 'col'; row.append(th);}
 head.append(row); table.append(head,body); return [table,body];
}
function renderGroupMatrix(scoped) {
 let groups = [...AUDIT.groups];
 if (cards.some(c => !c.groups.length)) groups.push('Ungrouped');
 const counts = new Map(groups.map(g => [g, Array.from({length:10},(_,i) => scoped.filter(c => c.tier === i+1 && (g === 'Ungrouped' ? !c.groups.length : c.groups.includes(g))).length)]));
 const total = g => counts.get(g).reduce((a,b)=>a+b,0);
 const order = $('group-sort').value;
 groups.sort((a,b) => (order === 'name' ? 0 : (order === 'small' ? total(a)-total(b) : total(b)-total(a))) || a.localeCompare(b));
 const [table,body] = tableWithHead(['Group',...roman.slice(1),'Total']);
 const max = Math.max(1,...Array.from(counts.values()).flat());
 for (const g of groups) {
  const row = el('tr'), th = el('th'); th.scope = 'row';
  const group = g === 'Ungrouped' ? '__none' : g;
  th.append(button(g,()=>drill({group}),'row-button')); row.append(th);
  counts.get(g).forEach((n,i) => {const td = el('td'); td.append(heatButton(n,max,g+' · Tier '+roman[i+1],()=>drill({group,tier:String(i+1)}))); row.append(td);});
  row.append(el('td','row-total',total(g))); body.append(row);
 }
 $('group-matrix').replaceChildren(table);
}
function renderRarityMatrix(scoped) {
 const [table,body] = tableWithHead(['Tier','C','R','E','L','Total']);
 for (let i=1;i<=4;i++) {table.tHead.rows[0].cells[i].title = AUDIT.rarities[i-1]; table.tHead.rows[0].cells[i].className = AUDIT.rarities[i-1];}
 const max = Math.max(1,...Array.from({length:10},(_,i)=>AUDIT.rarities.map(r=>scoped.filter(c=>c.tier===i+1&&c.rarity===r).length)).flat());
 for (let t=1;t<=10;t++) {
  const row = el('tr'), th = el('th','',roman[t]); th.scope = 'row'; row.append(th);
  for (const rarity of AUDIT.rarities) {
   const n = scoped.filter(c=>c.tier===t&&c.rarity===rarity).length, td = el('td');
   const b = heatButton(n,max,'Tier '+roman[t]+' · '+rarity,()=>drill({tier:String(t),rarity})); b.classList.add(rarity); td.append(b); row.append(td);
  }
  row.append(el('td','row-total',scoped.filter(c=>c.tier===t).length)); body.append(row);
 }
 $('rarity-matrix').replaceChildren(table);
}
function packTable(rows, kind) {
 const table = el('table','pack-table'), head = el('thead'), tr = el('tr'), body = el('tbody');
 for (const text of ['Pool','Identities','Possible draws','Avg. draws / pack']) {const th=el('th','',text);th.scope='col';tr.append(th);}
 head.append(tr); table.append(head,body);
 for (const row of rows) {
  const r = el('tr',row.risk?'risk':'');
  r.append(el('td','',roman[row.tier]+' · '+row.rarity));
  const link = el('td'); link.append(button(String(row.count),()=>{state.scope=kind;drill({tier:String(row.tier),rarity:row.rarity});}));
  r.append(link,el('td','',row.maxDraws),el('td','',Number(row.expectedDraws.toFixed(4)))); body.append(r);
 }
 const wrap=el('div','table-scroll');wrap.append(table);return wrap;
}
function renderPacks() {
 const kind = $('pack-kind').value, rows = AUDIT.buckets[kind], risks = rows.filter(r=>r.risk), host = $('pack-report');
 host.replaceChildren(el('p','pack-note',risks.length ?
  risks.length+' pools have fewer identities than a pack can ask for. Duplicate exclusion can leave a later draw with no candidate.' :
  'Every eligible pool has enough identities for its maximum possible draws under the current generator.'));
 if (risks.length) host.append(packTable(risks,kind));
 const full = el('details','pack-full'); full.append(el('summary','', 'Inspect all '+rows.length+' eligible pools'),packTable(rows,kind)); host.append(full);
 host.append(el('p','footnote','Complete pack pools, independent of the scope above. These are coverage checks and nominal roll expectations; they are not measured drop rates.'));
}
function renderCosts(scoped) {
 const host=$('cost-chart'), legend=el('div','cost-legend');legend.append(el('span','','━ Observed range'),el('span','','▱ Allowed budget'));host.replaceChildren(legend);
 const max=Math.max(...AUDIT.policy.tiers.map(t=>t.weighted_max),...cards.map(c=>c.cost));
 for (const p of AUDIT.policy.tiers) {
  const values=scoped.filter(c=>c.tier===p.tier).map(c=>c.cost), row=el('div','cost-row'),track=el('div','cost-track'),budget=el('span','cost-budget');
  budget.style.left=p.weighted_min/max*100+'%'; budget.style.width=(p.weighted_max-p.weighted_min)/max*100+'%'; track.append(budget);
  if(values.length) {const range=el('span','cost-range'),lo=Math.min(...values),hi=Math.max(...values);range.style.left=lo/max*100+'%';range.style.width=Math.max(.7,(hi-lo)/max*100)+'%';track.append(range);}
  row.append(el('span','',roman[p.tier]),track,el('span','',values.length?Math.min(...values)+'–'+Math.max(...values):'No cards'));
  row.title='Tier '+roman[p.tier]+': allowed '+p.weighted_min+'–'+p.weighted_max+' weighted points'; host.append(row);
 }
}
function portrait(c, large=false, draft=null) {
 const n=el('div','portrait');n.dataset.rarity=draft?.rarity||c.rarity;
 const img=el('img');img.src=large?c.art:c.thumbnail;img.alt=c.name;img.loading=large?'eager':'lazy';img.decoding='async';img.style.objectPosition=c.focus;
 n.append(img,el('span','tier-badge '+(draft?.rarity||c.rarity),roman[draft?.tier||c.tier]));
 (draft?.sides||c.sides).forEach((rank,i)=>n.append(el('span','rank '+['top','right','bottom','left'][i],rank===10?'A':rank)));
 return n;
}
function groupDestination(r) {return r.group || (r.action === 'retire' ? 'Proposed retirement' : 'Unassigned');}
function groupingFilters() {
 for(const [id,key] of [['grouping-group','proposedGroup'],['grouping-action','proposalAction'],['grouping-tier','proposalTier'],['grouping-search','proposalSearch']]) $(id).value=state[key];
}
function groupingDrill(group='',tier='') {
 Object.assign(state,{proposedGroup:group,proposalTier:tier,proposalAction:'',proposalSearch:'',proposalPage:0});
 groupingFilters();renderGrouping();$('grouping-filters').scrollIntoView({block:'start'});
}
function groupingSelection() {
 const q=state.proposalSearch.trim().toLocaleLowerCase();
 return cards.filter(c=>{
  const r=grouped.get(c.id);
  return (!state.proposedGroup||(state.proposedGroup==='__none'?!r.group:r.group===state.proposedGroup))&&
   (!state.proposalAction||(state.proposalAction==='changes'?r.action!=='retain':r.action===state.proposalAction))&&
   (!state.proposalTier||c.tier===Number(state.proposalTier))&&
   (!q||[c.name,...c.groups,...c.subtypes,groupDestination(r),r.note,...r.requirements].some(s=>s.toLocaleLowerCase().includes(q)));
 });
}
function renderCurve(id,rows) {
 const [table,body]=tableWithHead(['Tier','Common','Rare','Epic','Legendary','Total']);
 for(const data of rows){
  const row=el('tr'),th=el('th','',roman[data.tier]);th.scope='row';row.append(th);
  for(const rarity of AUDIT.rarities)row.append(el('td',rarity,String(data[rarity]||'—')));
  row.append(el('td','row-total',AUDIT.rarities.reduce((sum,r)=>sum+data[r],0)));body.append(row);
 }
 $(id).replaceChildren(table);
}
function renderGrouping() {
 const unassigned=grouping.rows.filter(r=>!r.group&&r.action!=='retire').length;
 const retired=grouping.rows.filter(r=>r.action==='retire').length;
 $('grouping-evidence').textContent=(cards.length-unassigned-retired)+' proposed assignments · '+unassigned+' unassigned · '+retired+' retirement candidates · '+grouping.rows.filter(r=>r.requirements.length).length+' tier / rarity reviews';
 const [table,body]=tableWithHead(['Proposed group',...roman.slice(1),'Mapped','Target']);
 const groups=[...grouping.groups,{name:'Unassigned',target:null}];
 for(const g of groups){
  const key=g.name==='Unassigned'?'__none':g.name,members=cards.filter(c=>key==='__none'?!grouped.get(c.id).group:grouped.get(c.id).group===key);
  const row=el('tr'),th=el('th');th.scope='row';th.append(button(g.name,()=>groupingDrill(key),'row-button'));row.append(th);
  for(let tier=1;tier<=10;tier++){
   const n=members.filter(c=>c.tier===tier).length,td=el('td');
   td.append(heatButton(n,20,g.name+' proposed · current tier '+roman[tier],()=>groupingDrill(key,String(tier))));row.append(td);
  }
  row.append(el('td','row-total',members.length),el('td','',g.target?g.target.join('–'):g.name==='God'?'Open':'Review'));body.append(row);
 }
 $('grouping-matrix').replaceChildren(table);
 renderCurve('ordinary-curve',grouping.ordinaryCurve);renderCurve('dragon-curve',grouping.dragonCurve);
 const selected=groupingSelection(),pages=Math.max(1,Math.ceil(selected.length/pageSize));state.proposalPage=Math.min(state.proposalPage,pages-1);
 $('grouping-summary').textContent=selected.length+' cards · current art and ranks';$('grouping-empty').hidden=selected.length>0;
 $('grouping-grid').replaceChildren(...selected.slice(state.proposalPage*pageSize,(state.proposalPage+1)*pageSize).map(c=>{
  const r=grouped.get(c.id),entry=el('article','catalog-card'),open=button('',()=>openCard(c.id),'card-open');open.dataset.proposalId=c.id;
  open.setAttribute('aria-label','Review assignment for '+c.name);
  open.append(portrait(c),el('span','card-name',c.name),el('span','card-meta '+c.rarity,'Current: '+roman[c.tier]+' · '+c.rarity));
  entry.append(open,el('p','grouping-change',(c.groups.join(' / ')||'Ungrouped')+' → '+groupDestination(r)),el('span','badge',groupActions[r.action]));
  if(r.note)entry.append(el('p','grouping-note',r.note));
  for(const note of r.requirements)entry.append(el('p','grouping-note requirement',note));
  const pin=button('',()=>togglePin(c.id),'pin-button');pin.dataset.pin=c.id;entry.append(pin);return entry;
 }));
 $('grouping-page').textContent='Page '+(state.proposalPage+1)+' of '+pages;
 $('grouping-previous').disabled=state.proposalPage===0;$('grouping-next').disabled=state.proposalPage>=pages-1;
 updatePins();saveLocation();
}
function pickFiltered() {
 const q=state.q.trim().toLocaleLowerCase();
 const result=scopeCards().filter(c=>(!state.group||(state.group==='__none'?!c.groups.length:c.groups.includes(state.group)))&&
  (!state.tier||c.tier===Number(state.tier))&&(!state.rarity||c.rarity===state.rarity)&&(!state.signal||c.flags.includes(state.signal))&&
  (!q||[c.name,...c.groups,...c.subtypes,...c.affinities,...c.holders.map(p=>p.name)].some(x=>x.toLocaleLowerCase().includes(q))));
 result.sort((a,b)=>{
  const value=state.sort==='tier'?a.tier-b.tier:state.sort==='cost'?b.cost-a.cost:state.sort==='holders'?a.holders.length-b.holders.length:
   state.sort==='basic'?b.edges.basic-a.edges.basic:state.sort==='reverse'?b.edges.reverse-a.edges.reverse:0;
  return value||a.name.localeCompare(b.name);
 });return result;
}
function renderExplorer() {
 filtered=pickFiltered();const pages=Math.max(1,Math.ceil(filtered.length/pageSize));state.page=Math.min(state.page,pages-1);
 const description=[filtered.length+' cards',state.group==='__none'?'Ungrouped':state.group,state.tier?'Tier '+roman[Number(state.tier)]:'',state.rarity,state.signal?signals[state.signal]:''].filter(Boolean);
 $('explorer-summary').textContent=description.join(' · '); $('empty').hidden=filtered.length>0;
 $('catalog-grid').replaceChildren(...filtered.slice(state.page*pageSize,(state.page+1)*pageSize).map(c=>{
  const entry=el('article','catalog-card'),open=button('',()=>openCard(c.id),'card-open');open.dataset.cardId=c.id;open.setAttribute('aria-label','Inspect '+c.name);
  const meta=el('div','card-meta');meta.append(el('span',c.rarity,c.rarity+' · '+roman[c.tier]),el('span','muted',c.cost+' points'));
  open.append(portrait(c),el('span','card-name',c.name),meta);entry.append(open,el('p','card-groups',c.groups.join(' · ')||'Ungrouped'));
  const actions=el('div','card-actions'),pin=button('',()=>togglePin(c.id),'pin-button');pin.dataset.pin=c.id;
  const note=el('span','card-signal');if(c.flags.length)note.append(el('i','signal-dot'),document.createTextNode(c.flags.length+' review signals'));
  note.title=c.flags.map(f=>signals[f]).join('\n');actions.append(pin,note);entry.append(actions);return entry;
 }));
 $('page-count').textContent='Page '+(state.page+1)+' of '+pages; $('previous-page').disabled=state.page===0;$('next-page').disabled=state.page>=pages-1;
 updatePins();saveLocation();
}
function togglePin(id) {
 if(pinned.includes(id))pinned=pinned.filter(x=>x!==id);
 else {if(pinned.length===4){notify('Compare up to four cards at once. Unpin a card to add another.');return;}pinned.push(id);}
 updatePins();
}
function updatePins() {
 for(const b of document.querySelectorAll('[data-pin]')){const yes=pinned.includes(b.dataset.pin);b.textContent=yes?'✓ Pinned':'Compare';b.setAttribute('aria-pressed',String(yes));b.setAttribute('aria-label',(yes?'Unpin ':'Pin ')+byID.get(b.dataset.pin).name);}
 $('compare-tray').hidden=!pinned.length;$('pinned-names').textContent=pinned.map(id=>byID.get(id).name).join(' · ');$('open-compare').disabled=pinned.length<2;
}
function facts(c, compact=false) {
 const dl=el('dl','detail-dl'), p=AUDIT.policy.tiers[c.tier-1];
 const pairs=[['Tier / rarity',roman[c.tier]+' · '+c.rarity+(c.unique?' · Unique':'')],
  ['Printed sides',c.sides.map(n=>n===10?'A':n).join(' / ')+' · top / right / bottom / left'],
  ['Weighted cost',c.cost+' · tier budget '+p.weighted_min+'–'+p.weighted_max],
  ['Creature groups',c.groups.join(' · ')||'None'],['Subtypes',c.subtypes.join(' · ')||'None'],
  ['Affinities',c.affinities.join(' · ')||'Neutral'],['Profile role',c.role],
  ['Basic edge wins',c.edges.basic===null?'No peers':c.edges.basic.toFixed(1)+'%'],
  ['Reverse edge wins',c.edges.reverse===null?'No peers':c.edges.reverse.toFixed(1)+'%'],
  ['Equal edges',c.edges.equal===null?'No peers':c.edges.equal.toFixed(1)+'%'],
  ['Acquisition',c.routes.map(r=>routeNames[r]).join(' · ')||'No implemented route']];
 if(!compact)pairs.push(['Rank policy',c.valid?'Passes existing validator':'Outside current policy'],['Released flag',c.available?'On':'Off · tournament-reserved']);
 for(const [name,value] of pairs)dl.append(el('dt','',name),el('dd','',value));
 return dl;
}
function openCard(id) {
 const c=byID.get(id);if(!c)return;selected=id;
 $('detail-title').textContent=c.name;$('detail-art').replaceChildren(portrait(c,true));$('full-art').href=c.art;
 const host=$('detail-facts');host.replaceChildren(facts(c));
 const g=grouped.get(c.id),proposal=el('div','grouping-detail');
 proposal.append(el('h3','','18-group proposal'),el('p','grouping-change',(c.groups.join(' / ')||'Ungrouped')+' → '+groupDestination(g)),el('span','badge',groupActions[g.action]));
 if(g.note)proposal.append(el('p','detail-note',g.note));
 for(const requirement of g.requirements)proposal.append(el('p','detail-note requirement',requirement));
 host.append(proposal);
 const pin=button('',()=>togglePin(c.id),'pin-button');pin.dataset.pin=c.id;host.append(pin);
 if(c.flags.length){const tags=el('p');for(const f of c.flags)tags.append(el('span','badge',signals[f]));host.append(tags);}
 host.append(el('p','detail-note','Edge comparisons use '+c.edges.comparisons+' opposing-side checks against other cards in this tier. They are not match win rates.'));
 if(c.acquisitionNote)host.append(el('p','detail-note','Catalog acquisition note: '+c.acquisitionNote+'. This text alone does not create an acquisition route.'));
 if(c.holders.length){const owners=el('details','holders'),list=el('ul');owners.append(el('summary','',c.holders.length+' authored NPC holders'));for(const p of c.holders)list.append(el('li','',p.name+' · '+p.hold+' · '+p.skill));owners.append(list);host.append(owners);}
 $('related').replaceChildren();
 for(const [title,ids] of [['Same tier and printed layout',c.matches],['Same-tier peers with stronger printed sides',c.stronger]]){
  if(!ids.length)continue;const box=el('details','related-section'),list=el('div','related-list');box.append(el('summary','',title+' ('+ids.length+')'));
  for(const peer of ids)list.append(button(byID.get(peer).name,()=>{openCard(peer);$('detail-dialog').scrollTop=0;}));box.append(list);$('related').append(box);
 }
 const r=reviews.get(id),after=r?.after||before(c);
 $('proposal-decision').value=r?.decision||'review';$('proposal-tier').value=String(after.tier);$('proposal-rarity').value=after.rarity;$('proposal-groups').value=after.groups.join(', ');
 for(let i=0;i<4;i++)$('side-'+i).value=String(after.sides[i]);
 $('proposal-note').value=r?.note||'';$('remove-proposal').disabled=!r;$('proposal-feedback').textContent='';
 draftCost();updatePins();if(!$('detail-dialog').open)$('detail-dialog').showModal();
}
function before(c) {return {tier:c.tier,rarity:c.rarity,groups:[...c.groups],sides:[...c.sides]};}
function draftCost() {
 const sides=[0,1,2,3].map(i=>Number($('side-'+i).value)),tier=Number($('proposal-tier').value),p=AUDIT.policy.tiers[tier-1];
 if(!p||sides.some(n=>!Number.isInteger(n)||n<1||n>10)){$('draft-cost').textContent='Sides must be between 1 and 10 (A).';return;}
 const cost=sides.reduce((n,s)=>n+AUDIT.policy.rank_costs[s],0);
 $('draft-cost').textContent=cost+' weighted points · budget '+p.weighted_min+'–'+p.weighted_max+(cost<p.weighted_min||cost>p.weighted_max?' · Outside budget':' · Within budget');
}
function reviewDocument() {
 return {kind:'tessera-catalog-review',schema:1,fingerprint:AUDIT.fingerprint,
  entries:[...reviews.values()].sort((a,b)=>a.name.localeCompare(b.name))};
}
function validateReview(doc) {
 if(!doc||doc.kind!=='tessera-catalog-review'||doc.schema!==1||doc.fingerprint!==AUDIT.fingerprint)throw Error('This review belongs to a different catalog snapshot or format.');
 if(!Array.isArray(doc.entries)||doc.entries.length>cards.length)throw Error('Invalid review entries.');
 const seen=new Set(), entries=[];
 for(const r of doc.entries){
  const c=r&&byID.get(r.id),a=r?.after;
  if(!c||seen.has(r.id)||r.name!==c.name||!Object.hasOwn(decisions,r.decision)||typeof r.note!=='string'||r.note.length>6000)throw Error('Invalid card review.');
  if(!a||!Number.isInteger(a.tier)||a.tier<1||a.tier>10||!AUDIT.rarities.includes(a.rarity)||!Array.isArray(a.sides)||a.sides.length!==4||a.sides.some(n=>!Number.isInteger(n)||n<1||n>10))throw Error('Invalid tier, rarity or printed sides for '+c.name+'.');
  if(!Array.isArray(a.groups)||a.groups.length>12||a.groups.some(g=>typeof g!=='string'||!g.trim()||g.length>80)||new Set(a.groups.map(g=>g.trim())).size!==a.groups.length)throw Error('Invalid creature groups for '+c.name+'.');
  const original=before(c);
  if(!r.before||['tier','rarity','groups','sides'].some(k=>JSON.stringify(r.before[k])!==JSON.stringify(original[k])))throw Error('Original values no longer match '+c.name+'.');
  seen.add(r.id);entries.push({id:c.id,name:c.name,before:original,after:{tier:a.tier,rarity:a.rarity,groups:a.groups.map(g=>g.trim()),sides:[...a.sides]},decision:r.decision,note:r.note});
 }return entries;
}
function persistReview() {
 try{localStorage.setItem(storageKey,JSON.stringify(reviewDocument()));}
 catch(_){notify('Browser storage is unavailable. Export your review before closing this page.');}
 $('review-count').textContent=String(reviews.size);renderNotebook();
}
function renderNotebook() {
 $('review-count').textContent=String(reviews.size);
 const entries=[...reviews.values()].sort((a,b)=>a.name.localeCompare(b.name));
 $('review-list').replaceChildren(...entries.map(r=>{
  const row=el('article','review-row'),img=el('img'),copy=el('div','review-text');img.src=byID.get(r.id).thumbnail;img.alt=r.name;img.loading='lazy';
  const changes=[];for(const key of ['tier','rarity','groups','sides'])if(JSON.stringify(r.before[key])!==JSON.stringify(r.after[key]))changes.push(key+': '+r.before[key]+' → '+r.after[key]);
  copy.append(el('span','decision',decisions[r.decision]),el('h3','',r.name),el('p','',changes.join(' · ')||'Current values retained'));
  if(r.note)copy.append(el('p','',r.note));
  row.append(img,copy,button('Review card',()=>openCard(r.id)));return row;
 }));
 if(!entries.length)$('review-list').append(el('p','empty','Open a card to record a decision or propose new values. Your saved reviews will appear here.'));
 $('briefs').replaceChildren(...AUDIT.briefs.map(b=>{
  const group=cards.filter(c=>c.groups.includes(b.group)),availableTiers=[...new Set(group.map(c=>c.tier))].sort((a,b)=>a-b);
  const missing=b.tiers.filter(t=>!availableTiers.includes(t));
  const n=el('article','brief');n.append(el('p','eyebrow',b.group),el('h3','',b.title),
   el('p','brief-evidence',group.length+' identities · current tiers '+availableTiers.map(t=>roman[t]).join(', ')+' · review '+b.tiers.map(t=>roman[t]).join(', ')),
   el('p','',b.role),el('p','',b.route),el('p','',missing.length===b.tiers.length?b.decision:'Coverage has changed since this brief was written; revisit its target tiers. '+b.decision),
   button('Inspect '+b.group,()=>{state.scope='all';drill({group:b.group});}));return n;
 }));
}
function download(name, text, type) {
 const url=URL.createObjectURL(new Blob([text],{type})),a=el('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1500);
}
function csvCell(v) {
 let text=String(v);if(/^[=+\-@\t\r]/.test(text))text="'"+text;return '"'+text.replaceAll('"','""')+'"';
}
function exportCSV() {
 const headings=['id','name','tier','rarity','top','right','bottom','left','weighted_cost','groups','affinities','unique','released','routes','npc_holders','basic_edge_pct','reverse_edge_pct','signals'];
 const rows=pickFiltered().map(c=>[c.id,c.name,c.tier,c.rarity,...c.sides,c.cost,c.groups.join('; '),c.affinities.join('; '),c.unique,c.available,c.routes.join('; '),c.holders.map(p=>p.name).join('; '),c.edges.basic,c.edges.reverse,c.flags.join('; ')]);
 download('tessera-catalog-selection.csv','\uFEFF'+[headings,...rows].map(row=>row.map(csvCell).join(',')).join('\r\n'),'text/csv;charset=utf-8');
}
function exportGroupingCSV() {
 const headings=['id','name','current_groups','proposed_group','review','current_tier','current_rarity','current_unique','current_sides','note','requirements','proposal_fingerprint'];
 const rows=groupingSelection().map(c=>{const r=grouped.get(c.id);return [c.id,c.name,c.groups.join('; '),r.group||'',groupActions[r.action],c.tier,c.rarity,c.unique,c.sides.join('; '),r.note,r.requirements.join(' '),grouping.fingerprint];});
 download('tessera-grouping-proposal.csv','\uFEFF'+[headings,...rows].map(row=>row.map(csvCell).join(',')).join('\r\n'),'text/csv;charset=utf-8');
}
function openComparison() {
 $('comparison').style.setProperty('--count',String(pinned.length));
 $('comparison').replaceChildren(...pinned.map(id=>{
  const c=byID.get(id),section=el('section');section.append(portrait(c,true),el('h3','',c.name),facts(c,true),button('Review card',()=>{$('compare-dialog').close();openCard(id);}));return section;
 }));$('compare-dialog').showModal();
}

for(const g of AUDIT.groups)$('group').append(option(g,g));
for(const g of grouping.groups)$('grouping-group').append(option(g.name,g.name));
for(let t=1;t<=10;t++)$('grouping-tier').append(option(String(t),roman[t]));
if(cards.some(c=>!c.groups.length))$('group').append(option('__none','Ungrouped'));
for(let t=1;t<=10;t++){ $('tier').append(option(String(t),roman[t]));$('proposal-tier').append(option(String(t),roman[t]));}
for(const r of AUDIT.rarities){$('rarity').append(option(r,r));$('proposal-rarity').append(option(r,r));}
for(const [path,hash] of Object.entries(AUDIT.sources)){
 const li=el('li'),a=el('a','',path);a.href='../'+path;li.append(a,document.createTextNode(' · '+hash.slice(0,12)));$('source-list').append(li);
}
const params=new URLSearchParams(location.hash.slice(1));
if(['overview','grouping','explorer','notebook','method'].includes(params.get('view')))state.view=params.get('view');
for(const key of ['scope','group','tier','rarity','signal','sort'])if([...$(key).options].some(o=>o.value===params.get(key)))state[key]=params.get(key);
state.q=(params.get('q')||'').slice(0,300);
for(const [id,key] of [['grouping-group','proposedGroup'],['grouping-action','proposalAction'],['grouping-tier','proposalTier']]){
 if([...$(id).options].some(o=>o.value===params.get(key)))state[key]=params.get(key);
 $(id).addEventListener('change',()=>{state[key]=$(id).value;state.proposalPage=0;renderGrouping();});
}
state.proposalSearch=(params.get('proposalSearch')||'').slice(0,300);
$('grouping-search').addEventListener('input',()=>{state.proposalSearch=$('grouping-search').value;state.proposalPage=0;renderGrouping();});
$('grouping-reset').addEventListener('click',()=>groupingDrill());
$('grouping-export').addEventListener('click',exportGroupingCSV);
for(const [id,delta] of [['grouping-previous',-1],['grouping-next',1]])$(id).addEventListener('click',()=>{state.proposalPage+=delta;renderGrouping();$('grouping-filters').scrollIntoView({block:'start'});});
try{const saved=localStorage.getItem(storageKey);if(saved)reviews=new Map(validateReview(JSON.parse(saved)).map(r=>[r.id,r]));}
catch(_){storageWarning='Saved review could not be loaded. Import an exported copy, or start a new review.';}
for(const b of document.querySelectorAll('[data-view]'))b.addEventListener('click',()=>showView(b.dataset.view));
for(const key of ['group','tier','rarity','signal','sort'])$(key).addEventListener('change',()=>{state[key]=$(key).value;state.page=0;renderExplorer();});
$('search').addEventListener('input',()=>{state.q=$('search').value;state.page=0;renderExplorer();});
$('scope').addEventListener('change',()=>{state.scope=$('scope').value;state.page=0;showView(state.view);});
$('group-sort').addEventListener('change',()=>renderGroupMatrix(scopeCards()));
$('pack-kind').addEventListener('change',renderPacks);
$('reset-filters').addEventListener('click',()=>{state.scope='all';drill();});
$('show-ranks').addEventListener('change',()=>document.body.classList.toggle('hide-ranks',!$('show-ranks').checked));
$('previous-page').addEventListener('click',()=>{state.page--;renderExplorer();$('explorer').scrollIntoView({block:'start'});});
$('next-page').addEventListener('click',()=>{state.page++;renderExplorer();$('explorer').scrollIntoView({block:'start'});});
$('export-csv').addEventListener('click',exportCSV);
$('export-review').addEventListener('click',()=>download('tessera-catalog-review.json',JSON.stringify(reviewDocument(),null,2)+'\n','application/json'));
$('import-review').addEventListener('click',()=>$('import-file').click());
$('import-file').addEventListener('change',async()=>{
 const file=$('import-file').files[0];if(!file)return;
 try{if(file.size>8_000_000)throw Error('Review file is too large.');const entries=validateReview(JSON.parse(await file.text()));for(const r of entries)reviews.set(r.id,r);persistReview();notify('Imported '+entries.length+' card reviews.');showView('notebook');}
 catch(e){notify('Import failed: '+e.message);}finally{$('import-file').value='';}
});
$('proposal-form').addEventListener('submit',e=>{
 e.preventDefault();const c=byID.get(selected);
 const entry={id:c.id,name:c.name,before:before(c),decision:$('proposal-decision').value,note:$('proposal-note').value,
  after:{tier:Number($('proposal-tier').value),rarity:$('proposal-rarity').value,groups:[...new Set($('proposal-groups').value.split(',').map(s=>s.trim()).filter(Boolean))],sides:[0,1,2,3].map(i=>Number($('side-'+i).value))}};
 try{const checked=validateReview({...reviewDocument(),entries:[entry]})[0];reviews.set(c.id,checked);persistReview();$('remove-proposal').disabled=false;$('proposal-feedback').textContent='Saved to this review.';}
 catch(error){$('proposal-feedback').textContent=error.message;}
});
$('proposal-tier').addEventListener('change',draftCost);
for(let i=0;i<4;i++)$('side-'+i).addEventListener('input',draftCost);
$('remove-proposal').addEventListener('click',()=>{reviews.delete(selected);persistReview();openCard(selected);$('proposal-feedback').textContent='Proposal removed.';});
for(const b of document.querySelectorAll('[data-close]'))b.addEventListener('click',()=>$(b.dataset.close).close());
$('open-compare').addEventListener('click',openComparison);
$('clear-compare').addEventListener('click',()=>{pinned=[];updatePins();});
renderStats();syncFilters();groupingFilters();renderNotebook();showView(state.view);if(storageWarning)notify(storageWarning);
