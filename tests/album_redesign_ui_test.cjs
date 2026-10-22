const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{pathToFileURL}=require('node:url'),{readFileSync}=require('node:fs');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
for(const [width,height]of [[1440,900],[1280,720],[2560,1080],[3840,2160]]){
 const host=spawn('/tmp/tessera-decks-host'),lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
 try{
 let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();
 const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
 page.on('pageerror',e=>errors.push(e.message));
 const enrich=s=>{Object.assign(s.collection,{holdID:1,roster:[{base:1,name:'Hulda',holdID:1,hold:'Whiterun',location:'The Bannered Mare',skill:'Beginner',friendlyOnly:false,wins:2,losses:1,draws:0,unlocked:true},{base:2,name:'Ysolda',holdID:1,hold:'Whiterun',location:'Whiterun market',skill:'Master',challengeRequirement:'Requires Famous reputation in their home hold.',reputationAward:'2',wins:1,losses:0,draws:0,friendlyWins:1,unlocked:true},{base:4,name:'Dorthe',holdID:1,hold:'Whiterun',location:'Riverwood',skill:'Beginner',friendlyOnly:true,wins:1,losses:0,draws:0,unlocked:true},{base:3,name:'Known player',holdID:2,hold:'Falkreath',location:'Inn',skill:'Experienced',wins:0,losses:0,draws:0,unlocked:false}],regions:[{id:1,name:'Whiterun',rules:0,trade:1,wins:2,reputationTitle:'Recognized',reputationNext:'Respected: 21.5/30 points; 7/10 different opponents; 1/3 Experienced or higher.'},{id:2,name:'Falkreath',rules:1,trade:1,wins:0,reputationTitle:'Unknown'}],packs:[1,0]});return s;};
 const update=async()=>page.evaluate(s=>window.ttcgState(s),enrich(packet));
 await page.exposeFunction('ttcgCommand',command=>{commands.push(command);pending=pending.then(async()=>{host.stdin.write(command+'\n');const row=await lines.next();assert(!row.done);packet=JSON.parse(row.value);await update();});});
 await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await update();
 const settle=async()=>{await page.waitForTimeout(40);await pending;};
 const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator(`#${id}-options [data-value="${value}"]`).click();};
 const fit=async ids=>{const frame=await page.locator('#table').boundingBox();for(const id of ids){const b=await page.locator('#'+id).boundingBox();assert(b&&b.x>=frame.x-2&&b.y>=frame.y-2&&b.x+b.width<=frame.x+frame.width+2&&b.y+b.height<=frame.y+frame.height+2,`${width} ${id} fits`);}};
 await page.locator('#library-open').click();await settle();assert.equal(packet.screen,'album');
 assert(await page.locator('#album-deck-editor').isHidden());assert(await page.locator('#album-title').isHidden());
 assert.match(await page.locator('#album-count').innerText(),/owned.*discovered/);await fit(['album-grid','inspect-card','album-tabs','album-back']);
 const browsingHeight=(await page.locator('#album-grid').boundingBox()).height;
 // Search, ordering, active filters and enlarged inspection operate on the same result set.
 await page.locator('#album-search').fill('dragon');const count=await page.locator('#album-grid .collection-card').count();assert(count>1);
 await select('album-sort','name');const names=await page.locator('#album-grid .card-name').allTextContents();assert.deepEqual(names,[...names].sort((a,b)=>a.localeCompare(b)));
 await page.locator('#album-grid .collection-card').first().click();await page.locator('#inspect-card').click();assert.equal(await page.locator('#inspect-position').innerText(),`1 / ${count}`);
 await page.locator('#album-confirm').focus();await page.keyboard.press('Tab');assert(await page.evaluate(()=>!!document.activeElement.closest('#album-modal')));await page.locator('#album-confirm').focus();await page.keyboard.press('ArrowRight');assert.equal(await page.locator('#inspect-position').innerText(),`2 / ${count}`);
 await page.locator('#inspect-previous').click();assert(await page.locator('#inspect-previous').isDisabled());await page.keyboard.press('Escape');assert(await page.locator('#table').isVisible());
 await page.locator('#album-filters-toggle').click();await select('album-tier','1');assert.equal(await page.locator('.filter-chip').count(),1);await page.keyboard.press('Escape');assert(await page.locator('#album-filter-panel').isHidden());assert(await page.locator('#table').isVisible());
 await page.locator('#album-filters-clear').click();assert.equal(await page.locator('#album-search').inputValue(),'');assert.equal(await page.locator('.filter-chip').count(),0);
 // Native-backed create, replace a full deck, reorder, delete a slot, save and reload.
 await page.locator('#album-decks-tab').click();await fit(['saved-decks-panel','album-deck','album-grid','inspect-card','album-deck-tools']);
 assert((await page.locator('#album-grid').boundingBox()).height<browsingHeight);
 const before=[...packet.collection.deck],replacement=packet.cards.findIndex((c,i)=>!before.includes(i)&&!c.unique);
 await page.locator('#album-search').fill(packet.cards[replacement].name);await page.locator('#album-card-'+replacement).click();
 await page.locator('#album-deck-2').click();assert.equal(await page.locator('#inspect-add').innerText(),'Replace '+packet.cards[packet.collection.deck[2]].name);
 await page.locator('#inspect-add').click();await settle();assert.deepEqual(packet.collection.deck,[before[0],before[1],replacement,before[3],before[4]]);
 await page.locator('#album-deck-2').dragTo(page.locator('#album-deck-0'));await settle();assert.deepEqual(packet.collection.deck,[replacement,before[0],before[1],before[3],before[4]]);
 await page.locator('#album-deck-0').focus();await page.keyboard.press('Shift+ArrowRight');await settle();assert.equal(packet.collection.deck[1],replacement);
 await page.locator('#album-remove').click();await settle();assert.equal(packet.collection.deck[1],-1);
 await page.locator('#album-card-'+replacement).dblclick();await settle();assert.equal(packet.collection.deck[1],replacement);
 await page.locator('#album-deck-copy').click();await page.locator('#deck-name').fill('Affinity & Beasts');await page.keyboard.press('Enter');await settle();assert.equal(packet.collection.savedDecks.length,2);
 assert.equal(await page.locator('.saved-deck-row').count(),2);const copyID=packet.collection.activeDeck;
 await page.locator('#album-deck-rename').click();await page.locator('#album-deck-name-input').fill('<b>Beasts</b>');await page.keyboard.press('Enter');await settle();assert.equal(await page.locator('#album-deck-name b').count(),0);
 await page.locator('#saved-deck-1').click();await settle();assert.equal(packet.collection.activeDeck,1);
 await page.evaluate(()=>send('reload'));await settle();assert.equal(packet.collection.deck[1],replacement);
 await page.locator('#saved-deck-'+copyID).click();await settle();
 await page.locator('#album-deck-delete').click();await page.keyboard.press('Escape');assert.equal(packet.collection.savedDecks.length,2);
 await page.locator('#album-deck-delete').click();await page.locator('#deck-submit').click();await settle();assert.equal(packet.collection.savedDecks.length,1);
 await page.locator('#album-deck-new').click();await page.locator('#deck-name').fill('New hand');await page.keyboard.press('Enter');await settle();assert.deepEqual(packet.collection.deck,[-1,-1,-1,-1,-1]);
 await page.locator('#album-search').fill('');for(const id of [0,1,2,3,4]){await page.locator('#album-card-'+id).dblclick();await settle();}assert.deepEqual(packet.collection.deck,[0,1,2,3,4]);
 if(width===1440){
  const fan=packet.cards.findIndex(c=>c.unique);await page.locator('#album-search').fill(packet.cards[fan].name);await page.locator('#album-card-'+fan).click();await page.locator('#album-deck-0').click();await page.locator('#inspect-add').click();await settle();assert.equal(packet.collection.deck[0],fan);
  await page.locator('#album-deck-1').click();assert(await page.locator('#inspect-add').isDisabled());assert.equal(await page.locator('#inspect-reason').innerText(),'Already at limit');
  await page.locator('#album-search').fill(packet.cards[0].name);await page.locator('#album-card-0').dragTo(page.locator('#album-deck-0'));await settle();assert.deepEqual(packet.collection.deck,[0,1,2,3,4]);
  await page.locator('#album-search').fill('no matching card');await page.locator('#album-cards-tab').click();await page.locator('#album-search').fill(packet.cards[replacement].name);await page.locator('#album-card-'+replacement).dblclick();
  assert.equal(await page.locator('#inspect-card .card-name').innerText(),packet.cards[replacement].name);assert.equal(await page.locator('#album-decks-tab').getAttribute('aria-pressed'),'true');assert.equal(await page.locator('#album-search').inputValue(),'');
  await page.locator('#album-cards-tab').click();await page.locator('#album-filters-clear').click();await page.locator('#album-decks-tab').click();
 }
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-decks-0.18.1.png'});
 // Chapter navigation and personal records never list profiles absent from the native roster.
 await page.locator('#album-opponents-tab').click();assert.equal(await page.locator('#journal-title').innerText(),'Whiterun');
 assert(await page.locator('#album-count').isHidden());assert(await page.locator('#album-gold').isHidden());
 await page.locator('.opponent-row[data-player="1"]').click();assert.match(await page.locator('#journal-player').innerText(),/Record/);assert.match(await page.locator('#journal-player').innerText(),/2 W/);
 assert(!(await page.locator('#opponents-list').innerText()).includes('Nazeem'));
 await page.locator('#journal-search').fill('ysolda');assert.equal(await page.locator('.opponent-row').count(),1);assert.match(await page.locator('#journal-player').innerText(),/Whiterun market/);assert.doesNotMatch(await page.locator('#journal-player').innerText(),/reputation|Friendly|Requires Famous/);assert.equal(await page.locator('#progress-step').innerText(),'Recognized · Wins: 2');assert.equal(await page.locator('#level-victories').count(),0);assert.doesNotMatch(await page.locator('.opponent-row').innerText(),/Reputation required/);
 await page.locator('#journal-chapter-2').click();assert.equal(await page.locator('#journal-search').inputValue(),'');assert.match(await page.locator('#hold-rules').innerText(),/Same/);assert.equal(await page.locator('#progress-step').innerText(),'Unknown · Wins: 0');
 await select('journal-filter','unplayed');assert.equal(await page.locator('.opponent-row').count(),1);
 await page.locator('#journal-chapter-1').click();assert.equal(await page.locator('.opponent-row').count(),1);await select('journal-filter','all');
 await page.locator('.opponent-row[data-player="4"]').click();assert.match(await page.locator('.opponent-row[data-player="4"]').innerText(),/Riverwood/);assert.match(await page.locator('#journal-player').innerText(),/Location\nRiverwood/);assert.doesNotMatch(await page.locator('#opponents-list').innerText(),/Friendly games/i);
 await fit(['journal-chapters','journal-title','opponents-list','journal-player']);
 if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-journal-0.18.1.png'});
 await page.evaluate(()=>window.ttcgKey('next'));assert(await page.locator('#album-tournaments').isVisible());await page.evaluate(()=>window.ttcgKey('next'));assert(await page.locator('#album-rules').isVisible());assert.equal(await page.locator('#album-rules-tab').getAttribute('aria-pressed'),'true');
 await page.locator('#rules-list button').filter({hasText:'Affinity'}).click();
 assert.match(await page.locator('#rules-copy').innerText(),/affinity markers/i);
 assert.match(await page.locator('#rules-copy').innerText(),/mismatch/i);
 assert(await page.locator('#rules-demo-board .card').first().evaluate(e=>e.getBoundingClientRect().width/e.parentElement.getBoundingClientRect().width>.9),'Demo cards fill their squares');
 await page.evaluate(()=>window.ttcgKey('next'));assert(await page.locator('#settings-panel').isVisible());assert.equal(await page.locator('#album-settings-tab').getAttribute('aria-pressed'),'true');
 await page.evaluate(()=>window.ttcgKey('next'));assert(await page.locator('#album-workspace').isVisible());assert.equal(await page.locator('#album-cards-tab').getAttribute('aria-pressed'),'true');
 await page.locator('#album-back').click();await settle();assert.equal(packet.screen,'lobby');assert.equal(await page.locator('#lobby-deck-select-trigger').innerText(),'New hand');
 // Static isolated states exercise discovery without mutating the native deck fixture.
 const snapshot=structuredClone(packet);snapshot.screen='album';snapshot.matchContext=false;snapshot.session+=100;
 Object.assign(snapshot.collection,{owned:snapshot.cards.map((_,i)=>i<8?1:0),known:snapshot.cards.map((_,i)=>i<10),packs:[0,0],revealed:[],revealID:0});
 await page.evaluate(s=>window.ttcgState(s),snapshot);assert(await page.locator('#album-back').isHidden());assert(await page.locator('#music').isHidden());
 await select('album-filter','all');assert.equal(await page.locator('#album-grid .collection-card').count(),10);await select('album-filter','missing');assert.equal(await page.locator('#album-grid .collection-card').count(),2);assert(await page.locator('#inspect-add').isDisabled());assert.equal(await page.locator('#inspect-reason').innerText(),'');
 await select('album-filter','owned');if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-cards-0.18.1.png'});
 snapshot.screen='album';snapshot.session++;snapshot.collection.packs=[0,0];snapshot.collection.revealed=[0,1,2];snapshot.collection.revealID=1;await page.evaluate(s=>window.ttcgState(s),snapshot);
 assert(await page.locator('#album-modal').isVisible());assert(await page.locator('#inspect-navigation').isHidden());await page.keyboard.press('Escape');await page.evaluate(s=>window.ttcgState(s),snapshot);assert(await page.locator('#album-modal').isHidden());
 assert.deepEqual(errors,[]);await page.close();console.log(`${width}×${height}: card browsing, discovery, filters, inspection, native saved decks/replacement/drag/reload, journal privacy, pack reveals passed`);
 }finally{host.kill();}
}
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exit(1)});
