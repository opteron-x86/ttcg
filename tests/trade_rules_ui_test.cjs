const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1440,900],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const base=JSON.parse(spawnSync('/tmp/ttcg-ui-host',[],{encoding:'utf8'}).stdout.trim());let session=300;
  const fixture=(trade,score)=>{const s=structuredClone(base);Object.assign(s,{session:session++,revision:9,phase:'result',screen:'lobby',score,settling:false,opening:false});s.fullHands=[[0,1,2,3,4],[0,9,10,9,10]];s.originalHands=structuredClone(s.fullHands);s.hands=[[null,null,null,null,null],[null,null,null,null,10]];s.board=s.fullHands.flat().slice(0,9).map((card,origin)=>({card,origin,owner:origin%2,flip:false}));Object.assign(s.collection,{trade,required:trade===2?2:trade===4?5:0,selectedMask:0,transferOwners:Array.from({length:10},(_,i)=>i/5|0),wager:10,staked:trade===2&&score[0]>score[1],paying:false,notice:'',choice:-1});return s;};
  let s=fixture(2,[6,4]);await page.evaluate(s=>ttcgState(s),s);await page.waitForFunction(()=>trade?.phase==='selecting');assert(await page.locator('#take-card').isDisabled());
  await page.locator('#prize-0').click();assert(await page.locator('#take-card').isDisabled());await page.locator('#prize-3').click();assert.equal(await page.locator('.trade-hand .selected').count(),2);assert(await page.locator('#take-card').isEnabled());
  await page.locator('#prize-4').click();assert.equal(await page.locator('.trade-hand .selected').count(),2,'Cannot over-select');await page.locator('#prize-3').click();assert(await page.locator('#take-card').isDisabled());await page.locator('#prize-3').click();
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/trade-diff-0.13.0.png'});
  await page.locator('#take-card').click();assert(commands.at(-1).endsWith(' 9'),'Slot mask preserves duplicate identities');assert.equal(await page.locator('#trade-owner-0 .card').count(),0);
  s.collection.staked=false;s.collection.selectedMask=9;await page.evaluate(s=>ttcgState(s),s);await page.waitForFunction(()=>trade?.phase==='done');assert.equal(await page.locator('#trade-owner-0 .card').count(),2);
  for(const [kind,score,owners,count0,count1] of [[3,[5,5],[0,1,0,0,0,0,1,1,1,1],1,1],[4,[4,6],[1,1,1,1,1,1,1,1,1,1],0,5],[2,[4,6],[1,1,0,0,0,1,1,1,1,1],0,2]]){
   s=fixture(kind,score);s.collection.transferOwners=owners;s.collection.selectedMask=kind===4?31:3;s.board.forEach(c=>c.owner=owners[c.origin]);
   await page.evaluate(s=>ttcgState(s),s);await page.waitForFunction(()=>trade?.phase==='done');assert(await page.locator('#take-card').isHidden());assert.equal(await page.locator('#trade-owner-0 .card').count(),count0);assert.equal(await page.locator('#trade-owner-1 .card').count(),count1);
   const frame=await page.locator('#table').boundingBox();for(const el of await page.locator('.trade-received .card').all()){const b=await el.boundingBox();assert(b.x>=frame.x&&b.x+b.width<=frame.x+frame.width&&b.y+b.height<=frame.y+frame.height,'Received cards fit');}
  }
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: Diff selection, duplicate slots, native acknowledgement, Direct draw exchange and All/Diff losses passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
