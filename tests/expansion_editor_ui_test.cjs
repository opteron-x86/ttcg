const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const {pathToFileURL}=require('node:url'),{chromium}=require('playwright');
const zip=require('../tools/expansion-editor/vendor/fflate-0.8.3.js');
const rankFactory=require('../tools/catalog-audit/rank-tools.js'),coreFactory=require('../tools/expansion-editor/core.js');
const out=path.resolve('build/expansion-editor'),html=fs.readFileSync('docs/expansion-editor.html','utf8');
const data=JSON.parse(html.match(/<script type="application\/json" id="editor-data">([\s\S]*?)<\/script>/)[1]);
const core=coreFactory(data,rankFactory);fs.mkdirSync(out,{recursive:true});
const url=pathToFileURL(path.resolve('docs/expansion-editor.html')).href;
const imageRoot=path.resolve('examples/card-expansion/TesseraUI/art/expansions/tessera-example');
function snapshot(p){return p.evaluate(()=>new Promise((resolve,reject)=>{const r=indexedDB.open('tessera-expansion-workshop',1);r.onsuccess=()=>{const db=r.result,t=db.transaction('state','readonly'),q=t.objectStore('state').get('current');q.onsuccess=()=>{db.close();resolve(q.result.doc);};q.onerror=()=>reject(q.error);};}));}
async function settled(p){await p.waitForFunction(()=>document.getElementById('save-status').textContent==='Draft saved in this browser');}
async function download(p,action){const wait=p.waitForEvent('download');await action();const d=await wait,filename=path.join(out,d.suggestedFilename());await d.saveAs(filename);await p.waitForFunction(()=>document.getElementById('busy').hidden);return filename;}
async function openFile(p,file,replace=true){await p.locator('#project-files').setInputFiles(file);await p.waitForFunction(()=>document.getElementById('busy').hidden);if(replace)await p.getByRole('button',{name:'Replace draft',exact:true}).click();await settled(p);}
async function addCard(p,{name,group='Dark Elf',tier='1',rarity='Common'}){await p.locator('#add-card').click();await p.locator('#new-group').selectOption(group);await p.locator('#new-tier').selectOption(tier);await p.locator('#new-rarity').selectOption(rarity);await p.locator('#new-card-name').fill(name);await p.locator('#modal').getByRole('button',{name:'Add card',exact:true}).click();}
async function clickSlot(p,name){await p.getByRole('button',{name:'Edit '+name,exact:true}).click();}

// Suggested values still use the authoritative profiles, without making empty cards.
for(const group of data.groups)for(const row of core.curve(group)){
 const c=core.newCard(row.tier,core.suggestedRarity(row.tier,group),group);
 assert(core.ranks.validRanks(c),group+JSON.stringify(c));assert(core.ranks.validRanks({...c,sides:core.ranks.pickRandomSides(c)}));
}
assert.deepEqual(core.curve('Dark Elf').map(r=>r.tier),[1,2,3,4,5,6,7,8,9]);
assert(core.curve('God').every(r=>core.suggestedRarity(r.tier,'God')==='Legendary'&&r.tier>=9));
assert.throws(()=>core.parseJSON('{"name":"first","name":"last"}'),/Duplicate/);
assert.throws(()=>core.parseJSON('{"cards":[{"name":"first","name":"last"}]}'),/Duplicate/);
assert.throws(()=>core.validateManifest({schemaVersion:1,setId:'x',name:'X',cards:[],bogus:true}),/unsupported field/);

(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const context=await browser.newContext({viewport:{width:1600,height:1100},reducedMotion:'reduce'}),p=await context.newPage();
  const errors=[],external=[],failed=[];p.on('pageerror',e=>errors.push(e.message));p.on('request',r=>{if(/^https?:/.test(r.url()))external.push(r.url());});p.on('requestfailed',r=>failed.push(r.url()));
  await p.goto(url);await settled(p);
  await p.locator('#set-name').fill('Ash & Ember');await p.locator('#set-name').blur();await settled(p);assert.equal((await snapshot(p)).setId,'ash-ember');
  for(const group of data.groups){
   await p.locator('#group-filter').selectOption(group);
   assert.equal(await p.locator('.tier-section').count(),core.curve(group).length);
   assert.equal(await p.locator('.add-card-target').count(),core.curve(group).length);
   assert.equal(await p.locator('.slot').count(),0,'Opening a group creates no blank cards');
  }
  assert.equal((await snapshot(p)).cards.length,0);
  await p.locator('#group-filter').selectOption('Dark Elf');
  const oneImage={name:'Test Artisan.png',mimeType:'image/png',buffer:fs.readFileSync(path.join(imageRoot,'clockwork-envoy.png'))};
  await p.locator('#art-files').setInputFiles(oneImage);await p.waitForFunction(()=>document.getElementById('message').textContent.startsWith('Imported 1'));await settled(p);
  await p.locator('#tier-4 .tier-rarity').selectOption('Epic');
  await p.getByRole('button',{name:'Select artwork Test Artisan.png',exact:true}).dragTo(p.locator('#tier-4 .add-card-target'));await settled(p);
  let created=(await snapshot(p)).cards;assert.equal(created.length,1);assert.equal(created[0].tier,4);assert.equal(created[0].rarity,'Epic');assert.deepEqual(created[0].groups,['Dark Elf']);assert.equal(created[0].name,'Test Artisan');assert(created[0].asset);assert(core.ranks.validRanks(created[0]));
  await p.getByRole('button',{name:'Select artwork Test Artisan.png',exact:true}).click();await p.locator('#tier-4 .add-card-target').click();await settled(p);
  created=(await snapshot(p)).cards;assert.equal(created.length,2);assert.notEqual(created[0].id,created[1].id);
  const before=await snapshot(p);
  await p.locator('#tier-4').getByRole('button',{name:'Randomize row',exact:true}).click();await settled(p);const after=await snapshot(p);
  for(let i=0;i<2;i++){assert.notDeepEqual(after.cards[i].sides,before.cards[i].sides);assert(core.ranks.validRanks(after.cards[i]));}
  await p.locator('#undo').click();await settled(p);assert.deepEqual(await snapshot(p),before);
  await p.locator('#redo').click();await settled(p);assert.deepEqual(await snapshot(p),after);
  // Desktop image drops create a card and import its art in one undoable action.
  await p.locator('#group-filter').selectOption('God');
  const desktopDrop=await p.evaluateHandle(bytes=>{const dt=new DataTransfer();dt.items.add(new File([new Uint8Array(bytes)],'Divine.png',{type:'image/png'}));return dt;},[...oneImage.buffer]);
  await p.locator('#tier-10 .add-card-target').dispatchEvent('drop',{dataTransfer:desktopDrop});await p.waitForFunction(()=>document.getElementById('busy').hidden);await settled(p);
  created=(await snapshot(p)).cards;assert.equal(created.length,3);assert.equal(created[2].tier,10);assert.deepEqual(created[2].groups,['God']);assert.equal(created[2].rarity,'Legendary');assert(created[2].unique);assert.deepEqual(created[2].acquisition.tournamentPrizes,['masters']);
  await p.locator('#undo').click();await settled(p);assert.deepEqual(await snapshot(p),after,'Undo removes the created card and imported artwork together');
  const invalidDrop=await p.evaluateHandle(()=>{const dt=new DataTransfer();dt.items.add(new File(['not an image'],'broken.png',{type:'image/png'}));return dt;});
  await p.locator('#tier-10 .add-card-target').dispatchEvent('drop',{dataTransfer:invalidDrop});await p.waitForFunction(()=>document.getElementById('message').textContent.startsWith('Imported 0'));assert.equal((await snapshot(p)).cards.length,2,'Failed images never leave a blank card behind');
  await p.locator('#group-filter').selectOption('Dark Elf');await p.locator('#tier-2 .add-card-target').click();await p.locator('#modal').getByRole('button',{name:'Add card',exact:true}).click();await settled(p);
  assert.equal((await snapshot(p)).cards.length,3,'Clicking a target adds only the requested slot');
  await p.locator('#remove-empty').click();await settled(p);assert.equal((await snapshot(p)).cards.length,2,'One action clears only unnamed, unassigned slots');
  await p.locator('#undo').click();await settled(p);assert.equal((await snapshot(p)).cards.length,3);await p.locator('#redo').click();await settled(p);assert.equal((await snapshot(p)).cards.length,2);
  await p.reload();await settled(p);assert.equal((await snapshot(p)).cards.length,2,'Empty tier rows do not reappear as cards after reload');
  await p.locator('#roster').evaluate(e=>e.scrollTop=0);await p.screenshot({path:path.join(out,'empty-tier-rows.png')});

  // Start the export fixture through the actual user controls.
  await p.locator('#new-project').click();await p.getByRole('button',{name:'Replace draft',exact:true}).click();await settled(p);
  await p.locator('#set-name').fill('Ash & Ember');await p.locator('#set-name').blur();
  await addCard(p,{name:'Ashlander Scout'});await addCard(p,{name:'Ashlander Scout-thumb',tier:'4',rarity:'Epic'});
  await addCard(p,{name:'Champion of the Ashlands',tier:'8',rarity:'Legendary'});await settled(p);
  const art=[['roadside-courier.png','Ashlander Scout.png'],['grove-sentinel.png','Ashlander Scout-thumb.jpg'],['clockwork-envoy.png','Craftsman.png']].map(([src,name])=>({name,mimeType:'image/png',buffer:fs.readFileSync(path.join(imageRoot,src))}));
  await p.locator('#art-files').setInputFiles(art);await p.waitForFunction(()=>document.getElementById('message').textContent.startsWith('Imported 3'));await settled(p);
  await p.locator('#match-art').click();await settled(p);assert.equal((await snapshot(p)).cards.filter(c=>c.asset).length,2);
  const beforeAssign=await snapshot(p),first=beforeAssign.cards[0];
  // Art drag changes only image assignment; existing name/id/strength are retained.
  await p.getByRole('button',{name:'Select artwork Craftsman.png',exact:true}).dragTo(p.locator('[data-card-id="'+first.uid+'"] .slot-pick'));await settled(p);
  const assigned=(await snapshot(p)).cards[0];assert.equal(assigned.id,first.id);assert.equal(assigned.name,first.name);assert.deepEqual(assigned.sides,first.sides);assert.notEqual(assigned.asset,first.asset);
  await clickSlot(p,'Ashlander Scout');await p.locator('#card-name').fill('Ashlander Envoy');await p.locator('#card-name').blur();await settled(p);assert.equal((await snapshot(p)).cards[0].id,'ashlander-scout','Renames preserve released identity');
  await p.locator('#crop-x').fill('30');await p.locator('#crop-y').fill('65');await p.locator('#crop-zoom').fill('1.3');await settled(p);
  await p.locator('#side-0').fill('10');await p.locator('#side-0').blur();await p.locator('#export').click();assert(await p.locator('#download-expansion').isDisabled());await p.locator('#close-modal').click();
  await p.locator('#balance').selectOption('custom');await p.locator('#export').click();assert(await p.locator('#download-expansion').isEnabled());await p.locator('#close-modal').click();
  await p.locator('#balance').selectOption('tessera');await p.getByRole('button',{name:'Suggested values',exact:true}).click();await settled(p);
  const state=await snapshot(p);assert.deepEqual(state.cards[0].crop,{x:30,y:65,zoom:1.3});assert.equal(state.cards[2].acquisition.tournamentPrizes[0],'invitational');
  const projectFile=await download(p,()=>p.locator('#save-project').click()),projectZip=zip.unzipSync(fs.readFileSync(projectFile));assert(projectZip['project.json']);assert.equal(Object.keys(projectZip).length,4);
  await p.reload();await settled(p);assert.deepEqual(await snapshot(p),state,'Metadata, original images and crops survive reload');
  await p.locator('#export').click();const modFile=await download(p,()=>p.locator('#download-expansion').click()),modZip=zip.unzipSync(fs.readFileSync(modFile));
  const manifestPath='SKSE/Plugins/Tessera/Expansions/ash-ember.json',manifest=JSON.parse(new TextDecoder().decode(modZip[manifestPath]));
  assert.equal(manifest.cards.length,3);assert.equal(Object.keys(modZip).length,5);assert(!('uid' in manifest.cards[0]));assert(!('asset' in manifest.cards[0]));assert(!('art' in manifest.cards[2]));assert(!Object.keys(modZip).some(k=>/project|README|\.png$|\\/.test(k)));
  core.validateManifest(manifest);
  for(const c of manifest.cards.filter(c=>c.art))for(const key of ['art','thumbnail'])assert(modZip['TesseraUI/art/'+c[key]],'Every referenced image is packaged');
  // Names ending in -thumb cannot collide with another card's thumbnail.
  assert.notEqual(manifest.cards[0].thumbnail,manifest.cards[1].art);
  const dimensions=await p.evaluate(async entries=>{const sizes=[];for(const [name,bytes] of entries){const b=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/webp'}));sizes.push([name,b.width,b.height]);b.close();}return sizes;},Object.entries(modZip).filter(([k])=>k.endsWith('.webp')).map(([k,b])=>[k,[...b]]));
  for(const [name,w,h] of dimensions)assert.deepEqual([w,h],name.includes('/thumbs/')?[240,320]:[768,1024]);
  const install=path.join(out,'install');fs.rmSync(install,{force:true,recursive:true});for(const [name,bytes] of Object.entries(modZip)){const dest=path.join(install,name);fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,bytes);}
  cp.execFileSync('c++',['-std=c++20','-O0','-Iinclude','-Ibuild/vcpkg_installed/x64-windows-clangcl/include','tests/expansion_editor_host.cpp','-o',path.join(out,'loader')],{stdio:'pipe'});
  assert.match(cp.execFileSync(path.join(out,'loader'),[install],{encoding:'utf8'}),/3 cards loaded/);

  await openFile(p,modFile);const roundTrip=await snapshot(p);assert.deepEqual(core.manifest(roundTrip),manifest);assert.equal(roundTrip.assets.length,2);assert.equal(roundTrip.cards.filter(c=>c.missingArt).length,0);
  const legacyFile=path.join(out,'legacy-expansion.zip');
  const legacyEntries=Object.fromEntries(Object.entries(modZip).map(([name,bytes])=>[name.replace('TesseraUI/','MeridianUI/ttcg/'),bytes]));
  fs.writeFileSync(legacyFile,zip.zipSync(legacyEntries));await openFile(p,legacyFile);
  const migrated=await snapshot(p);assert.deepEqual(core.manifest(migrated),manifest);assert.equal(migrated.cards.filter(c=>c.missingArt).length,0);
  await p.locator('#export').click();const migratedFile=await download(p,()=>p.locator('#download-expansion').click());
  assert(!Object.keys(zip.unzipSync(fs.readFileSync(migratedFile))).some(k=>k.includes('MeridianUI')),'Legacy imports export only TesseraUI');
  await openFile(p,projectFile);assert.deepEqual(await snapshot(p),state,'Project backups retain crops, artwork library and stable IDs');
  // Bad manifests and incomplete projects never replace the draft.
  const invalid={...manifest,unknown:true};await p.locator('#project-files').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(invalid))});await p.waitForFunction(()=>document.getElementById('message').classList.contains('error'));assert.deepEqual(await snapshot(p),state);
  await p.locator('#project-files').setInputFiles({name:'duplicate.json',mimeType:'application/json',buffer:Buffer.from('{"schemaVersion":1,"setId":"x","setId":"y","name":"X","cards":[]}')});await p.waitForFunction(()=>document.getElementById('message').textContent.includes('Duplicate JSON'));assert.deepEqual(await snapshot(p),state);
  const missingProject={...projectZip};delete missingProject[Object.keys(missingProject).find(k=>k.startsWith('artwork/'))];const broken=path.join(out,'broken-project.zip');fs.writeFileSync(broken,zip.zipSync(missingProject));await p.locator('#project-files').setInputFiles(broken);await p.waitForFunction(()=>document.getElementById('message').textContent.includes('missing'));assert.deepEqual(await snapshot(p),state);

  // JSON-only import flags missing references, instead of silently packaging broken paths.
  const manifestFile=path.join(out,'manifest.json');fs.writeFileSync(manifestFile,JSON.stringify(manifest));await openFile(p,manifestFile);let missing=await snapshot(p);assert.equal(missing.cards.filter(c=>c.missingArt).length,2);
  await p.locator('#export').click();assert(await p.locator('#download-expansion').isDisabled());await p.locator('#close-modal').click();
  for(const c of missing.cards.filter(c=>c.missingArt)){await clickSlot(p,c.name);await p.getByRole('button',{name:'Use card-back placeholder',exact:true}).click();}await settled(p);assert.equal(core.issues(await snapshot(p)).filter(i=>i.level==='error').length,0);
  await openFile(p,projectFile);await clickSlot(p,'Champion of the Ashlands');await p.locator('#inspector').evaluate(el=>el.scrollTop=el.scrollHeight);await p.screenshot({path:path.join(out,'workshop.png')});
  await p.setViewportSize({width:1100,height:800});assert(await p.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await p.setViewportSize({width:1600,height:1100});
  // A stale second tab must not overwrite a newer draft.
  const second=await context.newPage();await second.goto(url);await settled(second);await p.locator('#set-name').fill('Ash & Ember — revised');await p.locator('#set-name').blur();await settled(p);
  await second.locator('#set-name').fill('Stale tab');await second.waitForFunction(()=>document.getElementById('save-status').textContent.includes('Another tab'));
  assert.equal((await snapshot(p)).name,'Ash & Ember — revised');await second.close({runBeforeUnload:false});
  assert.deepEqual(errors,[]);assert.deepEqual(external,[]);assert.deepEqual(failed,[]);
  console.log('Expansion Workshop: empty group tiers, art-to-tier creation, click/desktop drops, no blank cards on failed import, one-action cleanup, shared strength rules, randomization/undo, art matching/crops, autosave/reload, project and mod ZIP round trips, strict validation, native loader, responsive layout, offline assets and stale-tab protection passed.');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
