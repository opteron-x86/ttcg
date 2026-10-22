const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height,reducedMotion] of [[1280,720,'no-preference'],[3840,2160,'reduce']]){
  const host=spawn('/tmp/ttcg-tournament-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
  host.stderr.on('data',data=>process.stderr.write(data));
  try{
   let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();const commands=[],errors=[],page=await browser.newPage({viewport:{width,height},reducedMotion});
   page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>{commands.push(text);pending=pending.then(async()=>{host.stdin.write(text+'\n');const line=await lines.next();assert(!line.done,'Native test host stopped');packet=JSON.parse(line.value);await page.evaluate(s=>ttcgState(s),packet);});});
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>ttcgState(s),packet);
   const settle=async()=>{await page.waitForTimeout(35);await pending;};
   const sendCommand=async(verb,args='')=>{await page.evaluate(([v,a])=>send(v,a),[verb,args]);await settle();};
   const event=()=>packet.collection.tournaments.events.find(e=>e.id===packet.collection.tournamentID);
   // Finish the second win of a real best-of-three final. Exercise every
   // prize class through capture playback, rather than opening its popup directly.
   for(const [circuit,reward,quantity] of [[0,3,1],[1,3,3],[2,3,5],[0,4,1],[1,4,2],[2,0,1],[0,1,1],[0,2,3]]){
    await sendCommand('test-final-setup',`${circuit} ${reward}`);commands.length=0;
    assert.equal(packet.phase,'match');assert.deepEqual(event().wins[6],[1,0]);assert.equal(event().champion,0);
    const move=packet.testFinalMove;await page.locator('#hand-0-'+move.hand).click();await page.locator('#square-'+move.square).click();await settle();
    assert.equal(packet.phase,'result');assert.equal(event().champion,20);assert.deepEqual(event().wins[6],[2,0]);assert(packet.settling);
    if(reward===3){assert.equal(event().foilUpgrades,quantity);const owned=packet.collection.owned.slice();await sendCommand('open-pack');await sendCommand('development','cards');assert.deepEqual(packet.collection.owned,owned,'Reward gate still prevents acquiring cards during the final animation');}
    assert(await page.locator('#tournament-outcome').isHidden(),'Rewards wait for the last capture and result announcement');
    await page.waitForFunction(()=>!document.getElementById('tournament-outcome').hidden,null,{timeout:8000});await settle();
    assert.equal(packet.settling,false,'Native completion acknowledgement must clear settling');
    assert.equal(packet.testResultSounds,1);assert(packet.testRuleSounds>0);
    assert.equal(commands.filter(c=>c.startsWith('settled ')).length,1);
    assert.equal(await page.locator('#tournament-outcome-title').innerText(),'Tournament won');assert(await page.locator('#post-match').isHidden());
    assert.match(await page.locator('#tournament-outcome-rewards').innerText(),new RegExp(event().purse+' gold'));
    if(reward===3){
     assert(await page.locator('#tournament-foil-reward').isVisible());
     if(width===1280&&circuit===2)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-final-foil-choice.png'});
     for(let remaining=quantity-1;remaining>=0;--remaining){const selected=Number(await page.locator('#tournament-foil-card').inputValue()),owned=packet.collection.owned[selected];await page.locator('#tournament-upgrade').click();await settle();assert.equal(event().foilUpgrades,remaining);assert.equal(packet.collection.owned[selected],owned-1);}
     assert(await page.locator('#tournament-foil-reward').isHidden());
    }else if(reward===0||reward===4){assert.equal(await page.locator('#tournament-outcome-rewards .art-scene').count(),quantity);for(const card of event().prizeCards)assert(packet.collection.owned[card]>0);}
    else {assert.match(await page.locator('#tournament-outcome-rewards').innerText(),new RegExp(event().prize));assert.equal(packet.collection.packs[reward===1?1:0],quantity);}
    // Late duplicate acknowledgements cannot rerun the award or restore spent upgrades.
    const owned=packet.collection.owned.slice(),packs=packet.collection.packs.slice();await sendCommand('settled');assert.deepEqual(packet.collection.owned,owned);assert.deepEqual(packet.collection.packs,packs);assert.equal(event().foilUpgrades,0);
    await page.locator('#tournament-outcome-next').click();await settle();assert(await page.locator('#album-tournaments').isVisible());assert.equal(await page.locator('#tournament-status').innerText(),'Champion');assert.match(await page.locator('#tournament-list .chapter-link').first().innerText(),/^★/);
   }
   assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height} ${reducedMotion}: final capture → acknowledged victory → all prize types, 1/3/5 foil choices, duplicate acknowledgements and champion history passed`);
  }finally{host.stdin.end();host.kill();}
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
