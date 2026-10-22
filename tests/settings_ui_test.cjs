const {chromium}=require('playwright'),{spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
const base=JSON.parse(spawnSync('/tmp/tessera-decks-host',['economy'],{input:'',encoding:'utf8'}).stdout.trim());
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080],[3440,1440],[1280,1024]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),commands=[],errors=[];
  const packet=structuredClone(base);packet.screen='album';packet.matchContext=false;
  packet.settings={collection:65,challenge:66,toggleGames:0};
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>commands.push(text));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const update=()=>page.evaluate(s=>ttcgState(s),packet);await update();
  assert.equal(await page.locator('#settings-open').count(),0,'No second settings entry point');
  await page.locator('#album-settings-tab').click();assert(await page.locator('#settings-panel').isVisible());
  assert.equal(await page.locator('#settings-section-developer').count(),0,'Developer controls only appear in development mode');
  assert.equal(await page.locator('#settings-title').innerText(),'Appearance');
  assert(await page.locator('#album-workspace').isHidden());assert(await page.locator('#album-count').isHidden());
  assert.equal(await page.evaluate(()=>activeOverlay()),null,'Settings uses normal album navigation');
  const initial=await page.locator('#settings-preview-name').innerText();
  await page.locator('#settings-preview-next').click();assert.notEqual(await page.locator('#settings-preview-name').innerText(),initial);
  await page.locator('#settings-preview-previous').click();assert.equal(await page.locator('#settings-preview-name').innerText(),initial);
  await page.locator('#settings-preview-turn').click();assert(await page.locator('#settings-preview-card .card-back').isVisible());
  await page.locator('#settings-preview-turn').click();
  const rankStyle=()=>page.locator('#settings-preview-card .rank.top').evaluate(e=>({shadow:getComputedStyle(e).textShadow,stroke:getComputedStyle(e).webkitTextStrokeWidth}));
  const original=await rankStyle();assert.notEqual(original.shadow,'none');assert.equal(original.stroke,'0px');
  // Preview while dragging, then save once on release. Failed writes restore the saved preference.
  const change=async(id,value,release=true)=>{await page.locator('#'+id).evaluate((e,[v,release])=>{e.value=String(v);e.dispatchEvent(new Event('input',{bubbles:true}));if(release)e.dispatchEvent(new Event('change',{bubbles:true}));},[value,release]);};
  await change('rank-shadow-opacity',0,false);assert((await rankStyle()).shadow.includes('rgba(5, 9, 7, 0)'));
  await update();assert((await rankStyle()).shadow.includes('rgba(5, 9, 7, 0)'),'Unrelated updates preserve an active drag');
  await change('rank-shadow-opacity',0);assert(commands.at(-1).endsWith('rank-shadow-opacity 0'));
  packet.settings.interfaceNotice="Couldn't save this setting.";await update();assert.equal((await rankStyle()).shadow,original.shadow);
  packet.settings.interfaceNotice='';packet.settings.rankShadowOpacity=0;await update();
  await page.locator('#rank-stroke').click();assert(commands.at(-1).endsWith('rank-stroke 1'));packet.settings.rankStroke=true;await update();
  await change('rank-stroke-weight',25);packet.settings.rankStrokeWeight=25;await update();assert(parseFloat((await rankStyle()).stroke)>1);
  // Controller adjustment retains focus, allowing consecutive fine adjustments.
  await page.locator('#rank-stroke-weight').focus();await page.evaluate(()=>ttcgKey('right'));assert(commands.at(-1).endsWith('rank-stroke-weight 26'));
  packet.settings.rankStrokeWeight=26;await update();assert.equal(await page.evaluate(()=>document.activeElement.id),'rank-stroke-weight');
  await page.evaluate(()=>ttcgKey('left'));assert(commands.at(-1).endsWith('rank-stroke-weight 25'));packet.settings.rankStrokeWeight=25;await update();
  const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator('#'+id+'-options [data-value="'+value+'"]').click();};
  const font=selector=>page.locator(selector).first().evaluate(e=>getComputedStyle(e).fontFamily);
  const nameFont=await font('#settings-preview-card .card-name'),valueFont=await font('#settings-preview-card .rank');
  await select('interface-font','3');assert(commands.at(-1).endsWith('interface-font 3'));assert(await page.locator('#interface-font-trigger').isDisabled());
  packet.settings.interfaceFont=3;await update();assert((await font('#settings-title')).startsWith('Arial'));
  for(const selector of ['#album-search','#shop-search','#journal-search','#album-deck-name-input'])assert((await font(selector)).startsWith('Arial'),selector+' follows the interface font');
  assert.equal(await font('#settings-preview-card .card-name'),nameFont,'Menu font leaves card lettering intact');
  assert.equal(await font('#settings-preview-card .rank'),valueFont,'Menu font leaves values intact');
  await select('card-name-font','2');assert(commands.at(-1).endsWith('card-name-font 2'));packet.settings.cardNameFont=2;await update();
  assert((await font('#settings-preview-card .card-name')).includes('Liberation Serif'));
  await select('card-value-font','4');assert(commands.at(-1).endsWith('card-value-font 4'));packet.settings.cardValueFont=4;await update();
  assert((await font('#settings-preview-card .rank')).startsWith('Verdana'));
  // A failed font write leaves the saved appearance intact, including a cached preview card.
  await select('interface-font','1');packet.settings.interfaceNotice="Couldn't save this setting.";await update();
  assert.equal(await page.locator('#interface-font').inputValue(),'3');assert((await font('#settings-title')).startsWith('Arial'));packet.settings.interfaceNotice='';
  // Default follows the theme again; it does not hard-code the preceding font.
  await select('interface-font','0');packet.settings.interfaceFont=0;await update();
  await page.evaluate(()=>document.documentElement.style.setProperty('--theme-font','Arial'));
  assert((await font('#settings-title')).startsWith('Arial'));await page.evaluate(()=>document.documentElement.style.removeProperty('--theme-font'));
  await select('card-name-font','0');packet.settings.cardNameFont=0;await update();assert.equal(await font('#settings-preview-card .card-name'),nameFont);
  await select('card-value-font','0');packet.settings.cardValueFont=0;await update();assert.equal(await font('#settings-preview-card .rank'),valueFont);
  await select('card-nameplate','2');assert(commands.at(-1).endsWith('nameplate 2'));packet.settings.nameplateStyle=2;await update();
  await select('card-name-case','2');assert(commands.at(-1).endsWith('name-case 2'));packet.settings.nameCase=2;await update();
  const name=page.locator('#settings-preview-card .card-name');assert.equal(await name.evaluate(e=>getComputedStyle(e).backgroundImage),'none');assert.equal(await name.evaluate(e=>getComputedStyle(e).textTransform),'uppercase');
  await page.locator('#card-face-names').click();packet.settings.showCardNames=false;await update();assert(await name.isHidden());assert(await page.locator('#card-nameplate-trigger').isDisabled());assert(await page.locator('#card-name-font-trigger').isDisabled());
  await page.locator('#card-face-classic').click();packet.settings.rankLayout=1;await update();
  assert.equal(await page.evaluate(()=>document.documentElement.dataset.rankLayout),'classic');
  await page.locator('#rank-finish-reset').click();assert(commands.at(-1).endsWith('rank-finish-reset 0'));
  Object.assign(packet.settings,{rankShadowOpacity:100,rankShadowBlur:2,rankShadowOffset:1,rankStroke:false,rankStrokeWeight:10});await update();assert.equal(await page.locator('#rank-stroke').getAttribute('aria-checked'),'false');assert.equal(await page.locator('#card-name-case').inputValue(),'2');
  // Fullscreen fills every aspect ratio; returning restores the selected window size.
  await select('settings-window-size','75');await page.locator('#settings-fullscreen').click();
  let rect=await page.locator('#table').boundingBox();assert(Math.abs(rect.x)<1&&Math.abs(rect.y)<1&&Math.abs(rect.width-width)<1&&Math.abs(rect.height-height)<1,JSON.stringify(rect));
  assert(await page.locator('#settings-window-size-trigger').isDisabled());assert.equal(await page.locator('#window-size').inputValue(),'100');
  await page.reload();await update();await page.locator('#album-settings-tab').click();rect=await page.locator('#table').boundingBox();assert(Math.abs(rect.width-width)<1,'Fullscreen persists after reload');
  await page.locator('#album-cards-tab').click();
  const card=await page.locator('#album-grid .collection-card').first().boundingBox();assert(Math.abs(card.width/card.height-.76)<.01,'Fullscreen cards keep their portrait proportions');
  await page.locator('#album-decks-tab').click();const deckCard=await page.locator('#album-grid .collection-card').first().boundingBox();assert(Math.abs(deckCard.width/deckCard.height-.76)<.01,'Fullscreen deck browser keeps portrait proportions');
  await page.locator('#album-settings-tab').click();
  await page.locator('#settings-fullscreen').click();assert.equal(await page.locator('#window-size').inputValue(),'75');
  await page.locator('#settings-section-gameplay').click();assert.equal(await page.locator('#vanilla-voices-only').getAttribute('aria-checked'),'true');
  await page.locator('#vanilla-voices-only').click();assert(commands.at(-1).endsWith('vanilla-voices 0'));assert(await page.locator('#vanilla-voices-only').isDisabled());
  packet.settings.vanillaVoicesOnly=false;await update();assert.equal(await page.locator('#vanilla-voices-only').getAttribute('aria-checked'),'false');
  assert(await page.locator('#settings-preview').isHidden());
  await page.locator('#settings-section-controls').click();await page.locator('#bind-collection').click();packet.settings.capturing='collection';await update();
  await page.locator('#album-cards-tab').click();assert(commands.at(-1).endsWith('binding '+packet.session+' '+packet.revision+' cancel'),'Leaving settings cancels pending capture');
  packet.settings.capturing='';await update();
  await page.locator('#album-settings-tab').click();await page.locator('#settings-section-appearance').click();
  // Every control fits its scroll area; preview stays put while scrolling to the backs.
  for(const id of ['rank-shadow-opacity','rank-stroke','card-back-refresh']){
   await page.locator('#'+id).focus();const b=await page.locator('#'+id).boundingBox(),body=await page.locator('.settings-body').boundingBox();assert(b.x>=body.x&&b.y>=body.y-1&&b.x+b.width<=body.x+body.width+1&&b.y+b.height<=body.y+body.height+1,id+' stays in scroll area');
  }
  const preview=await page.locator('#settings-preview-card').boundingBox();assert(preview.y>=0&&preview.y+preview.height<=height);
  if(width===1920){await page.locator('#rank-shadow-opacity').focus();await page.screenshot({path:'build/previews/settings-values.png'});await page.locator('#settings-section-appearance').click();await page.screenshot({path:'build/previews/settings-appearance.png'});}
  await page.locator('#album-settings-tab').focus();await page.evaluate(()=>ttcgKey('next'));assert.equal(await page.locator('#album-cards-tab').getAttribute('aria-pressed'),'true');
  await page.evaluate(()=>ttcgKey('previous'));assert.equal(await page.locator('#album-settings-tab').getAttribute('aria-pressed'),'true');
  await page.locator('#album-cards-tab').click();
  // New preferences follow shared rendering into the deck and an active match.
  packet.settings.rankStroke=true;packet.settings.rankStrokeWeight=20;packet.settings.cardValueFont=3;await update();await page.locator('#album-decks-tab').click();
  assert((await font('#album-deck .rank')).startsWith('Arial'),'Font applies to the shared deck renderer');
  assert(parseFloat(await page.locator('#album-deck .rank.top').first().evaluate(e=>getComputedStyle(e).webkitTextStrokeWidth))>0);
  await page.locator('#window-size').evaluate(e=>{e.value='100';e.dispatchEvent(new Event('change'));});
  const playing={...packet,session:packet.session+1,screen:'lobby',phase:'match',opening:false,settling:false,thinking:false,turn:0};playing.board[0]={card:0,owner:0};
  await page.evaluate(s=>ttcgState(s),playing);assert(await page.locator('#settings-panel').isHidden());
  assert((await font('#board .rank')).startsWith('Arial'),'Font applies to the shared board renderer');
  assert(parseFloat(await page.locator('#board .rank.top').first().evaluate(e=>getComputedStyle(e).webkitTextStrokeWidth))>0);
  assert.equal(await page.locator('#board .card-name').first().evaluate(e=>getComputedStyle(e).display),'none');
  for(const id of ['board','player-hand','rival-hand','match-footer']){const box=await page.locator('#'+id).boundingBox();assert(box.x>=-1&&box.y>=-1&&box.x+box.width<=width+1&&box.y+box.height<=height+1,id+' fits fullscreen');}
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: Settings navigation, live previews, saved appearance acknowledgements, fullscreen, voices, capture cancellation and shared rendering passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
