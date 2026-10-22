const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
const fixture=(program,input='',args=[])=>spawnSync(program,args,{input,encoding:'utf8'}).stdout.trim().split('\n').map(JSON.parse);
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 const frames=fixture('/tmp/ttcg-lesson-test','',['--frames']);
 for(const [width,height] of [[1280,720],[1440,900],[3840,2160]]){
   const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
   page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
   const s=fixture('/tmp/ttcg-ui-host')[0];Object.assign(s,{screen:'lesson',lesson:frames[0],opponent:'Hulda',matchContext:false});
   Object.assign(s.collection,{tutorial:true,practice:true,ruleRequest:0,canPlay:true,unlocked:false,introduced:true});
   const update=async()=>page.evaluate(x=>window.ttcgState(x),s);
   const idle=async()=>page.waitForFunction(()=>!demoBusy);
   const fits=async(ids)=>{const f=await page.locator('#table').boundingBox();for(const id of ids){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=f.x-1&&b.y>=f.y-1&&b.x+b.width<=f.x+f.width+1&&b.y+b.height<=f.y+f.height+1,`${width}: ${id} fits`);}};
   await update();assert(await page.locator('#game').isHidden());assert(await page.locator('#music').isHidden());
   assert.equal(await page.locator('#demo-player-hand .card').count(),5);
   assert(await page.locator('#table').evaluate(el=>el.classList.contains('table-lesson')));
   assert.equal(await page.locator('#demo-rules').innerText(),'Open');
   assert(await page.locator('#demo-player-nameplate').evaluate(el=>el.classList.contains('active')));
   await fits(['demo-player-nameplate','demo-rival-nameplate','demo-rules','demo-player-hand','demo-rival-hand','demo-board','demo-prompt','demo-play']);
   for(let step=0;step<9;step++){
     const d=frames[step];
     if(d.hand>=0){
       assert.equal(await page.locator('#demo-player-hand button:enabled').count(),1);
       assert.equal(await page.locator('#demo-board button:enabled').count(),0);
       // Keyboard/controller acceptance follows the same path as a mouse click.
       await page.locator(`#demo-hand-0-${d.hand}`).focus();await page.evaluate(()=>window.ttcgKey('confirm'));
       assert.equal(await page.locator('#demo-board button:enabled').count(),1);
       assert.equal(await page.locator(`#demo-square-${d.square}`).getAttribute('class'),'demo-square demo-target');
       await page.evaluate(()=>window.ttcgKey('confirm'));
       assert(commands.at(-1).endsWith(`place ${d.hand} ${d.square}`));
     }else{await page.locator('#demo-next').click();assert(commands.at(-1).endsWith('next'));}
     const count=commands.length;await page.evaluate(()=>document.getElementById('demo-next').onclick());assert.equal(commands.length,count,'No duplicate command while awaiting state');
     s.lesson=frames[step+1];s.revision++;
     await update();await page.evaluate(x=>window.ttcgState(x),s); // Duplicate publications must not restart playback.
     if(step===2){
       assert(await page.locator('#demo-square-4 .owner-1').count());
       assert.equal(await page.locator('#demo-player-score').innerText(),'5');
       await page.waitForTimeout(450);assert.equal(await page.locator('#demo-board .demo-rank').count(),2);
     }
     await idle();
     assert.equal(await page.locator('#demo-player-score').innerText(),String(frames[step+1].score[0]));
     for(let i=0;i<9;i++)if(s.lesson.board[i])assert.equal(await page.locator(`#demo-square-${i} .owner-${s.lesson.board[i].owner}`).count(),1);
     await fits(['demo-prompt','demo-play']);
     if(width===1440&&step===2){await page.screenshot({path:'build/clean-catalog/screenshots/guided-lesson.png'});}
   }
   assert.match(await page.locator('#demo-prompt').innerText(),/unplayed card counts/);
   assert.equal(await page.locator('#demo-rival-hand .card').count(),1);
   assert(await page.locator('#demo-replay').isVisible());
   await page.locator('#demo-replay').click();assert(commands.at(-1).endsWith('replay'));
   s.session++;s.revision=0;s.lesson=frames[0];await update();assert.equal(await page.locator('#demo-board .card').count(),0);
   await page.locator('#demo-play').click();assert(commands.at(-1).endsWith('play'));
   // Even a fixture containing real rival IDs must never render a pre-game hand.
   s.screen='lobby';s.rules=s.collection.fixedRules=16;s.collection.fixedTrade=0;s.collection.fixedWager=0;s.collection.maxWager=0;s.session++;await update();
   assert.equal(await page.locator('#opponent-deck').count(),0);assert(await page.locator('#guided-lesson').isHidden());
   assert.equal(await page.locator('#selected-deck .card').count(),5);assert.equal(await page.locator('#lobby-editor').count(),0);await fits(['selected-deck','start']);
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/lobby-0.11.0.png'});
   // Use actual snapshots from the match core to verify Open/Closed boundary.
   const packets=fixture('/tmp/ttcg-ui-host','start 1 0 16 0 0 0 1 2 3 4\n');
   const open=packets.at(-1);assert.equal(open.phase,'match');open.settling=false;open.opening=false;open.screen='lobby';
   await page.evaluate(x=>window.ttcgState(x),open);
   assert.equal(await page.locator('#rival-hand .card-face').count(),5);
   open.rules=0;open.hands[1]=Array(5).fill(-2);open.fullHands[1]=Array(5).fill(-2);open.revision++;await page.evaluate(x=>window.ttcgState(x),open);
   assert.equal(await page.locator('#rival-hand .card-back').count(),5);
   // Leave during an animation, then reopen. No old timer may alter the next view.
   s.screen='lesson';s.session+=10;s.lesson=frames[2];await update();s.lesson=frames[3];s.revision++;await update();
   await page.keyboard.press('Escape');assert(commands.at(-1).startsWith('close '));
   s.session++;s.lesson=frames[0];await update();await page.waitForTimeout(1600);
   assert.equal(await page.locator('#demo-board .card').count(),0);assert.equal(await page.locator('#demo-player-score').innerText(),'5');
   await page.emulateMedia({reducedMotion:'reduce'});s.lesson=frames[1];s.revision++;await update();await idle();assert.equal(await page.locator('#demo-board .card').count(),1);
   assert.deepEqual(errors,[]);console.log(`${width}x${height}: complete lesson, captures, focus, guards, replay, Open lobby/match boundary, cancellation and reduced motion passed`);await page.close();
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
