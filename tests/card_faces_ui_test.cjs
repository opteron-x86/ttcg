const {chromium}=require('playwright'),{spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
const base=JSON.parse(spawnSync('/tmp/tessera-decks-host',['economy'],{input:'',encoding:'utf8'}).stdout.trim());
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080],[3840,2160]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
  const packet=structuredClone(base);packet.screen='album';packet.matchContext=false;
  packet.settings={collection:65,challenge:66,toggleGames:0};
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>commands.push(text));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const update=()=>page.evaluate(s=>ttcgState(s),packet);await update();
  const first=page.locator('#album-grid .card-face').first();
  assert.notEqual(await first.locator('.card-name').evaluate(el=>getComputedStyle(el).display),'none');
  await page.locator('#album-settings-tab').click();await page.locator('#settings-section-gameplay').click();
  assert.equal(await page.locator('#bind-toggleGames').innerText(),'Unbound');
  assert.equal(await page.locator('#games-enabled').getAttribute('aria-checked'),'true');
  await page.locator('#games-enabled').click();assert(commands.at(-1).endsWith('games 0'));assert(await page.locator('#games-enabled').isDisabled());
  packet.settings.interfaceNotice="Couldn't save this setting.";await update();
  assert.equal(await page.locator('#games-enabled').getAttribute('aria-checked'),'true','Failed writes preserve game availability');
  packet.settings.gamesEnabled=false;packet.settings.interfaceNotice='';await update();
  assert.equal(await page.locator('#games-enabled').getAttribute('aria-checked'),'false');
  await page.locator('#settings-section-controls').click();await page.locator('#bind-toggleGames').click();assert(commands.at(-1).endsWith('begin toggleGames'));
  packet.settings.capturing='toggleGames';await update();assert(await page.locator('#card-face-classic').isDisabled());
  await page.keyboard.press('F6');assert(commands.at(-1).endsWith('set toggleGames 64'));
  packet.settings.capturing='';packet.settings.toggleGames=64;await update();
  assert.equal(await page.locator('#bind-toggleGames').innerText(),'F6');
  await page.locator('#settings-section-appearance').click();await page.locator('#card-face-classic').click();assert(commands.at(-1).endsWith('card-ranks 1'));assert(await page.locator('#card-face-classic').isDisabled());
  packet.settings.rankLayout=1;await update();assert.equal(await page.locator('#card-face-classic').getAttribute('aria-pressed'),'true');
  await page.locator('#card-face-names').click();assert(commands.at(-1).endsWith('card-names 0'));
  packet.settings.showCardNames=false;await update();
  assert.equal(await first.locator('.card-name').evaluate(el=>getComputedStyle(el).display),'none','Cached album cards follow the setting immediately');
  assert(await first.getAttribute('title'),'Names remain available on hover');
  // The shared preview follows both layouts and the independent name setting.
  const centerX=selector=>page.locator(selector).first().evaluate(el=>{
   const r=el.getBoundingClientRect(),c=el.closest('.card-face').getBoundingClientRect();return (r.x+r.width/2-c.x)/c.width;
  });
  assert((await centerX('#settings-preview-card .rank.top'))<.25);
  packet.settings.rankLayout=0;await update();assert((await centerX('#settings-preview-card .rank.top'))>.45);
  const ranksY=selector=>page.locator(selector).evaluate(card=>{
   const box=card.getBoundingClientRect();return Object.fromEntries(['top','right','bottom','left'].map(side=>{
    const r=card.querySelector('.rank.'+side).getBoundingClientRect();return [side,(r.y+r.height/2-box.y)/box.height];
   }));
  });
  let edges=await ranksY('#settings-preview-card .card-face');
  for(const side of ['right','left'])assert(Math.abs(edges[side]-.5)<.01,'Nameless side values center on the vertical edges');
  assert(Math.abs(edges.bottom-.9)<.01&&Math.abs(edges.top-.1)<.01,'Nameless bottom mirrors the top');
  packet.settings.showCardNames=true;await update();edges=await ranksY('#settings-preview-card .card-face');
  assert(Math.abs(edges.right-.395)<.01&&Math.abs(edges.bottom-.69)<.01,'Named faces keep clearance for the nameplate');
  packet.settings.showCardNames=false;await update();
  packet.settings.rankLayout=1;await update();
  const classic=await ranksY('#settings-preview-card .card-face');
  assert(Math.abs(classic.right-.175)<.01&&Math.abs(classic.bottom-.26)<.01,'Classic geometry is independent of nameplates');
  await page.locator('#card-face-classic').focus();
  assert(await page.evaluate(()=>visibleControls($('settings-panel')).includes($('card-face-classic'))),'Preview supports controller focus');
  if(width===1920)await page.screenshot({path:'build/previews/card-face-settings.png'});
  await page.locator('#album-cards-tab').click();
  const before=commands.length;await page.keyboard.press('F8');assert.equal(commands.length,before,'Disabled Challenge binding does nothing');
  await page.keyboard.press('F6');assert(commands.at(-1).endsWith('games 1'),'Toggle remains available while games are disabled');
  packet.settings.gamesEnabled=true;await update();
  await page.locator('#album-search').fill('Valenwood');
  const typed=commands.length;await page.keyboard.press('F6');assert.equal(commands.length,typed,'No toggle while entering search text');
  await page.locator('#album-search').fill('');await page.locator('#album-grid .collection-card').first().click();await page.locator('#inspect-card').click();
  assert.equal(await page.locator('.art-front .rank,.art-front .tier,.art-front .card-name,.art-front .affinity-modifier').count(),0,'Art inspection stays clean');
  await page.keyboard.press('Escape');
  await page.locator('#album-decks-tab').click();
  assert.equal(await page.locator('#album-deck-editor .card-face').count(),5);
  assert(await page.evaluate(()=>[...document.querySelectorAll('#album .card-face .card-name')].every(el=>getComputedStyle(el).display==='none')));
  await page.locator('#album-rules-tab').click();await page.locator('#rules-topic-open').click();
  assert(await page.locator('#rules-player-hand .card-face').count());
  assert((await centerX('#rules-player-hand .card-face:first-child .rank.top'))<.3);
  // Check small and large cards with an A, multiple affinities, modifiers and foil.
  const geometry=await page.evaluate(()=>{
   const original=state.cards.findIndex(c=>c.name==='Azura'&&c.foil),id=original>=0?original:state.cards.length-1;
   const results=[];const wrap=document.createElement('div');document.body.append(wrap);
   for(const width of [126,180,340])for(const modifier of [-2,2]){
    const mount=document.createElement('div');mount.style.cssText=`position:relative;width:${width}px;height:${width/.76}px`;
    mount.append(cardElement(id,1,modifier));wrap.append(mount);
    const card=mount.querySelector('.card-face'),box=card.getBoundingClientRect();
    const parts=[...card.querySelectorAll('.rank,.tier,.card-affinities,.affinity-modifier')].filter(e=>e.getBoundingClientRect().width>0);
    const rects=parts.map(e=>e.getBoundingClientRect());
    const overlap=(a,b)=>Math.min(a.right,b.right)-Math.max(a.left,b.left)>1&&Math.min(a.bottom,b.bottom)-Math.max(a.top,b.top)>1;
    results.push({width,modifier,inside:rects.every(r=>r.left>=box.left&&r.right<=box.right&&r.top>=box.top&&r.bottom<=box.bottom),overlap:rects.some((r,i)=>rects.slice(i+1).some(s=>overlap(r,s))),foil:card.classList.contains('foil')});
   }
   wrap.remove();return results;
  });
  for(const result of geometry){assert(result.inside,JSON.stringify(result));assert(!result.overlap,JSON.stringify(result));assert(result.foil);}
  const playing={...packet,session:20,revision:0,screen:'lobby',phase:'match',opening:false,settling:false,thinking:false,turn:0,rules:16};
  playing.board[0]={card:0,owner:0};
  await page.evaluate(s=>ttcgState(s),playing);
  assert.equal(await page.locator('#board .card-face .card-name').first().evaluate(el=>getComputedStyle(el).display),'none');
  assert((await centerX('#board .card-face .rank.top'))<.25);
  assert((await centerX('#player-hand .card-face .rank.top'))<.25);
  await page.keyboard.press('F6');assert(commands.at(-1).endsWith('games 0'),'The toggle is available during an active game');
  const prize={...playing,session:21,phase:'result',score:[6,4],originalHands:[[0,1,2,3,4],[5,6,7,8,9]],fullHands:[[0,1,2,3,4],[5,6,7,8,9]],collection:{...playing.collection,trade:1,required:1,reward:true,staked:true,notice:''}};
  await page.evaluate(s=>ttcgState(s),prize);await page.waitForFunction(()=>trade?.phase==='selecting');
  assert((await centerX('#prize-0 .card-face .rank.top'))<.25);
  assert.equal(await page.locator('#prize-0 .card-name').evaluate(el=>getComputedStyle(el).display),'none');
  assert.deepEqual(errors,[]);await page.close();
  console.log(`${width}x${height}: game switch, optional hotkey, face previews, shared layouts, inspection and rank geometry passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
