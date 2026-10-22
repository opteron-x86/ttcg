const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
  const host=spawn('/tmp/tessera-decks-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
  try{
   let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve(),casual=false;
   const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   const enrich=s=>{s.opponent='Ambarys Rendar';Object.assign(s.collection,{hold:'Eastmarch',skill:'Regular',fixedRules:144,fixedTrade:casual?0:2,fixedWager:casual?0:-1,maxWager:200,gold:95655,rivalGold:500,practice:casual});return s;};
   const update=()=>page.evaluate(s=>window.ttcgState(s),enrich(packet));
   await page.exposeFunction('ttcgCommand',command=>{commands.push(command);pending=pending.then(async()=>{host.stdin.write(command+'\n');packet=JSON.parse((await lines.next()).value);await update();});});
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await update();
   const settle=async()=>{await page.waitForTimeout(40);await pending;};
   const choose=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator('#'+id+'-options [data-value="'+value+'"]').click();await settle();};
   const command=async(v,args)=>{await page.evaluate(([v,args])=>send(v,args),[v,args]);await settle();};
   await page.evaluate(()=>{$('window-size').value='75';$('window-size')._refresh();resizeWindow();});
   const fits=async ids=>{const frame=await page.locator('#table').boundingBox();for(const id of ids){const box=await page.locator('#'+id).boundingBox();assert(box&&box.x>=frame.x-1&&box.y>=frame.y-1&&box.x+box.width<=frame.x+frame.width+1&&box.y+box.height<=frame.y+frame.height+1,id+' fits');}};
   const shell=()=>page.locator('#table').evaluate(e=>{const c=getComputedStyle(e);return [c.width,c.height,c.backgroundImage,c.border,c.boxShadow,getComputedStyle(document.querySelector('header')).margin];});
   const sharedStyle=selector=>page.locator(selector).evaluate(e=>{const c=getComputedStyle(e);return [c.backgroundImage,c.borderColor,c.borderRadius,c.boxShadow];});
   const frame=await shell(),button=await sharedStyle('#lobby-edit');
   await command('deck','-1 -1 2 3 4');assert.equal(await page.locator('#start').innerText(),'Complete deck');assert(await page.locator('#start').isEnabled());assert.equal((await page.locator('#challenge').innerText()).match(/3\/5/g).length,1);assert.equal(await page.locator('#lobby-editor').count(),0);
   await choose('wager','25');await fits(['library-open','lobby-edit','selected-deck','match-stakes','start'].filter(id=>id!=='match-stakes').concat(['normal-rules','wager-control']));
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/lobby-unified-incomplete.png'});
   await page.locator('#deck-1').click();await settle();assert.equal(packet.screen,'album');assert.equal(await page.locator('#album').getAttribute('data-view'),'decks');assert.equal(await page.evaluate(()=>albumTarget),1);assert.deepEqual(await shell(),frame);assert.deepEqual(await sharedStyle('#album-back'),button);
   await page.locator('#album-card-1').click();await page.locator('#inspect-add').click();await settle();assert.equal(packet.collection.deck[1],1);
   await page.locator('#album-back').click();await settle();assert.equal(packet.screen,'lobby');assert.equal(await page.locator('#wager').inputValue(),'25');assert.equal(await page.locator('#selected-deck .card').count(),4);
   await page.locator('#start').click();await settle();assert.equal(await page.evaluate(()=>albumTarget),0);await page.locator('#album-card-0').click();await page.locator('#inspect-add').click();await settle();await page.keyboard.press('Escape');await settle();assert.equal(packet.screen,'lobby');assert.equal(await page.locator('#start').innerText(),'Play');assert.equal(await page.locator('#wager').inputValue(),'25');
   await page.locator('#library-open').click();await settle();assert.equal(await page.locator('#album').getAttribute('data-view'),'cards');assert.match(await page.locator('#album-back').innerText(),/match setup/);await page.locator('#leave').click();await settle();assert.equal(packet.screen,'lobby');assert(!commands.some(c=>c.startsWith('close ')));
   await page.locator('#library-open').click();await settle();await page.evaluate(()=>{lastEscapeTime=-Infinity;window.ttcgEscape('native');});await settle();assert.equal(packet.screen,'lobby');await page.evaluate(()=>window.ttcgEscape('dom'));await settle();assert(!commands.some(c=>c.startsWith('close ')),'Duplicate native/DOM Escape cannot exit after returning to setup');
   assert.equal(await page.locator('#game-mode,#player-gold,#normal-trade').count(),0);
   assert.deepEqual(await page.locator('#normal-rules .rule-chip').allTextContents(),['Open','Legion','Diff']);
   assert(!commands.some(c=>c.includes('friendly')));
   casual=true;await update();assert.deepEqual(await page.locator('#normal-rules .rule-chip').allTextContents(),['Open','Legion','Casual']);assert(await page.locator('#wager-control').isHidden());assert.equal(await page.locator('#wager').inputValue(),'0');
   await page.locator('#lobby-edit').click();await settle();await page.locator('#album-back').click();await settle();assert.equal(await page.locator('#normal-rules .rule-chip').last().innerText(),'Casual');
   casual=false;await update();await choose('wager','25');
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/lobby-unified-ready.png'});
   await page.locator('#deck-0').click();assert.equal(await page.locator('#table-inspect-art .rank').count(),0);assert.equal(await page.locator('#table-inspect-ranks span').count(),4);await page.locator('#table-inspect-flip').click();assert.equal(await page.locator('#table-inspect-art .art-scene').getAttribute('data-side'),'back');await page.keyboard.press('Escape');
   await page.locator('#start').click();await settle();assert.equal(packet.phase,'match');assert(commands.some(c=>c.startsWith('start ')&&c.endsWith('144 2 25 0 1 2 3 4')));assert.deepEqual(await page.locator('#board-rules .rule-chip').allTextContents(),['Open','Legion','Diff']);assert(!/Trade|95655/.test(await page.locator('#board-rules').innerText()));assert.deepEqual(await shell(),frame);await fits(['board','player-hand','rival-hand','match-footer']);
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/match-unified.png'});
   assert.deepEqual(errors,[]);await page.close();console.log(`${width}×${height}: single editor, empty-slot routing, return paths, preserved wager, child-only casual display, shared frame/controls and match start passed`);
  }finally{host.kill();}
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
