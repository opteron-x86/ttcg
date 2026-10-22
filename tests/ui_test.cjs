const { chromium }=require(process.env.TTCG_PLAYWRIGHT || 'playwright');
const { spawn, spawnSync }=require('node:child_process');
const { createInterface }=require('node:readline');
const { pathToFileURL }=require('node:url');
const path=require('node:path');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
  for(const [width,height] of JSON.parse(process.env.TTCG_VIEWPORTS||'[[1440,900],[1280,720],[2560,1080],[3840,2160]]')) {
   const page=await browser.newPage({viewport:{width,height}});
   const errors=[]; page.on('pageerror',error=>errors.push(error.message));
   const host=spawn('/tmp/ttcg-ui-host'); let current;
   const received=[]; const waits=[];
   createInterface({input:host.stdout}).on('line',line=>{
    const state=JSON.parse(line); current=state;
    if(waits.length) waits.shift()(state); else received.push(state);
   });
   const next=()=>received.length?Promise.resolve(received.shift()):new Promise(resolve=>waits.push(resolve));
   let commands=0, live=true; const acknowledgements=[],ruleSounds=[],resultSounds=[];
   await page.exposeFunction('ttcgCommand',async text=>{ if(!live) { if(text.startsWith('rule ')) ruleSounds.push(text.split(' ').at(-1)); else if(text.startsWith('result ')) resultSounds.push(text); else acknowledgements.push(text); return; } if(text.startsWith('play ')) commands++; host.stdin.write(text+'\n'); const state=await next(); await page.evaluate(s=>window.ttcgState(s),state); });
   await page.goto(pathToFileURL(path.resolve(process.env.TTCG_UI_ROOT||'TesseraUI','index.html')).href);
   await page.evaluate(s=>window.ttcgState(s),await next());
   const select=async(id,value)=>{ await page.locator('#'+id+'-trigger').click(); await page.locator('#'+id+'-options [data-value=\"'+value+'\"]').click(); };
   assert.match((await page.locator('#normal-rules .rule-chip').allTextContents()).join(' · '),/Open · Same · Plus/);
   await page.locator('#start').click();
   await page.waitForFunction(()=>!document.getElementById('game').hidden);
   await page.waitForFunction(()=>document.querySelector('#player-hand button[data-placeable="true"]'));
   await page.locator('#player-hand button[data-placeable="true"]').first().click();
   if(width===1440) {
    await page.screenshot({path:'build/clean-catalog/screenshots/prototype-board.png'});
    await page.screenshot({path:'/tmp/ttcg-window-alpha.png',omitBackground:true});
   }
   const bounds=await page.locator('#board').boundingBox();
   assert(bounds.x>=0&&bounds.y>=0&&bounds.x+bounds.width<=width&&bounds.y+bounds.height<=height);
   for(const size of [60,75,90,95]) {
    await select('window-size',String(size));
    const frame=await page.locator('#table').boundingBox();
    assert(frame.x>0&&frame.y>0&&frame.x+frame.width<width&&frame.y+frame.height<height);
    assert(Math.abs(frame.x+frame.width/2-width/2)<1&&Math.abs(frame.y+frame.height/2-height/2)<1);
    for(const id of ['board','player-hand','rival-hand','turn']) {
     const box=await page.locator('#'+id).boundingBox();
     assert(box.x>=frame.x&&box.y>=frame.y&&box.x+box.width<=frame.x+frame.width&&box.y+box.height<=frame.y+frame.height,`${id} outside ${size}% frame`);
    }
   }
   await select('window-size','90');
   if(width===3840) {
    const square=await page.locator('#square-0').boundingBox();
    const hand=await page.locator('#hand-0-0').boundingBox();
    assert(square.height>410&&hand.height>328,'4K cards should exceed twice the old fixed caps');
   }
   // Controller cancel still deselects; Escape is tested as a direct exit below.
   await page.evaluate(()=>window.ttcgKey('cancel'));
   assert.equal(await page.locator('.selected').count(),0);
   let placements=0;
   while(await page.locator('#play-again').isHidden()) {
    await page.locator('#player-hand button[data-placeable="true"]').first().click();
    const previous=commands;
    await page.locator('#board .square.available:not(:disabled)').first().click();
    await page.waitForFunction(()=>document.querySelector('#player-hand button[data-placeable="true"]')||!document.getElementById('post-match').hidden);
    assert.equal(commands,previous+1); placements++;
    assert(placements<=5);
   }
   assert.equal(await page.locator('#board .card').count(),9);
   assert.equal(current.score[0]+current.score[1],10);
   await page.locator('#play-again').click();
   await page.waitForFunction(()=>document.getElementById('post-match').hidden);
   await page.waitForFunction(()=>!document.getElementById('challenge').hidden);
   await page.locator('#start').click();
   await page.waitForFunction(()=>document.querySelector('#player-hand button[data-placeable="true"]'));
   assert.equal(errors.length,0,errors.join('\n'));
   // Native controller navigation entry point chooses and plays a card.
   await page.evaluate(()=>window.ttcgKey('confirm'));
   assert.equal(await page.locator('.selected').count(),1);
   await page.evaluate(()=>window.ttcgKey('cancel'));
   assert.equal(await page.locator('.selected').count(),0);
   const musicRevision=current.revision;
   await page.locator('#music').click();
   await page.waitForFunction(()=>document.getElementById('music').getAttribute('aria-pressed')==='false');
   assert.equal(current.revision,musicRevision);
   await page.locator('#music').click();
   await page.waitForFunction(()=>document.getElementById('music').getAttribute('aria-pressed')==='true');
   // Dismiss an uncommitted option without changing size or leaving the match.
   await page.locator('#window-size-trigger').click();
   await page.keyboard.press('ArrowUp'); await page.keyboard.press('Escape');
   assert.equal(await page.locator('#window-size').inputValue(),'90');
   assert(await page.locator('#table').isVisible());
   live=false;
   // The first special capture seeds two separately presented Combo waves.
   const packets=spawnSync('/tmp/ttcg-ui-host',['--fixtures'],{encoding:'utf8'}).stdout.trim().split('\n').map(JSON.parse);
   await page.evaluate(s=>window.ttcgState(s),packets[0]);
   const fixture=packets[1];
   await page.evaluate(s=>{ window.ttcgState(s); window.ttcgState({...s,thinking:true}); },fixture);
   assert.equal(await page.locator('#rule-callout').innerText(),'Plus');
   assert.equal(await page.locator('#rule-callout .announcement-text').count(),1);
   const sweeps=await page.locator('.announcement-text').evaluate(el=>['::before','::after'].map(p=>getComputedStyle(el,p).animationName));
   assert.deepEqual(sweeps,['table-rule-line','table-rule-line-reverse']);
   assert.equal(await page.locator('.rule-highlight').count(),3);
   assert.equal(await page.locator('#square-4 .owner-1').count(),1,'Keep old owner during callout');
   assert.equal(await page.locator('#player-hand button[data-placeable="true"]').count(),0);
   await page.waitForFunction(()=>document.querySelector('#square-4 .card').getAnimations().length>0);
   assert(await page.locator('#rule-callout').isHidden(),'Callout clears before flipping');
   await page.evaluate(s=>{
    window.capturedNode=document.querySelector('#square-4 .card');
    window.captureAnimation=window.capturedNode.getAnimations()[0];
    window.ttcgState({...s,thinking:true});
   },fixture);
   assert(await page.evaluate(()=>window.capturedNode===document.querySelector('#square-4 .card')&&window.captureAnimation===window.capturedNode.getAnimations()[0]));
   await page.waitForFunction(()=>!document.getElementById('rule-callout').hidden&&document.getElementById('rule-callout').textContent==='Combo');
   assert.equal(await page.locator('#square-4 .owner-0').count(),1);
   assert.equal(await page.locator('#square-3 .owner-1').count(),1);
   if(width===1440) { await page.waitForTimeout(160); await page.screenshot({path:'build/clean-catalog/screenshots/prototype-combo.png'}); }
   await page.waitForFunction(()=>document.querySelector('#square-3 .owner-0')&&document.getElementById('rule-callout').hidden);
   assert.equal(await page.locator('#square-6 .owner-1').count(),1,'Later Combo wave stays unflipped');
   await page.waitForFunction(()=>document.querySelector('#square-6 .owner-0')&&document.getElementById('rule-callout').hidden&&document.querySelector('#square-6 .card').getAnimations().length===0);
   // The final flip can finish before its native acknowledgement crosses the browser bridge.
   for(let attempt=0;!acknowledgements.length&&attempt<200;attempt++)await new Promise(resolve=>setTimeout(resolve,10));
   assert.deepEqual(acknowledgements,[`settled ${fixture.session} ${fixture.revision}`]);
   assert.deepEqual(ruleSounds,['plus','combo'],'Combo announces and sounds only once across all waves');
   fixture.settling=false;
   await page.evaluate(s=>window.ttcgState(s),fixture);
   // Opponent recapture, without a callout, retains the full flip too.
   fixture.revision++; fixture.turn=0; fixture.settling=true;
   for(const cell of fixture.board) if(cell) cell.flip=false;
   fixture.board[3]={card:8,owner:1,flip:true};
   fixture.stages=[{label:'',highlights:0,captures:8}];
   await page.evaluate(s=>window.ttcgState(s),fixture);
   assert.equal(await page.locator('#square-3 .owner-0').count(),1);
   assert.equal(await page.locator('#player-hand button[data-placeable="true"]').count(),0);
   await page.waitForFunction(()=>document.querySelector('#square-3 .owner-1')&&document.querySelector('#square-3 .card').getAnimations().length===0);
   assert.equal(acknowledgements.length,2);
   // Session changes cancel pending callouts and their acknowledgement.
   fixture.revision++; fixture.stages=[{label:'Same',highlights:8,captures:8}];
   fixture.board[3]={card:8,owner:0,flip:true};
   await page.evaluate(s=>window.ttcgState(s),fixture);
   assert.equal(await page.locator('#rule-callout').innerText(),'Same');
   fixture.session++; fixture.revision=0; fixture.settling=false; fixture.stages=[];
   await page.evaluate(s=>window.ttcgState(s),fixture);
   await page.waitForTimeout(700);
   assert(await page.locator('#rule-callout').isHidden());
   assert.equal(acknowledgements.length,2,'Canceled move must never acknowledge another session');
   // Reduced motion still presents the rule and updates ownership once.
   await page.emulateMedia({reducedMotion:'reduce'});
   fixture.revision++; fixture.settling=true; fixture.board[3]={card:8,owner:1,flip:true};
   fixture.stages=[{label:'Same / Plus',highlights:8,captures:8}];
   await page.evaluate(s=>window.ttcgState(s),fixture);
   assert.equal(await page.locator('#rule-callout').innerText(),'Same / Plus');
   await page.waitForFunction(()=>document.querySelector('#square-3 .owner-1')&&document.getElementById('rule-callout').hidden);
   assert.equal(await page.evaluate(()=>document.querySelector('#square-3 .card').getAnimations().length),0);
   assert.equal(acknowledgements.length,3);
   assert.deepEqual(ruleSounds,['plus','combo','same','same','plus']);
   // Results wait for the final capture, announce once, then offer Play Again / Leave.
   await page.emulateMedia({reducedMotion:'no-preference'});
   for(const [index,score,label] of [[0,[6,4],'You Win!'],[1,[4,6],'You Lose'],[2,[5,5],'Draw']]) {
    const outcome=JSON.parse(JSON.stringify(fixture));
    outcome.session=900+index; outcome.revision=9; outcome.phase='result'; outcome.score=score; outcome.settling=true;
    outcome.hands=[[9,null,null,null,null],[null,null,null,null,null]];
    outcome.board=Array.from({length:9},(_,i)=>({card:i,owner:i<score[0]-1?0:1,flip:i===0}));
    outcome.stages=[{label:'',highlights:0,captures:1}];
    const before=resultSounds.length;
    await page.evaluate(s=>window.ttcgState(s),outcome);
    assert(await page.locator('#rule-callout').isHidden());
    assert.equal(resultSounds.length,before,'No result sound before the last flip');
    await page.waitForFunction(()=>!document.getElementById('rule-callout').hidden);
    assert.equal(await page.locator('#rule-callout').innerText(),label);
    assert.equal(await page.evaluate(()=>document.querySelector('#square-0 .card').getAnimations().length),0);
    assert(await page.locator('#play-again').isHidden());
    await page.evaluate(s=>window.ttcgState({...s,musicEnabled:false}),outcome);
    assert.equal(resultSounds.length,before+1,'Duplicate state must not replay result sound');
    if(width===1440&&index===0) { await page.waitForTimeout(600); await page.screenshot({path:'build/clean-catalog/screenshots/prototype-win.png'}); }
    await page.waitForFunction(()=>document.getElementById('rule-callout').hidden);
    outcome.settling=false; await page.evaluate(s=>window.ttcgState(s),outcome);
    assert(await page.locator('#play-again').isVisible());
    assert.equal(resultSounds.length,before+1);
   }
   // Closing during a result cancels the timer and its acknowledgement.
   const canceled={...fixture,session:990,revision:9,phase:'result',score:[5,5],settling:true,stages:[]};
   const beforeCancel=acknowledgements.length;
   await page.evaluate(s=>window.ttcgState(s),canceled);
   assert.equal(await page.locator('#rule-callout').innerText(),'Draw');
   await page.keyboard.press('Escape');
   await page.waitForTimeout(1900);
   assert.equal(acknowledgements.length,beforeCancel+1); // Close only; no Settled.
   assert(acknowledgements.at(-1).startsWith('close '));
   assert(await page.locator('#rule-callout').isHidden());
   fixture.session=1000; fixture.revision=0; fixture.settling=false; fixture.stages=[];
   await page.evaluate(s=>window.ttcgState(s),fixture);
   // Resize controls also work through the native controller callback.
   await page.locator('#window-size-trigger').focus();
   await page.evaluate(()=>{ window.ttcgKey('confirm'); window.ttcgKey('up'); });
   assert.equal(await page.locator('#window-size').inputValue(),'90','Navigating options must not resize');
   await page.evaluate(()=>window.ttcgKey('confirm'));
   assert.equal(await page.locator('#window-size').inputValue(),'85');
   // The X exits immediately. Escape works even with a selected card or legacy keyCode.
   fixture.session++; fixture.revision=0; fixture.settling=false; fixture.stages=[];
   await page.evaluate(s=>window.ttcgState(s),fixture);
   assert.equal(await page.locator('#leave').innerText(),'');
   assert.equal(await page.locator('#music').innerText(),'');
   assert.equal(await page.locator('#leave svg').count(),1);
   await page.locator('#leave').click(); assert(await page.locator('#table').isHidden());
   fixture.session++;
   await page.evaluate(s=>window.ttcgState(s),fixture);
   await page.locator('#player-hand button[data-placeable="true"]').first().click();
   await page.keyboard.press('Escape'); assert(await page.locator('#table').isHidden());
   fixture.session++;
   await page.evaluate(s=>{ window.ttcgState(s); window.dispatchEvent(new KeyboardEvent('keydown',{keyCode:27})); },fixture);
   assert(await page.locator('#table').isHidden());
   fixture.session++;
   await page.evaluate(s=>window.ttcgState(s),fixture);
   await page.locator('#window-size-trigger').click();
   // This is a new keypress, outside the preceding Escape's duplicate window.
   await page.waitForTimeout(220);
   await page.evaluate(()=>{ window.ttcgEscape('native'); window.ttcgEscape('dom'); });
   assert(await page.locator('#table').isVisible(),'Duplicate key reports must not close after dismissing an option list');
   assert(await page.locator('#window-size-options').isHidden());
   await page.evaluate(()=>window.ttcgEscape('native')); assert(await page.locator('#table').isHidden());
   await page.reload();
   assert.equal(await page.locator('#window-size').inputValue(),'85');
   assert.equal(errors.length,0,errors.join('\n'));
   console.log(`${width}x${height}: matches, controls, Tessera-style callouts, ordered Combo, all results and sound timing passed`);
   host.kill(); await page.close();
  }
 } finally { await browser.close(); }
})().catch(error=>{ console.error(error); process.exit(1); });
