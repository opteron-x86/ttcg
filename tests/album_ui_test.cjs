const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]) {
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  await page.goto(pathToFileURL(path.resolve(process.env.TTCG_UI_ROOT||'TesseraUI','index.html')).href);
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  s.screen='album';s.opponent='';
  Object.assign(s.collection,{owned:Array(s.cards.length).fill(0),deck:Array(5).fill(-1),known:Array(s.cards.length).fill(false),packs:[0,0],revealed:[],revealID:0});
  const update=()=>page.evaluate(s=>window.ttcgState(s),s);
  const select=async(id,v)=>{
    if(['album-tier','album-group','album-affinity','album-rarity','album-finish'].includes(id)&&await page.locator('#album-filter-panel').isHidden())await page.locator('#album-filters-toggle').click();
    await page.locator(`#${id}-trigger`).click();await page.locator(`#${id}-options [data-value="${v}"]`).click();
  };
  await update();assert(await page.locator('#album').isVisible());assert(await page.locator('#challenge').isHidden());assert(await page.locator('#game').isHidden());
  assert.match(await page.locator('#album-count').innerText(),/0 owned · 0\/\d+ discovered/);assert(await page.locator('#album-back').isHidden());
  await select('album-filter','all');assert.equal(await page.locator('#album-grid .collection-card').count(),0);assert(await page.locator('#inspect-add').isDisabled());
  s.collection.packs=[1,0];s.revision++;await update();assert.equal(await page.locator('#album-card-pack').count(),1);await page.locator('#album-card-pack').click();await page.locator('#inspect-open-pack').click();assert(commands.at(-1).startsWith('open-pack '));
  assert(await page.locator('#inspect-card .pack-preview').evaluate(e=>{const r=e.getBoundingClientRect();return r.width>100&&r.height>100&&getComputedStyle(e).backgroundImage.includes('assets/packs/standard.png');}),'Pack inspector has its rendered image');
  s.revision++;s.collection.packs=[0,0];s.collection.owned=Array.from({length:s.cards.length},(_,id)=>id<8?1:0);s.collection.known=s.collection.owned.map(Boolean);s.collection.deck=[0,1,2,3,4];s.collection.revealID=1;s.collection.revealed=[0,1,2,3,4,5,6,7];await update();
  assert.equal(await page.locator('#pack-reveal .card').count(),8);assert(await page.locator('#album-modal').isVisible());
  await page.keyboard.press('Escape');assert(await page.locator('#album-modal').isHidden());assert(await page.locator('#table').isVisible());
  await update();assert(await page.locator('#album-modal').isHidden(),'Repeated state cannot replay a consumed pack');
  await select('album-filter','owned');assert.equal(await page.locator('#album-grid button').count(),8);
  await page.locator('#album-card-6').click();assert.equal(await page.locator('#inspect-card .card-name').innerText(),s.cards[6].name);
  s.collection.deck=[0,1,2,3,4];await update();assert(await page.locator('#inspect-add').isHidden());
  await select('album-filter','missing');assert.equal(await page.locator('#album-grid .collection-card').count(),0);
  await select('album-filter','owned');await page.locator('#album-search').fill(s.cards[6].name);assert.equal(await page.locator('#album-grid button').count(),1);assert.equal(await page.locator('#album-grid button').getAttribute('id'),'album-card-6');await page.locator('#album-search').fill('');
  const frame=await page.locator('#table').boundingBox();
  await page.locator('#album-decks-tab').click();
  for(const id of ['album-grid','inspect-card','album-deck']){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=frame.x-2&&b.y>=frame.y-2&&b.x+b.width<=frame.x+frame.width+2&&b.y+b.height<=frame.y+frame.height+2,id+' fits');}
  await page.locator('#album-cards-tab').click();
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album.png'});
  // Collection navigation survives close/reopen.
  s.session++;s.screen='album';s.opponent='';await update();
  await select('album-filter','owned');await page.locator('#album-card-6').click();
  await page.locator('#album-opponents-tab').click();s.collection.revealed=[];await page.evaluate(()=>window.ttcgReset());
  s.session++;await update();assert(await page.locator('#opponents-panel').isVisible());
  assert.equal(await page.locator('#album-opponents-tab').getAttribute('aria-pressed'),'true');
  await page.locator('#album-cards-tab').click();assert.equal(await page.locator('#album-tier').inputValue(),'0');assert.equal(await page.locator('#inspect-card .card-name').innerText(),s.cards[6].name);
  // Native settlement removed a card from slot 3: the other positions survive.
  s.collection.deck=[0,1,-1,3,4];s.collection.owned[2]=0;s.revision++;await update();
  await page.locator('#album-decks-tab').click();
  assert(await page.locator('#album-deck-2').evaluate(el=>el.classList.contains('needs-card')));
  assert.equal(await page.locator('#album-deck-3 .card-name').innerText(),s.cards[3].name);
  await page.locator('#album-card-6').dblclick();assert(commands.at(-1).endsWith('0 1 6 3 4'));
  await page.keyboard.press('Escape');assert(await page.locator('#table').isHidden());assert.deepEqual(errors,[]);
  console.log(`${width}x${height}: album, inspection, packs, deck saving, discovery, exit passed`);await page.close();
 }
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
