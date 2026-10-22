const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});const host=spawn('/tmp/tessera-decks-host',['--economy']);try{
 const lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();
 const page=await browser.newPage({viewport:{width:1440,height:900},reducedMotion:'reduce'}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 const update=()=>page.evaluate(s=>window.ttcgState(s),packet);
 await page.exposeFunction('ttcgCommand',command=>{pending=pending.then(async()=>{host.stdin.write(command+'\n');packet=JSON.parse((await lines.next()).value);await update();});});
 await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await update();
 const settle=async()=>{await page.waitForTimeout(50);await pending;};const command=async(v,args='')=>{await page.evaluate(([v,a])=>send(v,a),[v,args]);await settle();};
 const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator(`#${id}-options [data-value="${value}"]`).click();};
 const count=packet.cards.filter(c=>!c.foil).length,foil=count;assert.equal(count,703);assert.equal(packet.cards[foil].cardID,packet.cards[0].cardID+0x80000000);
 await command('browse','album');assert.match(await page.locator('#album-count').innerText(),/703 owned.*703\/703 discovered/);
 await page.locator('#album-search').fill(packet.cards[0].name);assert.equal(await page.locator('#album-grid .collection-card').count(),2);assert.equal(await page.locator('#album-grid .foil').count(),1);
 await page.locator('#album-card-'+foil).click();assert.match(await page.locator('#inspect-metadata').innerText(),/Foil/);
 const before=await page.locator('#inspect-card .foil-film').evaluate(e=>{const s=getComputedStyle(e);return [s.opacity,s.backgroundPosition,s.animationName,s.transform,getComputedStyle(e.closest('.foil')).getPropertyValue('--foil-strength')];});
 assert.equal(Number(before[0]),.72);assert.equal(before[2],'none');assert.equal(Number(before[4]),.72);
 await page.mouse.move(800,700);await page.waitForTimeout(400);const after=await page.locator('#inspect-card .foil-film').evaluate(e=>{const s=getComputedStyle(e);return [s.opacity,s.backgroundPosition,s.animationName,s.transform,getComputedStyle(e.closest('.foil')).getPropertyValue('--foil-strength')];});assert.deepEqual(before,after);
 await page.locator('#album-filters-toggle').click();await select('album-finish','foil');await page.locator('#album-filters-done').click();assert.equal(await page.locator('#album-grid .collection-card').count(),1);
 await page.screenshot({path:'build/clean-catalog/screenshots/economy-foil.png'});
 // Normal and foil copies share the two-copy limit, while ownership is exact by finish.
 await command('deck',`${foil} 0 1 2 3`);assert.equal(packet.collection.deck[0],foil);
 await page.locator('#album-decks-tab').click();await page.locator('#album-search').fill(packet.cards[0].name);await page.locator('#album-card-0').click();await page.locator('#album-deck-2').click();assert(await page.locator('#inspect-add').isDisabled());
 await command('deck',`${foil} 0 0 2 3`);assert.deepEqual(packet.collection.deck,[foil,0,1,2,3]);
 await command('reload');assert.equal(packet.collection.deck[0],foil);assert.equal(await page.locator('#album-deck-0 .foil').count(),1);assert(await page.locator('.saved-deck-preview .foil').count()>0);
 const unique=packet.cards.findIndex(c=>c.unique),uf=unique+count;await command('deck',`${unique} ${uf} 1 2 3`);assert.equal(packet.collection.deck[0],foil);
 await page.locator('#album-search').fill('');
 await command('browse','album');await page.locator('#album-cards-tab').click();
 await page.locator('#album-card-pack').click();await page.locator('#inspect-open-pack').click();await settle();assert.equal(await page.locator('#pack-reveal .card').count(),5);assert.equal(await page.locator('#album-modal-title').innerText(),'Tessera pack');
 for(const [width,height] of [[1440,900],[1280,720]]){await page.setViewportSize({width,height});await page.waitForTimeout(80);const boxes=await page.locator('#pack-reveal .pack-card').evaluateAll(es=>es.map(e=>{const r=e.getBoundingClientRect();return {x:r.x,y:r.y,right:r.right,bottom:r.bottom};}));assert(boxes.every(b=>Math.abs(b.y-boxes[0].y)<2&&b.x>=0&&b.right<=width&&b.bottom<=height));const confirm=await page.locator('#album-confirm').boundingBox();assert(confirm.y+confirm.height<=height);}
 await page.setViewportSize({width:1440,height:900});await page.locator('#album-confirm').click();
 // A hidden opponent's finish is not revealed; the player's foil reaches the match intact.
 await command('deck',`${foil} 0 1 2 3`);await command('browse','lobby');await command('start',`0 0 0 ${foil} 0 1 2 3`);assert.equal(packet.fullHands[0][0],foil);assert(packet.fullHands[1].every(i=>i===-2));assert.equal(await page.locator('#hand-0-0 .foil').count(),1);assert.equal(await page.locator('#opponent-hand .foil').count(),0);
 assert.deepEqual(errors,[]);console.log('Foil UI: Prismatic at 72%, reduced motion, filters, identity counts, mixed decks, saved-deck reload, five-card reveal and hidden-hand privacy passed');
 }finally{host.kill();await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
