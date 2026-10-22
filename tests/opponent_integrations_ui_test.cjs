const {chromium}=require('playwright'),{spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  s.screen='album';s.matchContext=false;s.settings={collection:65,challenge:66,residents:1};
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',v=>commands.push(v));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const update=()=>page.evaluate(s=>ttcgState(s),s);await update();await page.locator('#album-settings-tab').click();await page.locator('#settings-section-gameplay').click();
  const mode=page.getByRole('combobox',{name:'Mod-added players'}),add=page.locator('#resident-add');
  assert.equal(await mode.innerText(),'Selected residents');assert(await add.isDisabled());
  await mode.click();await page.getByRole('option',{name:'All eligible residents',exact:true}).click();
  assert(commands.at(-1).endsWith('residents 2'));assert(await mode.isDisabled());
  s.settings.residents=2;await update();assert.equal(await mode.innerText(),'All eligible residents');assert(await mode.isEnabled());
  s.settings.residents=0;s.settings.residentCandidate='Add Resident of a Distant Town as a Tessera player.';s.settings.canAddResident=true;await update();
  assert.equal(await mode.innerText(),'Off');assert(await add.isEnabled());await add.click();assert(commands.at(-1).includes(`npc-add ${s.session} ${s.revision}`));assert(await add.isDisabled());
  s.settings.canAddResident=false;s.settings.residentCandidate='Resident of a Distant Town already plays Tessera.';await update();assert(await add.isDisabled());
  s.settings.canAddResident=true;s.settings.residentCandidate='Add Resident of a Distant Town as a Tessera player.';s.settings.capturing='challenge';await update();assert(await mode.isDisabled());assert(await add.isDisabled());
  s.settings.capturing='';await update();await mode.focus();assert(await page.evaluate(()=>visibleControls($('settings-panel')).includes($('resident-mode-trigger'))));
  const bounds=await mode.boundingBox(),body=await page.locator('.settings-body').boundingBox();assert(bounds.x>=body.x&&bounds.x+bounds.width<=body.x+body.width+1);
  assert(await mode.evaluate(el=>el.scrollWidth<=el.clientWidth));
  await page.screenshot({path:`build/npc-settings-${width}.png`});await page.locator('#album-cards-tab').click();
  (s.collection.regions??=[]).push({id:12,name:'Elsewhere',rules:16,trade:1,unaffiliated:true});
  (s.collection.roster??=[]).push({base:0xE0000000,name:'Resident of a Distant Town',location:'New lands',hold:'Elsewhere',holdID:12,skill:'Regular',wins:1,losses:0,draws:0,unlocked:true});await update();
  await page.locator('#album-opponents-tab').click();await page.locator('#journal-chapter-12').click();
  assert.equal(await page.locator('#journal-title').innerText(),'Elsewhere');assert(await page.locator('#progress-step').isHidden());
  assert.match(await page.locator('#hold-rules').innerText(),/Open\s+One/);assert.equal(await page.locator('.opponent-row').count(),1);
  assert.match(await page.locator('#journal-player').innerText(),/New lands/);assert.doesNotMatch(await page.locator('#journal-player').innerText(),/Whiterun/);
  assert.deepEqual(errors,[]);await page.close();
  console.log(`${width}x${height}: resident modes, acknowledgements, manual enrollment, keyboard capture, shared layout and Elsewhere journal passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
