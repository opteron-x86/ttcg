const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
function fixture(command,args=[]){const r=spawnSync(command,args,{encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(r.status,0,r.stderr);return JSON.parse(r.stdout.trim());}
const examples=fixture('/tmp/ttcg-new_rules-test',['--ui']),base=fixture('/tmp/ttcg-ui-host');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1920,1080]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const decorate=m=>({...m,screen:'lobby',matchContext:true,collection:{...base.collection,fixedRules:m.rules,fixedTrade:0,fixedWager:0,canPlay:true,staked:false,reward:false}});
  for(const rule of ['order','chaos']){
   const s=decorate(examples[rule]);await page.evaluate(s=>ttcgState(s),s);
   assert.equal(await page.locator('#player-hand [data-placeable="true"]').count(),1);
   assert.equal(await page.locator(`#hand-0-${s.forcedHand}`).getAttribute('aria-pressed'),'true');
   const wrong=(s.forcedHand+1)%5,before=commands.length;
   await page.evaluate(h=>requestPlacement(h,0),wrong);assert.equal(commands.length,before);
   await page.locator(`#hand-0-${wrong}`).click();assert(await page.locator('#table-inspection').isVisible());
   await page.locator('#table-inspect-close').click();await page.locator('#square-0').click();
   assert(commands.at(-1).endsWith(`${s.forcedHand} 0`));
  }
  commands.length=0;await page.evaluate(s=>ttcgState(s),decorate(examples.swap));
  await page.waitForFunction(()=>document.getElementById('rule-callout').textContent==='Swap');
  assert.equal(await page.locator('.swap-highlight').count(),2);
  await page.evaluate(s=>ttcgState(s),decorate(examples.swap));
  assert.equal(await page.locator('.swap-highlight').count(),2,'Duplicate updates keep Swap highlights');
  await page.waitForTimeout(1300);assert.equal(await page.locator('.swap-highlight').count(),2,'Swap stays visible longer than the old 1.1 second callout');
  assert(!commands.some(c=>c.startsWith('opening ')),'Starting-player audio waits for Swap');
  await page.waitForFunction(()=>!busy);assert.equal(commands.filter(c=>c.startsWith('opening ')).length,1);assert(commands.at(-1).startsWith('settled '));
  await page.evaluate(s=>ttcgState(s),decorate(examples.suddenBefore));commands.length=0;
  await page.evaluate(s=>ttcgState(s),decorate(examples.suddenDraw));
  await page.waitForFunction(()=>document.getElementById('rule-callout').textContent==='Sudden Death · Redeal');
  assert(await page.locator('#post-match').isHidden());assert(await page.locator('#reward').isHidden());
  await page.waitForFunction(()=>!busy);assert(!commands.some(c=>c.startsWith('result ')));assert(commands.at(-1).startsWith('settled '));
  await page.evaluate(s=>ttcgState(s),decorate(examples.suddenRedeal));await page.waitForFunction(()=>!busy);
  assert.equal(await page.locator('#board .card').count(),0);assert.equal(await page.locator('#player-hand .card').count(),5);assert.equal(await page.locator('#rival-hand .card').count(),5);
  // Prize positions and finishes refer to original copies, not redealt hands.
  const traded=decorate(examples.trade);traded.score=[6,4];Object.assign(traded.collection,{trade:1,reward:true,staked:true,required:1});
  await page.evaluate(s=>ttcgState(s),traded);await page.waitForFunction(()=>trade?.phase==='selecting');
  assert.deepEqual(await page.evaluate(()=>trade.hands),traded.originalHands);
  assert.equal(await page.locator('#trade-card-0 .foil').count(),1);
  for(let h=0;h<5;++h)assert.equal(await page.locator(`#prize-${h} .card-name`).innerText(),traded.cards[traded.originalHands[1][h]].name);
  await page.locator('#prize-2').click();await page.locator('#take-card').click();assert(commands.at(-1).endsWith('2'));
  // The longest valid combination remains above the board at every UI scale.
  const crowded=decorate({...examples.order,session:200,rules:31415});await page.evaluate(s=>ttcgState(s),crowded);
  const layout=await page.evaluate(()=>{const chips=document.querySelector('#game .rule-chips'),b=document.getElementById('board').getBoundingClientRect();return {chips:chips.getBoundingClientRect().bottom,board:b.top,viewport:innerHeight,bottom:document.getElementById('match-footer').getBoundingClientRect().bottom};});
  assert(layout.chips<=layout.board+1,JSON.stringify(layout));assert(layout.bottom<=height+1,JSON.stringify(layout));
  const album={...base,session:220,screen:'album',collection:{...base.collection,known:base.cards.map(()=>true),packs:[0,0],revealed:[],fixedRules:31415}};
  await page.evaluate(s=>ttcgState(s),album);await page.locator('#album-rules-tab').click();
  for(const [topic,label] of [['order','Order'],['chaos','Chaos'],['fallenace','Fallen Ace'],['suddenDeath','Sudden Death'],['samewall','Same Wall'],['swap','Swap']]){
   await page.locator(`#rules-topic-${topic}`).click();assert.equal(await page.locator('#rules-title').innerText(),label);
   const total=await page.evaluate(()=>rulesFrames().length);assert(total>1);assert(await page.locator('#rules-demo-caption').innerText());
   for(let n=1;n<total;++n){
    const hand=page.locator('#rules-player-hand button:enabled');if(await hand.count()){await hand.click();await page.locator('#rules-demo-board button:enabled').click();}else await page.locator('#rules-demo-next').click();
    await page.waitForFunction(()=>!rulesBusy);
   }
   assert.equal(await page.locator('#rules-demo-position').innerText(),`${total} / ${total}`);
   const box=await page.locator('#rules-demo-replay').boundingBox();assert(box&&box.y+box.height<=height+1);
   assert(await page.locator('#rules-demo-next').isHidden());
   if(topic==='samewall'){assert.equal(await page.locator('#rules-demo-board button:nth-child(3) .owner-0').count(),1);}
   if(width===1920&&topic==='samewall')await page.screenshot({path:'/tmp/tessera-same-wall.png'});
  }
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: forced hands, Swap opening, Sudden Death transition, crowded rules and six playable examples passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
