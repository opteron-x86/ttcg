const {chromium}=require('playwright'),{spawnSync}=require('node:child_process');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const result=spawnSync('/tmp/ttcg-card-expansions-host',{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(result.status,0,result.stderr);
 const packet=JSON.parse(result.stdout),root=path.resolve('TesseraUI');let blocked=0,reads=0;
 const server=http.createServer((req,res)=>{
  try{
   const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
   // Reproduce a browser that cannot see art in a separate physical mod root.
   if(name.startsWith('/art/expansions/')){blocked++;res.writeHead(404).end();return;}
   const file=path.resolve(root,'.'+name);if(!file.startsWith(root+path.sep))throw Error('path');
   const mime={'.html':'text/html','.js':'text/javascript','.css':'text/css','.png':'image/png','.webp':'image/webp','.svg':'image/svg+xml'}[path.extname(file)]||'application/octet-stream';
   res.setHeader('Content-Type',mime);res.end(fs.readFileSync(file));
  }catch{res.writeHead(404).end();}
 });await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',()=>{});
  await page.exposeFunction('ttcgReadArt',async request=>{
   const [id,file]=request.split('\n');reads++;
   const result=spawnSync('/tmp/ttcg-card-expansions-host',['--art',file],{encoding:'utf8',maxBuffer:24*1024*1024});assert.equal(result.status,0,result.stderr);
   await page.evaluate(reply=>ttcgArtResult(reply),{id:Number(id),...JSON.parse(result.stdout)});
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);await page.evaluate(s=>ttcgState(s),packet);
  assert.match(await page.locator('#album-count').innerText(),/Tessera 8\/703 · Expansions 5\/5/);
  await page.locator('#album-filters-toggle').click();await page.locator('#album-set-trigger').click();
  await page.getByRole('option',{name:'Example Expansion',exact:true}).click();await page.locator('#album-filters-done').click();
  assert.equal(await page.locator('#album-grid .collection-card').count(),6);
  const withArt=packet.cards.findIndex(c=>c.id==='tessera-example:grove-sentinel'&&!c.foil);
  await page.locator(`#album-card-${withArt}`).click();
  await page.waitForFunction(()=>document.querySelector('#inspect-card img')?.naturalWidth>0);
  assert.match(await page.locator('#inspect-card img').getAttribute('src'),/^data:image\/png;base64,/);
  assert.match(await page.locator('#inspect-metadata').innerText(),/Spirit/);assert.match(await page.locator('#inspect-metadata').innerText(),/Example Expansion/);
  const placeholder=packet.cards.findIndex(c=>c.id==='tessera-example:unpainted-wanderer'&&!c.foil);
  await page.locator(`#album-card-${placeholder}`).click();await page.waitForFunction(()=>document.querySelector('#inspect-card img')?.naturalWidth>0);
  assert.match(await page.locator('#inspect-card img').getAttribute('src'),/backs\/mosaic.webp/);
  // Existing filters still combine with set membership; clearing returns core cards.
  await page.locator('#album-filters-toggle').click();await page.locator('#album-rarity-trigger').click();await page.getByRole('option',{name:'Legendary',exact:true}).click();await page.locator('#album-filters-done').click();
  assert.equal(await page.locator('#album-grid .collection-card').count(),1);
  await page.locator('#album-filters-clear').click();assert.equal(await page.locator('#album-grid .collection-card').count(),14);
  const missing=structuredClone(packet);missing.collection.missingSets=['removed-set'];await page.evaluate(s=>ttcgState(s),missing);
  assert.match(await page.locator('#album-status').innerText(),/Reinstall the expansion/);
  const broken=structuredClone(packet);broken.expansionErrors=['broken.json: unknown group'];await page.evaluate(s=>ttcgState(s),broken);
  assert.match(await page.locator('#album-status').innerText(),/could not be loaded/);
  assert.match(await page.locator('#album-status').getAttribute('title'),/broken.json/);
  // Use the actual expansion prize draw and native award packet in both the
  // tournament detail and its victory popup, including placeholder artwork.
  const prizeResult=spawnSync('/tmp/ttcg-card-expansions-host',['--trophy'],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(prizeResult.status,0,prizeResult.stderr);
  const champion=JSON.parse(prizeResult.stdout),event=champion.collection.tournaments.events[0];assert.equal(event.prize,'The Wayfarer');assert.equal(champion.collection.owned[event.trophy],1);
  await page.evaluate(s=>ttcgState(s),champion);
  await page.locator('#album-tournaments-tab').click();
  assert.equal(await page.locator('#tournament-prize-name').innerText(),'The Wayfarer');
  await page.evaluate(e=>showTournamentOutcome(e),event);
  assert.equal(await page.locator('#tournament-outcome-title').innerText(),'Tournament won');
  assert.match(await page.locator('#tournament-outcome-rewards').innerText(),/The Wayfarer/);
  assert.equal(await page.locator('#tournament-outcome-rewards .art-scene').count(),1);
  await page.waitForFunction(()=>document.querySelector('#tournament-outcome-rewards img')?.naturalWidth>0);
  if(width===1920)await page.screenshot({path:'build/expansions/tournament-prize.png'});
  await page.evaluate(()=>document.getElementById('tournament-outcome').hidden=true);
  assert.deepEqual(errors,[]);await page.evaluate(s=>ttcgState(s),packet);
  await page.locator('#album-cards-tab').click();
  if(width===1920){fs.mkdirSync('build/expansions',{recursive:true});await page.screenshot({path:'build/expansions/album.png'});}
  await page.close();
 }
 assert(blocked>0&&reads>0);console.log('Expansion album: real manifest, separate-root native art, placeholders, filters, missing-set notices and native tournament trophy victory passed');
 }finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
