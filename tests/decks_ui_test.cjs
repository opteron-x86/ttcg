const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {for(const [width,height] of [[1280,720],[1440,900],[3840,2160]]){
   const host=spawn('/tmp/tessera-decks-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
   const first=JSON.parse((await lines.next()).value),page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
   let pending=Promise.resolve();page.on('pageerror',e=>errors.push(e.message));
   await page.exposeFunction('ttcgCommand',command=>{
     commands.push(command);pending=pending.then(async()=>{host.stdin.write(command+'\n');const row=await lines.next();assert(!row.done);await page.evaluate(s=>window.ttcgState(s),JSON.parse(row.value));});
   });
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>window.ttcgState(s),first);
   const count=async n=>page.waitForFunction(n=>state.collection.savedDecks.length===n,n);
   const choose=async(id,prefix='lobby')=>{if(prefix==='album'){await page.locator('#album-decks-tab').click();await page.locator('#saved-deck-'+id).click();await page.waitForFunction(id=>state.collection.activeDeck===id,id);return;}await page.locator(`#${prefix}-deck-select-trigger`).click();await page.locator(`#${prefix}-deck-select-options button[data-value="${id}"]`).click();await page.waitForFunction(id=>state.collection.activeDeck===id,id);};
   const fits=async(ids)=>{const table=await page.locator('#table').boundingBox();for(const id of ids){const box=await page.locator('#'+id).boundingBox();assert(box&&box.x>=table.x-1&&box.y>=table.y-1&&box.x+box.width<=table.x+table.width+1&&box.y+box.height<=table.y+table.height+1,`${width}: ${id} fits`);}};
   assert.equal(await page.locator('#lobby-deck-select-trigger').innerText(),'Deck 1');await fits(['lobby-deck-tools','selected-deck','start']);
   const original=first.collection.deck;
   await page.locator('#library-open').click();await page.waitForFunction(()=>state.screen==='album');
   await page.locator('#album-decks-tab').click();
   await page.locator('#album-deck-new').click();await page.locator('#deck-name').fill('');assert(await page.locator('#deck-submit').isDisabled());
   await page.locator('#deck-name').fill('Same & Plus');await fits(['deck-form','deck-name','deck-submit']);await page.keyboard.press('Enter');await count(2);
   await page.locator('#album-back').click();await page.waitForFunction(()=>state.screen==='lobby');
   assert.equal(await page.locator('#start').innerText(),'Complete deck');await page.locator('#lobby-edit').click();await page.waitForFunction(()=>state.screen==='album');
   assert.deepEqual(await page.evaluate(()=>state.collection.deck),[-1,-1,-1,-1,-1]);
   for(let i=0;i<5;++i){await page.locator('#album-card-'+i).dblclick();await page.waitForFunction(n=>state.collection.deck.filter(x=>x>=0).length===n,i+1);}
   await page.locator('#album-back').click();await page.waitForFunction(()=>state.screen==='lobby');assert(!(await page.locator('#start').isDisabled()));await choose(1);assert.deepEqual(await page.evaluate(()=>state.collection.deck),original);
   await choose(2);assert.deepEqual(await page.evaluate(()=>state.collection.deck),[0,1,2,3,4]);
   await page.locator('#library-open').click();await page.waitForFunction(()=>state.screen==='album');
   await page.locator('#album-decks-tab').click();await choose(2,'album');
   await page.locator('#album-deck-copy').click();await page.locator('#deck-name').fill('Azura’s hand');await page.locator('#deck-submit').click();await count(3);
   await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('<b>Same</b>');await page.keyboard.press('Enter');await page.waitForFunction(()=>state.collection.savedDecks[2].name==='<b>Same</b>');assert.equal(await page.locator('#album-deck-name b').count(),0);
   await page.locator('#album-deck-delete').click();await page.keyboard.press('Escape');assert(await page.locator('#deck-dialog').isHidden());assert(await page.locator('#table').isVisible());assert.equal(await page.evaluate(()=>state.collection.savedDecks.length),3);
   await page.locator('#album-deck-delete').click();await page.locator('#deck-submit').click();await count(2);
   await choose(2,'album');
   await page.locator('#album-deck-0').focus();await page.keyboard.press('Shift+ArrowRight');await page.waitForFunction(()=>state.collection.deck[0]===1);
   await page.evaluate(()=>send('reload'));await pending;await page.waitForFunction(()=>state.collection.deck[0]===1&&state.collection.savedDecks[1].name==='Same & Plus');
   await fits(['album-deck-tools','album-deck','album-back']);
   await page.locator('#album-deck-new').click();await page.locator('#deck-name').fill('Unused');await page.locator('#deck-submit').click();await count(3);
   assert(await page.evaluate(()=>state.collection.deckReserved[0]>=1),'Inactive deck protects owned copies');
   await choose(2,'album');await page.locator('#album-back').click();await page.waitForFunction(()=>state.screen==='lobby');
   assert.equal(await page.locator('#lobby-deck-select-trigger').innerText(),'Same & Plus');
   await page.screenshot({path:`/tmp/tessera-decks-${width}.png`});
   await page.locator('#start').click();await page.waitForFunction(()=>state.phase==='match');
   assert.deepEqual(await page.evaluate(()=>state.collection.deck),[1,0,2,3,4]);
   assert(commands.some(c=>c.startsWith('start ')&&c.endsWith('1 0 2 3 4')));
   await pending;assert.deepEqual(errors,[]);host.kill();await page.close();console.log(`${width}×${height}: native-backed deck CRUD, lobby/album selection, ordering, save reload, modal input and starting selected hand passed`);
 }}finally{await browser.close();}
})().catch(error=>{console.error(error);process.exit(1);});
