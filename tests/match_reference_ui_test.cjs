const {chromium}=require('playwright'),{spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
const base=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8'}).stdout.trim());
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1920,1080],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const match={...base,session:500,revision:0,screen:'lobby',phase:'match',opening:false,settling:false,thinking:false,turn:0,rules:18,collection:{...base.collection,fixedRules:18,trade:2}};
  await page.evaluate(s=>ttcgState(s),match);
  await page.locator('#board-rules .rule-chip').filter({hasText:'Plus'}).hover();
  assert(await page.locator('#rule-tooltip').isVisible());assert.match(await page.locator('#rule-tooltip').innerText(),/pairs add/);
  await page.locator('#board-rules .rule-chip').filter({hasText:'Plus'}).click();
  assert(await page.locator('#match-rules').isVisible());assert.equal(await page.locator('#rules-title').innerText(),'Plus');
  assert(await page.locator('#rule-tooltip').isHidden());assert.equal(await page.evaluate(()=>activeInteractionScope().id),'match-rules');
  const demoBox=await page.locator('#rules-demo-board').boundingBox(),panelBox=await page.locator('#rules-panel').boundingBox();
  assert(demoBox.x>=panelBox.x&&demoBox.x+demoBox.width<=panelBox.x+panelBox.width);
  const replayBox=await page.locator('#rules-demo-replay').boundingBox();assert(replayBox.y+replayBox.height<=height+1);
  assert.equal(await page.locator('#rules-rival-hand button').first().evaluate(el=>getComputedStyle(el).opacity),'1');
  if(width===1920)await page.screenshot({path:'build/match-rules-plus.png'});
  await page.locator('#rules-player-hand button:enabled').click();await page.locator('#rules-demo-board button:enabled').click();
  await page.waitForFunction(()=>!rulesBusy);assert.equal(commands.length,0,'Reference examples never play a live card');

  await page.locator('#rules-topic-trade').click();
  assert.equal(await page.locator('#rules-trades dt').count(),5);
  const copy=await page.locator('#rules-trades').innerText();for(const term of ['6–4','7–3','8–2','unplayed','draw'])assert(copy.includes(term));
  const panel=await page.locator('#rules-panel').boundingBox();assert(panel.y+panel.height<=height+1);assert(panel.width>width*.35,'Reference copy retains a readable column');
  assert.equal(await page.locator('#rules-list').evaluate(el=>getComputedStyle(el).flexDirection),'column');
  const details=await page.locator('#rules-trades').boundingBox();assert(details.y+details.height<=panel.y+panel.height+1,'All five trade rules fit');
  if(width===1920)await page.screenshot({path:'build/match-rules-trade.png'});
  await page.evaluate(()=>ttcgKey('cancel'));assert(await page.locator('#match-rules').isHidden());
  assert.equal(await page.locator('#album #album-rules').count(),1,'Shared chapter is returned to album');assert.equal(commands.length,0);
  // Trade inspection must not select a prize or lose the selection already made.
  const tradeState={...match,session:501,phase:'result',score:[6,4],originalHands:[[0,1,2,3,4],[5,6,7,8,9]],fullHands:[[0,1,2,3,4],[5,6,7,8,9]],collection:{...match.collection,trade:1,required:1,reward:true,staked:true,notice:''}};
  await page.evaluate(s=>ttcgState(s),tradeState);await page.waitForFunction(()=>trade?.phase==='selecting');
  await page.locator('#prize-1').click();await page.locator('#prize-2').click({button:'right'});
  assert(await page.locator('#table-inspection').isVisible());assert.equal(await page.locator('#table-inspect-types').innerText(),base.cards[7].groups.join(' · '));
  assert.deepEqual(await page.evaluate(()=>[...trade.choices]),[1]);assert.equal(commands.length,0);
  await page.evaluate(()=>ttcgKey('cancel'));assert(await page.locator('#reward').isVisible());
  await page.locator('#trade-card-0').focus();await page.keyboard.press('i');assert.equal(await page.locator('#table-inspect-name').innerText(),base.cards[0].name);
  await page.locator('#table-inspect-close').click();await page.locator('#prize-4').focus();await page.locator('#trade-inspect').click();assert.equal(await page.locator('#table-inspect-name').innerText(),base.cards[9].name);
  if(width===1920)await page.screenshot({path:'build/trade-inspection.png'});
  await page.locator('#table-inspect-close').click();await page.locator('#take-card').click();assert(commands.at(-1).endsWith(' 1'));
  // An opponent transfer waits for an open inspection, then resumes once.
  const loss={...tradeState,session:502,score:[4,6],collection:{...tradeState.collection,staked:false,reward:false,choice:2}};
  await page.evaluate(s=>ttcgState(s),loss);await page.waitForFunction(()=>trade?.phase==='opponent');await page.locator('#trade-card-2').click();
  await page.waitForTimeout(1200);assert(await page.locator('#table-inspection').isVisible());assert.equal(await page.evaluate(()=>trade.phase),'reviewing');
  await page.locator('#table-inspect-close').click();await page.waitForFunction(()=>!document.getElementById('post-match').hidden);
  assert.equal(await page.locator('#trade-owner-1 .card').count(),1);
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: rule hints/reference and trade inspection passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
