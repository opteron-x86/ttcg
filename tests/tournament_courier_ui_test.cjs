const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']}),host=spawn('/tmp/ttcg-tournament-host');host.stderr.on('data',data=>process.stderr.write(data));
 try{
  const lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();const errors=[],commands=[],page=await browser.newPage({viewport:{width:1440,height:900}});
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',command=>{commands.push(command);pending=pending.then(async()=>{host.stdin.write(command+'\n');const line=await lines.next();assert(!line.done,'Native tournament host stopped');packet=JSON.parse(line.value);await page.evaluate(s=>ttcgState(s),packet);});});
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>ttcgState(s),packet);
  const settle=async()=>{await page.waitForTimeout(50);await pending;};const sendCommand=async verb=>{await page.evaluate(v=>send(v),verb);await settle();};
  for(const circuit of [1,2]){
   await sendCommand('test-courier-rewind');
   assert.equal(packet.collection.tournaments.events.length,1,'Only the delivered Masters letter is visible');
   assert.equal(packet.collection.tournaments.events[0].circuit,2);assert.equal(packet.collection.tournaments.events[0].ends,175);
   assert(await page.locator('#tournament-action').isDisabled());assert.match(await page.locator('#tournament-reason').innerText(),/Speak to Hulda/);
   await sendCommand('test-courier-delivery');assert.equal(packet.collection.tournaments.events.length,2);
   await sendCommand('test-host');const event=packet.collection.tournaments.events.find(e=>e.circuit===circuit);assert(event.canEnter);
   await page.locator('#tournament-event-'+event.id).click();await settle();assert(await page.locator('#tournament-action').isEnabled());
   await page.locator('#tournament-action').click();await page.locator('#tournament-confirm-action').click();await settle();
   assert.equal(packet.collection.tournamentID,event.id);assert.equal(packet.screen,'lobby');assert.equal(packet.collection.fixedTrade,0);assert.equal(packet.collection.fixedWager,0);
   await page.locator('#start').click();await settle();assert.equal(packet.phase,'match');assert(await page.locator('#game').isVisible());
  }
  assert.deepEqual(errors,[]);console.log('Courier clock rollback: existing Masters letter recovered, Invitational delivery, Hulda registration and both playable tournament lobbies passed');
 }finally{host.stdin.end();host.kill();await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
