const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
for(const [width,height]of [[1280,720],[1440,900],[3840,2160]]){
 const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
 const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8'}).stdout);s.session=100;s.revision=9;s.phase='result';s.score=[10,0];s.settling=false;
 s.fullHands=[[0,1,2,3,4],[5,6,7,8,9]];s.originalHands=structuredClone(s.fullHands);s.hands=[[null,null,null,null,4],[null,null,null,null,null]];s.board=[0,1,2,3,5,6,7,8,9].map((card,i)=>({card,owner:0,origin:i<4?i:i+1,flip:false}));
 Object.assign(s.collection,{trade:0,wager:0,staked:false,reward:false,paying:false,canPlay:false,playUnavailableReason:"They don't have enough cards for a game."});
 await page.evaluate(s=>window.ttcgState(s),s);await page.locator('#post-match').waitFor({state:'visible'});
 assert(await page.locator('#play-again').isHidden());assert.equal(await page.locator('#post-match .modal-actions button:visible').count(),1);assert.equal(await page.locator('#post-match-reason').innerText(),s.collection.playUnavailableReason);assert.equal(await page.evaluate(()=>document.activeElement.id),'post-leave');await page.keyboard.press('Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),'post-leave');
 const count=commands.length;await page.evaluate(()=>document.getElementById('play-again').click());assert.equal(commands.length,count,'Hidden rematch cannot issue prepare');
 s.revision++;s.collection.canPlay=true;s.collection.playUnavailableReason='';await page.evaluate(s=>window.ttcgState(s),s);assert(await page.locator('#play-again').isVisible());assert(await page.locator('#post-match-reason').isHidden());await page.locator('#play-again').click();assert(commands.at(-1).startsWith('prepare '));assert(await page.locator('#post-leave').isDisabled());
 // Preparation can fail even with two playable hands (for example, pending
 // settlement). A same-revision reply must release both buttons for retry/exit.
 s.collection.notice='The previous game is still finishing.';await page.evaluate(s=>window.ttcgState(s),s);
 assert(await page.locator('#post-leave').isEnabled());assert(await page.locator('#play-again').isEnabled());
 assert.equal(await page.locator('#post-match-reason').innerText(),s.collection.notice);
 await page.locator('#play-again').click();assert(await page.locator('#play-again').isDisabled());assert(await page.locator('#post-leave').isDisabled());
 // A late native rejection must also release Leave when a hand is depleted.
 s.collection.notice='';
 s.revision++;s.collection.canPlay=false;s.collection.playUnavailableReason="You don't have enough cards for a game.";await page.evaluate(s=>window.ttcgState(s),s);assert(await page.locator('#post-leave').isEnabled());assert(await page.locator('#play-again').isHidden());
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/rematch-unavailable.png'});
 await page.locator('#post-leave').click();assert(commands.at(-1).startsWith('close '));
 // All chapter states share exact geometry and keep only the left border.
 s.session++;s.revision=0;s.screen='album';s.phase='ready';s.collection.regions=[{id:1,name:'Whiterun',rules:0,trade:1},{id:8,name:'Eastmarch',rules:128,trade:2}];s.collection.holdID=1;s.collection.roster=[];s.collection.packs=[0,0];s.collection.revealed=[];
 await page.evaluate(s=>window.ttcgState(s),s);
 const style=el=>{const c=getComputedStyle(el);return {font:c.fontSize,padding:c.padding,lineHeight:c.lineHeight,border:[c.borderTopWidth,c.borderRightWidth,c.borderBottomWidth],background:c.backgroundImage,color:c.color};};
 await page.locator('#album-opponents-tab').click();await page.locator('#journal-chapter-8').click();await page.mouse.move(0,0);const journal=await page.locator('#journal-chapter-8').evaluate(style);assert.deepEqual(journal.border,['0px','0px','0px']);
 await page.locator('#journal-chapter-1').hover();const hover=await page.locator('#journal-chapter-1').evaluate(style);
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/journal-navigation-unified.png'});
 await page.locator('#album-rules-tab').click();await page.locator('#rules-topic-open').click();await page.mouse.move(0,0);assert.deepEqual(await page.locator('#rules-topic-open').evaluate(style),journal);
 await page.locator('#rules-topic-same').hover();assert.deepEqual(await page.locator('#rules-topic-same').evaluate(style),hover);
 await page.locator('#rules-topic-open').focus();await page.keyboard.press('Tab');const focus=await page.locator('#rules-topic-threeopen').evaluate(style);assert.deepEqual(focus.border,['0px','0px','0px']);
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/rules-navigation-unified.png'});
 await page.locator('#album-cards-tab').click();await page.locator('#album-card-0').click();await page.locator('#inspect-card').click();await page.locator('.art-scene').focus();await page.keyboard.press('Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),'inspect-flip','Tab advances exactly once');await page.keyboard.press('Shift+Tab');assert(await page.locator('.art-scene').evaluate(e=>e===document.activeElement));
 assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: unavailable rematch, native rejection recovery, Leave, unified navigation states and single focus trap passed`);
}
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1)});
