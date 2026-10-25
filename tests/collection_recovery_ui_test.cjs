const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const fs=require('node:fs'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
const base=JSON.parse(fs.readFileSync('build/test-fixtures/album.json','utf8'));
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080],[3440,1440]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
  const packet=structuredClone(base);packet.settings.development.enabled=false;packet.collection.missingSets=['removed-test-set'];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>commands.push(text));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const update=async()=>{++packet.revision;await page.evaluate(s=>ttcgState(s),packet);};await update();
  await page.locator('#album-settings-tab').click();assert.equal(await page.locator('#settings-section-developer').count(),0);
  await page.locator('#settings-section-gameplay').click();assert(await page.locator('#collection-remove-missing').isVisible());
  assert.match(await page.locator('#collection-missing-sets').innerText(),/removed-test-set/);
  assert.match(await page.locator('#album-status').innerText(),/Settings > Gameplay/);
  await page.locator('#collection-remove-missing').click();assert.equal(commands.length,0);
  assert.equal(await page.evaluate(()=>document.activeElement.id),'collection-recovery-cancel');
  await page.locator('#collection-recovery-cancel').click();assert.equal(commands.length,0);
  assert.equal(await page.evaluate(()=>document.activeElement.id),'collection-remove-missing');
  await page.locator('#collection-remove-missing').click();await page.evaluate(()=>ttcgKey('cancel'));
  assert(await page.locator('#collection-recovery-confirm').isHidden());assert.equal(commands.length,0);
  await page.locator('#collection-remove-missing').click();await page.locator('#collection-recovery-confirm-action').click();
  await page.waitForFunction(()=>document.querySelector('#collection-remove-missing').disabled);
  assert.equal(commands.length,1);assert(commands[0].endsWith('remove-missing confirm'));
  packet.collection.missingSets=[];await update();assert(await page.locator('#collection-recovery').isHidden());
  packet.settings.development.enabled=true;await update();await page.locator('#settings-section-developer').click();
  await page.locator('#dev-clear-collection').click();assert.equal(commands.length,1);
  assert.match(await page.locator('#collection-recovery-confirm-copy').innerText(),/every card and foil/);
  assert.match(await page.locator('#collection-recovery-confirm-copy').innerText(),/all characters/);
  await page.locator('#collection-recovery-cancel').click();assert.equal(commands.length,1);
  await page.locator('#dev-clear-collection').click();await update();assert(await page.locator('#collection-recovery-confirm').isHidden());
  assert.equal(commands.length,1,'A new revision cancels stale confirmation');
  packet.collection.recoveryBlocked='Finish or forfeit your interrupted game first.';await update();assert(await page.locator('#dev-clear-collection').isDisabled());
  packet.collection.recoveryBlocked='';packet.interruptedMatch={opponent:'Hulda',canResume:true};await update();assert(await page.locator('#dev-clear-collection').isDisabled());
  delete packet.interruptedMatch;await update();
  await page.locator('#dev-clear-collection').click();
  const modal=await page.locator('#collection-recovery-confirm .modal').boundingBox();assert(modal.x>=0&&modal.y>=0&&modal.x+modal.width<=width+1&&modal.y+modal.height<=height+1);
  if(width===1920){fs.mkdirSync('build/previews',{recursive:true});await page.screenshot({path:'build/previews/collection-recovery-confirm.png'});}
  await page.locator('#collection-recovery-confirm-action').click();assert.equal(commands.length,2);assert(commands[1].endsWith('clear confirm'));
  packet.collection.owned.fill(0);packet.collection.deck.fill(-1);packet.collection.discovered?.fill(false);packet.revealCards=false;
  packet.settings.development.enabled=false;await update();assert.equal(await page.locator('#settings-section-developer').count(),0);
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: collection recovery confirmations, cancellation, stale requests, developer gating and blocked actions passed`);
 }}finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
