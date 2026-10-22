const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]){
  const host=spawn('/tmp/ttcg-development-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
  try{
   let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();const page=await browser.newPage({viewport:{width,height}}),errors=[];
   page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',command=>{pending=pending.then(async()=>{host.stdin.write(command+'\n');packet=JSON.parse((await lines.next()).value);await page.evaluate(s=>ttcgState(s),packet);});});
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>ttcgState(s),packet);
   const settle=async()=>{await page.waitForTimeout(35);await pending;};
   const sendCommand=async(verb,args='')=>{await page.evaluate(([v,a])=>send(v,a),[verb,args]);await settle();};
   const select=async(key,value)=>{await page.locator('#dev-'+key+'-trigger').click();await page.locator('#dev-'+key+'-options [data-value="'+value+'"]').click();await settle();};
   await page.locator('#album-settings-tab').click();await page.locator('#settings-section-developer').click();assert(await page.locator('#development-settings').isVisible());
   for(const key of ['RegularTournaments','InvitationalTournaments','MastersTournaments']){await page.locator('#dev-'+key).click();await settle();assert.equal(packet.settings.development[key],1);assert.equal(await page.locator('#dev-'+key).getAttribute('aria-checked'),'true');}
   assert.equal(packet.collection.tournaments.events.length,1,'Only regulars are visible before the courier arrives');
   await select('TournamentHold','8');assert.equal(packet.settings.development.TournamentHold,8);
   const deck=packet.collection.deck.slice(),n=packet.cards.length/2;
   await page.locator('#dev-add-cards').click();await settle();assert(packet.collection.owned.slice(0,n).every(n=>n>0));assert.deepEqual(packet.collection.deck,deck);
   let owned=packet.collection.owned.slice();await page.locator('#dev-add-cards').click();await settle();assert.deepEqual(packet.collection.owned,owned);
   await page.locator('#dev-add-foils').click();await settle();assert(packet.collection.owned.every(n=>n>0));
   for(const key of ['ShowAllPlayers','UnlockAllPlayers','RevealAllCards','AllowAnyNPC','RecordResults']){const before=packet.settings.development[key];await page.locator('#dev-'+key).click();await settle();assert.equal(packet.settings.development[key],before?0:1);}
   assert(await page.evaluate(()=>visibleControls($('settings-panel')).some(el=>el.id==='dev-overrides-toggle')),'Overrides participate in keyboard and controller navigation');
   await page.locator('#dev-overrides-toggle').focus();await page.keyboard.press('Enter');await page.locator('#dev-hold-rules').click();await settle();
   await page.locator('#dev-rules [data-rule="8192"]').click();await settle();assert(packet.settings.development.Rules&1,'Same Wall enables Same');
   await page.locator('#dev-rules [data-rule="1"]').click();await settle();assert(!(packet.settings.development.Rules&8192),'Removing Same removes Same Wall');
   for(const bit of [512,1024,128,256,64]){await page.locator('#dev-rules [data-rule="'+bit+'"]').click();await settle();}
   assert.equal(packet.settings.development.Rules,1024|256|64);
   await select('TradeRule','4');await select('Wager','25');await select('OpponentSkill','3');await select('RuleLesson','4');
   const config=packet.settings.development;owned=packet.collection.owned.slice();
   const frame=await page.locator('#table').boundingBox();for(const id of ['settings-title','album-cards-tab']){const box=await page.locator('#'+id).boundingBox();assert(box.x>=frame.x&&box.y>=frame.y&&box.x+box.width<=frame.x+frame.width&&box.y+box.height<=frame.y+frame.height,id+' remains inside the window');}
   if(width===1440){await page.locator('#settings-title').scrollIntoViewIfNeeded();await page.screenshot({path:'build/clean-catalog/screenshots/development-settings.png'});}
   await page.locator('#album-cards-tab').click();assert(await page.locator('#settings-panel').isHidden());
   assert.equal(Number((await page.locator('#album-count').innerText()).split(' ')[0]),n);
   await sendCommand('reload');assert.deepEqual(packet.collection.owned,owned);assert.deepEqual(packet.settings.development,config);
   await page.locator('#album-settings-tab').click();await page.locator('#settings-section-developer').click();assert.equal(await page.locator('#dev-RegularTournaments').getAttribute('aria-checked'),'true');
   await sendCommand('test-disabled');assert(await page.locator('#development-settings').isHidden());
   await sendCommand('development','foils');assert.deepEqual(packet.collection.owned,owned,'Master switch also rejects forged commands');
   assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: development controls, courier gating, immediate grants, rule conflicts, saved settings and hidden controls passed`);
  }finally{host.stdin.end();host.kill();}
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
