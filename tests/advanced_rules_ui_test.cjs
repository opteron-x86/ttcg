const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
const frames=arg=>{const r=spawnSync('/tmp/ttcg-advanced_rules-test',[arg],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(r.status,0,r.stderr);return r.stdout.trim().split('\n').map(JSON.parse);};
const matches=frames('--ui');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
for(const [width,height] of [[1280,720],[1440,900],[3840,2160]]){
 const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
 const base=JSON.parse(spawnSync('/tmp/ttcg-ui-host',[],{encoding:'utf8',maxBuffer:16*1024*1024}).stdout.trim());Object.assign(base.collection,{ruleLesson:2,known:base.cards.map(()=>true),packs:[0,0],revealed:[],revealID:0,ruleRequest:0,canPlay:true,practice:true,fixedRules:128,fixedTrade:0,fixedWager:0});
 let session=100;
 for(const rule of [64,128,256,32]){
  const demos=frames(String(rule));const s=structuredClone(base);Object.assign(s,{session:session++,screen:'lesson',phase:'ready',lesson:demos[0],matchContext:true});
  await page.evaluate(s=>ttcgState(s),s);assert.match(await page.locator('#demo-rules').innerText(),new RegExp({64:'Three Open',128:'Legion',256:'Decimation',32:'Affinity'}[rule]));
  if(rule===64)assert.equal(await page.locator('#demo-rival-hand .card-back').count(),2);
  for(let n=1;n<demos.length;++n){const d=demos[n-1];if(d.hand>=0){await page.locator('#demo-hand-0-'+d.hand).click();await page.locator('#demo-square-'+d.square).click();assert(commands.at(-1).endsWith(`place ${d.hand} ${d.square}`));}else{await page.locator('#demo-next').click();assert(commands.at(-1).endsWith('next'));}
   s.lesson=demos[n];s.revision=n;await page.evaluate(s=>ttcgState(s),s);
   if(n===1&&(rule===128||rule===256)){await page.waitForFunction(()=>!document.getElementById('demo-callout').hidden);assert.equal(await page.locator('#demo-callout').innerText(),rule===128?'Legion':'Decimation');
    const banner=await page.locator('#demo-callout .announcement-text').evaluate(el=>{const css=getComputedStyle(el);return {font:css.fontFamily,animation:css.animationName,variant:css.fontVariantCaps};});
    assert.match(banner.font,/Georgia/);assert.equal(banner.animation,'table-announcement');assert.equal(banner.variant,'small-caps');
    if(width===1440)await page.screenshot({path:`build/clean-catalog/screenshots/lesson-${rule===128?'legion':'decimation'}-banner.png`});}
   await page.waitForFunction(()=>!demoBusy);
  }
  assert(await page.locator('#demo-replay').isVisible());if(rule===256){assert.equal(await page.locator('#demo-square-4 .owner-1').count(),1);if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/decimation-lesson-0.14.0.png'});}
  if(rule===128||rule===32)assert.equal(await page.locator('#demo-square-4 .owner-0').count(),1);
  await page.locator('#demo-replay').click();assert(commands.at(-1).endsWith('replay'));
 }
 const decorate=m=>({...m,collection:base.collection,screen:'lobby',matchContext:true});let s=decorate(structuredClone(matches[0]));s.session=session++;
 await page.evaluate(s=>ttcgState(s),s);assert.equal(await page.locator('#rival-hand .card-back .affinity-modifier').count(),0);
 const next=decorate(structuredClone(matches[1]));next.session=s.session;await page.evaluate(s=>ttcgState(s),next);
 await page.waitForFunction(()=>!document.getElementById('rule-callout').hidden);assert.match(await page.locator('#rule-callout').innerText(),/Decimation.*Beast/);
 await page.waitForFunction(()=>!busy);assert.match(await page.locator('#group-status').innerText(),/Beast −1/);assert.equal(await page.locator('#square-0 .affinity-modifier').innerText(),'−1');
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/decimation-match-0.14.0.png'});
 s=decorate(structuredClone(matches[2]));s.session=session++;await page.evaluate(s=>ttcgState(s),s);await page.locator('#hand-0-1').click();await page.locator('#square-4').click();
 assert(await page.locator('#group-choice').isHidden());assert(commands.at(-1).endsWith('1 4'),'Single-group cards place immediately');
 const lobby=structuredClone(base);Object.assign(lobby,{session:session++,screen:'lobby'});await page.evaluate(s=>ttcgState(s),lobby);assert.equal(await page.locator('#rule-lesson').innerText(),'Show Legion');await page.locator('#rule-lesson').click();assert(commands.at(-1).startsWith('rule-lesson '));
 assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: four lessons, hidden modifiers, group announcement, single-group placement and lesson access passed`);
}
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
