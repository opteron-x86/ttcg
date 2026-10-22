const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]) {
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];
  const s=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
  s.screen='album';s.matchContext=false;
  s.settings={collection:65,challenge:66,capturing:'',notice:''};
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',text=>commands.push(text));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const update=()=>page.evaluate(s=>window.ttcgState(s),s);
  await update();await page.locator('#album-settings-tab').click();assert(await page.locator('#settings-panel').isVisible());
  await page.locator('#settings-section-gameplay').click();
  const pause=page.locator('#pause-world-in-album');
  const pauseSwitch=name=>page.locator('#'+({'Pause in shops':'pause-world-in-shops','Pause in match setup':'pause-world-in-match-setup','Pause in match':'pause-world-during-matches'}[name]));
  assert.equal(await pause.getAttribute('aria-checked'),'true','Missing preference defaults to paused');
  await pause.click();assert(commands.at(-1).endsWith('settings '+s.session+' '+s.revision+' pause-album 0'));
  assert(await pause.isDisabled(),'Wait for the native acknowledgement');
  s.settings.pauseWorldInAlbum=false;await update();assert.equal(await pause.getAttribute('aria-checked'),'false');assert(await pause.isEnabled());
  await pause.press('Space');assert(await pause.isDisabled());assert(commands.at(-1).endsWith('pause-album 1'));
  s.settings.interfaceNotice="Couldn't save this setting.";await update();
  assert.equal(await pause.getAttribute('aria-checked'),'false','Failed writes retain the saved value');
  assert.equal(await page.locator('#interface-status').innerText(),s.settings.interfaceNotice);
  s.settings.interfaceNotice='';s.settings.pauseWorldInAlbum=true;await update();
  assert(await page.locator('#interface-status').isHidden());assert.equal(await pause.getAttribute('aria-checked'),'true');
  assert(await page.locator('#pause-world-in-shops').isHidden(),'Experimental options start collapsed');
  assert(await page.locator('#pause-world-in-match-setup').isHidden());
  assert(await page.locator('#pause-world-during-matches').isHidden());
  assert(await page.evaluate(()=>visibleControls($('settings-panel')).some(el=>el.id==='experimental-pause-toggle')),'Experimental section supports controller navigation');
  await page.locator('#experimental-pause-toggle').focus();await page.evaluate(()=>ttcgKey('confirm'));
  assert(await page.locator('#experimental-pause-note').isVisible());
  const otherPauseOptions=[['Pause in shops','pauseWorldInShops','pause-shop'],['Pause in match setup','pauseWorldInMatchSetup','pause-setup'],['Pause in match','pauseWorldDuringMatches','pause-match']];
  assert.equal(await page.locator('.world-pause-settings [role="switch"]').count(),4);
  for(const [name,key,command] of otherPauseOptions){
   const control=pauseSwitch(name);
   assert.equal(await control.getAttribute('aria-checked'),'true');
   await control.click();assert(await control.isDisabled());assert(commands.at(-1).endsWith(command+' 0'));
   s.settings[key]=false;await update();assert.equal(await control.getAttribute('aria-checked'),'false');
   assert.equal(await pause.getAttribute('aria-checked'),'true','Independent album preference');
   for(const [otherName] of otherPauseOptions)if(otherName!==name)assert.equal(await pauseSwitch(otherName).getAttribute('aria-checked'),'true','Other screens keep their preference');
   await control.press('Space');assert(await control.isDisabled());assert(commands.at(-1).endsWith(command+' 1'));
   s.settings[key]=true;await update();
  }
  const body=await page.locator('.settings-body').boundingBox();
  for(const control of await page.locator('.world-pause-settings [role="switch"]').all()){
   await control.focus();const bounds=await control.boundingBox();assert(bounds.x>=body.x&&bounds.x+bounds.width<=body.x+body.width&&bounds.y>=body.y&&bounds.y+bounds.height<=body.y+body.height,'Every pause switch scrolls fully into view');
   assert(await control.evaluate(el=>el.scrollWidth<=el.clientWidth),'Switch text fits without horizontal overflow');
  }
  if(width===1440)await page.screenshot({path:'build/settings-experimental.png'});
  await page.locator('#settings-section-controls').click();await page.locator('#bind-collection').click();assert(commands.at(-1).endsWith('begin collection'));
  s.settings.capturing='collection';await update();
  assert(await pause.isDisabled(),'No settings changes during key capture');
  for(const [name] of otherPauseOptions)assert(await pauseSwitch(name).isDisabled());
  await page.keyboard.press('F6');assert(commands.at(-1).endsWith('set collection 64'));
  s.settings.collection=64;s.settings.capturing='';await update();assert.equal(await page.locator('#bind-collection').innerText(),'F6');
  await page.locator('#bind-collection').click();s.settings.capturing='collection';await update();await page.keyboard.press('F8');
  assert(commands.at(-1).endsWith('set collection 66'));s.settings.notice='Already used for Challenge.';await update();
  assert.equal(await page.locator('#binding-status').innerText(),'Already used for Challenge.');
  await page.keyboard.press('Escape');assert(commands.at(-1).endsWith(' cancel'));assert(await page.locator('#settings-panel').isVisible());
  s.settings.capturing='';s.settings.notice='';await update();
  await page.locator('#unbind-collection').click();assert(commands.at(-1).endsWith('unbind collection'));s.settings.collection=0;await update();
  assert.equal(await page.locator('#bind-collection').innerText(),'Unbound');assert(await page.locator('#unbind-collection').isDisabled());
  if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/album-settings.png'});
  await page.locator('#album-cards-tab').click();assert(await page.locator('#settings-panel').isHidden());
  await page.keyboard.press('F7');assert(await page.locator('#table').isVisible(),'Old/unbound key does nothing');
  await page.locator('#album-settings-tab').click();await page.locator('#settings-section-controls').click();await page.locator('#reset-collection').click();assert(commands.at(-1).endsWith('reset collection'));
  assert.equal(await pause.getAttribute('aria-checked'),'true','Preference survives closing and reopening settings');
  s.settings.collection=65;await update();await page.locator('#album-cards-tab').click();
  s.settings.collection=18;s.screen='album';s.matchContext=false;await update();await page.locator('#album-search').fill('');await page.locator('#album-search').press('KeyE');assert(await page.locator('#table').isVisible(),'Typing a bound key cannot close the view');
  await page.locator('#leave').focus();await page.keyboard.press('KeyE');assert(await page.locator('#table').isHidden(),'New key closes the view');
  s.session++;s.phase='match';await update();assert(await page.locator('#album-settings-tab').isHidden());await page.evaluate(()=>window.ttcgReset());
  assert(await page.locator('#settings-panel').isHidden());assert.deepEqual(errors,[]);
  console.log(`${width}x${height}: four independent pause switches, acknowledgement/failure, layout, key capture, shortcuts and cleanup passed`);await page.close();
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
