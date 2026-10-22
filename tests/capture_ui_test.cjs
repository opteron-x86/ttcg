const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 const fixtures=spawnSync('/tmp/ttcg-ui-host',['--multi-fixtures'],{encoding:'utf8'});assert.equal(fixtures.status,0,fixtures.stderr);
 const [before,after]=fixtures.stdout.trim().split('\n').map(JSON.parse),mask=after.stages[0].captures;
 const targets=after.board.map((cell,i)=>mask&(1<<i)?i:-1).filter(i=>i>=0);assert(targets.length>=2);
 // Exercise the GPU material while the existing ownership/capture animation runs.
 for(const packet of [before,after])for(const card of packet.cards)card.foil=true;
 for(const reducedMotion of ['no-preference','reduce']){
  const page=await browser.newPage({viewport:{width:1440,height:900},reducedMotion}),commands=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>commands.push(text));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  await page.evaluate(s=>ttcgState(s),before);
  await page.evaluate(()=>{
   window.captureStarts=[];const animate=Element.prototype.animate;
   Element.prototype.animate=function(...args){if(this.parentElement?.matches('#board .square'))captureStarts.push({square:this.parentElement.id,time:performance.now(),opacity:getComputedStyle(this.parentElement).opacity});return animate.apply(this,args);};
   window.ownerChanges=[];new MutationObserver(changes=>{for(const change of changes){const oldOwner=change.oldValue?.match(/owner-[01]/)?.[0],owner=change.target.className.match(/owner-[01]/)?.[0];if(change.target.matches('#board .card')&&oldOwner&&owner&&oldOwner!==owner)ownerChanges.push({square:change.target.parentElement.id,time:performance.now()});}}).observe(document.getElementById('board'),{subtree:true,attributes:true,attributeFilter:['class'],attributeOldValue:true});
  });
  await page.evaluate(s=>ttcgState(s),after);
  assert(await page.locator('#board .square:disabled').count()>0,'Board input is locked during captures');
  assert(await page.locator('#board .square:disabled').evaluateAll(nodes=>nodes.every(n=>getComputedStyle(n).opacity==='1')),'Locking the board never dims cards');
  if(reducedMotion==='no-preference'){
   await page.waitForFunction(n=>captureStarts.length===n,targets.length);
   const starts=await page.evaluate(()=>captureStarts);assert(starts.every(a=>a.opacity==='1'));
   assert(Math.max(...starts.map(a=>a.time))-Math.min(...starts.map(a=>a.time))<32,'All cards in a capture wave start together');
   assert.deepEqual(starts.map(a=>a.square).sort(),targets.map(i=>'square-'+i).sort());
  }
  await page.waitForFunction(([ids,board])=>ids.every(i=>document.querySelector('#square-'+i+' .owner-'+board[i].owner)&&!document.querySelector('#square-'+i+' .card').getAnimations().length),[targets,after.board]);
  // The catalog-derived fixture can finish a match; its result callout precedes acknowledgement.
  await page.waitForFunction(()=>!busy);
  assert.equal(commands.filter(c=>c.startsWith('settled ')).length,1);
  assert(await page.locator('#board .square').evaluateAll(nodes=>nodes.every(n=>getComputedStyle(n).opacity==='1')));
  if(reducedMotion==='reduce')assert.equal(await page.evaluate(()=>captureStarts.length),0);
  else {const changes=await page.evaluate(ids=>ownerChanges.filter(c=>ids.includes(c.square)),targets.map(i=>'square-'+i));assert.equal(changes.length,targets.length);assert(Math.max(...changes.map(c=>c.time))-Math.min(...changes.map(c=>c.time))<32,'Ownership changes together at the flip midpoint');}
  assert.deepEqual(errors,[]);await page.close();console.log(`${reducedMotion}: native simultaneous capture, full-opacity board, synchronized ownership and one acknowledgement passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
