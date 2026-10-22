'use strict';
(() => {
 const data = JSON.parse(document.getElementById('builder-data').textContent);
 const templates = new Map(data.templates.map(t => [t.id,t])), cards = new Map(data.cards.map(c => [c.id,c]));
 const proposed = new Map(data.proposals.flatMap(p => p.entries).map(e => [e.id,e]));
 const reviewed = new Map(data.proposals.flatMap(p => p.sourceEntries).map(e => [e.id,e]));
 const rarities = data.rarities, affinities = ['Strength','Intelligence','Willpower','Agility','Endurance'];
 const roman = ['','I','II','III','IV','V','VI','VII','VIII','IX','X'];
 const newCardToken = 'new-card-placeholder';
 const storageKey = 'tessera-roster-workshop-v2-' + data.fingerprint;
 const clone = value => JSON.parse(JSON.stringify(value));
 const $ = id => document.getElementById(id);
 const undo = [], redo = [], pageSize = 24;
 let rosters = new Map(data.templates.map(t => [t.id,t.slots.map(s => ({...clone(s),assignment:null,confirmed:false}))]));
 let template = templates.get('dark-elf-v2'), slots = rosters.get(template.id);
 let selected = null, page = 0, editing = null, dragging = null, protectStorage = false, lastNameEdit = null;
 const endpoint = document.querySelector('meta[name="workshop-save-api"]').content;
 const sync = endpoint ? createWorkshopSync({endpoint,key:storageKey,snapshot:workspace,validate:readWorkspace,download,
  busy:() => Boolean(document.querySelector('dialog[open],.slot-name:focus')),
  apply:(doc,keepView) => {
   const current = template.id;
   adopt(keepView ? {...doc,currentGroup:current} : doc);
   selected = null; editing = null; lastNameEdit = null; undo.length = 0; redo.length = 0;
   document.querySelectorAll('dialog[open]').forEach(d => d.close());
   render();
  }}) : null;

 function el(tag, cls, text) {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  if (text !== undefined) node.textContent = text;
  return node;
 }
 function button(text, action, cls) {
  const b = el('button',cls,text); b.type = 'button'; b.addEventListener('click',action); return b;
 }
 function option(value, label) {const o = el('option','',label); o.value = value; return o;}
 function isNew(a) {return a?.kind === 'new';}
 function cardName(cardId) {return placement(cardId)?.slot.assignment.name || cards.get(cardId)?.name || 'New card';}
 function message(text) {$('message').textContent = text; $('message').hidden = !text;}
 function slotLabel(s, t = template) {return 'Tier ' + roman[s.tier] + ' · slot ' + (rosters.get(t.id).filter(r => r.tier === s.tier).findIndex(r => r.id === s.id) + 1);}
 function allSlots() {return [...rosters.values()].flat();}
 function placement(cardId) {
  for (const [id,list] of rosters) {const s = list.find(s => s.assignment?.cardId === cardId); if (s) return {slot:s,template:templates.get(id)};}
  return null;
 }
 function defaults(s) {return template.slots.find(p => p.id === s.id) || template.presets.find(p => p.id === s.preset);}
 const {cost,validRanks,randomProfiles,pickRandomSides} = createTesseraRankTools(data);
 function randomizeSlots(targets) {
  if (!targets.length) return;
  const tier = targets[0].tier, ids = new Set(targets.map(s => s.id));
  const used = new Set(slots.filter(s => s.tier === tier && !ids.has(s.id)).map(s => s.sides.join()));
  const changes = [];
  // Place the most constrained profiles first so a broad Common pool cannot
  // consume the few layouts available to a narrow peak profile.
  for (const s of [...targets].sort((a,b) => randomProfiles(a).length - randomProfiles(b).length)) {
   const sides = pickRandomSides(s,used);
   if (sides) {changes.push([s,sides]); used.add(sides.join());}
   else used.add(s.sides.join());
  }
  if (!changes.length) {message('No alternative valid side layout is available.'); return;}
  mutate(() => {for (const [s,sides] of changes) {s.sides = sides; s.confirmed = false;}},
   changes.length === 1 ? 'Side values randomized. Undo restores the previous layout.' : 'Randomized ' + changes.length + ' cards in tier ' + roman[tier] + '. Undo restores the row.');
 }
 function randomizeButton(s) {
  const b = button('',() => randomizeSlots([s]),'randomize-card');
  b.title = 'Randomize side values'; b.setAttribute('aria-label','Randomize values for ' + slotLabel(s));
  const svg = document.createElementNS('http://www.w3.org/2000/svg','svg');
  svg.setAttribute('viewBox','0 0 20 20'); svg.setAttribute('aria-hidden','true');
  const path = document.createElementNS(svg.namespaceURI,'path');
  path.setAttribute('d','M2 5h3c4 0 6 10 10 10h3m-3-3 3 3-3 3M2 15h3c4 0 6-10 10-10h3m-3-3 3 3-3 3');
  svg.append(path); b.append(svg); return b;
 }
 function valueErrors(s, t = template) {
  const errors = [];
  if (!validRanks(s)) errors.push('Side values fall outside this tier’s rank policy.');
  if (!t.allowedRarities[s.tier]?.includes(s.rarity)) errors.push(t.mode === 'god' ? 'God slots are Legendary trophies.' : t.mode === 'dragon' ? 'Dragons use Rare/Epic, plus Legendary trophies in VIII–IX.' : s.tier >= 8 ? 'Tiers VIII–IX are reserved for Legendaries.' : 'Use Common, Rare or Epic in tiers I–VII.');
  if (s.rarity === 'Legendary' && !s.unique) errors.push('Legendary trophy slots must be unique.');
  if (s.unique && s.rarity !== 'Legendary') errors.push('Unique trophy slots use Legendary rarity.');
  return errors;
 }
 function slotErrors(s, roster = allSlots(), t = template) {
  const errors = valueErrors(s,t);
  if (s.assignment && !s.assignment.name.trim()) errors.push('Give this card a name.');
  if (s.assignment?.name.trim() && roster.some(other => other.id !== s.id &&
    other.assignment?.name.trim().toLocaleLowerCase() === s.assignment.name.trim().toLocaleLowerCase())) errors.push('Another slot has the same card name.');
  return errors;
 }
 function envelope(t = template, roster = rosters.get(t.id)) {
  return {kind:'tessera-catalog-template',schema:2,catalogFingerprint:data.fingerprint,sideOrder:[...data.policy.side_order],
   balanceProposal:{version:data.workshopPolicy.version,ordinaryPeakFraction:data.workshopPolicy.ordinaryPeakFraction,rarityNotes:clone(data.workshopPolicy.rarityNotes)},
   template:{id:t.id,revision:t.revision,group:t.group,label:t.label},slots:clone(roster)};
 }
 function workspace() {
  return {kind:'tessera-roster-workshop',schema:2,catalogFingerprint:data.fingerprint,sideOrder:[...data.policy.side_order],
   ordinaryCurve:clone(data.workshopPolicy.ordinaryCurve),
   currentGroup:template.id,groups:data.templates.map(t => envelope(t))};
 }
 function adopt(doc) {
  rosters = new Map(doc.groups.map(g => [g.template.id,g.slots]));
  template = templates.get(doc.currentGroup); slots = rosters.get(template.id);
 }
 function activate(id) {
  template = templates.get(id); slots = rosters.get(id); selected = null; page = 0; editing = null; lastNameEdit = null;
  $('library-search').value = ''; $('library-filter').value = 'all'; $('library-pool').value = 'pool';
  render(); $('slot-scroll').scrollTop = 0; $('library-scroll').scrollTop = 0; save(); message('');
 }
 // Import only editable slots. All source identities and before/after rows are rebuilt
 // from this catalog, never from file-supplied art URLs or derived export fields.
 function readDraft(doc) {
  if (!doc || doc.kind !== 'tessera-catalog-template' || doc.schema !== 2) throw Error('Choose a roster workshop export.');
  if (doc.catalogFingerprint !== data.fingerprint) throw Error('This file belongs to a different catalog snapshot.');
  const t = templates.get(doc.template?.id);
  if (!t || doc.template.revision !== t.revision || doc.template.group !== t.group) throw Error('This file uses a different template revision.');
  if (JSON.stringify(doc.sideOrder) !== JSON.stringify(data.policy.side_order)) throw Error('Unexpected side order.');
  if (!Array.isArray(doc.slots) || doc.slots.length > 500) throw Error('Invalid slot list.');
  const found = new Map(), identities = new Set();
  for (const s of doc.slots) {
   const original = t.slots.find(p => p.id === s?.id), preset = t.presets.find(p => p.id === s?.preset);
   const added = s && typeof s.id === 'string' && s.id.startsWith(t.slug + '-' + s.tier + '-added-') && /^[a-z0-9-]{1,100}$/.test(s.id);
   if (!s || (!original && !added) || found.has(s.id) || !t.tiers.includes(s.tier) || (original && original.tier !== s.tier) || !preset || preset.tier !== s.tier) throw Error('Unknown, repeated or moved slot.');
   if (!rarities.includes(s.rarity) || typeof s.unique !== 'boolean' || typeof s.confirmed !== 'boolean' ||
     !Array.isArray(s.sides) || s.sides.length !== 4 || s.sides.some(n => !Number.isInteger(n) || n < 1 || n > 10) ||
     !Array.isArray(s.affinities) || s.affinities.some(a => !affinities.includes(a)) || new Set(s.affinities).size !== s.affinities.length) throw Error('Invalid game values in ' + slotLabel(s) + '.');
   let assignment = null;
   if (s.assignment !== null) {
    const a = s.assignment;
    const known = isNew(a) ? typeof a.cardId === 'string' && /^new-[0-9a-f]{32}$/.test(a.cardId) : a?.kind === undefined && cards.has(a?.cardId);
    if (!a || !known || identities.has(a.cardId) || typeof a.name !== 'string' || a.name.length > 120 || /[\u0000-\u001f\u007f]/.test(a.name)) throw Error('Unknown or repeated card, or invalid name.');
    identities.add(a.cardId); assignment = {cardId:a.cardId,name:a.name,...(isNew(a) ? {kind:'new'} : {})};
   }
   found.set(s.id,{id:s.id,preset:s.preset,tier:s.tier,rarity:s.rarity,sides:[...s.sides],affinities:[...s.affinities],unique:s.unique,assignment,confirmed:s.confirmed});
  }
  const result = [...found.values()].sort((a,b) => a.tier - b.tier);
  if (result.some(s => s.confirmed && (!s.assignment || slotErrors(s,result,t).length))) throw Error('An empty or invalid slot is marked confirmed.');
  return envelope(t,result);
 }
 function readWorkspace(doc) {
  if (!doc || doc.kind !== 'tessera-roster-workshop' || doc.schema !== 2 || doc.catalogFingerprint !== data.fingerprint ||
    !Array.isArray(doc.groups) || doc.groups.length !== templates.size || !templates.has(doc.currentGroup)) throw Error('Choose a workshop export for this catalog snapshot.');
  const groups = doc.groups.map(readDraft), ids = groups.map(g => g.template.id);
  if (new Set(ids).size !== templates.size) throw Error('Repeated or missing group.');
  const all = groups.flatMap(g => g.slots), assigned = all.filter(s => s.assignment);
  if (new Set(assigned.map(s => s.assignment.cardId)).size !== assigned.length) throw Error('A card is placed in more than one group.');
  for (const g of groups) if (g.slots.some(s => s.confirmed && slotErrors(s,all,templates.get(g.template.id)).length)) throw Error('A confirmed card has a duplicate name in another group.');
  return {...doc,groups};
 }
 function download(text, filename) {
  const url = URL.createObjectURL(new Blob([text],{type:'application/json'}));
  const a = el('a'); a.href = url; a.download = filename; a.click();
  setTimeout(() => URL.revokeObjectURL(url),1000);
 }
 function save() {
  if (sync) {sync.changed(workspace()); return;}
  if (protectStorage) {$('save-status').textContent = 'Autosave unavailable · export to keep your work'; return;}
  try {localStorage.setItem(storageKey,JSON.stringify(workspace())); $('save-status').textContent = 'All groups saved in this browser';}
  catch (_) {$('save-status').textContent = 'Autosave unavailable · export to keep your work';}
 }
 function load() {
  let raw;
  try {
   raw = localStorage.getItem(storageKey);
   if (raw) adopt(readWorkspace(JSON.parse(raw)));
  } catch (_) {
   if (raw) {
    const recover = button('Download saved draft',() => download(raw,'tessera-template-recovery.json'));
    recover.id = 'recover-save'; $('save-status').after(recover);
    try {localStorage.setItem(storageKey + '-recovery-' + Date.now(),raw);}
    catch (_) {protectStorage = true;}
    message('The saved workshop could not be loaded. It has been kept for recovery; blank templates are shown below.');
   }
  }
 }
 function mutate(action, text, nameEdit = null) {
  if (sync && !sync.ready()) return;
  const before = JSON.stringify(workspace()); action(); rosters.set(template.id,slots);
  // Name collisions can invalidate a previously confirmed slot as well as the edited one.
  for (const [id,list] of rosters) for (const s of list) if (!s.assignment || slotErrors(s,allSlots(),templates.get(id)).length) s.confirmed = false;
  if (JSON.stringify(workspace()) === before) {if (!nameEdit) render(); return;}
  if (!nameEdit || lastNameEdit !== nameEdit) undo.push(before);
  lastNameEdit = nameEdit;
  if (undo.length > 60) undo.shift(); redo.length = 0;
  save();
  if (nameEdit) {renderCounts(); updateNameFeedback();} else render();
  if (text) message(text);
 }
 function historyStep(from, to) {
  if (sync && !sync.ready()) return;
  if (!from.length) return;
  to.push(JSON.stringify(workspace())); adopt(JSON.parse(from.pop())); selected = null; lastNameEdit = null;
  save(); render(); message('');
 }
 function place(cardId, targetId) {
  const target = slots.find(s => s.id === targetId), from = placement(cardId), origin = from?.slot;
  if (!cards.has(cardId) && !origin && cardId !== newCardToken) return;
  if (!target || target === origin) return;
  selected = null;
  mutate(() => {
   const identity = origin ? origin.assignment : cardId === newCardToken ? {cardId:'new-' + workshopID(),name:'',kind:'new'} : {cardId,name:cards.get(cardId).name};
   if (origin) {origin.assignment = from.template.id === template.id ? target.assignment : null; origin.confirmed = false;}
   target.assignment = identity; target.confirmed = false;
  },cardId === newCardToken ? 'Name your new card below the card back.' : from && from.template.id !== template.id ? 'Moved from ' + from.template.group + '. Undo restores both groups.' : origin ? 'Placements swapped; slot values are preserved.' : 'Card placed.');
  if (cardId === newCardToken) document.querySelector('[data-slot-id="' + targetId + '"] .slot-name')?.focus();
 }
 function dragSource(node, cardId) {
  node.draggable = true;
  node.addEventListener('dragstart',event => {
   dragging = cardId; event.dataTransfer.effectAllowed = 'move';
   event.dataTransfer.setData('application/x-tessera-card',cardId);
   event.dataTransfer.setData('text/plain',cardId);
  });
  node.addEventListener('dragend',() => {dragging = null; document.querySelectorAll('.drop-target').forEach(n => n.classList.remove('drop-target'));});
 }
 function addSlot(tier, rarity) {
  if (slots.length >= 500) return;
  const count = a => slots.filter(s => s.affinities.includes(a)).length;
  const candidates = template.presets.filter(p => p.tier === tier && p.rarity === rarity).sort((a,b) => {
   const copies = p => slots.filter(s => s.tier === tier && s.sides.join() === p.sides.join()).length;
   return copies(a) - copies(b) || count(a.affinities[0]) - count(b.affinities[0]);
  });
  const p = candidates[0];
  mutate(() => {
   slots.push({...clone(p),id:template.slug + '-' + tier + '-added-' + workshopID(),preset:p.id,assignment:null,confirmed:false});
   slots.sort((a,b) => a.tier - b.tier);
  },'Added a ' + rarity + ' slot to tier ' + roman[tier] + '.');
 }
 function fillEmpty() {
  let filled = 0;
  mutate(() => {
   const available = template.pool.map(id => cards.get(id)).filter(c => !placement(c.id) && proposed.get(c.id)?.decision !== 'retire');
   const sourceTier = c => proposed.get(c.id)?.after.tier || c.tier;
   const sourceRarity = c => proposed.get(c.id)?.after.rarity || c.rarity;
   // Reserve exact matches first, then make conservative suggestions within one
   // tier. Missing subjects remain empty rather than forcing distant promotions.
   for (const exact of [true,false]) for (const s of slots.filter(s => !s.assignment)) {
    const choices = available.filter(c => c.unique === s.unique && Math.abs(sourceTier(c) - s.tier) <= (exact ? 0 : 1) &&
      (!exact || sourceRarity(c) === s.rarity)).sort((a,b) => {
     const score = c => Math.abs(sourceTier(c) - s.tier) * 10 + (sourceRarity(c) !== s.rarity ? 3 : 0) + (reviewed.get(c.id)?.decision === 'keep' ? -1 : 0);
     return score(a) - score(b) || a.name.localeCompare(b.name);
    });
    if (choices.length) {
     const c = choices[0]; s.assignment = {cardId:c.id,name:c.name}; s.confirmed = false;
     available.splice(available.indexOf(c),1); filled++;
    }
   }
  });
  message(filled ? 'Filled ' + filled + ' empty slots with nearby candidates. Review the names and artwork before confirming.' : 'No nearby unused candidates. Choose artwork from the library or leave room for a new card.');
 }
 function portrait(s) {
  const p = el('div','portrait'); p.dataset.rarity = s.rarity;
  if (s.assignment) {
   const c = cards.get(s.assignment.cardId), img = el('img');
   img.src = isNew(s.assignment) ? data.cardBack : c.thumbnail;
   img.alt = s.assignment.name || c?.name || 'New card'; img.loading = 'lazy'; img.draggable = false;
   img.style.objectPosition = c?.focus || 'center'; p.append(img);
   if (isNew(s.assignment)) p.classList.add('placeholder-portrait');
  } else p.append(el('span','empty-label','Place a card'));
  p.append(el('span','tier-badge',roman[s.tier]));
  ['top','right','bottom','left'].forEach((side,i) => p.append(el('span','rank ' + side,s.sides[i] === 10 ? 'A' : s.sides[i])));
  return p;
 }
 function renderSlots() {
  const scroll = $('slot-scroll').scrollTop;
  const sections = [];
  for (const tier of template.tiers) {
   const members = slots.filter(s => s.tier === tier), section = el('section','tier-section'); section.id = 'tier-' + tier;
   const heading = el('div','tier-heading'), policy = data.policy.tiers.find(p => p.tier === tier);
   const title = el('div','tier-title');
   title.append(el('h3','','Tier ' + roman[tier]),el('span','',members.filter(s => s.assignment).length + '/' + members.length + ' placed · ' + policy.weighted_min + '–' + policy.weighted_max + ' weighted'));
   const controls = el('div','tier-controls'), rarity = el('select','add-rarity');
   rarity.setAttribute('aria-label','Rarity for new tier ' + roman[tier] + ' slot');
   rarity.append(...template.allowedRarities[tier].map(r => option(r,r)));
   rarity.value = template.allowedRarities[tier].find(r => members.filter(s => s.rarity === r).length < template.slots.filter(s => s.tier === tier && s.rarity === r).length) || template.allowedRarities[tier][0];
   const add = button('+ Slot',() => addSlot(tier,rarity.value),'add-slot'); add.setAttribute('aria-label','Add slot to tier ' + roman[tier]); add.disabled = slots.length >= 500;
   const randomize = button('Randomize',() => randomizeSlots(members),'randomize-tier');
   randomize.setAttribute('aria-label','Randomize values for tier ' + roman[tier]); randomize.title = 'Randomize side values for every slot in this tier'; randomize.disabled = !members.length;
   controls.append(randomize,rarity,add); heading.append(title,controls);
   const grid = el('div','slot-grid');
   for (const s of members) {
    const errors = slotErrors(s), node = el('article','template-slot' + (s.confirmed ? ' confirmed' : '') + (errors.length ? ' invalid' : ''));
    node.dataset.slotId = s.id; if (s.assignment) node.dataset.cardId = s.assignment.cardId;
    node.addEventListener('dragover',event => {if (dragging || event.dataTransfer.types.includes('application/x-tessera-card')) {event.preventDefault(); event.dataTransfer.dropEffect = 'move'; node.classList.add('drop-target');}});
    node.addEventListener('dragleave',event => {if (!node.contains(event.relatedTarget)) node.classList.remove('drop-target');});
    node.addEventListener('drop',event => {
     event.preventDefault(); node.classList.remove('drop-target');
     const cardId = event.dataTransfer.getData('application/x-tessera-card');
     dragging = null; place(cardId,s.id);
    });
    const label = el('div','slot-label'); label.append(el('span','','Slot ' + (members.indexOf(s) + 1)));
    const actions = el('div','slot-label-actions');
    const clear = button('Clear',() => mutate(() => {s.assignment = null; s.confirmed = false;},'Placement cleared. Undo restores it.'),'clear-placement');
    clear.setAttribute('aria-label','Clear ' + slotLabel(s)); clear.disabled = !s.assignment;
    const remove = button('−',() => mutate(() => {slots = slots.filter(other => other.id !== s.id); selected = null;},'Slot removed. Undo restores its placement and values.'),'remove-slot');
    remove.setAttribute('aria-label','Remove ' + slotLabel(s)); remove.title = 'Remove this slot'; actions.append(randomizeButton(s),clear,remove); label.append(actions);
    const face = button('',() => {
     if (selected) place(selected,s.id);
     else if (s.assignment) {selected = s.assignment.cardId; renderLibrary(); message('Select another slot to move or swap this card.');}
     else message('Select a card from the original library, then select this slot.');
    },'slot-face');
    face.setAttribute('aria-label',slotLabel(s) + (s.assignment ? ': ' + (s.assignment.name || 'Unnamed card') : ': empty'));
    face.title = 'Select a source card, then click here to place it. Drag to another slot to move or swap.';
    face.append(portrait(s)); if (s.assignment) dragSource(face,s.assignment.cardId);
    const name = el('input','slot-name'); name.type = 'text'; name.maxLength = 120;
    name.value = s.assignment?.name || ''; name.placeholder = 'Card name'; name.disabled = !s.assignment;
    name.setAttribute('aria-label','Card name for ' + slotLabel(s));
    // Keep the active input and its sibling buttons mounted while typing. Replacing
    // them on blur can swallow the user's following click on Confirm or Values.
    name.addEventListener('input',() => mutate(() => {s.assignment.name = name.value; s.confirmed = false;},null,s.id));
    name.addEventListener('blur',() => {lastNameEdit = null;});
    const meta = el('div','slot-meta ' + s.rarity,s.rarity + (s.unique ? ' · Unique' : '') + ' · ' + s.sides.reduce((a,b) => a+b,0) + ' total');
    meta.title = cost(s) + ' weighted points. ' + data.workshopPolicy.rarityNotes[s.rarity];
    const affinity = el('select','slot-affinity'); affinity.setAttribute('aria-label','Affinity for ' + slotLabel(s));
    affinity.append(option('','Neutral'),...affinities.map(a => option(a,a)),option('multiple',s.affinities.length > 1 ? s.affinities.join(' + ') : 'Multiple…'));
    affinity.value = s.affinities.length > 1 ? 'multiple' : s.affinities[0] || '';
    affinity.addEventListener('change',() => {
     if (affinity.value === 'multiple') openSlot(s.id);
     else mutate(() => {s.affinities = affinity.value ? [affinity.value] : []; s.confirmed = false;});
    });
    const buttons = el('div','slot-actions');
    const adjust = button('Values',() => openSlot(s.id)); adjust.setAttribute('aria-label','Edit values for ' + slotLabel(s));
    const confirm = button(s.confirmed ? '✓ Confirmed' : 'Confirm',() => mutate(() => {s.confirmed = !s.confirmed;}),'slot-confirm' + (s.confirmed ? ' confirmed-button' : ''));
    confirm.setAttribute('aria-label',(s.confirmed ? 'Unconfirm ' : 'Confirm ') + slotLabel(s));
    confirm.disabled = !s.assignment || errors.length > 0; confirm.setAttribute('aria-pressed',String(s.confirmed));
    buttons.append(adjust,confirm); node.append(label,face,name);
    if (isNew(s.assignment)) node.append(el('span','art-pending','Artwork pending'));
    node.append(meta,affinity,buttons);
    if (errors.length) node.append(el('p','slot-warning',errors.join(' ')));
    grid.append(node);
   }
   section.append(heading,grid); sections.push(section);
  }
  $('tiers').replaceChildren(...sections); $('slot-scroll').scrollTop = scroll;
 }
 function renderLibrary() {
  const q = $('library-search').value.trim().toLocaleLowerCase(), pool = $('library-pool').value, filter = $('library-filter').value;
  const placed = new Map([...rosters].flatMap(([id,list]) => list.filter(s => s.assignment).map(s => [s.assignment.cardId,{slot:s,template:templates.get(id)}])));
  const matches = data.cards.filter(c => (pool === 'all' || template.pool.includes(c.id)) &&
   (!q || (c.name + ' ' + c.groups.join(' ')).toLocaleLowerCase().includes(q)) &&
   (filter === 'all' || (filter === 'used' ? placed.has(c.id) : filter === 'unused' ? !placed.has(c.id) : proposed.get(c.id)?.decision === 'retire')))
   .sort((a,b) => a.name.localeCompare(b.name));
  page = Math.max(0,Math.min(page,Math.ceil(matches.length / pageSize) - 1));
  $('library-count').textContent = matches.length + ' cards';
  $('selection-hint').textContent = selected ? cardName(selected) + ' selected. Choose a slot. Esc cancels.' : 'You can also select a card, then select a slot.';
  $('new-card').setAttribute('aria-pressed',String(selected === newCardToken));
  $('library-empty').hidden = Boolean(matches.length);
  $('library-grid').replaceChildren(...matches.slice(page * pageSize,(page + 1) * pageSize).map(c => {
   const node = el('article','source-card'); node.dataset.sourceId = c.id;
   const pick = button('',() => {selected = selected === c.id ? null : c.id; renderLibrary();},'source-pick');
   pick.setAttribute('aria-label','Select ' + c.name); pick.setAttribute('aria-pressed',String(selected === c.id));
   dragSource(pick,c.id);
   const img = el('img'); img.src = c.thumbnail; img.alt = c.name; img.draggable = false; img.loading = 'lazy'; img.style.objectPosition = c.focus;
   pick.append(img,el('span','source-name',c.name));
   node.append(pick,el('span','source-meta',roman[c.tier] + ' · ' + c.rarity + ' · ' + (c.affinities.join(' / ') || 'Neutral')));
   if (placed.has(c.id)) {const used = placed.get(c.id); node.append(el('span','source-used',(used.template.id === template.id ? '' : used.template.group + ' · ') + slotLabel(used.slot,used.template)));}
   else if (proposed.get(c.id)?.decision === 'retire') node.append(el('p','source-status',reviewed.get(c.id)?.decision === 'retire' ? 'Your retirement candidate' : 'Suggested cut'));
   const inspect = button('View original',() => preview(c.id),'inspect'); inspect.setAttribute('aria-label','View original ' + c.name); node.append(inspect);
   return node;
  }));
  $('library-page').textContent = (matches.length ? page + 1 : 0) + ' / ' + Math.ceil(matches.length / pageSize);
  $('library-previous').disabled = page === 0;
  $('library-next').disabled = (page + 1) * pageSize >= matches.length;
 }
 function updateNameFeedback() {
  for (const s of slots) {
   const node = document.querySelector('[data-slot-id="' + s.id + '"]'), errors = slotErrors(s);
   node.classList.toggle('invalid',Boolean(errors.length)); node.classList.toggle('confirmed',s.confirmed);
   const confirm = node.querySelector('.slot-confirm'); confirm.disabled = !s.assignment || errors.length > 0;
   confirm.textContent = s.confirmed ? '✓ Confirmed' : 'Confirm'; confirm.classList.toggle('confirmed-button',s.confirmed);
   confirm.setAttribute('aria-label',(s.confirmed ? 'Unconfirm ' : 'Confirm ') + slotLabel(s)); confirm.setAttribute('aria-pressed',String(s.confirmed));
   node.querySelector('.slot-warning')?.remove();
   if (errors.length) node.append(el('p','slot-warning',errors.join(' ')));
   if (s.assignment) {
    node.querySelector('.slot-face').setAttribute('aria-label',slotLabel(s) + ': ' + (s.assignment.name || 'Unnamed card'));
    node.querySelector('img').alt = s.assignment.name || cardName(s.assignment.cardId);
   }
  }
 }
 function renderCounts() {
  const filled = slots.filter(s => s.assignment), confirmed = filled.filter(s => s.confirmed);
  $('placement-count').textContent = filled.length + '/' + slots.length + ' placed · ' + confirmed.length + ' confirmed';
  $('distribution').replaceChildren(el('span','muted','Placed / slots'),...rarities.map(r => el('span',r,r + ' ' + filled.filter(s => s.rarity === r).length + '/' + slots.filter(s => s.rarity === r).length)));
  $('undo').disabled = !undo.length; $('redo').disabled = !redo.length;
  $('template-title').textContent = template.label;
  $('group-select').value = template.id;
  for (const option of $('group-select').options) {
   const list = rosters.get(option.value), t = templates.get(option.value);
   option.textContent = t.group + ' · ' + list.filter(s => s.assignment).length + '/' + list.length;
  }
  $('workshop-count').textContent = [...rosters.values()].filter(list => list.some(s => s.assignment)).length + ' / 18 groups started';
  $('group-description').textContent = template.mode === 'god' ? 'Legendary uniques · tiers IX–X' : template.mode === 'dragon' ? 'Rare and Epic · tiers VI–IX · Legendary uniques in VIII–IX' : 'I–VII: 3 Common, 1 Rare, 1 Epic · VIII–IX: Legendary uniques';
  $('library-pool').options[0].textContent = template.group + ' candidates';
 }
 function render() {
  renderCounts(); renderSlots(); renderLibrary();
  $('tier-jumps').replaceChildren(...template.tiers.map(tier => button(roman[tier],() => {
   const section = $('tier-' + tier); $('slot-scroll').scrollTop += section.getBoundingClientRect().top - $('slot-scroll').getBoundingClientRect().top - 12;
  })));
 }
 function preview(cardId) {
  const c = cards.get(cardId), user = reviewed.get(cardId), draft = proposed.get(cardId);
  $('card-title').textContent = c.name;
  const img = el('img'); img.src = c.art; img.alt = c.name; $('preview-art').replaceChildren(img);
  const info = $('preview-info'); info.replaceChildren(el('h3','','Original card'),el('p','',c.groups.join(' / ') + ' · Tier ' + roman[c.tier] + ' · ' + c.rarity + (c.unique ? ' · Unique' : '')),
   el('p','','Top ' + c.sides[0] + ' · Right ' + c.sides[1] + ' · Bottom ' + c.sides[2] + ' · Left ' + c.sides[3]),el('p','',c.affinities.join(' / ') || 'Neutral'));
  if (user) info.append(el('h3','','Your saved review'),el('p','',user.decision === 'retire' ? 'Retirement candidate' : user.decision === 'keep' ? 'Keep' : 'Proposed tier ' + roman[user.after.tier] + ' · ' + user.after.rarity),el('p','',user.note || ''));
  if (draft) info.append(el('h3','','Earlier Dark Elf proposal'),el('p','',draft.decision === 'retire' ? 'Suggested cut' : 'Tier ' + roman[draft.after.tier] + ' · ' + draft.after.rarity),el('p','',draft.note));
  $('card-dialog').showModal();
 }
 function editValues() {
  const s = slots.find(s => s.id === editing);
  return {...s,rarity:$('edit-rarity').value,unique:$('edit-unique').checked,
   sides:[0,1,2,3].map(i => Number($('edit-side-' + i).value)),
   affinities:affinities.filter(a => $('edit-affinities').querySelector('input[value="' + a + '"]').checked)};
 }
 function validateEditor() {
  if (!editing) return;
  const s = editValues(), errors = valueErrors(s), policy = data.policy.tiers.find(p => p.tier === s.tier);
  $('edit-validation').dataset.valid = String(!errors.length);
  $('edit-validation').textContent = s.sides.reduce((a,b) => a+b,0) + ' printed total · ' + cost(s) + ' weighted points · tier range ' + policy.weighted_min + '–' + policy.weighted_max + '. ' + (errors.join(' ') || 'Valid side profile, including peak and flank limits.');
 }
 function fillEditor(s) {
  $('edit-rarity').value = s.rarity; $('edit-unique').checked = s.unique;
  s.sides.forEach((n,i) => $('edit-side-' + i).value = n);
  $('edit-affinities').querySelectorAll('input').forEach(i => i.checked = s.affinities.includes(i.value));
  validateEditor();
 }
 function openSlot(id) {
  editing = id; const s = slots.find(s => s.id === id);
  $('slot-title').textContent = slotLabel(s); fillEditor(s); $('slot-dialog').showModal();
 }
 function before(c) {return Object.fromEntries(['name','art','tier','rarity','sides','groups','affinities','unique'].map(k => [k,clone(c[k])]));}
 function exportDraft(t = template) {
  const roster = rosters.get(t.id), doc = envelope(t), assigned = roster.filter(s => s.assignment), identities = new Set(assigned.map(s => s.assignment.cardId));
  doc.entries = assigned.map(s => {
   const c = cards.get(s.assignment.cardId);
   return {slotId:s.id,...(isNew(s.assignment) ? {kind:'new',cardId:s.assignment.cardId,needsArtwork:true} : {}),
    source:c ? {id:c.id,name:c.name,art:c.art,form:c.form} : null,before:c ? before(c) : null,
    after:{name:s.assignment.name,art:c?.art || null,tier:s.tier,rarity:s.rarity,sides:[...s.sides],groups:[t.group],affinities:[...s.affinities],unique:s.unique},confirmed:s.confirmed};
  });
  doc.pendingArtwork = doc.entries.filter(e => e.needsArtwork).map(e => ({cardId:e.cardId,slotId:e.slotId,name:e.after.name}));
  doc.complete = roster.length > 0 && assigned.length === roster.length;
  doc.confirmedCount = assigned.filter(s => s.confirmed).length;
  doc.readyForImplementation = doc.complete && !doc.pendingArtwork.length && doc.confirmedCount === roster.length && !roster.some(s => slotErrors(s,allSlots(),t).length);
  doc.unassigned = t.pool.filter(id => !identities.has(id)).map(id => ({id,name:cards.get(id).name,placedIn:placement(id)?.template.group || null,originalReviewDecision:reviewed.get(id)?.decision || null,proposalDecision:proposed.get(id)?.decision || null}));
  doc.removedSlots = t.slots.filter(s => !roster.some(r => r.id === s.id)).map(s => s.id);
  doc.addedSlots = roster.filter(s => !t.slots.some(r => r.id === s.id)).map(s => s.id);
  doc.unassignedAreRetirements = false;
  return doc;
 }
 function exportWorkshop() {
  const doc = workspace(); doc.groups = data.templates.map(t => exportDraft(t));
  doc.readyForImplementation = doc.groups.every(g => g.readyForImplementation);
  doc.unassignedAreRetirements = false;
  return doc;
 }
 function summary() {
  const target = $('summary-content'), assigned = slots.filter(s => s.assignment), doc = exportDraft();
  const table = el('table'), head = el('thead'), heading = el('tr'), body = el('tbody');
  ['Tier','Placed / slots','Common','Rare','Epic','Legendary','Total / weighted','Confirmed'].forEach(t => heading.append(el('th','',t)));
  head.append(heading);
  for (const tier of template.tiers) {
   const members = slots.filter(s => s.tier === tier), filled = members.filter(s => s.assignment), row = el('tr');
   const range = values => values.length ? Math.min(...values) + '–' + Math.max(...values) : '—';
   [roman[tier],filled.length + ' / ' + members.length,...rarities.map(r => members.filter(s => s.rarity === r).length),range(members.map(s => s.sides.reduce((a,b) => a+b,0))) + ' / ' + range(members.map(cost)),members.filter(s => s.confirmed).length].forEach(v => row.append(el('td','',v)));
   body.append(row);
  }
  table.append(head,body);
  target.replaceChildren(el('p','',template.group + ': ' + assigned.length + ' of ' + slots.length + ' placed. ' + doc.confirmedCount + ' confirmed.'),table,
   el('p','muted','Totals add the four printed sides. Weighted points charge more for high edges. Both ranges include empty slots.'),el('p','',doc.addedSlots.length + ' slots added · ' + doc.removedSlots.length + ' default slots removed.'),el('h3','','Rarity profiles'));
  for (const rarity of rarities) target.append(el('p',rarity,rarity + ': ' + data.workshopPolicy.rarityNotes[rarity]));
  target.append(el('h3','','Affinities'));
  const affinityCounts = el('div','summary-affinities');
  for (const a of ['Neutral',...affinities]) {
   const has = s => a === 'Neutral' ? !s.affinities.length : s.affinities.includes(a);
   affinityCounts.append(el('span','',a + ': ' + assigned.filter(has).length + ' / ' + slots.filter(has).length));
  }
  target.append(affinityCounts,el('p','muted','Placed / slots. Cards with several affinities count toward each one.'),el('h3','','Original → proposed'));
  const changed = doc.entries.filter(e => e.before && ['name','tier','rarity','sides','groups','affinities','unique'].some(k => JSON.stringify(e.before[k]) !== JSON.stringify(e.after[k])));
  target.append(el('p','',changed.length + ' assigned cards have changes. ' + doc.unassigned.length + ' candidates remain unplaced. Unplaced cards are listed in the export for later review; they are not marked for deletion.'));
  if (doc.pendingArtwork.length) target.append(el('h3','','New cards'),el('p','',doc.pendingArtwork.length + ' awaiting artwork: ' + doc.pendingArtwork.map(c => c.name || 'Unnamed card').join(', ') + '.'));
  const issues = slots.filter(s => slotErrors(s).length);
  if (issues.length) target.append(el('p','requirement',issues.map(s => slotLabel(s) + ': ' + slotErrors(s).join(' ')).join('\n')));
  else target.append(el('p','','All slot values satisfy the rank policy.'));
  const seen = new Map(), duplicates = [];
  for (const s of slots) {const key = s.tier + ':' + s.sides.join(','); if (seen.has(key)) duplicates.push(slotLabel(seen.get(key)) + ' / ' + slotLabel(s)); else seen.set(key,s);}
  if (duplicates.length) target.append(el('p','muted','Shared side layouts: ' + duplicates.join('; ') + '.'));
  target.append(el('p','',doc.readyForImplementation ? 'Every slot is confirmed. This roster is ready to export for implementation review.' : doc.complete && doc.confirmedCount === slots.length && doc.pendingArtwork.length ? 'The roster is confirmed; new cards still need artwork. Export to continue the art review.' : 'You can export a draft at any point. Confirm every filled slot when the roster is ready.'));
  const groups = el('table','group-summary'), rows = el('tbody');
  for (const t of data.templates) {
   const d = exportDraft(t), row = el('tr');
   row.append(el('th','',t.group),el('td','',d.entries.length + ' / ' + d.slots.length + ' placed'),el('td','',d.confirmedCount + ' confirmed'));
   rows.append(row);
  }
  groups.append(rows); target.append(el('h3','','All groups'),groups);
  $('summary-dialog').showModal();
 }

 $('group-select').append(...data.templates.map(t => option(t.id,t.group)));
 const newArt = el('img'); newArt.src = data.cardBack; newArt.alt = ''; newArt.draggable = false;
 $('new-card').prepend(newArt);
 $('new-card').addEventListener('click',() => {selected = selected === newCardToken ? null : newCardToken; renderLibrary();});
 dragSource($('new-card'),newCardToken);
 $('group-select').addEventListener('change',() => activate($('group-select').value));
 $('edit-rarity').append(...rarities.map(r => option(r,r)));
 for (const affinity of affinities) {
  const label = el('label','checkbox'), input = el('input'); input.type = 'checkbox'; input.value = affinity;
  label.append(input,document.createTextNode(affinity)); $('edit-affinities').append(label);
 }
 for (const node of document.querySelectorAll('[data-close]')) node.addEventListener('click',() => $(node.dataset.close).close());
 $('slot-dialog').addEventListener('close',() => {editing = null; renderSlots();});
 $('slot-form').addEventListener('input',validateEditor);
 $('slot-form').addEventListener('change',validateEditor);
 $('slot-form').addEventListener('submit',event => {
  event.preventDefault(); const s = slots.find(s => s.id === editing), next = editValues();
  mutate(() => {for (const key of ['rarity','unique','sides','affinities']) s[key] = next[key]; s.confirmed = false;});
  $('slot-dialog').close();
 });
 $('restore-slot').addEventListener('click',() => fillEditor(defaults(slots.find(s => s.id === editing))));
 $('randomize-values').addEventListener('click',() => {
  const s = editValues(), used = new Set(slots.filter(other => other.tier === s.tier && other.id !== s.id).map(other => other.sides.join()));
  const sides = pickRandomSides(s,used);
  if (sides) fillEditor({...s,sides});
 });
 $('undo').addEventListener('click',() => historyStep(undo,redo));
 $('redo').addEventListener('click',() => historyStep(redo,undo));
 $('clear-layout').addEventListener('click',() => {selected = null; mutate(() => {for (const s of slots) {s.assignment = null; s.confirmed = false;}},'All placements cleared. Slot values are preserved. Undo restores the layout.');});
 $('suggested').addEventListener('click',fillEmpty);
 $('summary-open').addEventListener('click',summary);
 for (const id of ['library-search','library-pool','library-filter']) $(id).addEventListener(id === 'library-search' ? 'input' : 'change',() => {page = 0; renderLibrary(); $('library-scroll').scrollTop = 0;});
 $('library-previous').addEventListener('click',() => {page--; renderLibrary(); $('library-scroll').scrollTop = 0;});
 $('library-next').addEventListener('click',() => {page++; renderLibrary(); $('library-scroll').scrollTop = 0;});
 $('export').addEventListener('click',() => {
  const doc = exportWorkshop(); download(JSON.stringify(doc,null,2) + '\n','tessera-roster-workshop.json');
  message('Exported all 18 groups, including added and removed slots.');
 });
 $('export-group').addEventListener('click',() => {download(JSON.stringify(exportDraft(),null,2) + '\n','tessera-' + template.slug + '-template.json'); message('Exported the ' + template.group + ' roster.');});
 $('import').addEventListener('click',() => $('import-file').click());
 $('import-file').addEventListener('change',async () => {
  const file = $('import-file').files[0]; if (!file) return;
  try {
   const parsed = JSON.parse(await file.text()); let next;
   if (parsed.kind === 'tessera-roster-workshop') next = readWorkspace(parsed);
   else {
    const group = readDraft(parsed);
    next = workspace(); next.groups = next.groups.map(g => g.template.id === group.template.id ? group : g); next.currentGroup = group.template.id;
    next = readWorkspace(next);
   }
   selected = null; mutate(() => adopt(next)); message('Imported placements and slot values. Undo restores the previous workshop.');
  } catch (error) {message('Import failed: ' + error.message);}
  finally {$('import-file').value = '';}
 });
 document.addEventListener('keydown',event => {
  if (event.key === 'Escape' && !document.querySelector('dialog[open]')) {selected = null; renderLibrary(); message('');}
  const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
  if (!typing && !document.querySelector('dialog[open]') && (event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'z') {
   event.preventDefault(); event.shiftKey ? historyStep(redo,undo) : historyStep(undo,redo);
  }
 });
 if (data.initialWorkshop) adopt(readWorkspace(data.initialWorkshop));
 if (sync) {render(); sync.start();}
 else {load(); save(); render();}
})();
