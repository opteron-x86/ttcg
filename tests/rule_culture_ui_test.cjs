const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
function packets(binary,args=[]){const p=spawnSync(binary,args,{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(p.status,0,p.stderr);return p.stdout.trim().split('\n').map(JSON.parse);}
const fixtures=packets('/tmp/ttcg-rule_culture-test',['--ui']),base=packets('/tmp/ttcg-ui-host')[0];
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1440,900],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  let session=500;const s=structuredClone(base);Object.assign(s,{screen:'lobby',phase:'ready',settling:false,opponent:'Local player'});
  Object.assign(s.collection,{practice:true,canPlay:true,ruleRequest:0,fixedTrade:0,fixedWager:0,maxWager:0});
  async function update(f){s.session=session++;s.revision=0;Object.assign(s.collection,structuredClone(f));s.rules=s.collection.fixedRules;await page.evaluate(s=>ttcgState(s),s);}
  await update(fixtures[0]);assert(await page.locator('#rule-request').isVisible());assert.match(await page.locator('#rule-request-text').innerText(),/Decimation from Solstheim.*replaces Legion/);
  await page.locator('#decline-rule').click();assert(commands.at(-1).endsWith('trial 0'));
  await update(fixtures[0]);await page.locator('#accept-rule').click();assert(commands.at(-1).endsWith('trial 1'));assert(await page.locator('#accept-rule').isDisabled());
  await update(fixtures[1]);assert(await page.locator('#rule-request').isHidden());assert.match((await page.locator('#normal-rules .rule-chip').allTextContents()).join(' · '),/Open · Decimation/);assert.match(await page.locator('#culture-status').innerText(),/Trying Decimation/);assert(await page.locator('#start').isEnabled());
  await update(fixtures[2]);assert.match(await page.locator('#culture-result').textContent(),/Decimation has spread to Eastmarch.*Legion has been abolished/);
  await update(fixtures[3]);assert(await page.locator('#abolition-open').isVisible());await page.locator('#abolition-open').click();assert(await page.locator('#abolition-dialog').isVisible());assert(await page.locator('#abolition-confirm').isDisabled());
  assert.deepEqual(await page.locator('#abolition-options button').allTextContents(),['Open','Decimation']);
  assert(await page.locator('#abolition-options button').nth(1).isDisabled());
  assert.match(await page.locator('#abolition-options button').nth(1).getAttribute('title'),/temporarily protected/);
  await page.locator('#abolition-options button').first().click();assert.match(await page.locator('#abolition-description').innerText(),/Open still active.*Unplayed hands will then be hidden.*seven game days/);
  const count=commands.length;await page.keyboard.press('Escape');assert(await page.locator('#abolition-dialog').isHidden());assert.equal(commands.length,count);
  await page.locator('#abolition-open').click();await page.locator('#abolition-options button').first().click();
  for(let n=0;n<8;n++){await page.keyboard.press('Tab');assert(await page.evaluate(()=>!!document.activeElement.closest('#abolition-dialog')));}
  const box=await page.locator('#abolition-dialog .modal').boundingBox();assert(box.x>=0&&box.y>=0&&box.x+box.width<=width+1&&box.y+box.height<=height+1);
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/rule-abolition.png'});
  await page.locator('#abolition-confirm').click();assert(commands.at(-1).endsWith('abolish 16'));
  await update(fixtures[4]);assert(await page.locator('#abolition-dialog').isHidden());assert.match(await page.locator('#culture-status').innerText(),/Abolition challenge.*Open.*remains active/);assert(await page.locator('#abolition-open').isHidden());
  await update(fixtures[5]);assert.match(await page.locator('#culture-status').innerText(),/seven|7 game days/);assert(await page.locator('#abolition-open').isHidden());
  // No Closed badge, including a hold with no remaining special rules.
  await update({fixedRules:0,culture:{localRules:0},hold:'Hjaalmarch',holdID:4});assert.match((await page.locator('#normal-rules .rule-chip').allTextContents()).join(' · '),/Hidden hands/);assert.doesNotMatch((await page.locator('#normal-rules .rule-chip').allTextContents()).join(' · '),/\bOpen\b|\bClosed\b/);
  // Stacked modifiers display their actual amount, including effective rank tooltips.
  const mods=await page.evaluate(()=>[2,-2].map(mod=>{const card=cardElement(0,0,mod);return {label:card.querySelector('.affinity-modifier').textContent,title:card.querySelector('.rank').title};}));assert.deepEqual(mods.map(m=>m.label),['+2','−2']);assert.match(mods[0].title,/\+ 2 =/);assert.match(mods[1].title,/− 2 =/);
  s.screen='album';await page.evaluate(()=>{albumTab='opponents';journalHold=8;});
  await update({...fixtures[2],regions:[{id:8,name:'Eastmarch',rules:fixtures[2].fixedRules,trade:2,ruleChange:fixtures[2].culture.result}]});
  assert.equal(await page.locator('#journal-rule-change').count(),0);
  assert.match(await page.locator('#hold-rules').textContent(),/Open.*Decimation/);assert(!/Legion/.test(await page.locator('#hold-rules').textContent()));
  assert.equal(await page.locator('#journal-carried-rules').count(),0);
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: native spread offers, conflict terms, abolition selection/cooldowns, keyboard focus, changed lessons, journal history and ±2 display passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
