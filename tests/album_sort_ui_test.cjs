const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1440,900],[2560,1080],[3840,2160]]){
  const host=spawn('/tmp/tessera-decks-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
  try{
   const packet=JSON.parse((await lines.next()).value);packet.screen='album';packet.collection.owned=packet.collection.owned.map((n,i)=>i<packet.cards.length/2?(i%7+1):0);
   const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[];page.on('pageerror',e=>errors.push(e.message));
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>window.ttcgState(s),packet);
   const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator(`#${id}-options [data-value="${value}"]`).click();};
   const ids=()=>page.locator('#album-grid .collection-card').evaluateAll(nodes=>nodes.map(n=>Number(n.id.replace('album-card-',''))));
   const cardCount=packet.cards.length/2;assert.equal(cardCount,703);
   for(const sort of ['tier','name','rarity','quantity']){
    await select('album-sort',sort);await select('album-direction','asc');const asc=await ids();assert.equal(asc.length,703);
    const rarity={Common:0,Rare:1,Epic:2,Legendary:3};
    const value=id=>sort==='name'?packet.cards[id].name:sort==='tier'?packet.cards[id].tier:sort==='rarity'?rarity[packet.cards[id].rarity]:packet.collection.owned[id];
    for(let n=1;n<asc.length;n++){const a=value(asc[n-1]),b=value(asc[n]);assert(sort==='name'?a.localeCompare(b)<=0:a<=b,`${sort} ascending values`);}
    await select('album-direction','desc');assert.deepEqual(await ids(),[...asc].reverse(),`${width} ${sort} reverses every result`);
   }
   await page.locator('#album-decks-tab').click();assert.equal(await page.locator('#album-direction').inputValue(),'asc');
   await page.locator('#album-cards-tab').click();assert.equal(await page.locator('#album-direction').inputValue(),'desc');
   for(const tier of [7,8,9,10]){
    const count=packet.cards.slice(0,cardCount).filter(c=>c.tier===tier).length;
    await page.locator('#album-filters-toggle').click();await select('album-tier',String(tier));await page.locator('#album-filters-done').click();assert.equal((await ids()).length,count,`Tier ${tier}`);
   }
   await page.locator('#album-filters-clear').click();await page.locator('#album-filters-toggle').click();
   assert.deepEqual(await page.locator('#album-group option').evaluateAll(es=>es.slice(1).map(e=>e.value)),[...new Set(packet.cards.flatMap(c=>c.groups))].sort());assert.equal(await page.locator('#album-group option').count(),19);await select('album-group','God');await page.locator('#album-filters-done').click();assert((await ids()).length>0);
   const frame=await page.locator('#table').boundingBox();
   for(const id of ['album-direction-trigger','album-sort-trigger','album-search','album-grid','inspect-card']){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=frame.x-2&&b.y>=frame.y-2&&b.x+b.width<=frame.x+frame.width+2&&b.y+b.height<=frame.y+frame.height+2,`${width}: ${id} fits`);}
   assert.deepEqual(errors,[]);if(width===1280){require('node:fs').mkdirSync('build/clean-catalog/screenshots',{recursive:true});await page.screenshot({path:'build/clean-catalog/screenshots/album-sort-720.png'});}
   await page.close();
  }finally{host.kill();}
 }
 console.log('Album sorting: all four criteria, reversed ties, view preferences, VII–X counts, 18 groups and 720p–4K layouts passed');
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
