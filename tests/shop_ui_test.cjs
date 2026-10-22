const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});const host=spawn('/tmp/ttcg-shop-host');try{
 const lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();
 const page=await browser.newPage({viewport:{width:1440,height:1000},reducedMotion:'reduce'}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.exposeFunction('ttcgCommand',command=>{pending=pending.then(async()=>{host.stdin.write(command+'\n');packet=JSON.parse((await lines.next()).value);await page.evaluate(s=>window.ttcgState(s),packet);});});
 await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>window.ttcgState(s),packet);
 const settle=async()=>{await page.waitForTimeout(70);await pending;},command=async(v,args='')=>{await page.evaluate(([v,a])=>send(v,a),[v,args]);await settle();};
 const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator(`#${id}-options [data-value="${value}"]`).click();};
 const search=async name=>page.locator('#shop-search').fill(name);
 assert(await page.locator('#shop').isVisible());assert(await page.locator('#album').isHidden());assert(await page.locator('#challenge').isHidden());assert(await page.locator('#music').isHidden());
 assert.equal(await page.locator('#shop-name').innerText(),'Sayma');assert.equal(await page.locator('#shop-grid button').count(),14);
 assert.equal(await page.locator('#shop-gold').innerText(),'50,000');
 await page.screenshot({path:'build/card-shop-buy.png'});
 const target=packet.shop.buy[0],previous=packet.collection.owned[target.id],balance=packet.shop.gold,merchant=packet.shop.merchantGold;
 await page.locator('#shop-card-'+target.id).click();await page.locator('#shop-action').click();await settle();
 assert.equal(packet.collection.owned[target.id],previous+1);assert.equal(packet.shop.gold,balance-target.price);assert.equal(packet.shop.merchantGold,merchant+target.price);
 // Reusing an old command revision cannot purchase a second copy.
 const stale=`shop ${packet.session} ${packet.revision-1} buy ${target.id} 1 0`;await page.evaluate(s=>window.ttcgCommand(s),stale);await settle();assert.equal(packet.collection.owned[target.id],previous+1);
 await page.locator('#shop-sell-tab').click();await select('shop-inventory','surplus');
 assert(packet.shop.sell.some(row=>row.surplus));assert.equal(await page.locator('#shop-grid .foil').count(),0);
 const surplus=packet.shop.sell.find(row=>row.surplus>0&&row.id===0);await search(packet.cards[0].name);await page.locator('#shop-card-0').click();
 const beforeSell=packet.shop.gold;await page.locator('#shop-action').click();await settle();assert(await page.locator('#shop-confirm').isHidden());assert.equal(packet.shop.gold,beforeSell+surplus.price);assert.equal(packet.collection.owned[0],2);
 await page.locator('#shop-buyback-tab').click();assert.equal(await page.locator('#shop-grid button').count(),1);assert.match(await page.locator('#shop-action').innerText(),/Buy back · 5 gold/);
 await page.locator('#shop-action').click();await settle();assert.equal(packet.collection.owned[0],3);assert.equal(packet.shop.gold,beforeSell);assert(await page.locator('#shop-empty').isVisible());
 // Exact foil copy, confirmation, Escape, art rotation, and non-gray cards.
 await page.locator('#shop-sell-tab').click();await select('shop-inventory','all');
 const foil=packet.shop.sell.find(row=>packet.cards[row.id].foil);await search(packet.cards[foil.id].name);await page.locator('#shop-card-'+foil.id).click();
 await page.locator('#shop-preview').click();assert(await page.locator('#table-inspection').isVisible());assert.equal(await page.locator('.art-front .rank,.art-front .tier,.art-front .card-name').count(),0);
 await page.locator('#table-inspect-flip').click();assert.equal(await page.locator('.art-scene').getAttribute('data-side'),'back');await page.keyboard.press('Escape');
 await page.locator('#shop-action').click();assert.match(await page.locator('#shop-confirm-copy').innerText(),/foil/);await page.keyboard.press('Escape');assert.equal(packet.collection.owned[foil.id],1);
 await page.locator('#shop-action').click();await page.locator('#shop-confirm-action').click();await settle();assert.equal(packet.collection.owned[foil.id],0);assert.equal(packet.collection.owned[0],3);
 // Selling all three copies needs deck confirmation; cancel keeps every copy.
 await page.locator('#shop-card-0').click();await select('shop-quantity','3');await page.locator('#shop-action').click();assert.match(await page.locator('#shop-confirm-copy').innerText(),/empty slot/);await page.locator('#shop-cancel').click();assert.equal(packet.collection.owned[0],3);
 // Tier X sale, permanent resale through reload and a two-day restock.
 const unique=packet.shop.sell.find(row=>packet.cards[row.id].unique&&packet.cards[row.id].tier===10);
 await search(packet.cards[unique.id].name);await page.locator('#shop-card-'+unique.id).click();await page.locator('#shop-action').click();assert.match(await page.locator('#shop-confirm-copy').innerText(),/unique/);
 await page.screenshot({path:'build/card-shop-unique-confirm.png'});await page.locator('#shop-confirm-action').click();await settle();assert.equal(packet.collection.owned[unique.id],0);
 await command('reload');assert(packet.shop.buy.some(row=>row.id===unique.id));assert(packet.shop.buy.every(row=>row.buyback===0));
 await command('test-restock');assert(packet.shop.buy.some(row=>row.id===unique.id));assert(!packet.shop.buy.some(row=>row.id===foil.id));
 await search(packet.cards[unique.id].name);await page.locator('#shop-card-'+unique.id).click();assert.match(await page.locator('#shop-action').innerText(),/36,000 gold/);
 await page.locator('#shop-action').click();await settle();assert.equal(packet.collection.owned[unique.id],1);
 // No affordability grayscale is applied to inspectable cards.
 await command('test-poor');await search('');await page.locator('#shop-buy-tab').click();assert(await page.locator('#shop-action').isDisabled());
 const colors=await page.locator('#shop-grid button').evaluateAll(es=>es.map(e=>({disabled:e.disabled,opacity:getComputedStyle(e).opacity,filter:getComputedStyle(e).filter})));
 assert(colors.every(e=>!e.disabled&&e.opacity==='1'&&e.filter==='none'));assert.match(await page.locator('#shop-reason').innerText(),/enough gold/);await command('test-rich');
 // Shared navigation still works after returning to the album.
 await command('test-album');assert(await page.locator('#shop').isHidden());await page.locator('#album-decks-tab').click();assert.equal(await page.locator('#album-tabs .section-tabs').count(),0);
 await command('test-shop');await page.locator('#shop-sell-tab').click();await search(packet.cards[0].name);await page.locator('#shop-card-0').click();
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
  await page.setViewportSize({width,height});await page.waitForTimeout(60);
  const frame=await page.locator('#table').boundingBox();for(const id of ['shop-tabs','shop-grid','shop-notice']){
   const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=frame.x-2&&b.y>=frame.y-2&&b.x+b.width<=frame.x+frame.width+2&&b.y+b.height<=frame.y+frame.height+2,`${width}: ${id} fits`);
  }
  const overflow=await page.locator('#shop').evaluate(e=>e.scrollWidth>e.clientWidth||e.scrollHeight>e.clientHeight+1);assert(!overflow,`${width}: trading panel overflow`);
 }
 await page.setViewportSize({width:1440,height:1000});await page.screenshot({path:'build/card-shop-sell.png'});
 assert.deepEqual(errors,[]);console.log('Shop UI: live purchases, surplus sale, buyback, foil inspection/confirmation, deck guards, Tier X resale, affordability and layout passed');
 }finally{host.kill();await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
