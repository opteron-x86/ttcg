const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]) {
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('ttcgCommand',s=>commands.push(s));
  await page.goto(pathToFileURL(path.resolve(process.env.TTCG_UI_ROOT||'TesseraUI','index.html')).href);
  // Sample the rendered cards during movement as well as at settled phases.
  // Disabled trade slots lock input; they must not fade the artwork on arrival.
  await page.evaluate(()=>{
   window.tradeDimFrames=[];
   const sample=()=>{
    for(const card of document.querySelectorAll('.trade-hand .card,.trade-received>.card,.trade-flight>.card')){
     if(!card.checkVisibility())continue;
     let opacity=1;
     for(let node=card;node;node=node.parentElement)opacity*=Number(getComputedStyle(node).opacity);
     if(opacity<.99&&window.tradeDimFrames.length<20)window.tradeDimFrames.push({phase:trade?.phase,slot:card.parentElement.id||card.parentElement.className,opacity});
    }
    window.tradeAppearanceFrame=requestAnimationFrame(sample);
   };
   sample();
  });
  const initial=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  // At the primary viewport, exercise foil snapshots through gathering and transfers.
  if(width===1440)for(const card of initial.cards)card.foil=true;
  const fixture=(session,score=[6,4],one=true,wager=25)=>{
   const s=structuredClone(initial);s.session=session;s.revision=9;s.phase='result';s.score=score;
   s.fullHands=[[0,1,2,3,4],[8,9,10,9,10]];s.originalHands=structuredClone(s.fullHands);
   s.hands=[[null,null,null,null,4],[null,null,null,null,null]];
   s.board=[...s.fullHands[0].slice(0,4),...s.fullHands[1]].map((card,i)=>({card,owner:i%2,flip:false}));
   Object.assign(s.collection,{trade:one?1:0,wager,staked:score[0]>score[1]&&one,reward:score[0]>score[1]&&one,choice:2,paying:false,notice:''});
   return s;
  };
  let s=fixture(20);
  await page.evaluate(s=>window.ttcgState(s),s);
  assert(await page.locator('.trade-flight').count()>0,'Cards travel off the board');
  assert(await page.locator('#take-card').isDisabled());
  await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='selecting');
  assert.equal(await page.locator('.trade-hand .card').count(),10);
  assert.equal(await page.locator('#trade-gold').innerText(),'+25 gold');
  const frame=await page.locator('#table').boundingBox();
  for(const item of await page.locator('.trade-panel,.trade-hand button,.trade-received,#take-card').all()) {
   const b=await item.boundingBox();assert(b.x>=frame.x&&b.y>=frame.y&&b.x+b.width<=frame.x+frame.width&&b.y+b.height<=frame.y+frame.height,'Trade fits the window');
  }
  await page.locator('#prize-3').click();
  assert.equal(await page.locator('.trade-hand .selected').count(),1);
  const chosen=await page.locator('#prize-3').boundingBox(),other=await page.locator('#prize-2').boundingBox();
  await page.waitForTimeout(250);
  assert((await page.locator('#prize-3').boundingBox()).y<other.y,'Selection lifts');
  if(width===1440) await page.screenshot({path:'build/clean-catalog/screenshots/trade-selection.png'});
  await page.locator('#take-card').click();
  await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='claiming');
  assert.equal(commands.filter(c=>c.startsWith('claim ')).length,1);
  await page.evaluate(()=>document.getElementById('take-card').click());
  assert.equal(commands.filter(c=>c.startsWith('claim ')).length,1,'Double confirms cannot duplicate payouts');
  assert.equal(await page.locator('#trade-owner-0 .card').count(),0,'No transfer before native acknowledgement');
  // Same-revision messages must preserve the selected slot and animation.
  await page.evaluate(s=>window.ttcgState(s),s);
  assert.equal(await page.locator('#prize-3').getAttribute('aria-pressed'),'true');
  s.collection.staked=false;s.collection.reward=false;
  await page.evaluate(s=>window.ttcgState(s),s);
  await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='done');
  assert.equal(await page.locator('#trade-owner-0 .card-name').innerText(),s.cards[9].name);
  assert.equal(await page.locator('#trade-owner-0 .owner-0').count(),1);
  assert.equal(await page.locator('#prize-3.transferred').count(),1);
  assert.equal(await page.locator('#prize-1.transferred').count(),0,'The other identical copy stays');
  await page.waitForFunction(()=>!document.getElementById('post-match').hidden);
  assert(await page.locator('#reward').isHidden());assert(await page.locator('#match-footer').isHidden());
  assert.equal(await page.locator('#turn').innerText(),'');
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/post-match.png'});
  await page.locator('#play-again').click();assert(commands.at(-1).startsWith('prepare '));
  // Opponent choice and ownership come from native settlement.
  s=fixture(21,[4,6]);await page.evaluate(s=>window.ttcgState(s),s);
  await page.waitForFunction(()=>document.querySelector('#trade-card-2.selected'));
  assert(await page.locator('#take-card').isHidden(),'No confirmation for opponent choice');
  assert.equal(await page.locator('#trade-gold').innerText(),'−25 gold');
  assert.equal(await page.locator('.trade-hand button:enabled').count(),10,'Cards are available for inspection, with no choice action');
  if(width===1440) await page.screenshot({path:'build/clean-catalog/screenshots/trade-opponent.png'});
  await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='done');
  assert.equal(await page.locator('#trade-owner-1 .owner-1').count(),1);
  assert.equal(await page.locator('#trade-owner-1 .card-name').innerText(),s.cards[2].name);
  await page.waitForFunction(()=>!document.getElementById('post-match').hidden);
  await page.locator('#post-leave').click();assert(commands.at(-1).startsWith('close '));assert(await page.locator('#table').isHidden());
  // Draw and gold-only games have no selected/transferred card.
  for(const [session,score,one,goldText] of [[22,[5,5],true,'25 gold returned'],[23,[6,4],false,'+25 gold'],[24,[4,6],false,'−25 gold']]) {
   s=fixture(session,score,one);await page.evaluate(s=>window.ttcgState(s),s);
   assert(await page.locator('#take-card').isHidden(),'No confirmation without a player card choice');
   await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='done');
   assert.equal(await page.locator('#trade-gold').innerText(),goldText);
   assert.equal(await page.locator('.trade-received .card').count(),0);
  }
  // Cancellation clears all clones and makes the lobby usable again.
  s=fixture(25);await page.evaluate(s=>window.ttcgState(s),s);
  await page.evaluate(()=>window.ttcgReset());
  assert.equal(await page.locator('.trade-flight').count(),0);
  const ready=structuredClone(initial);ready.session=26;await page.evaluate(s=>window.ttcgState(s),ready);
  await page.waitForTimeout(1100);
  assert(await page.locator('#reward').isHidden());assert(await page.locator('#challenge').isVisible());
  await page.emulateMedia({reducedMotion:'reduce'});
  s=fixture(27,[4,6]);await page.evaluate(s=>window.ttcgState(s),s);
  await page.waitForFunction(()=>document.getElementById('reward').dataset.phase==='done');
  assert.equal(await page.locator('.trade-flight').count(),0);
  assert.deepEqual(await page.evaluate(()=>{cancelAnimationFrame(window.tradeAppearanceFrame);return window.tradeDimFrames;}),[],'Trade artwork stays opaque through gathering, choice and transfer');
  assert.deepEqual(errors,[]);
  console.log(`${width}x${height}: trade gathering, selection, confirmed ownership, duplicates, opponent choice, gold, draws, reset and reduced motion passed`);
  await page.close();
 }
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
