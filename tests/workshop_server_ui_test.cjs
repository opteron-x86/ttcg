const assert = require('node:assert/strict');
const fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const {spawn} = require('node:child_process'), {once} = require('node:events');
const {pathToFileURL} = require('node:url');
const {chromium} = require(process.env.TTCG_PLAYWRIGHT || 'playwright');
const dir = fs.mkdtempSync(path.join(os.tmpdir(),'tessera-workshop-test-'));
const output = path.resolve('build/catalog-audit'); fs.mkdirSync(output,{recursive:true});
const data = JSON.parse(fs.readFileSync('docs/catalog-audit-data.json','utf8'));
const t = data.templates.find(t=>t.id==='dark-elf-v2'), first=t.slots[0].id;
const prefix = 'tessera-roster-workshop-v2-' + data.fingerprint + '-shared-draft-';
const cardSlot = (p,id=first)=>p.locator('[data-slot-id="'+id+'"]');
const current = doc=>doc.groups.find(g=>g.template.id===t.id);
let child, port, base, logs='';
async function start(requested=0) {
 child=spawn('python3',['tools/serve-roster-workshop.py','--port',String(requested),'--state-dir',dir],{stdio:['ignore','pipe','pipe']});
 child.stderr.on('data',s=>{logs=(logs+s).slice(-6000);});
 await new Promise((resolve,reject)=>{
  let text=''; const timeout=setTimeout(()=>reject(Error('Server startup timeout: '+logs)),10000);
  child.once('exit',code=>{clearTimeout(timeout);reject(Error('Server exited: '+code+' '+logs));});
  child.stdout.on('data',s=>{text+=s;const match=text.match(/listening on 0\.0\.0\.0:(\d+)/);if(match){clearTimeout(timeout);port=Number(match[1]);resolve();}});
 });
 base='http://127.0.0.1:'+port;
}
async function stop() {if(child && child.exitCode===null){const exit=once(child,'exit');child.kill('SIGTERM');await exit;}}
async function host() {return (await fetch(base+'/api/workshop')).json();}
async function saved(p) {await p.waitForFunction(()=>document.querySelector('#save-status').textContent==='All groups saved to shared workshop');return host();}
async function exported(p) {
 const event=p.waitForEvent('download');await p.locator('#export').click();const file=await event;
 const target=path.join(dir,'export-'+Date.now()+'.json');await file.saveAs(target);return JSON.parse(fs.readFileSync(target,'utf8'));
}
async function imported(p,doc) {
 await p.locator('#import-file').setInputFiles({name:'workshop.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(doc))});
 await p.waitForFunction(()=>document.querySelector('#message').textContent.startsWith('Imported'));await saved(p);
}
async function main() {
 await start();
 const address=Object.values(os.networkInterfaces()).flat().find(a=>a.family==='IPv4'&&!a.internal&&/^(192\.168\.|10\.|172\.(1[6-9]|2\d|3[01])\.)/.test(a.address))?.address;
 assert(address,'Use a real LAN address to exercise an insecure HTTP context');
 const url='http://'+address+':'+port+'/';
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 const errors=[];
 try {
  const local=await browser.newPage({viewport:{width:1600,height:1080}});
  await local.goto(pathToFileURL(path.resolve('docs/catalog-template-builder.html')).href);
  const blank=structuredClone(data.initialWorkshop);blank.currentGroup=t.id;
  blank.groups=blank.groups.map(g=>({...g,slots:data.templates.find(t=>t.id===g.template.id).slots.map(s=>({...s,assignment:null,confirmed:false}))}));
  await local.locator('#import-file').setInputFiles({name:'blank.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(blank))});
  await local.waitForFunction(()=>document.querySelector('#message').textContent.startsWith('Imported'));

  await local.locator('#library-search').fill('Guar Stablemaster');
  await local.getByRole('button',{name:'Select Guar Stablemaster',exact:true}).click();
  await cardSlot(local).locator('.slot-face').click();
  await cardSlot(local).locator('.slot-name').fill('LAN Guar Keeper');
  const seed=await exported(local);
  const contexts=await Promise.all([browser.newContext(),browser.newContext()]);
  const [p,q]=await Promise.all(contexts.map(c=>c.newPage()));
  for(const page of [p,q]){page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());await page.setViewportSize({width:1600,height:1080});}
  await p.goto(url);await p.waitForFunction(()=>document.querySelector('#save-status').textContent==='Shared workshop ready');
  assert.equal(await p.evaluate(()=>window.isSecureContext),false,'Exercise LAN HTTP, not trusted localhost');
  assert.equal((await host()).workshop,null,'Opening a page must not commit blank templates');
  await imported(p,seed);
  await p.locator('#tier-1 .add-slot').click();await saved(p);
  assert.equal(current((await host()).workshop).slots.length,41,'Custom IDs work over LAN HTTP');
  const placeholder=t.slots[1].id;
  await p.locator('#new-card').click();await cardSlot(p,placeholder).locator('.slot-face').click();
  await cardSlot(p,placeholder).locator('.slot-name').fill('Ashlander Astronomer');
  await cardSlot(p,placeholder).locator('.slot-confirm').click();await saved(p);
  const invented=current((await host()).workshop).slots.find(s=>s.id===placeholder);
  assert.equal(invented.assignment.kind,'new');assert.equal(invented.confirmed,true);
  await q.goto(url);await saved(q);
  assert.equal(await cardSlot(q).locator('.slot-name').inputValue(),'LAN Guar Keeper');
  assert.equal(await q.locator('.template-slot').count(),41);
  assert.equal(await cardSlot(q,placeholder).locator('.slot-name').inputValue(),'Ashlander Astronomer');
  await cardSlot(q,placeholder).locator('img').evaluate(i=>i.decode());
  assert((await cardSlot(q,placeholder).locator('img').getAttribute('src')).endsWith('backs/mosaic.webp'));
  await q.locator('#library-grid img').evaluateAll(imgs=>Promise.all(imgs.slice(0,4).map(i=>i.decode())));
  await cardSlot(p).locator('.slot-name').fill('Shared Guar Keeper');await p.locator('#template-title').evaluate(n=>n.focus());await saved(p);
  await q.waitForFunction(id=>document.querySelector('[data-slot-id="'+id+'"] .slot-name').value==='Shared Guar Keeper',first);

  // Hold one browser on an older revision; its write must never overwrite the host.
  await q.route('**/api/workshop',route=>route.request().method()==='GET'?route.fulfill({status:304,body:''}):route.continue());
  await cardSlot(p).locator('.randomize-card').click();const newest=await saved(p);
  await q.locator('#tier-2 .randomize-tier').click();
  await q.waitForFunction(()=>document.querySelector('#save-status').textContent==='Shared save has newer changes');
  assert.equal((await host()).revision,newest.revision);
  const unsaved=await exported(q);
  assert.notDeepEqual(current(unsaved).slots,current(newest.workshop).slots);
  assert((await q.evaluate(prefix=>Object.keys(localStorage).filter(k=>k.startsWith(prefix)).length,prefix))>0);
  await q.unroute('**/api/workshop');
  await q.getByRole('button',{name:'Load shared version',exact:true}).click();await saved(q);
  assert.deepEqual(current(await exported(q)).slots,current(newest.workshop).slots);
  assert(await q.locator('#shared-recovery').isVisible());
  await q.locator('#shared-recovery summary').click();await q.getByRole('button',{name:'Resume draft',exact:true}).click();
  assert.equal(await q.locator('#save-status').textContent(),'Shared save has newer changes','An older recovery never force-saves');
  await q.getByRole('button',{name:'Load shared version',exact:true}).click();await saved(q);

  // Survive an unavailable server and a reload without losing the queued draft.
  await p.route('**/api/workshop',route=>route.abort());
  await cardSlot(p).locator('.randomize-card').click();
  await p.waitForFunction(()=>document.querySelector('#save-status').textContent==='Not saved to shared workshop');
  const offline=await exported(p);
  await p.reload();await p.waitForFunction(()=>document.querySelector('#save-status').textContent==='Waiting for shared workshop');
  await p.unroute('**/api/workshop');await p.getByRole('button',{name:'Retry connection',exact:true}).click();await saved(p);
  await p.locator('#shared-recovery summary').click();await p.getByRole('button',{name:'Resume draft',exact:true}).click();await saved(p);
  assert.deepEqual(current((await host()).workshop).slots,current(offline).slots);

  // A committed request with a lost response is retried without a false conflict.
  let drop=true;
  await p.route('**/api/workshop',async route=>{
   if(route.request().method()==='PUT'&&drop){drop=false;await route.fetch();await route.abort();}else await route.continue();
  });
  await cardSlot(p).locator('.randomize-card').click();
  await p.waitForFunction(()=>document.querySelector('#save-status').textContent==='Not saved to shared workshop');
  const committed=await host();
  await p.getByRole('button',{name:'Retry save',exact:true}).click();await saved(p);
  assert.equal((await host()).revision,committed.revision);
  await p.unroute('**/api/workshop');
  assert(fs.readdirSync(path.join(dir,'backups')).filter(f=>f.endsWith('.json')).length>2);

  // Restart the host using the same saved state and URL.
  await stop();await start(port);await p.reload();await saved(p);
  assert.deepEqual(current((await host()).workshop).slots,current(committed.workshop).slots);
  await p.screenshot({path:path.join(output,'roster-workshop-lan.png')});
  await p.setViewportSize({width:390,height:844});
  assert(await p.locator('#save-status').isVisible());
  assert(await p.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  assert.deepEqual(errors,[]);
  console.log('Shared workshop: LAN HTTP, import, two browsers, live updates, stale writes, recovery, reconnect, retry, backups and restart passed.');
 }finally {await browser.close();}
}
main().catch(error=>{console.error(error);console.error(logs);process.exitCode=1;}).finally(async()=>{await stop();fs.rmSync(dir,{recursive:true,force:true});});
