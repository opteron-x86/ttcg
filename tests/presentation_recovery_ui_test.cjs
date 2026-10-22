const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
const run=spawnSync('/tmp/ttcg-ui-host',['--multi-fixtures'],{encoding:'utf8'});
assert.equal(run.status,0,run.stderr);
const [before,after]=run.stdout.trim().split('\n').map(JSON.parse);
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:900}}),commands=[],reports=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('ttcgCommand',command=>commands.push(command));
  await page.exposeFunction('ttcgReport',report=>reports.push(JSON.parse(report)));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  // Fail the second animation after the first capture animation has started.
  // Recovery must cancel that animation and display all committed owners.
  await page.evaluate(s=>ttcgState(s),before);
  await page.evaluate(()=>{
   const animate=Element.prototype.animate;let captures=0;
   Element.prototype.animate=function(...args){
    if(this.parentElement?.matches('#board .square')&&++captures===2){Element.prototype.animate=animate;throw new Error('Injected capture animation failure');}
    return animate.apply(this,args);
   };
  });
  await page.evaluate(s=>ttcgState(s),after);
  await page.waitForFunction(()=>!busy);
  await page.waitForTimeout(50);
  assert.deepEqual(commands.filter(c=>c.startsWith('settled ')),[`settled ${after.session} ${after.revision}`]);
  assert(await page.evaluate(board=>board.every((cell,i)=>!cell||document.querySelector(`#square-${i} .owner-${cell.owner}`)),after.board));
  assert.equal(await page.evaluate(()=>flips.size),0);
  assert(reports.some(r=>r.context==='captures'&&r.message==='Injected capture animation failure'&&r.session===after.session&&r.revision===after.revision));
  // A duplicate packet must neither replay the failed capture nor acknowledge twice.
  await page.evaluate(s=>ttcgState(s),after);await page.waitForTimeout(50);
  assert.equal(commands.filter(c=>c.startsWith('settled ')).length,1);
  const initial=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{encoding:'utf8'}).stdout);
  const opening={...initial,session:200,revision:0,phase:'match',turn:0,opening:true,settling:true};
  await page.evaluate(()=>{window.originalDrawStarter=drawStarter;drawStarter=async()=>{throw new Error('Injected opening failure');};});
  await page.evaluate(s=>ttcgState(s),opening);await page.waitForTimeout(50);
  assert.deepEqual(commands.filter(c=>c.startsWith('settled 200 ')),['settled 200 0']);
  assert(reports.some(r=>r.context==='opening'&&r.session===200));
  // Restore input only after the native acknowledgement, then allow a real move.
  assert.equal(await page.locator('#player-hand [data-placeable="true"]').count(),0);
  await page.evaluate(s=>ttcgState({...s,opening:false,settling:false}),opening);
  await page.locator('#player-hand [data-placeable="true"]').first().click();
  await page.locator('#square-0').click();assert(commands.at(-1).startsWith('play 200 0 '));
  // Reject an old asynchronous presentation after reset and a new match. It
  // must not acknowledge or cancel the new match's still-pending opening.
  await page.evaluate(()=>{drawStarter=()=>new Promise((_,reject)=>{window.rejectOldOpening=reject;});});
  await page.evaluate(s=>ttcgState({...s,session:201}),opening);
  await page.evaluate(()=>{ttcgReset();drawStarter=originalDrawStarter;});
  await page.evaluate(s=>ttcgState({...s,session:202}),opening);
  await page.evaluate(()=>rejectOldOpening(new Error('Late opening failure')));
  await page.waitForTimeout(50);
  assert.equal(commands.filter(c=>c.startsWith('settled 201 ')||c.startsWith('settled 202 ')).length,0);
  assert(reports.some(r=>r.message==='Late opening failure'&&r.session===201),'Report the failed session, not the replacement match');
  assert(await page.evaluate(()=>busy));assert(await page.locator('#starter-draw').isVisible());
  await page.waitForFunction(()=>!busy);
  await page.waitForTimeout(50);
  assert.deepEqual(commands.filter(c=>c.startsWith('settled 202 ')),['settled 202 0']);
  // A renderer that still fails after recovery must not commit another move.
  // Native monitoring will suspend this table; diagnostics must survive it.
  await page.evaluate(()=>{window.originalRender=render;render=()=>{throw new Error('Persistent render failure');};});
  await page.evaluate(s=>ttcgState({...s,session:203}),opening);await page.waitForTimeout(50);
  assert.equal(commands.filter(c=>c.startsWith('settled 203 ')).length,0);
  assert(reports.some(r=>r.context==='presentation recovery'&&r.session===203));
  await page.evaluate(()=>{render=originalRender;ttcgReset();});
  assert.deepEqual(errors,[]);
  console.log('Presentation recovery: partial capture/opening failure, committed owners, restored input, one acknowledgement, stale callbacks and persistent errors passed');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
