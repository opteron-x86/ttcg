'use strict';

// randomUUID requires a secure context; LAN HTTP still provides getRandomValues.
function workshopID() {
 const bytes = crypto.getRandomValues(new Uint8Array(16));
 bytes[6] = (bytes[6] & 15) | 64; bytes[8] = (bytes[8] & 63) | 128;
 return [...bytes].map(n => n.toString(16).padStart(2,'0')).join('');
}

function createWorkshopSync({endpoint,key,snapshot,validate,apply,busy,download}) {
 const $ = id => document.getElementById(id), prefix = key + '-shared-draft-', empty = snapshot();
 let revision = 'empty', baseline = '', ready = false, pending = null, sending = false;
 let pulling = false, blocked = false, timer = null, draftKey = prefix + workshopID();
 const signature = doc => JSON.stringify(doc.groups);
 function status(text) {$('save-status').textContent = text; $('save-status').dataset.mode = 'shared';}
 function notice(text, actions = []) {
  const node = $('shared-sync'); node.replaceChildren(); node.hidden = !text;
  if (!text) return;
  const label = document.createElement('span'); label.textContent = text; node.append(label);
  for (const [text,action] of actions) {
   const b = document.createElement('button'); b.type = 'button'; b.textContent = text;
   b.addEventListener('click',action); node.append(b);
  }
 }
 function keepDraft() {
  try {localStorage.setItem(draftKey,JSON.stringify(pending)); return true;}
  catch (_) {return false;}
 }
 function clearDraft() {try {localStorage.removeItem(draftKey);} catch (_) { /* Saving on the host still works. */ }}
 function downloadMine() {download(JSON.stringify(snapshot(),null,2) + '\n','tessera-unsaved-workshop.json');}
 function showConflict() {
  status('Shared save has newer changes');
  notice('Another session saved changes. Your edits are kept separately until you load the shared version.',
   [['Download my edits',downloadMine],['Load shared version',() => pull(true)]]);
 }
 function recoveries() {
  const holder = $('shared-recovery'); holder.replaceChildren(); holder.hidden = true;
  const drafts = [];
  try {
   for (let i = 0; i < localStorage.length; i++) {
    const id = localStorage.key(i);
    if (!id.startsWith(prefix) || (pending && id === draftKey)) continue;
    const raw = localStorage.getItem(id);
    try {const doc = JSON.parse(raw); drafts.push({id,raw,doc});}
    catch (_) {drafts.push({id,raw,doc:null});}
   }
  } catch (_) {return;}
  if (!drafts.length) return;
  drafts.sort((a,b) => (b.doc?.updatedAt || '').localeCompare(a.doc?.updatedAt || ''));
  holder.hidden = false;
  const summary = document.createElement('summary'); summary.textContent = 'Unsaved drafts in this browser (' + drafts.length + ')';
  const select = document.createElement('select'); select.setAttribute('aria-label','Unsaved draft');
  for (const [i,draft] of drafts.entries()) {
   const o = document.createElement('option'); o.value = i;
   o.textContent = draft.doc?.updatedAt ? new Date(draft.doc.updatedAt).toLocaleString() : 'Recovered draft'; select.append(o);
  }
  const controls = document.createElement('div'); controls.append(select);
  const choose = () => drafts[Number(select.value)];
  for (const [text,action] of [
   ['Resume draft',() => {
    if (!ready || sending) return;
    const draft = choose();
    try {
     const doc = validate(draft.doc.workshop);
     if (typeof draft.doc.revision !== 'string') throw Error('Missing save revision.');
     // Preserve any different branch already open in this tab.
     if (pending) keepDraft();
     draftKey = draft.id;
     pending = {...draft.doc,workshop:doc}; apply(doc,false);
     blocked = pending.revision !== revision; recoveries();
     if (blocked) showConflict(); else {notice(''); flush();}
    } catch (_) {notice('This draft could not be loaded. Download it to keep a copy.');}
   }],
   ['Download draft',() => {const draft = choose(); download(draft.doc?.workshop ? JSON.stringify(draft.doc.workshop,null,2) + '\n' : draft.raw,'tessera-recovered-workshop.json');}],
   ['Discard draft',() => {try {localStorage.removeItem(choose().id); recoveries();} catch (_) {notice('The browser could not remove this draft.');}}]
  ]) {
   const b = document.createElement('button'); b.type = 'button'; b.textContent = text; b.addEventListener('click',action); controls.append(b);
  }
  holder.append(summary,controls);
 }
 async function request(options = {}) {
  const controller = new AbortController(), timeout = setTimeout(() => controller.abort(),10000);
  try {return await fetch(endpoint,{cache:'no-store',...options,signal:controller.signal});}
  finally {clearTimeout(timeout);}
 }
 function later() {clearTimeout(timer); timer = setTimeout(flush,350);}
 function changed(doc) {
  if (!ready) return;
  const next = signature(doc);
  if (next === (pending ? signature(pending.workshop) : baseline)) return;
  pending = {revision,changeId:workshopID(),updatedAt:new Date().toISOString(),workshop:doc};
  const kept = keepDraft();
  if (blocked) {showConflict(); return;}
  status('Saving to shared workshop…');
  if (!kept) notice('Browser recovery is unavailable. Keep this tab open until the shared save finishes.',[['Download my edits',downloadMine]]);
  later();
 }
 async function flush() {
  clearTimeout(timer);
  if (!ready || !pending || sending || blocked || pulling) return;
  const sent = pending; sending = true;
  status('Saving to shared workshop…');
  try {
   const response = await request({method:'PUT',headers:{'Content-Type':'application/json','X-Tessera-Workshop':'1',
    'If-Match':'"' + sent.revision + '"','X-Workshop-Change':sent.changeId},body:JSON.stringify(sent.workshop)});
   if (response.status === 409) {blocked = true; showConflict(); return;}
   const result = await response.json();
   if (!response.ok) throw Error(result.error || 'Save failed.');
   revision = result.revision; baseline = signature(sent.workshop);
   if (pending === sent) {pending = null; clearDraft();}
   else {pending.revision = revision; keepDraft();}
   status(pending ? 'Saving to shared workshop…' : 'All groups saved to shared workshop');
   notice(''); recoveries();
  } catch (error) {
   const kept = keepDraft();
   status('Not saved to shared workshop');
   notice((kept ? 'Your edits are saved in this browser. ' : 'Keep this tab open or export your edits. ') +
    (error.message === 'Failed to fetch' || error.name === 'AbortError' ? 'The server is unavailable.' : error.message),
    [['Retry save',flush],['Download my edits',downloadMine]]);
  } finally {
   sending = false;
   if (pending && pending !== sent && !blocked) later();
  }
 }
 async function pull(force = false) {
  if (pulling || sending || (pending && !force)) return;
  pulling = true;
  try {
   const response = await request({headers:ready && !force ? {'If-None-Match':'"' + revision + '"'} : {}});
   if (response.status === 304) return;
   const result = await response.json();
   if (!response.ok) throw Error(result.error || 'Could not load the shared workshop.');
   const doc = result.workshop ? validate(result.workshop) : null;
   if (ready && !force && (pending || blocked || busy())) {blocked = true; showConflict(); return;}
   const wasReady = ready;
   if (pending) {keepDraft(); pending = null; draftKey = prefix + workshopID();}
   apply(doc || empty,wasReady);
   revision = result.revision; baseline = signature(doc || empty);
   ready = true; blocked = false;
   document.body.classList.remove('shared-loading');
   document.querySelectorAll('[data-shared-lock]').forEach(n => n.inert = false);
   status(doc ? 'All groups saved to shared workshop' : 'Shared workshop ready');
   notice(doc ? '' : 'Import your workshop export to continue an existing draft, or start placing cards.');
   recoveries();
  } catch (error) {
   status(ready ? 'Shared server unavailable' : 'Waiting for shared workshop');
   notice(error.message === 'Failed to fetch' || error.name === 'AbortError' ? 'The shared server could not be reached.' : error.message,
    [['Retry connection',() => pull(force)]]);
  } finally {pulling = false;}
 }
 return {
  ready:() => ready,
  changed,
  start() {
   status('Loading shared workshop…');
   document.body.classList.add('shared-loading');
   document.querySelectorAll('[data-shared-lock]').forEach(n => n.inert = true);
   pull();
   setInterval(() => {if (pending) flush(); else if (!blocked) pull();},5000);
   window.addEventListener('online',() => pending ? flush() : pull());
   window.addEventListener('focus',() => {if (!blocked) {if (pending) flush(); else pull();}});
   window.addEventListener('beforeunload',event => {if (pending) {event.preventDefault(); event.returnValue = '';}});
  }
 };
}
