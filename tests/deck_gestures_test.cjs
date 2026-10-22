const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
 for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]) {
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  await page.goto(pathToFileURL(path.resolve(process.env.TTCG_UI_ROOT||'TesseraUI','index.html')).href);
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());s.screen='album';
  Object.assign(s.collection,{owned:Array.from({length:s.cards.length},(_,i)=>i===0?2:i<8?1:0),deck:Array(5).fill(-1),known:Array(s.cards.length).fill(true),packs:[0,0],revealed:[],revealID:0});
  await page.evaluate(s=>window.ttcgState(s),s);
  const names=()=>page.locator('#album-deck .card-name').allTextContents();
  await page.locator('#album-card-0').click();assert.deepEqual(await names(),[],'Single click only inspects');
  await page.locator('#album-card-0').dblclick();assert.deepEqual(await names(),[s.cards[0].name]);
  await page.locator('#album-card-0').dblclick();assert.equal((await names()).length,2);
  await page.locator('#album-card-0').dblclick();assert.equal((await names()).length,2,'Cannot add a third unowned copy');
  await page.locator('#album-card-1').dragTo(page.locator('#album-deck-4'));
  assert.deepEqual(await names(),[0,0,1].map(id=>s.cards[id].name),'Drop on an empty slot preserves the target position');
  await page.locator('#album-card-2').dragTo(page.locator('#album-deck-1'));
  assert.deepEqual(await names(),[0,2,0,1].map(id=>s.cards[id].name),'Drop on an occupied slot inserts');
  await page.locator('#album-card-3').dblclick();assert.equal((await names()).length,5);
  await page.locator('#album-card-4').dblclick();assert.equal((await names()).length,5,'Five-card limit');
  await page.locator('#album-deck-1').dragTo(page.locator('#album-deck-4'));
  assert.deepEqual(await names(),[0,0,3,1,2].map(id=>s.cards[id].name),'Move preserves relative order of other cards');
  await page.locator('#album-deck-4').dragTo(page.locator('#album-deck-0'));
  assert.deepEqual(await names(),[2,0,0,3,1].map(id=>s.cards[id].name));
  await page.locator('#album-deck-0').focus();await page.keyboard.press('Shift+ArrowRight');
  assert.deepEqual(await names(),[0,2,0,3,1].map(id=>s.cards[id].name));
  assert(commands.at(-1).endsWith('0 2 0 3 1'),'Exact order sent to native save');
  s.collection.deck=[0,2,0,3,1];s.session++;await page.evaluate(s=>window.ttcgState(s),s);
  assert.deepEqual(await names(),[0,2,0,3,1].map(id=>s.cards[id].name),'Reopen restores native order');
  assert.equal(await page.locator('.deck-drop,.deck-dragging').count(),0);
  // Cancel a drag outside the deck: no save or removal.
  const before=commands.length;await page.locator('#album-deck-1').dragTo(page.locator('#inspect-card'));
  assert.equal(commands.length,before);assert.equal((await names()).length,5);
  const source=await page.locator('#album-deck-1').boundingBox();
  await page.mouse.move(source.x+source.width/2,source.y+source.height/2);await page.mouse.down();await page.mouse.move(source.x+source.width/2+15,source.y+source.height/2+10);
  assert.equal(await page.locator('.album-drag-ghost').count(),1);
  await page.keyboard.press('Escape');await page.mouse.up();
  assert.equal(commands.length,before);assert(await page.locator('#table').isVisible());assert.equal((await names()).length,5);
  await page.mouse.move(source.x+source.width/2,source.y+source.height/2);await page.mouse.down();await page.mouse.move(source.x+source.width/2+15,source.y+source.height/2+10);
  await page.evaluate(s=>window.ttcgState(s),s);await page.mouse.up();
  assert.equal(await page.locator('.album-drag-ghost').count(),0);assert.equal(commands.length,before,'Native refresh cancels an in-progress drag');
  assert.deepEqual(errors,[]);console.log(`${width}x${height}: inspect/double-click, duplicate bounds, drag insertion, move ordering, keyboard reorder, saved order, cancellation passed`);await page.close();
 }
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
