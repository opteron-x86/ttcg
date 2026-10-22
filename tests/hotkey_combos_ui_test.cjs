const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),fs=require('node:fs'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
function packet(binary,args=[]){const p=spawnSync(binary,args,{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(p.status,0,p.stderr);return JSON.parse(p.stdout.trim());}
const chords=packet('/tmp/ttcg-hotkey_combos-test',['--ui']),base=packet('/tmp/ttcg-ui-host');
fs.mkdirSync('build/hotkey-combos/previews',{recursive:true});
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1920,1080],[3440,1440]]){
  const page=await browser.newPage({viewport:{width,height}}),commands=[],errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  const s=structuredClone(base);Object.assign(s,{screen:'album',matchContext:false,phase:'ready'});
  s.settings={collection:65,challenge:66,toggleGames:0,capturing:'',notice:'',heldModifiers:0};
  async function update(){s.revision++;await page.evaluate(s=>ttcgState(s),s);}
  async function expectCommand(suffix){
   const deadline=Date.now()+2000;
   while(!commands.at(-1)?.endsWith(suffix)&&Date.now()<deadline)await new Promise(resolve=>setTimeout(resolve,10));
   assert(commands.at(-1)?.endsWith(suffix),`Expected ${suffix}, received ${commands.at(-1)}`);
  }
  async function controls(){await page.locator('#album-settings-tab').click();await page.locator('#settings-section-controls').click();}
  async function begin(field){await page.locator('#bind-'+field).click();s.settings.capturing=field;await update();}
  async function accept(field,value,modifiers=0){s.settings[field]=value;s.settings.capturing='';s.settings.heldModifiers=modifiers;await update();}
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await update();await controls();
  await begin('collection');let count=commands.length;
  await page.keyboard.down('ShiftLeft');assert.equal(commands.length,count,'Modifier alone never becomes the shortcut');
  assert.equal(await page.locator('#bind-collection').textContent(),'LShift + …');
  await page.keyboard.down('F7');await expectCommand('set collection '+chords.leftShiftF7);
  await accept('collection',chords.leftShiftF7,chords.leftShiftF7&~255);
  assert.equal(await page.locator('#bind-collection').textContent(),'LShift + F7');
  count=commands.length;await page.keyboard.down('F7');await page.keyboard.up('ShiftLeft');await page.keyboard.down('F7');
  assert.equal(commands.length,count,'Repeats and releasing the modifier first cannot replay the binding');await page.keyboard.up('F7');
  s.settings.heldModifiers=0;await update();

  // The opposite side is separately captured; both can use the same main key.
  await begin('challenge');await page.keyboard.press('ShiftRight+F7');
  await expectCommand('set challenge '+chords.rightShiftF7);
  await accept('challenge',chords.rightShiftF7);assert.equal(await page.locator('#bind-challenge').textContent(),'RShift + F7');
  await begin('toggleGames');await page.keyboard.press('ControlLeft+AltRight+F8');
  await expectCommand('set toggleGames '+chords.controlAltF8);
  await accept('toggleGames',chords.controlAltF8);assert.equal(await page.locator('#bind-toggleGames').textContent(),'LCtrl + RAlt + F8');
  if(width===1920)await page.screenshot({path:'build/hotkey-combos/previews/controls.png'});

  // A failed/conflicting write keeps the old binding and lets capture continue.
  await begin('challenge');await page.keyboard.press('ShiftLeft+F7');s.settings.notice='Already used for Album.';await update();
  assert.equal(await page.locator('#binding-status').textContent(),s.settings.notice);
  await page.keyboard.press('ControlRight+ShiftRight+F7');
  const rightCombo=chords.rightShiftF7|2048;
  await expectCommand('set challenge '+rightCombo);
  s.settings.notice='';await accept('challenge',rightCombo);assert.equal(await page.locator('#bind-challenge').textContent(),'RCtrl + RShift + F7');

  // Every supported modifier can be held together and stays readable.
  await begin('toggleGames');
  const mods=['ControlLeft','ControlRight','ShiftLeft','ShiftRight','AltLeft','AltRight'];
  for(const key of mods)await page.keyboard.down(key);
  await page.keyboard.press('F6');await expectCommand('set toggleGames '+chords.allModifiersF6);
  for(const key of mods.toReversed())await page.keyboard.up(key);
  await accept('toggleGames',chords.allModifiersF6);
  const binding=page.locator('#bind-toggleGames');assert.equal(await binding.textContent(),'LCtrl + RCtrl + LShift + RShift + LAlt + RAlt + F6');
  assert(await binding.evaluate(e=>e.scrollWidth<=e.clientWidth),'Long combinations wrap inside the button');
  const bounds=await binding.boundingBox(),body=await page.locator('.settings-body').boundingBox();
  assert(bounds.x>=body.x&&bounds.x+bounds.width<=body.x+body.width,'Binding stays inside settings');

  await begin('toggleGames');count=commands.length;
  await page.keyboard.press('ShiftLeft');await page.keyboard.press('ControlRight');await page.keyboard.press('AltLeft');
  assert.equal(commands.length,count);await page.keyboard.press('Escape');await expectCommand(' cancel');
  s.settings.capturing='';await update();assert.equal(await binding.textContent(),'LCtrl + RCtrl + LShift + RShift + LAlt + RAlt + F6');
  await page.locator('#reset-toggleGames').click();await expectCommand('reset toggleGames');await accept('toggleGames',0);
  await page.locator('#unbind-challenge').click();await expectCommand('unbind challenge');await accept('challenge',0);
  await page.locator('#reset-challenge').click();await expectCommand('reset challenge');await accept('challenge',66);

  // Full chords activate in the UI; partial/wrong/extra modifiers do not.
  await page.locator('#album-cards-tab').click();await page.locator('#leave').focus();count=commands.length;
  for(const key of ['F7','ShiftRight+F7','ControlLeft+ShiftLeft+F7','ShiftLeft+F8','MetaLeft+F7'])await page.keyboard.press(key);
  assert.equal(commands.length,count);assert(await page.locator('#table').isVisible());
  await page.keyboard.press('ShiftLeft+F7');assert(await page.locator('#table').isHidden());assert(commands.at(-1).startsWith('close '));
  s.session++;await update();await page.locator('#album-cards-tab').click();
  s.settings.toggleGames=chords.controlAltF8;await update();await page.locator('#leave').focus();count=commands.length;
  for(const key of ['ControlLeft+F8','AltRight+F8','ControlRight+AltRight+F8'])await page.keyboard.press(key);
  assert.equal(commands.length,count);await page.keyboard.press('ControlLeft+AltRight+F8');await expectCommand('games 0');

  // Modifier keydown may predate UI focus; use the native held-key snapshot.
  s.settings.heldModifiers=256;await update();count=commands.length;
  await page.evaluate(()=>document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{code:'F7',key:'F7',shiftKey:true,bubbles:true})));
  assert.equal(commands.length,count+1);assert(await page.locator('#table').isHidden());
  s.settings.heldModifiers=0;s.session++;await update();await page.locator('#album-cards-tab').click();
  s.settings.collection=256|18;await update();await page.locator('#album-search').focus();count=commands.length;
  await page.keyboard.press('ShiftLeft+KeyE');assert.equal(commands.length,count,'Typing a full bound chord cannot close the view');
  await page.locator('#leave').focus();await page.keyboard.down('ShiftLeft');
  await page.evaluate(()=>window.dispatchEvent(new Event('blur')));await page.keyboard.up('ShiftLeft');await page.keyboard.press('KeyE');
  assert.equal(commands.length,count,'Blur/release clears modifiers');await page.keyboard.press('ShiftLeft+KeyE');assert(await page.locator('#table').isHidden());

  // Native settings, not browser storage, restore chord labels after a reload.
  s.settings.collection=chords.leftShiftF7;s.settings.challenge=chords.rightShiftF7;s.settings.toggleGames=chords.controlAltF8;s.session++;
  await page.reload();await update();await controls();
  assert.deepEqual(await page.locator('.binding-row button[id^="bind-"]').allTextContents(),['LShift + F7','RShift + F7','LCtrl + RAlt + F8']);
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: chord capture, left/right and multiple modifiers, repeats, conflict recovery, exact shortcuts, search typing, focus and reload passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
