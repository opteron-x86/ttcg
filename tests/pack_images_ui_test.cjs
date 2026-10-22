const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const path=require('node:path'),fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  // An older native bridge must not replace the images with a preview surface.
  await page.addInitScript(()=>{window.nativePreviewCalls=0;window.ttcgPackModels=()=>window.nativePreviewCalls++;});
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  s.screen='album';s.matchContext=false;s.opponent='';
  Object.assign(s.collection,{owned:s.cards.map((_,i)=>i<80?1:0),known:s.cards.map((_,i)=>i<80),packs:[2,3],revealed:[],revealID:0});
  const update=()=>page.evaluate(s=>window.ttcgState(s),s);
  const first=()=>page.locator('#album-grid > button').evaluateAll(es=>es.slice(0,2).map(e=>e.id));
  const select=async(id,value)=>page.evaluate(([id,value])=>{const e=document.getElementById(id);e.value=value;e.dispatchEvent(new Event('change',{bubbles:true}));},[id,value]);
  const inspector=async kind=>{
   const result=await page.locator('#inspect-card .pack-preview').evaluate(async e=>{
    const css=getComputedStyle(e),r=e.getBoundingClientRect(),image=new Image();
    image.src=css.backgroundImage.match(/^url\("?(.*?)"?\)$/)[1];await image.decode();
    return {kind:e.dataset.kind,visible:css.visibility!=='hidden'&&r.width>100&&r.height>100,size:[image.naturalWidth,image.naturalHeight]};
   });
   assert.deepEqual(result,{kind:String(kind),visible:true,size:[512,672]});
  };
  await update();await inspector(0);
  assert.equal(await page.locator('#album-pack-shelf').count(),0);
  assert.deepEqual(await first(),['album-card-pack','album-tournament-pack']);
  assert.equal(await page.locator('#album-card-pack .owned-count').innerText(),'×2');
  assert.equal(await page.locator('#album-tournament-pack .owned-count').innerText(),'×3');
  for(const direction of ['desc','asc']){await select('album-direction',direction);assert.deepEqual(await first(),['album-card-pack','album-tournament-pack']);}
  await page.locator('#album-search').fill('nothing matches this');
  assert.equal(await page.locator('#album-grid .collection-card').count(),0);assert.deepEqual(await first(),['album-card-pack','album-tournament-pack']);
  await select('album-tier','7');assert.deepEqual(await first(),['album-card-pack','album-tournament-pack']);
  await select('album-tier','0');await page.locator('#album-search').fill('');
  await page.locator('#album-grid').evaluate(e=>{e.scrollTop=75;});await inspector(0);
  await page.locator('#album-grid').evaluate(e=>{e.scrollTop=0;});
  await page.locator('#album-filters-toggle').click();await page.locator('#album-filters-done').click();
  await page.locator('#album-sort-trigger').click();await page.keyboard.press('Escape');
  await page.locator('#album-settings-tab').click();await page.locator('#album-cards-tab').click();await inspector(0);
  await select('window-size','75');
  await page.locator('#album-tournament-pack').click();await inspector(1);
  assert.equal(await page.locator('#inspect-count').innerText(),'Tournament pack');
  await page.locator('#inspect-open-pack').click();assert.match(commands.at(-1),/^tournament \d+ \d+ pack 0$/);
  // Opening one pack retains its tile; only a depleted type disappears.
  s.collection.packs[1]=2;s.collection.revealed=[0,1,2,3,4];s.collection.revealKind='tournament';s.collection.revealID=1;s.revision++;
  await update();await page.locator('#album-confirm').click();await inspector(1);
  assert.equal(await page.locator('#album-tournament-pack .owned-count').innerText(),'×2');
  s.collection.revealed=[];s.collection.packs[0]=0;s.revision++;await update();
  assert.equal(await page.locator('#album-card-pack').count(),0);assert.equal(await page.locator('#album-grid > button').first().getAttribute('id'),'album-tournament-pack');
  await page.locator('#album-decks-tab').click();assert.equal(await page.locator('#album-grid .pack-tile').count(),0);
  await page.locator('#album-cards-tab').click();await page.locator('#album-tournament-pack').click();await inspector(1);
  await page.locator('#album-grid .collection-card').first().click();assert.equal(await page.locator('#inspect-card .pack-preview').count(),0);
  await page.locator('#album-tournament-pack').click();await inspector(1);
  s.collection.packs=[2,3];s.revision++;await update();
  const assets=await page.locator('.pack-preview').evaluateAll(es=>es.map(e=>getComputedStyle(e).backgroundImage));
  assert(assets.some(a=>a.includes('standard.png'))&&assets.some(a=>a.includes('tournament.png')));
  if(width===1440){fs.mkdirSync('build/previews',{recursive:true});await page.screenshot({path:'build/previews/album-pack-images.png'});}
  s.collection.packs=[0,0];s.revision++;await update();assert.equal(await page.locator('#album-grid .pack-tile').count(),0);assert(await page.locator('#inspect-open-pack').isHidden());
  assert.equal(await page.evaluate(()=>window.nativePreviewCalls),0);
  await page.evaluate(()=>ttcgReset());assert(await page.locator('#table').isHidden());assert.deepEqual(errors,[]);
  await page.close();console.log(`${width}×${height}: pack images loaded, pinned entries, quantities, filters, opening and depletion passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
