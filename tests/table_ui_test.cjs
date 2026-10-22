const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path'),fs=require('node:fs'),assert=require('node:assert/strict');
const thumbnails=new Map(fs.readdirSync('TesseraUI/art/thumbs').map(name=>[path.parse(name).name.toLowerCase(),name]));
function resolveThumbnails(packet){for(const card of packet.cards){const name=thumbnails.get(card.name.toLowerCase());if(name)card.thumbnail='thumbs/'+name;}return packet;}
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {for(const [width,height] of [[1280,720],[1920,1080],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));page.on('requestfailed',r=>errors.push(r.url()+': '+r.failure().errorText));
  await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const packet=JSON.parse(spawnSync('/tmp/tessera-decks-host',['--economy'],{encoding:'utf8',input:''}).stdout);
  // The portable host lacks the native artwork scanner; honor local JPG/PNG thumbnails.
  resolveThumbnails(packet);
  packet.collection.deck=[0,1,2,3,4];packet.collection.hold='The Reach';packet.collection.skill='Skilled';
  await page.evaluate(s=>window.ttcgState(s),packet);
  const fits=async(...ids)=>{const table=await page.locator('#table').boundingBox();assert(table.x>=0&&table.y>=0&&table.x+table.width<=width+1&&table.y+table.height<=height+1);for(const id of ids){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=table.x-1&&b.y>=table.y-1&&b.x+b.width<=table.x+table.width+1&&b.y+b.height<=table.y+table.height+1,`${width}: ${id} fits`);}};
  const names=()=>page.locator('#selected-deck .card-name').allTextContents();
  assert.equal(await page.locator('#lobby-editor').count(),0);await fits('selected-deck','start','challenge-skill');
  await page.locator('#deck-0').click();assert(await page.locator('#table-inspection').isVisible());assert.equal(await page.locator('#table-inspect-name').innerText(),packet.cards[0].name);
  await page.keyboard.press('Tab');assert(await page.evaluate(()=>!!document.activeElement.closest('#table-inspection')));assert.equal(await page.locator('#table-inspect-art .rank').count(),0);await page.locator('#table-inspect-flip').click();assert.equal(await page.locator('#table-inspect-art .art-scene').getAttribute('data-side'),'back');await page.keyboard.press('Escape');assert(await page.locator('#table-inspection').isHidden());assert.equal(commands.length,0);
  if(width===1920)await page.screenshot({path:'build/clean-catalog/screenshots/table-lobby.png'});
  await page.locator('#lobby-edit').click();assert(commands.at(-1).endsWith('album'));
  packet.session++;packet.screen='album';await page.evaluate(s=>window.ttcgState(s),packet);assert.equal(await page.locator('#album').getAttribute('data-view'),'decks');
  await fits('album-deck','album-grid','album-back');
  await page.locator('#album-deck-2').click();await page.locator('#album-card-5').click();await page.locator('#inspect-add').click();assert(commands.at(-1).endsWith('0 1 5 3 4'));
  const foil=packet.cards.findIndex(c=>c.foil&&c.baseIndex===0);assert(foil>=0);
  assert(await page.evaluate(foil=>{albumDeck=[0,foil,2,3,4];return !canAddAlbumCard(0,2)&&canAddAlbumCard(0,0);},foil));
  await page.locator('#album-back').click();assert(commands.at(-1).endsWith('lobby'));packet.screen='lobby';
  packet.session++;packet.phase='match';packet.turn=0;packet.rules=19;packet.collection.wager=25;
  packet.hands=[[0,1,2,3,4],[5,6,7,8,9]];packet.board=Array(9).fill(null);packet.score=[5,5];
  await page.evaluate(s=>window.ttcgState(s),packet);await fits('board','player-hand','rival-hand','turn');
  assert.equal(await page.locator('#game [draggable="true"]').count(),0,'Board drag stays inside CEF');
  const boardSource=await page.locator('#hand-0-0').boundingBox(),refreshCount=commands.length;
  await page.mouse.move(boardSource.x+20,boardSource.y+20);await page.mouse.down();await page.mouse.move(boardSource.x+40,boardSource.y+40);
  assert.equal(await page.locator('.table-drag-ghost').count(),1);await page.evaluate(s=>window.ttcgState(s),packet);await page.mouse.up();
  assert.equal(await page.locator('.table-drag-ghost').count(),0);assert.equal(commands.length,refreshCount,'A native refresh cancels an unfinished drag');
  await page.locator('#hand-0-0').click();await page.locator('#square-0').hover();assert.equal(await page.locator('.placement-ghost').count(),1);
  await page.keyboard.press('i');assert(await page.locator('#table-inspection').isVisible());assert.equal(await page.locator('.placement-ghost').count(),0);await fits('table-inspect-art','table-inspect-close');
  const blocked=commands.length;await page.evaluate(()=>requestPlacement(0,0));assert.equal(commands.length,blocked,'Inspection blocks background placement');await page.keyboard.press('Escape');
  await page.locator('#square-0').click();await page.waitForTimeout(20);assert.equal(commands.filter(c=>c.startsWith('play ')).length,1);
  await page.evaluate(()=>requestPlacement(0,1));assert.equal(commands.filter(c=>c.startsWith('play ')).length,1,'Pending placement cannot be sent twice');
  packet.revision++;await page.evaluate(s=>window.ttcgState(s),packet);
  await page.locator('#hand-0-1').dragTo(page.locator('#square-4'));await page.waitForTimeout(20);assert(commands.at(-1).endsWith('1 4'),'Drag sends native hand and square indices');
  packet.revision++;packet.turn=1;packet.board[4]={card:1,owner:0};packet.hands[0][1]=null;
  await page.evaluate(s=>window.ttcgState(s),packet);const rivalTurn=commands.length;
  await page.locator('#hand-1-0').click();assert.equal(await page.locator('#table-inspect-name').innerText(),packet.cards[5].name);await page.keyboard.press('Escape');
  await page.locator('#square-4').click();assert.equal(await page.locator('#table-inspect-name').innerText(),packet.cards[1].name);await page.keyboard.press('Escape');
  await page.evaluate(()=>requestPlacement(0,0));assert.equal(commands.length,rivalTurn,'Opponent turn allows inspection without playing');
  packet.revision++;packet.rules=0;packet.hands[1]=Array(5).fill(-2);await page.evaluate(s=>window.ttcgState(s),packet);
  assert.equal(await page.locator('#rival-hand .card-back').count(),5);assert.equal(await page.locator('#rival-hand .card-name').count(),0);
  await page.evaluate(()=>{document.getElementById('hand-1-0').dispatchEvent(new MouseEvent('contextmenu',{bubbles:true}));inspectTableCard(-2,1)});assert(await page.locator('#table-inspection').isHidden());
  packet.revision++;packet.rules=32;packet.turn=0;packet.tiles=[1,2,4,8,16,0,0,0,0];await page.evaluate(s=>window.ttcgState(s),packet);assert.equal(await page.locator('.tile-affinity').count(),4);
  await page.locator('#hand-0-0').click();assert.equal(await page.locator('.tile-affinity .affinity-icon').count(),4);assert.deepEqual((await page.locator('.tile-affinity').allTextContents()).map(text=>text.trim()),['','','','']);await fits('board','player-hand','rival-hand','match-footer');
  if(width===1920)await page.screenshot({path:'build/clean-catalog/screenshots/table-board.png'});
  const fixtures=spawnSync('/tmp/ttcg-ui-host',['--fixtures'],{encoding:'utf8'}).stdout.trim().split('\n').map(JSON.parse).map(resolveThumbnails);
  await page.evaluate(s=>window.ttcgState(s),fixtures[0]);await page.evaluate(s=>window.ttcgState(s),fixtures[1]);
  assert.equal(await page.locator('#rule-callout').innerText(),'Plus');assert.equal(await page.locator('.rule-highlight').count(),3);
  await page.waitForFunction(()=>document.getElementById('rule-callout').textContent==='Combo');
  if(width===1920){await page.waitForTimeout(170);await page.screenshot({path:'build/clean-catalog/screenshots/table-combo.png'});}
  await page.waitForFunction(()=>!busy);assert.equal(await page.locator('#square-6 .owner-0').count(),1);
  // UI teardown clears inspection, drag and presentation state.
  await page.evaluate(()=>window.ttcgReset());assert(await page.locator('#table').isHidden());assert.equal(await page.locator('.placement-ghost').count(),0);
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: table layout, editor, native commands, inspection, hidden hands, affinity and capture stages passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
