const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8',input:''}).stdout.trim());s.screen='album';s.matchContext=false;
  const hand=['Deathless Draugr','Insidious Spirit','Assembled Sanitizer','Poetic Armiger',"M'aiq the Liar"].map(name=>s.cards.findIndex(c=>c.name===name));assert(hand.every(i=>i>=0));
  Object.assign(s.collection,{owned:s.cards.map(()=>2),known:s.cards.map(()=>true),deck:hand,savedDecks:[{id:1,name:'Moonlit roads',cards:hand}],activeDeck:1});
  await page.evaluate(s=>window.ttcgState(s),s);await page.locator('#album-decks-tab').click();
  const bounds=async id=>page.locator('#'+id).boundingBox();
  const fits=async ids=>{const frame=await bounds('table');for(const id of ids){const b=await bounds(id);assert(b&&b.x>=frame.x-1&&b.y>=frame.y-1&&b.x+b.width<=frame.x+frame.width+1&&b.y+b.height<=frame.y+frame.height+1,id+' fits');}};
  assert.equal(await page.locator('#album-deck-more').count(),0);assert(await page.locator('#album-deck-copy').isVisible());
  const list=await bounds('saved-decks-list'),create=await bounds('album-deck-new');assert(create.y>=list.y+list.height);
  const border=await page.locator('.saved-deck-row').evaluate(e=>{const c=getComputedStyle(e);return [c.borderTopWidth,c.borderRightWidth,c.borderBottomWidth];});assert.deepEqual(border,['0px','0px','0px']);
  const originalY=(await bounds('album-search')).y,composition=await page.locator('#deck-composition').innerText();
  assert(await page.locator('#album-remove').isDisabled());assert(await page.locator('#deck-composition .affinity-icon').count()>0);
  const replacement=s.cards.findIndex((c,i)=>c.tier===1&&!hand.includes(i));await page.locator('#album-card-'+replacement).click();
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/deck-layout-refreshed.png'});
  for(const slot of [0,2,4,4]){
   await page.locator('#album-deck-'+slot).click();assert.equal((await bounds('album-search')).y,originalY,'Selection must not shift collection');
   assert.equal(await page.locator('#deck-composition').innerText(),composition);assert(await page.locator('#deck-composition').isVisible());
  }
  await page.locator('#album-deck-0').click();assert.equal(await page.locator('#inspect-add').innerText(),'Replace '+s.cards[hand[0]].name);
  await fits(['album-deck-new','album-deck-rename','album-deck-delete','deck-composition','deck-slot-actions','inspect-add','album-grid']);
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/deck-layout-selected.png'});
  await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('Canceled');await page.keyboard.press('Escape');
  assert.equal(commands.length,0);assert(await page.locator('#album').isVisible());
  await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('Native cancel');
  await page.evaluate(()=>{lastEscapeTime=-Infinity;window.ttcgEscape('native');window.ttcgEscape('dom');});assert.equal(commands.length,0);assert(await page.locator('#album').isVisible());

  await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('');await page.keyboard.press('Enter');assert(await page.locator('#album-deck-name-input').isVisible());assert.equal(commands.length,0);
  await page.locator('#album-deck-name-input').fill('A much longer deck name');await page.keyboard.press('Enter');await page.waitForTimeout(30);assert(commands.at(-1).endsWith('rename 1 A much longer deck name'));
  await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('Saved on blur');await page.locator('#album-search').click();await page.waitForTimeout(30);assert(commands.at(-1).endsWith('rename 1 Saved on blur'));assert.equal(await page.evaluate(()=>document.activeElement.id),'album-search');
  // Partial/empty decks retain the same collection position; the title stays bounded.
  for(const n of [2,0,5]){s.revision++;s.collection.deck=hand.map((id,i)=>i<n?id:-1);s.collection.savedDecks[0]={id:1,name:'A very long name for a saved deck',cards:s.collection.deck};await page.evaluate(s=>window.ttcgState(s),s);assert.equal((await bounds('album-search')).y,originalY);await fits(['album-deck-rename','album-deck-delete','deck-slot-actions']);assert.equal(await page.locator('#album-clear').isDisabled(),n===0);}
  await page.locator('#album-rules-tab').click();
  for(const [topic,backs] of [['open',0],['threeopen',2],['hidden',5]]){
   await page.locator('#rules-topic-'+topic).click();assert(await page.locator('#rules-demo-controls').isHidden());assert(await page.locator('#rules-demo-caption').isHidden());assert.equal(await page.locator('#rules-demo .demo-target').count(),0);assert.equal(await page.locator('#rules-demo button:enabled').count(),0);assert.equal(await page.locator('#rules-rival-hand .card-back').count(),backs);await fits(['rules-demo-board','rules-player-hand','rules-rival-hand']);
   if(width===1440)await page.screenshot({path:`build/clean-catalog/screenshots/rules-static-${topic}.png`});
  }
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}×${height}: stable deck layout, named replacement, inline rename, empty/partial/full decks and static visibility examples passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
