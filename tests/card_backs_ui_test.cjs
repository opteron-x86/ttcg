const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawn,spawnSync}=require('node:child_process'),{createInterface}=require('node:readline');
const fs=require('node:fs'),os=require('node:os'),http=require('node:http'),path=require('node:path'),assert=require('node:assert/strict');
const ui=path.resolve('TesseraUI'),temporary=fs.mkdtempSync(path.join(os.tmpdir(),'tessera-backs-ui-')),art=path.join(temporary,'art');
fs.mkdirSync(art);fs.cpSync(path.join(ui,'art/backs'),path.join(art,'backs'),{recursive:true});
(async()=>{
 const server=http.createServer((req,res)=>{
  const url=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
  const root=url.startsWith('/art/backs/')?temporary:ui,file=path.resolve(root,'.'+url);
  if(!file.startsWith(root+path.sep)){res.writeHead(403).end();return;}
  const types={'.js':'application/javascript','.css':'text/css','.html':'text/html','.png':'image/png','.webp':'image/webp','.jpg':'image/jpeg','.jpeg':'image/jpeg'};
  try{res.setHeader('Content-Type',types[path.extname(file).toLowerCase()]||'application/octet-stream');res.end(fs.readFileSync(file));}catch{res.writeHead(404).end();}
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
 try{
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  for(const [width,height] of [[1440,900],[1280,720],[3840,2160]]){
   const preferences=path.join(temporary,`settings-${width}.txt`),host=spawn('/tmp/ttcg-card-backs-host',[art,preferences]);
   const lines=createInterface({input:host.stdout})[Symbol.asyncIterator]();
   const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[];
   try{
    let packet=JSON.parse((await lines.next()).value),pending=Promise.resolve();
    page.on('pageerror',e=>errors.push(e.message));
    await page.exposeFunction('ttcgCommand',command=>{pending=pending.then(async()=>{host.stdin.write(command+'\n');packet=JSON.parse((await lines.next()).value);await page.evaluate(s=>ttcgState(s),packet);});});
    await page.goto(`http://127.0.0.1:${server.address().port}/index.html`);
    await page.evaluate(s=>ttcgState(s),packet);
    const settle=async()=>{await page.waitForTimeout(35);await pending;};
    const command=async(verb,args='')=>{await page.evaluate(([v,a])=>send(v,a),[verb,args]);await settle();};
    const choice=id=>page.locator(`#card-back-choices [data-back="${id}"]`);
    const imageIs=async name=>page.waitForFunction(name=>getComputedStyle(document.documentElement).getPropertyValue('--card-back-image').includes(name),name);
    const backgroundIs=async(selector,name)=>assert.match(await page.locator(selector).first().evaluate(el=>getComputedStyle(el).backgroundImage),new RegExp(name));
    await page.locator('#album-settings-tab').click();await page.waitForFunction(()=>[...document.querySelectorAll('.card-back-choice')].every(b=>!b.disabled));
    assert.deepEqual(await page.locator('.card-back-name').allTextContents(),['Mosaic','Dragon','Celestial']);
    assert.equal(await choice('mosaic').getAttribute('aria-pressed'),'true');
    await choice('mosaic').focus();await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(()=>document.activeElement.dataset.back),'dragon');
    await page.keyboard.press('Space');await settle();assert.equal(packet.cardBackID,'dragon');await imageIs('dragon.webp');
    await page.keyboard.press('Tab');await page.keyboard.press('Space');await settle();
    assert.equal(packet.cardBackID,'celestial','Keyboard selects the next back');await imageIs('celestial.webp');
    const restored=JSON.parse(spawnSync('/tmp/ttcg-card-backs-host',[art,preferences],{input:'',encoding:'utf8'}).stdout);
    assert.equal(restored.cardBackID,'celestial','A fresh native process restores the saved selection');
    const modal=await page.locator('#table').boundingBox(),done=await page.locator('#album-cards-tab').boundingBox();
    assert(modal.y>=0&&modal.y+modal.height<=height&&done.y>=modal.y&&done.y+done.height<=modal.y+modal.height,'Settings and album navigation fit in the viewport');
    await page.locator('.card-back-help summary').focus();
    const help=await page.locator('.card-back-help summary').boundingBox(),body=await page.locator('.settings-body').boundingBox();
    assert(help.y>=body.y&&help.y+help.height<=body.y+body.height+2,`Card-back help scrolls into view: ${JSON.stringify({help,body})}`);
    if(width===1440)await page.screenshot({path:'build/clean-catalog/screenshots/card-back-settings.png'});
    await command('test-readonly');await choice('dragon').click();await settle();assert.equal(packet.cardBackID,'celestial');
    assert.match(await page.locator('#card-back-status').innerText(),/Couldn't save/);await command('test-writable');
    await page.locator('#settings-section-controls').click();await page.locator('#bind-collection').click();await settle();assert(await choice('dragon').isDisabled());assert(await page.locator('#card-back-refresh').isDisabled());
    await command('cardback','select dragon');assert.equal(packet.cardBackID,'celestial','Capture also blocks native selection');
    await page.locator('#binding-cancel').click();await settle();await page.locator('#settings-section-appearance').click();
    const customName="Moon & Star's_夜 100%.WEBP",customID="custom/moon & star's_夜 100%",customFile=path.join(art,'backs/custom',customName);
    fs.copyFileSync(path.join(art,'backs/celestial.webp'),customFile);
    await page.locator('#card-back-refresh').click();await settle();await choice(customID).click();await settle();assert.equal(packet.cardBackID,customID);
    await imageIs(encodeURIComponent(customName));
    assert(!fs.readFileSync(preferences,'utf8').includes('夜'),'INI stores Unicode as ASCII escapes');
    await page.locator('#album-cards-tab').click();await settle();
    await page.locator('#album-card-pack').click();await backgroundIs('#inspect-card .pack-preview','assets/packs/standard.png');
    await page.locator('#album-grid .collection-card').first().click();await page.locator('#inspect-card').click();
    await page.locator('#inspect-flip').click();await backgroundIs('.art-back','100%25');await page.keyboard.press('Escape');
    await page.locator('#album-rules-tab').click();await page.locator('#rules-list button').filter({hasText:'Hidden hands'}).click();
    await backgroundIs('#rules-rival-hand .card-back','100%25');
    await command('test-match');assert(await page.locator('#album-settings-tab').isHidden());await backgroundIs('#rival-hand .card-back','100%25');
    await command('test-ready');await page.locator('#album-settings-tab').click();
    fs.unlinkSync(customFile);await page.locator('#card-back-refresh').click();await settle();assert.equal(packet.cardBackID,'mosaic');assert(packet.cardBackMissing);
    await imageIs('mosaic.webp');assert.match(await page.locator('#card-back-status').innerText(),/not found/);
    fs.copyFileSync(path.join(art,'backs/dragon.webp'),customFile);await page.locator('#card-back-refresh').click();await settle();
    assert.equal(packet.cardBackID,customID);await imageIs(encodeURIComponent(customName));
    // An invalid replacement must not leave hidden hands or inspection blank.
    fs.writeFileSync(customFile,'invalid WebP');await page.locator('#card-back-refresh').click();await settle();await imageIs('mosaic.webp');
    await page.waitForFunction(()=>document.querySelector('.card-back-choice[data-loaded="false"]'));
    assert(await choice(customID).isDisabled());assert.match(await page.locator('#card-back-status').innerText(),/couldn’t be opened/);
    fs.copyFileSync(path.join(art,'backs/dragon.webp'),customFile);await page.locator('#card-back-refresh').click();await settle();await imageIs(encodeURIComponent(customName));
    await choice('mosaic').click();await settle();fs.unlinkSync(customFile);await command('cardback','refresh');
    await command('test-development');await page.locator('#settings-section-developer').click();assert(await page.locator('#development-settings').isVisible());
    await page.locator('#dev-add-cards').scrollIntoViewIfNeeded();assert(await page.locator('#dev-add-cards').isVisible());
    await page.locator('#settings-section-appearance').click();await page.locator('.card-back-help summary').click();assert(await page.locator('.card-back-help code').isVisible());
    assert.deepEqual(errors,[]);console.log(`${width}x${height}: saved backs, keyboard, Unicode custom images, native failures, refresh, missing/broken fallback and shared surfaces passed`);
   }finally{await page.close();host.stdin.end();host.kill();}
  }
 }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));fs.rmSync(temporary,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
