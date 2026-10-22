const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 const page=await browser.newPage({viewport:{width:1440,height:900}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
 const state=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());state.screen='album';state.matchContext=false;state.revealCards=true;
 Object.assign(state.collection,{holdID:1,regions:[{id:1,name:'Whiterun',rules:16,trade:1},{id:11,name:'Travellers',travellers:true}],roster:[
  {base:1,name:'Ysolda',holdID:1,hold:'Whiterun',location:'Whiterun market',skill:'Master',wins:0},
  {base:0x954bf,name:"M'aiq the Liar",holdID:11,hold:'Travellers',traveller:true,location:'The roads of Skyrim',skill:'Master',wins:1,losses:2,draws:0,unlocked:true}]});
 await page.evaluate(s=>window.ttcgState(s),state);await page.locator('#album-opponents-tab').click();
 assert.equal(await page.locator('.opponent-row').count(),1);await page.locator('#journal-chapter-11').click();
 assert.equal(await page.locator('#journal-title').innerText(),'Travellers');assert.equal(await page.locator('.opponent-row').count(),1);
 assert.match(await page.locator('#journal-player').innerText(),/M'aiq the Liar/);assert.match(await page.locator('#journal-player').innerText(),/1 W · 2 L · 0 D/);
 assert.doesNotMatch(await page.locator('#journal-player').innerText(),/Hold|Whiterun/);assert.equal(await page.locator('#hold-rules').innerText(),'');assert(await page.locator('#progress-step').isHidden());
 await page.locator('#journal-chapter-0').click();assert.equal(await page.locator('.opponent-row').count(),2);
 await page.locator('#journal-search').fill("m'aiq");assert.equal(await page.locator('.opponent-row').count(),1);
 await page.locator('#album-cards-tab').click();await page.locator('#album-search').fill("M'aiq the Liar");
 await page.evaluate(()=>{$('album-filter').value='all';$('album-finish').value='normal';$('album-filter').dispatchEvent(new Event('change'));});
 const card=page.locator('#album-grid .collection-card');assert.equal(await card.count(),1);await card.click();
 const image=page.locator('#inspect-card img').first();assert(await image.evaluate(i=>i.complete&&i.naturalWidth>0));
 await page.screenshot({path:'build/clean-catalog/screenshots/maiq-card.png'});assert.deepEqual(errors,[]);
 console.log('Travellers: separate chapter, correct filtering/history, no local reputation or rules, and M’aiq artwork passed');
 }finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1)});
