const {chromium}=require('playwright'),{spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url'),path=require('node:path'),fs=require('node:fs'),assert=require('node:assert/strict');
const snapshot=(host,args=[])=>JSON.parse(spawnSync(host,args,{input:'',encoding:'utf8',maxBuffer:16*1024*1024}).stdout.trim());
const base=snapshot('/tmp/tessera-decks-host',['economy']),shop=snapshot('/tmp/ttcg-shop-host');
const themes=['tessera','charcoal','oxblood','parchment','umber'],surfaces=['felt','slate','leather','parchment','walnut'];
const out=path.resolve('build/previews/themes');fs.mkdirSync(out,{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{for(const [width,height] of [[1280,720],[1920,1080],[3440,1440]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[],commands=[];
  page.on('pageerror',e=>errors.push(e.message));await page.exposeFunction('ttcgCommand',c=>commands.push(c));
  let packet={...structuredClone(base),screen:'album',matchContext:false,settings:{theme:0,boardSurface:0,boardTexture:10}};
  const update=async()=>{packet.revision++;await page.evaluate(s=>ttcgState(s),packet);};
  const style=(selector,key,pseudo)=>page.locator(selector).first().evaluate((el,[key,pseudo])=>getComputedStyle(el,pseudo)[key],[key,pseudo]);
  const select=async(id,value)=>{await page.locator('#'+id+'-trigger').click();await page.locator('#'+id+'-options [data-value="'+value+'"]').click();};
  const fits=async(child,parent)=>{
   const a=await page.locator(child).boundingBox(),b=await page.locator(parent).boundingBox();
   assert(a&&b&&a.x>=b.x-2&&a.y>=b.y-2&&a.x+a.width<=b.x+b.width+2&&a.y+a.height<=b.y+b.height+2,`${width}: ${child} fits ${parent}`);
  };
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);await update();await page.locator('#album-settings-tab').click();
  assert.deepEqual(await page.locator('#interface-theme option').allTextContents(),['Tessera','Charcoal','Oxblood','Parchment','Umber']);
  // Check real menu writes and host acknowledgements, then all theme defaults.
  for(const [index,theme] of themes.entries()){
   await select('interface-theme',String(index));assert(commands.at(-1).endsWith('theme '+index));
   assert(await page.locator('#interface-theme-trigger').isDisabled());packet.settings.theme=index;await update();
   assert.equal(await page.locator('html').getAttribute('data-theme'),theme);
   assert.equal(await page.locator('html').getAttribute('data-surface'),surfaces[index]);
   assert.equal(await page.locator('#board-texture').inputValue(),'10');
   assert.equal(await style('#table>header','backgroundImage'),'none','Shared header has no inset band');
   await page.locator('#board-texture').focus();await fits('#board-texture','.settings-body');await fits('#settings-board-preview','.settings-body');
   const boxes=await page.locator('#settings-board-preview .demo-square').evaluateAll(es=>es.map(e=>({w:e.offsetWidth,h:e.offsetHeight,x:e.offsetLeft,y:e.offsetTop})));
   assert.equal(new Set(boxes.map(b=>b.x)).size,3);assert.equal(new Set(boxes.map(b=>b.y)).size,3);
   assert(boxes.every(b=>b.w<50&&b.h<65),'Board sample does not inherit full-size tutorial cells');
   if(index)assert.equal(await style('#table','backgroundImage'),await style('#reward .trade-panel','backgroundImage'),'Trade results use the theme');
   if(width===1920){await page.locator('#settings-section-appearance').click();await page.screenshot({path:path.join(out,'runtime-'+theme+'-settings.png')});}
  }
  // Failed writes restore the saved selection without changing the theme.
  await select('interface-theme','1');packet.settings.interfaceNotice="Couldn't save this setting.";await update();
  assert.equal(await page.locator('#interface-theme').inputValue(),'4');assert.equal(await page.locator('html').getAttribute('data-theme'),'umber');
  packet.settings.interfaceNotice='';
  await select('board-surface','3');assert(commands.at(-1).endsWith('board-surface 3'));packet.settings.boardSurface=3;await update();
  await select('interface-theme','2');packet.settings.theme=2;await update();
  assert.equal(await page.locator('html').getAttribute('data-surface'),'felt','Explicit surface survives a theme change');
  const texture=page.locator('#board-texture');
  await texture.evaluate(e=>{e.value='37';e.dispatchEvent(new Event('input',{bubbles:true}));});await update();
  assert.equal(await texture.inputValue(),'37','State refresh preserves an active texture drag');
  await texture.evaluate(e=>e.dispatchEvent(new Event('change',{bubbles:true})));assert(commands.at(-1).endsWith('board-texture 37'));
  await update();assert.equal(await texture.inputValue(),'10','Host owns the persisted texture value');
  packet.settings.boardTexture=37;packet.settings.interfaceFont=1;await update();
  assert((await style('#settings-title','fontFamily')).startsWith('Georgia'),'Font choice wins over theme default');
  await select('board-surface','1');packet.settings.boardSurface=1;await update();assert(await texture.isDisabled());
  assert.equal(await style('#settings-board-preview','backgroundImage','::before'),'none');
  packet.settings.boardSurface=0;packet.settings.interfaceFont=0;await update();
  // Reload with native preferences restores the entire appearance without browser state.
  await page.reload();await update();await page.locator('#album-settings-tab').click();
  assert.equal(await page.locator('#interface-theme').inputValue(),'2');assert.equal(await texture.inputValue(),'37');
  assert((await style('#settings-title','fontFamily')).startsWith('Arial'));
  await page.locator('#album-rules-tab').click();
  assert.equal(await style('#rules-demo-board','backgroundImage'),await style('#settings-board-preview','backgroundImage'),'Rules examples share the chosen surface');
  // Palette and surface survive transitions into a live match and a shop.
  const settings=packet.settings;
  for(const [index,theme] of themes.entries()){
   settings.theme=index;settings.boardTexture=10;settings.boardSurface=0;
   packet={...structuredClone(base),session:packet.session+1,revision:1,screen:'lobby',phase:'match',opening:false,settling:false,thinking:false,turn:0,settings};
   packet.board[0]={card:0,owner:0};await update();
   await fits('#board','#table');await fits('#player-hand','#table');await fits('#rival-hand','#table');
   assert.equal(await page.locator('html').getAttribute('data-theme'),theme);
   assert.equal(await style('#board','backgroundImage'),await style('#rules-demo-board','backgroundImage'));
   assert.equal(await style('#board .card','opacity'),'1');
   await page.evaluate(()=>document.querySelector('#board .square').classList.add('rule-highlight'));
   assert.equal(await style('#board .rule-highlight','zIndex'),'3');assert.notEqual(await style('#board .rule-highlight','boxShadow'),'none');
   if(width===1920)await page.screenshot({path:path.join(out,'runtime-'+theme+'-match.png')});
   packet={...structuredClone(shop),session:packet.session+1,settings};await update();await fits('#shop-grid','#table');
   if(index){
    assert.equal(await style('#shop-name','color'),await style('#table .brand','color'));
    assert.equal(await style('#shop-confirm .modal','backgroundImage'),await style('#table','backgroundImage'));
   }
   if(width===1920)await page.screenshot({path:path.join(out,'runtime-'+theme+'-shop.png')});
  }
  // All material assets ship locally and decode, including in the parchment UI.
  const textures=await page.evaluate(async()=>{
   const root=getComputedStyle(document.documentElement);
   return Promise.all(['fine','paper','leather','wood','slate'].map(async name=>{
    const url=root.getPropertyValue('--grain-'+name).trim().slice(4,-1).replaceAll('"','');
    const img=new Image();img.src=url;await img.decode();return [img.naturalWidth,img.naturalHeight];
   }));
  });assert(textures.every(([w,h])=>w>=384&&h>=384));
  assert.deepEqual(errors,[]);await page.close();
  console.log(`${width}x${height}: theme selection, acknowledgements, surface mixing, font overrides, reload, board previews, shared scenes and material loading passed`);
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
