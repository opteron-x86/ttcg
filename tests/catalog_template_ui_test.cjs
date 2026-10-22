const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), {pathToFileURL} = require('node:url');
const {chromium} = require(process.env.TTCG_PLAYWRIGHT || 'playwright');
const output = path.resolve('build/catalog-audit');
fs.mkdirSync(output,{recursive:true});
const url = pathToFileURL(path.resolve('docs/catalog-template-builder.html')).href;
const data = JSON.parse(fs.readFileSync('docs/catalog-audit-data.json','utf8'));
const templates = new Map(data.templates.map(t=>[t.id,t])), template=templates.get('dark-elf-v2');
const key = 'tessera-roster-workshop-v2-' + data.fingerprint;
const auditKey = 'tessera-catalog-review-v1-' + data.fingerprint;
const card = name => data.cards.find(c=>c.name===name);
const values = s => Object.fromEntries(['id','preset','tier','rarity','sides','affinities','unique'].map(k=>[k,s[k]]));
const saved = p => p.evaluate(k=>JSON.parse(localStorage.getItem(k)),key);
const group = async (p,id=template.id) => (await saved(p)).groups.find(g=>g.template.id===id);
const slot = (p,id) => p.locator('[data-slot-id="'+id+'"]');
function checkRandomProfile(s) {
 const index=s.sides.reduce((n,side)=>n*10+side-1,0), bits=Buffer.from(data.rankChecks[s.tier][s.rarity==='Common'?'Common':'Other'],'base64');
 assert(bits[index>>3]&(1<<(index%8)),'Randomized sides must satisfy the authoritative rank policy');
 const profiles=data.workshopPolicy.profiles[s.tier], examples=profiles[s.rarity]||[];
 const weighted=sides=>sides.reduce((sum,n)=>sum+data.policy.rank_costs[n],0);
 if(s.rarity!=='Legendary'&&examples.length){
  assert(weighted(s.sides)<=Math.max(...examples.map(weighted)),'Keep the rarity’s strength ceiling');
  assert(Math.max(...s.sides)<=Math.max(...examples.flat()),'Keep the rarity’s edge ceiling');
  if(s.rarity==='Epic')assert.equal(Math.max(...s.sides),Math.max(...examples.flat()),'Epics keep their standout edge');
  if(s.rarity!=='Common'&&profiles.Common)assert(weighted(s.sides)>Math.max(...profiles.Common.map(weighted)),'Premium rarities stay above the Common budget');
 }
}
function unchangedExceptSides(before,after){
 assert.notDeepEqual(after.sides,before.sides,'A roll should choose a different layout');
 assert.equal(after.confirmed,false);
 assert.deepEqual({...after,sides:before.sides,confirmed:before.confirmed},before,'Only sides and confirmation change');
 checkRandomProfile(after);
}
async function exportFile(p,all=false) {
 const wait=p.waitForEvent('download');await p.locator(all?'#export':'#export-group').click();
 const download=await wait, filename=path.join(output,download.suggestedFilename());
 await download.saveAs(filename);return JSON.parse(fs.readFileSync(filename,'utf8'));
}
async function importFile(p,doc,success=true) {
 await p.locator('#import-file').setInputFiles({name:'proposal.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(doc))});
 await p.waitForFunction(ok=>document.getElementById('message').textContent.startsWith(ok?'Imported':'Import failed:'),success);
}
async function selectCard(p,name) {
 await p.locator('#library-search').fill(name);
 await p.getByRole('button',{name:'Select '+name,exact:true}).click();
}
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
  const p=await browser.newPage({viewport:{width:1600,height:1080},reducedMotion:'reduce'});
  const errors=[],failed=[],external=[];
  p.on('pageerror',e=>errors.push(e.message));p.on('requestfailed',r=>failed.push(r.url()));
  p.on('request',r=>{if(/^https?:/.test(r.url()))external.push(r.url());});
  await p.goto(url);
  const populated=await saved(p);
  assert.equal(populated.groups.flatMap(g=>g.slots).filter(s=>s.assignment).length,674,'Fresh workshop preserves the imported roster');
  const gods=populated.groups.find(g=>g.template.id==='god-v2');
  assert.equal(gods.slots.filter(s=>s.assignment).length,26);
  assert(gods.slots.filter(s=>s.assignment).every(s=>s.confirmed&&s.tier===10&&s.unique&&s.rarity==='Legendary'));
  await p.locator('#group-select').selectOption('god-v2');
  await p.locator('#tier-10 .slot-face img').evaluateAll(imgs=>Promise.all(imgs.map(i=>i.decode())));
  assert.equal(await p.locator('#tier-10 .template-slot').count(),26);
  const blank=structuredClone(data.initialWorkshop);blank.currentGroup=template.id;
  blank.groups=blank.groups.map(g=>({...g,slots:templates.get(g.template.id).slots.map(s=>({...s,assignment:null,confirmed:false}))}));
  await importFile(p,blank);
  await p.evaluate(k=>localStorage.setItem(k,'preserved-review'),auditKey);
  assert.equal(await p.locator('#group-select option').count(),18);
  assert.match(await p.locator('#placement-count').innerText(),/0\/40 placed/);
  assert.equal(await p.locator('.template-slot.invalid').count(),0);
  const original=await group(p),[first,second]=original.slots;
  for(const t of data.templates){
   await p.locator('#group-select').selectOption(t.id);
   assert.equal(await p.locator('.template-slot').count(),t.slots.length);
   assert.equal(await p.locator('.template-slot.invalid').count(),0,t.group);
   assert.equal(await p.locator('.tier-section').count(),t.tiers.length);
   assert.match(await p.locator('#library-count').innerText(),new RegExp('^'+t.pool.length+' cards$'));
  }
  await p.locator('#group-select').selectOption(template.id);

  // Exercise every supported tier and rarity, including both exception groups.
  // Each row is one undoable action and the rest of the workshop stays identical.
  for(const id of ['dark-elf-v2','dragon-v2','god-v2']){
   await p.locator('#group-select').selectOption(id);
   for(const tier of templates.get(id).tiers){
    const before=await saved(p), previous=await group(p,id);
    await p.locator('#tier-'+tier+' .randomize-tier').click();
    const after=await group(p,id), changed=after.slots.filter(s=>s.tier===tier);
    for(const s of changed)unchangedExceptSides(previous.slots.find(b=>b.id===s.id),s);
    assert.equal(new Set(changed.map(s=>s.sides.join())).size,changed.length,'Avoid duplicate layouts within a row');
    assert.deepEqual(after.slots.filter(s=>s.tier!==tier),previous.slots.filter(s=>s.tier!==tier));
    const savedAfter=await saved(p);
    assert.deepEqual(savedAfter.groups.filter(g=>g.template.id!==id),before.groups.filter(g=>g.template.id!==id));
    await p.locator('#undo').click();assert.deepEqual(await saved(p),before);
    await p.locator('#redo').click();assert.deepEqual(await saved(p),savedAfter);
    await p.locator('#undo').click();
   }
  }
  await p.locator('#group-select').selectOption(template.id);

  // Actual drag-and-drop changes identity only. Source metadata is not copied.
  await p.locator('#library-search').fill('Guar Stablemaster');
  await p.locator('.source-pick').dragTo(slot(p,first.id).locator('.slot-face'));
  assert.equal((await group(p)).slots[0].assignment.cardId,card('Guar Stablemaster').id);
  assert.deepEqual(values((await group(p)).slots[0]),values(first));
  await selectCard(p,'Dres Spy');await slot(p,second.id).locator('.slot-face').click();
  await slot(p,first.id).locator('.slot-name').fill('Guar Keeper');
  await slot(p,first.id).getByRole('button',{name:/^Confirm /}).click();
  assert.equal((await group(p)).slots[0].confirmed,true,'Name editing cannot swallow the Confirm click');
  const confirmedBefore=await saved(p);
  await slot(p,first.id).getByRole('button',{name:/^Randomize values/}).click();
  const singleRoll=await saved(p), rolled=(await group(p)).slots[0];
  unchangedExceptSides(confirmedBefore.groups.find(g=>g.template.id===template.id).slots[0],rolled);
  assert.deepEqual((await group(p)).slots.slice(1),confirmedBefore.groups.find(g=>g.template.id===template.id).slots.slice(1));
  const rolledExport=await exportFile(p);
  assert.deepEqual(rolledExport.entries.find(e=>e.slotId===first.id).after.sides,rolled.sides);
  await p.reload();assert.deepEqual(await saved(p),singleRoll,'Randomized values persist and export');
  // Restore this branch through Import because page reload intentionally resets undo history.
  await importFile(p,confirmedBefore);
  await slot(p,first.id).getByRole('button',{name:/^Edit values/}).click();
  await p.locator('#randomize-values').click();
  const editorRoll=await Promise.all([0,1,2,3].map(i=>p.locator('#edit-side-'+i).inputValue().then(Number)));
  assert.notDeepEqual(editorRoll,(await group(p)).slots[0].sides);
  assert.equal(await p.locator('#edit-validation').getAttribute('data-valid'),'true');
  await p.keyboard.press('Escape');assert.deepEqual(await saved(p),confirmedBefore,'Cancel discards rolls made inside the editor');
  await slot(p,first.id).locator('.slot-face').dragTo(slot(p,second.id).locator('.slot-face'));
  let draft=await group(p);
  assert.equal(draft.slots[0].assignment.cardId,card('Dres Spy').id);
  assert.equal(draft.slots[1].assignment.name,'Guar Keeper');
  assert.deepEqual(draft.slots.map(values),original.slots.map(values));
  assert.equal(draft.slots[1].confirmed,false);
  await p.locator('#undo').click();assert.equal((await group(p)).slots[0].assignment.name,'Guar Keeper');
  await p.locator('#redo').click();assert.equal((await group(p)).slots[1].assignment.name,'Guar Keeper');
  await p.locator('#undo').click();
  await slot(p,first.id).locator('.slot-affinity').selectOption('Endurance');
  assert.equal((await group(p)).slots[0].confirmed,false);
  await slot(p,first.id).getByRole('button',{name:/^Edit values/}).click();
  await p.locator('#edit-rarity').selectOption('Epic');
  await p.locator('#edit-affinities input[value="Intelligence"]').check();
  await p.getByRole('button',{name:'Save slot',exact:true}).click();
  assert.deepEqual((await group(p)).slots[0].affinities,['Intelligence','Endurance']);
  const editedFirst=structuredClone((await group(p)).slots[0]);

  // Higher printed totals do not bypass the authoritative peak/flank rules.
  const tier2=original.slots.find(s=>s.tier===2);
  await slot(p,tier2.id).getByRole('button',{name:/^Edit values/}).click();
  for(const [i,n] of [7,4,1,1].entries())await p.locator('#edit-side-'+i).fill(String(n));
  assert.match(await p.locator('#edit-validation').innerText(),/15 weighted points/);
  assert.equal(await p.locator('#edit-validation').getAttribute('data-valid'),'false');
  await p.getByRole('button',{name:'Save slot',exact:true}).click();
  assert(await slot(p,tier2.id).getByRole('button',{name:/^Confirm /}).isDisabled());
  await slot(p,tier2.id).getByRole('button',{name:/^Edit values/}).click();
  await p.locator('#restore-slot').click();await p.getByRole('button',{name:'Save slot',exact:true}).click();

  // Slots themselves can be added/removed, including the last slot in a tier.
  await p.locator('#tier-1 .add-rarity').selectOption('Rare');
  await p.getByRole('button',{name:'Add slot to tier I',exact:true}).click();
  const added=(await group(p)).slots.find(s=>s.id.includes('-added-'));
  assert.equal(added.rarity,'Rare');assert.equal(added.assignment,null);
  assert.equal(await p.locator('#tier-1 .template-slot').count(),6);
  await selectCard(p,'Brutal Ashlander');await slot(p,added.id).locator('.slot-face').click();
  await slot(p,added.id).getByRole('button',{name:/^Remove /}).click();
  assert(!(await group(p)).slots.some(s=>s.id===added.id));
  await p.locator('#undo').click();assert.equal((await group(p)).slots.find(s=>s.id===added.id).assignment.cardId,card('Brutal Ashlander').id);
  const tier9=original.slots.filter(s=>s.tier===9);
  for(const s of tier9)await slot(p,s.id).getByRole('button',{name:/^Remove /}).click();
  assert.equal(await p.locator('#tier-9 .template-slot').count(),0);
  assert(await p.locator('#tier-9 .randomize-tier').isDisabled());
  assert(await p.getByRole('button',{name:'Add slot to tier IX',exact:true}).isVisible());
  await p.getByRole('button',{name:'Add slot to tier IX',exact:true}).click();
  const addedLegend=(await group(p)).slots.find(s=>s.tier===9);
  assert.equal(addedLegend.rarity,'Legendary');assert.equal(addedLegend.unique,true);
  const addedBefore=await group(p);
  await slot(p,added.id).getByRole('button',{name:/^Randomize values/}).click();
  unchangedExceptSides(addedBefore.slots.find(s=>s.id===added.id),(await group(p)).slots.find(s=>s.id===added.id));
  await p.locator('#undo').click();assert.deepEqual(await group(p),addedBefore);

  // Suggestions fill gaps without overwriting names, custom values or other groups.
  await p.locator('#suggested').click();
  assert.deepEqual((await group(p)).slots[0],editedFirst);
  assert((await group(p)).slots.filter(s=>s.assignment).length>20);
  await p.locator('#slot-scroll').evaluate(n=>n.scrollTop=0);
  await p.locator('#library-search').fill('');
  await p.locator('#library-grid img').evaluateAll(imgs=>Promise.all(imgs.slice(0,6).map(i=>i.decode())));
  await p.screenshot({path:path.join(output,'roster-workshop.png')});
  const dark=await group(p);
  await p.locator('#group-select').selectOption('argonian-v2');
  assert.match(await p.locator('#placement-count').innerText(),/0\/40/);
  await p.locator('#suggested').click();
  const argonian=await group(p,'argonian-v2');
  assert(argonian.slots.some(s=>s.assignment));
  await p.locator('#group-select').selectOption(template.id);
  assert.deepEqual(await group(p),dark);
  await p.reload();assert.deepEqual(await group(p),dark);
  assert.deepEqual(await group(p,'argonian-v2'),argonian);
  assert.equal(await p.evaluate(k=>localStorage.getItem(k),auditKey),'preserved-review');

  // Moving a source from another group cannot duplicate its identity. Undo spans
  // both groups so it restores the source as well as the destination.
  await p.locator('#group-select').selectOption('argonian-v2');
  await p.locator('#library-pool').selectOption('all');await selectCard(p,'Guar Stablemaster');
  assert.match(await p.locator('.source-used').innerText(),/Dark Elf/);
  const argSlot=argonian.slots[0];await slot(p,argSlot.id).locator('.slot-face').click();
  assert(!(await group(p)).slots.some(s=>s.assignment?.cardId===card('Guar Stablemaster').id));
  assert.equal((await group(p,'argonian-v2')).slots[0].assignment.cardId,card('Guar Stablemaster').id);
  await p.locator('#undo').click();assert.deepEqual(await group(p),dark);assert.deepEqual(await group(p,'argonian-v2'),argonian);

  // Whole-workshop and individual group exports retain arbitrary slot structure.
  await p.locator('#group-select').selectOption(template.id);
  const exported=await exportFile(p), all=await exportFile(p,true), working=await saved(p);
  assert.equal(all.groups.length,18);assert.equal(all.kind,'tessera-roster-workshop');
  assert.equal(exported.schema,2);assert(exported.addedSlots.includes(added.id));
  assert(tier9.every(s=>exported.removedSlots.includes(s.id)));
  assert.equal(exported.unassignedAreRetirements,false);assert.equal(exported.readyForImplementation,false);
  const keeper=exported.entries.find(e=>e.source.id===card('Guar Stablemaster').id);
  assert.equal(keeper.before.tier,1);assert.equal(keeper.after.tier,1);assert.equal(keeper.after.name,'Guar Keeper');
  assert.deepEqual(keeper.after.affinities,['Intelligence','Endurance']);
  await p.locator('#clear-layout').click();await importFile(p,exported);
  assert.deepEqual(await group(p),dark);assert.deepEqual(await group(p,'argonian-v2'),argonian);
  await p.locator('#clear-layout').click();await importFile(p,all);
  assert.deepEqual(await saved(p),working);
  await p.locator('#undo').click();assert.equal((await group(p)).slots.filter(s=>s.assignment).length,0);
  await p.locator('#redo').click();assert.deepEqual(await saved(p),working);

  const bads=[];
  const wrong=structuredClone(exported);wrong.catalogFingerprint='old';bads.push(wrong);
  const repeated=structuredClone(exported);repeated.slots[1].assignment=repeated.slots[0].assignment;bads.push(repeated);
  const unknown=structuredClone(exported);unknown.slots[0].assignment.cardId='missing';bads.push(unknown);
  const badSlot=structuredClone(exported);badSlot.slots[1].id='bad-id';bads.push(badSlot);
  const badTier=structuredClone(exported);badTier.slots[0].tier=10;bads.push(badTier);
  const badPreset=structuredClone(exported);badPreset.slots[0].preset='missing';bads.push(badPreset);
  const badAffinity=structuredClone(exported);badAffinity.slots[0].affinities=['invented'];bads.push(badAffinity);
  const missingGroup=structuredClone(all);missingGroup.groups.pop();bads.push(missingGroup);
  const crossGroup=structuredClone(all);crossGroup.groups[0].slots[0].assignment=crossGroup.groups[2].slots[0].assignment;bads.push(crossGroup);
  for(const bad of bads){await importFile(p,bad,false);assert.deepEqual(await saved(p),working);}
  const altered=structuredClone(exported);altered.entries[0].before.name='Forged';altered.entries[0].after.art='https://invalid.example/art.png';
  await importFile(p,altered);assert.equal((await exportFile(p)).entries[0].before.name,'Guar Stablemaster');

  // Confirmed, complete groups can be exported independently of unfinished groups.
  const complete=structuredClone(exported);complete.slots=complete.slots.filter(s=>s.assignment);complete.slots.forEach(s=>s.confirmed=true);
  await importFile(p,complete);assert.equal((await exportFile(p)).readyForImplementation,true);
  assert.equal((await exportFile(p,true)).readyForImplementation,false);
  await importFile(p,exported);
  await p.locator('#summary-open').click();
  assert.match(await p.locator('#summary-content').innerText(),/slots added/);
  assert.equal(await p.locator('.group-summary tr').count(),18);
  await p.screenshot({path:path.join(output,'roster-summary.png')});await p.keyboard.press('Escape');
  await p.locator('#library-search').fill('Dres Spy');
  await p.getByRole('button',{name:'View original Dres Spy',exact:true}).click();
  await p.locator('#preview-art img').evaluate(i=>i.decode());assert.match(await p.locator('#preview-info').innerText(),/Original card/);await p.keyboard.press('Escape');

  // The reusable card back creates independent identities; slot values stay put.
  const beforeNew=await saved(p), beforeNewGroup=await group(p);
  await p.getByRole('button',{name:'Select new card',exact:true}).click();await slot(p,first.id).locator('.slot-face').click();
  const newFirst=(await group(p)).slots[0];
  assert.equal(newFirst.assignment.kind,'new');assert.match(newFirst.assignment.cardId,/^new-[0-9a-f]{32}$/);
  assert.deepEqual(values(newFirst),values(beforeNewGroup.slots[0]));
  assert(await slot(p,first.id).locator('.slot-name').evaluate(n=>n===document.activeElement));
  assert(await slot(p,first.id).locator('.slot-confirm').isDisabled());
  await slot(p,first.id).locator('.slot-name').fill('Velothi Star Reader');
  await slot(p,first.id).locator('.slot-confirm').click();
  assert((await group(p)).slots[0].confirmed);
  await p.locator('#new-card').dragTo(slot(p,second.id).locator('.slot-face'));
  await slot(p,second.id).locator('.slot-name').fill('Ghostgate Pilgrim');
  let created=await group(p);
  assert.notEqual(created.slots[0].assignment.cardId,created.slots[1].assignment.cardId);
  assert.equal(created.slots[0].assignment.name,'Velothi Star Reader');
  assert.deepEqual(values(created.slots[1]),values(beforeNewGroup.slots[1]));
  await slot(p,first.id).locator('img').evaluate(i=>i.decode());
  assert((await slot(p,first.id).locator('img').getAttribute('src')).endsWith('backs/mosaic.webp'));
  await slot(p,first.id).locator('.randomize-card').click();created=await group(p);
  assert.equal(created.slots[0].assignment.cardId,newFirst.assignment.cardId);
  await slot(p,first.id).locator('.slot-face').dragTo(slot(p,second.id).locator('.slot-face'));
  assert.equal((await group(p)).slots[1].assignment.name,'Velothi Star Reader');
  assert.deepEqual(values((await group(p)).slots[1]),values(created.slots[1]));
  await slot(p,second.id).locator('.slot-face').click();await slot(p,created.slots[2].id).locator('.slot-face').click();
  assert.equal((await group(p)).slots[2].assignment.name,'Velothi Star Reader');
  await p.locator('#undo').click();await p.locator('#undo').click();assert.deepEqual(await group(p),created);
  await p.locator('#suggested').click();assert.deepEqual((await group(p)).slots.slice(0,2),created.slots.slice(0,2));
  const newState=await saved(p), newExport=await exportFile(p), newWorkshop=await exportFile(p,true);
  const newEntry=newExport.entries.find(e=>e.cardId===newFirst.assignment.cardId);
  assert.equal(newEntry.kind,'new');assert.equal(newEntry.source,null);assert.equal(newEntry.before,null);
  assert.equal(newEntry.after.art,null);assert.equal(newEntry.needsArtwork,true);assert.equal(newExport.pendingArtwork.length,2);
  assert.equal(newEntry.after.name,'Velothi Star Reader');
  await p.reload();assert.deepEqual(await saved(p),newState);
  await p.locator('#clear-layout').click();await importFile(p,newWorkshop);assert.deepEqual(await saved(p),newState);
  const newComplete=structuredClone(newExport);newComplete.slots=newComplete.slots.filter(s=>s.assignment?.kind==='new');newComplete.slots.forEach(s=>s.confirmed=true);
  await importFile(p,newComplete);const pendingArt=await exportFile(p);
  assert.equal(pendingArt.complete,true);assert.equal(pendingArt.confirmedCount,2);assert.equal(pendingArt.readyForImplementation,false,'Card backs are never exported as finished artwork');
  await p.locator('#summary-open').click();assert.match(await p.locator('#summary-content').innerText(),/2 awaiting artwork: Velothi Star Reader, Ghostgate Pilgrim/);await p.keyboard.press('Escape');
  await p.screenshot({path:path.join(output,'roster-new-cards.png')});
  const malformedNew=structuredClone(newComplete);malformedNew.slots[0].assignment.cardId=card('Dres Spy').id;
  await importFile(p,malformedNew,false);assert.equal((await group(p)).slots.length,2);
  const missingKind=structuredClone(newComplete);delete missingKind.slots[0].assignment.kind;await importFile(p,missingKind,false);
  await importFile(p,beforeNew);
  for(const width of [390,768,1920]){
   await p.setViewportSize({width,height:1000});
   for(const id of ['dark-elf-v2','dragon-v2','god-v2']){
    await p.locator('#group-select').selectOption(id);
   assert(await p.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),width+' '+id+' should fit');
    assert(await p.locator('.randomize-tier').first().isVisible());
    assert(await p.locator('#new-card').isVisible());
   }
   await p.locator('#group-select').selectOption(template.id);
   if(width===390){
    await selectCard(p,'Dres Spy');await slot(p,first.id).locator('.slot-face').click();
    assert.equal((await group(p)).slots[0].assignment.cardId,card('Dres Spy').id);
    await p.screenshot({path:path.join(output,'roster-mobile.png'),fullPage:true});
   }
  }
  // The imported full roster round-trips with custom slots and unchanged values.
  await importFile(p,populated);
  assert.deepEqual(await saved(p),populated);
  await p.reload();assert.deepEqual(await saved(p),populated);
  await p.evaluate(k=>localStorage.setItem(k,'broken draft text'),key);await p.reload();
  assert(await p.locator('#recover-save').isVisible());
  assert(await p.evaluate(k=>Object.keys(localStorage).some(x=>x.startsWith(k+'-recovery-')&&localStorage.getItem(x)==='broken draft text'),key));
  assert.equal(await p.evaluate(k=>localStorage.getItem(k),auditKey),'preserved-review');
  assert.deepEqual(errors,[]);assert.deepEqual(failed,[]);assert.deepEqual(external,[]);
  console.log('Roster workshop: card/row randomization, rarity budgets, undo, 18 groups, slot editing, drag/swap, identity uniqueness, confirmation, export/import, recovery and responsive offline UI passed.');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exit(1);});
