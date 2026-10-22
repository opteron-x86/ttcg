const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
 const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
 page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',s=>commands.push(s));
 await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
 const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
 const foil=s.cards.findIndex(c=>c.foil);s.screen='album';s.matchContext=false;s.opponent='';
 Object.assign(s.collection,{owned:s.cards.map((_,i)=>i<21||i===foil?1:0),known:s.cards.map((_,i)=>i<21||i===foil),deck:[0,1,2,3,4],holdID:1,regions:[{id:1,name:'Whiterun',rules:0,trade:1}],packs:[2,0],revealed:[],revealID:0});
 await page.evaluate(s=>window.ttcgState(s),s);
 const fits=async ids=>{const f=await page.locator('#table').boundingBox();for(const id of ids){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=f.x-2&&b.y>=f.y-2&&b.x+b.width<=f.x+f.width+2&&b.y+b.height<=f.y+f.height+2,`${width} ${id} fits: ${JSON.stringify(b)}`);}};
 await page.locator('#album-card-0').click();assert(await page.locator('#inspect-add').isHidden());assert.equal(await page.locator('#inspect-edit-deck').count(),0);assert.equal(await page.locator('#inspect-reason').innerText(),'');
 await page.locator('#album-card-pack').click();assert(await page.locator('#inspect-add').isHidden());assert.doesNotMatch(await page.locator('#album .album-inspector').innerText(),/unopened|Five cards|Tiers|slot|Deck full|Edit deck/);
 assert.equal(await page.locator('#album .inspect-actions button:visible').count(),1);await fits(['album-card-pack','inspect-open-pack','album-grid']);
 await page.locator('#inspect-open-pack').click();assert(commands.at(-1).startsWith('open-pack '));
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-pack-refresh.png'});
 await page.locator('#album-card-'+foil).click();await page.locator('#inspect-card').click();
 assert.equal(await page.locator('.art-front .rank,.art-front .tier,.art-front .card-name,.art-front .card-affinities').count(),0);
 assert.equal(await page.locator('.art-front .foil-film').count(),1);assert(await page.locator('.art-front img').evaluate(e=>e.complete&&e.naturalWidth>0));
 await fits(['pack-reveal','inspect-tools','inspect-navigation','album-confirm']);
 await page.locator('#inspect-flip').click();assert.equal(await page.locator('.art-scene').getAttribute('data-side'),'back');
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-card-back.png'});
 await page.locator('#inspect-reset').click();const box=await page.locator('.art-scene').boundingBox();
 await page.mouse.move(box.x+box.width*.3,box.y+box.height*.5);await page.mouse.down();await page.mouse.move(box.x+box.width*.75,box.y+box.height*.35,{steps:8});await page.mouse.up();
 assert.notEqual(await page.locator('.art-turn').evaluate(e=>e.style.transform),'rotateX(0deg) rotateY(0deg)');assert.notEqual(await page.locator('.art-front').evaluate(e=>e.style.getPropertyValue('--light-x')),'50%');
 await page.locator('.art-scene').focus();await page.keyboard.press('Home');await page.keyboard.press('ArrowRight');assert.match(await page.locator('.art-turn').evaluate(e=>e.style.transform),/15deg/);
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-art-refresh.png'});
 await page.keyboard.press('Escape');assert(await page.locator('#album-modal').isHidden());
 await page.locator('#album-rules-tab').click();
 for(const topic of ['play','open','threeopen','hidden','same','plus','reverse','affinity','legion','decimation']){
  await page.locator('#rules-list button').nth(['play','open','threeopen','hidden','same','plus','reverse','affinity','legion','decimation'].indexOf(topic)).click();
  if(['open','threeopen','hidden'].includes(topic)){
   assert(await page.locator('#rules-demo-controls').isHidden());assert(await page.locator('#rules-demo-caption').isHidden());
   assert.equal(await page.locator('#rules-demo button:enabled').count(),0);assert.equal(await page.locator('#rules-demo .demo-target').count(),0);
   assert.equal(await page.locator('#rules-rival-hand .card-back').count(),{open:0,threeopen:2,hidden:5}[topic]);
   await fits(['rules-title','rules-demo-board','rules-player-hand','rules-rival-hand']);continue;
  }
  if(topic==='play')assert.equal(await page.locator('#rules-demo-caption').innerText(),'Choose Piercing Twilight.');
  let total=await page.evaluate(()=>rulesFrames().length);
  assert.equal(await page.locator('#rules-demo-position').innerText(),`1 / ${total}`);
  if(topic==='hidden')assert.equal(await page.locator('#rules-rival-hand .card-back').count(),5);
  if(topic==='threeopen')assert.equal(await page.locator('#rules-rival-hand .card-back').count(),2);
  if(topic==='play')await page.waitForTimeout(800);assert.equal(await page.locator('#rules-demo-position').innerText(),`1 / ${total}`,'Lessons wait for input');
  await fits(['rules-title','rules-demo-board','rules-player-hand','rules-rival-hand','rules-demo-caption','rules-demo-replay']);
  for(let i=0;i<total-1;i++){
   const hand=page.locator('#rules-player-hand button:enabled');
   if(await hand.count()){await hand.click();assert.equal(await page.locator('#rules-demo-board button:enabled').count(),1);await page.locator('#rules-demo-board button:enabled').click();}
   else await page.locator('#rules-demo-next').click();
   await page.waitForFunction(()=>!rulesBusy);
   const expected=await page.evaluate(()=>rulesFrames()[rulesFrameIndex]);
   assert.equal(await page.locator('#rules-player-score').innerText(),String(expected.score[0]));
   for(let j=0;j<9;j++)if(expected.board[j])assert.equal(await page.locator(`#rules-demo-board button:nth-child(${j+1}) .owner-${expected.board[j].owner}`).count(),1);
  }
  assert.equal(await page.locator('#rules-panel').evaluate(e=>e.scrollTop),0,'The complete example fits without scrolling away the heading');
  if(width===1440&&topic==='plus')await page.screenshot({path:'build/clean-catalog/screenshots/album-rules-refresh.png'});
  if(topic==='plus'){await page.evaluate(s=>window.ttcgState({...s,revision:s.revision+1}),s);assert.equal(await page.locator('#rules-demo-position').innerText(),`${total} / ${total}`,'A state publication preserves the lesson position');}
  assert(await page.locator('#rules-demo-next').isHidden());await page.locator('#rules-demo-back').click();assert.equal(await page.locator('#rules-demo-position').innerText(),`${total-1} / ${total}`);
  await page.locator('#rules-demo-replay').click();assert.equal(await page.locator('#rules-demo-position').innerText(),`1 / ${total}`);
 }
 // Switching chapters cancels a capture animation before it can change the next example.
 await page.emulateMedia({reducedMotion:'no-preference'});await page.locator('#rules-list button').filter({hasText:/^Same$/}).click();await page.locator('#rules-player-hand button:enabled').click();await page.locator('#rules-demo-board button:enabled').click();
 await page.locator('#rules-list button').filter({hasText:/^Plus$/}).click();await page.waitForTimeout(1700);assert.equal(await page.locator('#rules-demo-position').innerText(),'1 / 2');
 await page.locator('#album-cards-tab').click();await page.locator('#album-card-0').click();
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-cards-refresh.png'});
 assert.deepEqual(errors,[]);assert.equal(commands.length,1,'Practice and inspection never mutate the collection');await page.close();console.log(`${width}×${height}: pack actions, full art, rotation/foil, all legacy rule examples and cancellation passed`);
}
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1)});
