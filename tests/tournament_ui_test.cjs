const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
  const host=spawn('/tmp/ttcg-tournament-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
  try{
   let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
   page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>{commands.push(c);pending=pending.then(async()=>{host.stdin.write(c+'\n');packet=JSON.parse((await lines.next()).value);await page.evaluate(s=>ttcgState(s),packet);});});
   await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await page.evaluate(s=>ttcgState(s),packet);
   const settle=async()=>{await page.waitForTimeout(40);await pending;};const send=async(v,args='')=>{await page.evaluate(([v,args])=>send(v,args),[v,args]);await settle();};
   const event=()=>packet.collection.tournaments.events.find(e=>e.id===1);
   assert(await page.locator('#album-tournaments').isVisible());assert.equal(packet.collection.tournaments.events.length,0);
   await send('test-host');assert.equal(packet.collection.tournaments.events.length,0);
   // Receipt without the former broken Papyrus callback stays hidden in the old
   // save, then recovery on load exposes the event and the host's entry action.
   await send('test-receive-letter');assert.equal(packet.collection.tournaments.events.length,0);
   await send('reload');assert(event());assert(await page.locator('#tournament-action').isEnabled());
   await send('test-venue','0');await send('test-host');await send('test-venue','1');
   assert(await page.locator('#album-tournaments').isVisible());assert.equal(await page.locator('#tournament-title').innerText(),'Whiterun invitational');assert.equal(await page.locator('#tournament-bracket .tournament-pair').count(),7);
   const firstSlot=page.locator('#tournament-bracket .tournament-entrant').first();
   assert.equal(await firstSlot.innerText(),'Your place');assert.equal(event().bracket.slice(0,8).filter(p=>p.id).length,7);assert(event().bracket[0].reserved);
   assert.equal(await page.locator('#tournament-bracket section:nth-child(2) .tournament-pair').first().innerText(),'');
   assert.equal(await page.locator('#tournament-bracket section:nth-child(3) .tournament-pair').innerText(),'');
   assert.equal(await page.locator('#tournament-entry').innerText(),'150 gold entry');assert.match(await page.locator('#tournament-dates').innerText(),/Closes 24 Last Seed, 4E 201/);
   assert(await page.locator('#tournament-action').isDisabled());assert.match(await page.locator('#tournament-reason').innerText(),/Speak to Hulda/);
   assert(event().bracket.slice(8).every(p=>p.id===0));assert(!('skill' in event().bracket[0]));
   await send('test-host');assert(await page.locator('#tournament-action').isEnabled());
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-registration.png'});
   const frame=await page.locator('#table').boundingBox();for(const id of ['tournament-title','tournament-bracket','tournament-action']){const box=await page.locator('#'+id).boundingBox();assert(box&&box.x>=frame.x&&box.y>=frame.y&&box.x+box.width<=frame.x+frame.width+1&&box.y+box.height<=frame.y+frame.height+1,id+' fits');}
   await page.locator('#tournament-action').click();assert(await page.locator('#tournament-confirm').isVisible());
   assert.equal(await page.locator('#tournament-confirm-title').innerText(),'Enter for 150 gold?');assert(await page.locator('#tournament-confirm-copy').isHidden());assert.equal(await page.locator('#tournament-confirm-action').innerText(),'Enter');
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-confirmation.png'});
   await page.keyboard.press('Escape');assert(await page.locator('#tournament-confirm').isHidden());assert(!commands.some(c=>c.includes('enter')));
   await page.evaluate(()=>confirmTournament('Enter tournament?','','enter 1','Enter'));assert(await page.locator('#tournament-confirm-copy').isHidden());assert.equal(await page.locator('#tournament-confirm-title').innerText(),'Enter tournament?');await page.keyboard.press('Escape');
   const before=packet.collection.gold;await page.locator('#tournament-action').click();await page.locator('#tournament-confirm-action').click();await settle();assert(event().joined);assert.equal(packet.collection.gold,before-150);assert.equal(packet.screen,'lobby');await send('tournament','board');assert.equal(await page.locator('#tournament-status').innerText(),'In progress');
   assert.equal(event().bracket[0].name,'Test Player');assert(!event().bracket[0].reserved);assert.equal(await page.locator('#tournament-bracket .reserved').count(),0);
   await send('reload');assert(event().joined);
   await page.locator('#tournament-edit').click();assert.equal(await page.locator('#album').getAttribute('data-view'),'decks');await page.locator('#album-tournaments-tab').click();
   assert.equal(packet.collection.tournaments.hour,100);assert.equal(await page.locator('#tournament-status').innerText(),'In progress');assert(event().bracket.slice(8).every(p=>p.id===0));
   await send('test-venue','0');assert(await page.locator('#tournament-action').isDisabled());await send('test-venue','1');
   for(let round=1;round<=3;round++){
    await page.locator('#tournament-action').click();await settle();assert.equal(packet.screen,'lobby');assert(await page.locator('#wager-control').isHidden());assert.equal(packet.collection.fixedTrade,0);
    if(round===1){await page.locator('#library-open').click();await settle();assert(await page.locator('#album-tournaments').isVisible());await page.locator('#tournament-action').click();await settle();}
    await page.locator('#lobby-edit').click();await settle();await page.locator('#album-back').click();await settle();assert.equal(packet.collection.tournamentID,1);
    await page.locator('#start').click();await settle();assert.equal(packet.phase,'match');assert(commands.some(c=>c.startsWith('start ')&&c.split(' ').slice(4,6).join(' ')==='0 0'));
    await page.locator('#leave').click();assert(await page.locator('#forfeit').isVisible());await page.locator('#keep-playing').click();
    if(round===1){await send('test-result','-1');assert(await page.locator('#tournament-outcome').isVisible());await page.locator('#tournament-outcome-next').click();await settle();assert.equal(packet.screen,'lobby');await page.locator('#start').click();await settle();}
    const pair=[0,0,4,6][round];
    await send('test-result','0');assert.equal(event().round,round);assert.deepEqual(event().wins[pair],[1,0]);assert.equal(await page.locator('#tournament-outcome-next').innerText(),'Next game');
    await page.locator('#tournament-outcome-next').click();await settle();assert.equal(packet.screen,'lobby');await page.locator('#start').click();await settle();
    await send('test-result','0');assert.equal(await page.locator('#tournament-outcome-title').innerText(),round===1?'Through to the semifinals':round===2?'Through to the final':'Tournament won');
    if(round<3){
     assert.equal(event().round,round+1);if(width===1440&&round===1)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-advancement.png'});
     await page.locator('#tournament-outcome-bracket').click();await settle();assert(await page.locator('#album-tournaments').isVisible());assert.equal(event().announcement,0);
     if(round===1){assert(event().bracket.slice(8,12).every(p=>p.id));assert(event().bracket.slice(12).every(p=>!p.id));}
     await send('reload');assert.equal(event().round,round+1);
    }else{
     assert(await page.locator('#tournament-outcome-rewards').isVisible());assert.match(await page.locator('#tournament-outcome-rewards').innerText(),/1,?800 gold/);
     if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-champion.png'});
     // Reload preserves the unacknowledged reward announcement.
     await send('reload');assert(await page.locator('#tournament-outcome').isVisible());await page.locator('#tournament-outcome-next').click();await settle();
    }
   }
   assert.equal(await page.locator('#tournament-status').innerText(),'Champion');assert.equal(event().champion,20);assert.match(await page.locator('#tournament-event-1').innerText(),/^★/);
   await page.locator('#tournament-available').click();assert.equal(await page.locator('#tournament-list .chapter-link').count(),0);
   await page.locator('#tournament-history').click();assert.match(await page.locator('#tournament-event-1').innerText(),/^★/);
   // Discovery only lights the album marker; it does not open a popup or mark
   // every discovered event as read. Unseen status survives saving and loading.
   await page.locator('#album-cards-tab').click();await send('test-regular','2');assert.equal(packet.collection.tournaments.events.length,1);
   assert.equal(await page.locator('#album-tournaments-tab').getAttribute('class')||'','');
   await send('test-hold','2');const regular=packet.collection.tournaments.events.find(e=>e.circuit===0);assert(regular);assert.equal(regular.ends-regular.starts,72);
   assert(regular.unseen);assert.equal(await page.locator('#album').getAttribute('data-view'),'cards');assert(await page.locator('#album-tournaments-tab.unseen').count());
   await send('reload');assert(packet.collection.tournaments.events.find(e=>e.id===regular.id).unseen);
   await send('test-regular','3');assert.equal(packet.collection.tournaments.events.length,2);
   await send('test-hold','3');const remote=packet.collection.tournaments.events.find(e=>e.hold===3);assert(remote?.unseen);await send('test-hold','2');
   // Opening an inn's board resets History and prefers the local tournament,
   // even though another hold's event was announced more recently.
   await send('tournament','board');assert.equal(await page.locator('#tournament-available').getAttribute('aria-pressed'),'true');
   assert.equal(await page.locator('#tournament-list .chapter-link').first().getAttribute('id'),'tournament-event-'+regular.id);
   assert.equal(await page.locator('#tournament-status').innerText(),'Open for entry');assert.match(await page.locator('#tournament-reason').innerText(),/Valga Vinicia/);
   assert.match(await page.locator('#tournament-event-'+regular.id+' .chapter-detail').innerText(),/Closes .*Last Seed, 4E 201/);
   assert(!packet.collection.tournaments.events.find(e=>e.id===regular.id).unseen);assert(await page.locator('#tournament-event-'+remote.id+'.unseen').count());assert(await page.locator('#album-tournaments-tab.unseen').count());
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-discovery.png'});
   await page.locator('#tournament-event-'+remote.id).click();await settle();assert.equal(await page.locator('#album-tournaments-tab.unseen').count(),0);
   await send('tournament','board');assert.equal(await page.locator('#tournament-event-'+regular.id).getAttribute('aria-pressed'),'true');
   await send('test-time',String(regular.ends));assert(!packet.collection.tournaments.events.some(e=>e.id===regular.id));assert.equal(await page.locator('#tournament-list .chapter-link').count(),0);
   await page.locator('#tournament-history').click();assert.equal(await page.locator('#tournament-list .chapter-link').count(),1);assert.match(await page.locator('#tournament-event-1 .chapter-detail').innerText(),/^Held /);
   // A local post-game notice is acknowledged once and links to the bracket.
   await send('browse','lobby');await send('test-regular','3');await send('test-hold','3');await send('test-post-result');
   assert(await page.locator('#post-tournament').isVisible());assert.match(await page.locator('#post-tournament-copy').innerText(),/The Winking Skeever.*Closes/);
   const notice=packet.collection.tournaments.events.find(e=>!e.champion&&e.hold===3);
   await page.locator('#post-tournament-view').click();await settle();assert.equal(await page.locator('#tournament-event-'+notice.id).getAttribute('aria-pressed'),'true');assert.equal(await page.locator('#tournament-available').getAttribute('aria-pressed'),'true');
   await send('reload');assert(!packet.collection.tournaments.events.find(e=>e.id===notice.id).unseen);
   await send('test-post-result');assert(await page.locator('#post-tournament').isHidden());
   await send('tournament','board');await page.locator('#tournament-history').click();
   // Both pack kinds share the Cards shelf, inspector and reveal. Opening the
   // last tournament pack must keep its title when selection falls back.
   await send('test-rewards');assert.equal(await page.locator('#tournament-open-pack').count(),0);
   await page.locator('#album-cards-tab').click();assert.equal(await page.locator('#album-grid .pack-tile').count(),2);
   await page.locator('#album-tournament-pack').click();assert.equal(await page.locator('#inspect-count').innerText(),'Tournament pack');
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-packs.png'});
   for(let remaining=1;remaining>=0;remaining--){
    await page.locator('#inspect-open-pack').click();await settle();assert.equal(packet.collection.packs[1],remaining);assert.equal(packet.collection.packs[0],1);
    assert(await page.locator('#album-modal').isVisible());assert.equal(await page.locator('#album-modal-title').innerText(),'Tournament pack');assert.equal(await page.locator('#pack-reveal .card').count(),5);await page.locator('#album-confirm').click();
    assert.equal(await page.locator('#album-tournament-pack').count(),remaining?1:0);
   }
   await page.locator('#album-card-pack').click();await page.locator('#inspect-open-pack').click();await settle();assert.equal(packet.collection.packs[0],0);assert.equal(await page.locator('#album-modal-title').innerText(),'Tessera pack');await page.locator('#album-confirm').click();assert.equal(await page.locator('#album-grid .pack-tile').count(),0);assert(await page.locator('#inspect-open-pack').isHidden());
   await page.locator('#album-tournaments-tab').click();
   assert.equal(await page.locator('#tournament-rewards').count(),0);assert(await page.locator('#tournament-upgrade').isHidden());
   // Foil choices live in the victory popup, including the native result-screen
   // path. Each click consumes one reward and preserves total card ownership.
   await send('test-foil-reward');assert(await page.locator('#tournament-outcome').isVisible());assert(await page.locator('#tournament-foil-reward').isVisible());
   if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-foil-reward.png'});
   for(const id of ['tournament-upgrade','tournament-outcome-next','tournament-foil-preview']){const box=await page.locator('#'+id).boundingBox(),frame=await page.locator('#table').boundingBox();assert(box&&box.x>=frame.x&&box.y>=frame.y&&box.x+box.width<=frame.x+frame.width+1&&box.y+box.height<=frame.y+frame.height+1,id+' fits');}
   const preview=await page.locator('#tournament-foil-preview').boundingBox(),cardPreview=await page.locator('#tournament-foil-preview .card').boundingBox();assert(cardPreview.width<=preview.width+1&&cardPreview.height<=preview.height+1,'Card art stays inside its preview');
   await page.locator('#tournament-foil-card-trigger').click();await page.locator('#tournament-foil-card-options button').last().click();
   for(let remaining=1;remaining>=0;remaining--){
    const selected=Number(await page.locator('#tournament-foil-card').inputValue()),owned=packet.collection.owned[selected],total=packet.collection.owned.reduce((a,b)=>a+b,0);
    await page.locator('#tournament-upgrade').click();await settle();assert.equal(event().foilUpgrades,remaining);assert.equal(packet.collection.owned[selected],owned-1);assert.equal(packet.collection.owned.reduce((a,b)=>a+b,0),total);
   }
   assert(await page.locator('#tournament-foil-reward').isHidden());await page.locator('#tournament-outcome-next').click();await settle();assert(await page.locator('#tournament-confirm').isHidden());assert(await page.locator('#tournament-upgrade').isHidden());
   // Back/escape asks once before forfeiture; cancelling keeps all choices.
   await send('test-foil-reward');await page.keyboard.press('Escape');assert(await page.locator('#tournament-confirm').isVisible());assert.equal(await page.locator('#tournament-confirm-title').innerText(),'Leave 2 foil upgrades unused?');
   await page.locator('#tournament-cancel').click();assert.equal(event().foilUpgrades,2);assert(await page.locator('#tournament-foil-reward').isVisible());
   await page.locator('#tournament-upgrade').click();await settle();assert.equal(event().foilUpgrades,1);
   await page.locator('#tournament-outcome-next').click();assert.equal(await page.locator('#tournament-confirm-title').innerText(),'Leave 1 foil upgrade unused?');await page.locator('#tournament-confirm-action').click();await settle();assert.equal(event().foilUpgrades,0);assert.equal(event().announcement,0);
   const beforeRejected=packet.collection.owned;await send('tournament','foil 1 0');assert.deepEqual(packet.collection.owned,beforeRejected);
   // Native close/load paths also expire unused upgrades; the UI is not the
   // authority, and neither pack kind can be opened during reward selection.
   await send('test-rewards');await send('test-foil-reward');await send('open-pack');await send('tournament','pack 0');assert.deepEqual(packet.collection.packs,[1,2]);
   await send('close');assert.equal(event().foilUpgrades,0);assert.equal(event().announcement,0);
   await send('test-foil-reward');await send('reload');assert.equal(event().foilUpgrades,0);assert(await page.locator('#tournament-foil-reward').isHidden());
   await send('test-foil-reward');await send('test-no-normal-cards');assert(await page.locator('#tournament-upgrade').isDisabled());assert(await page.locator('#tournament-foil-empty').isVisible());
   await page.locator('#tournament-outcome-leave').click();await page.locator('#tournament-confirm-action').click();await settle();assert.equal(event().foilUpgrades,0);
   for(let circuit=0;circuit<3;++circuit){
    await send('test-epic-reward',String(circuit));const prize=event().prizeCards;assert.equal(prize.length,circuit+1);
    assert.equal(await page.locator('#tournament-outcome-rewards .art-scene').count(),prize.length);
    assert.deepEqual(await page.locator('#tournament-outcome-rewards figcaption').allTextContents(),prize.map(id=>packet.cards[id].name+(packet.cards[id].foil?' · Foil':'')));
    if(circuit)assert.equal(await page.locator('#tournament-outcome-rewards .art-front.foil').count(),prize.length);
    const frame=await page.locator('#table').boundingBox();for(const art of await page.locator('#tournament-outcome-rewards .art-scene').all()){const box=await art.boundingBox();assert(box.x>=frame.x&&box.y>=frame.y&&box.x+box.width<=frame.x+frame.width&&box.y+box.height<=frame.y+frame.height,'All reward cards fit');}
    if(width===1440&&circuit===2)await page.screenshot({path:'build/clean-catalog/screenshots/tournament-epic-foils.png'});
    await page.locator('#tournament-outcome-next').click();await settle();assert.equal(await page.locator('#tournament-prize-art .art-scene').count(),prize.length);
   }
   assert.deepEqual(errors,[]);await page.close();console.log(`${width}×${height}: tournament discovery/BO3, physical pack shelf, victory foil choices, one-time conversions, forfeiture, native close/load expiry and empty choices passed`);
  }finally{host.kill();}
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1);});
