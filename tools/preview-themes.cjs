// Developer-only theme studies built from the real Tessera DOM and card renderer.
// Run: node tools/preview-themes.cjs [--no-capture]
// Output: build/previews/themes/index.html (also works when served from the repo root).
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'..'),source=path.join(root,'tools/theme-preview'),out=path.join(root,'build/previews/themes');
const themes=['charcoal','oxblood','parchment','umber','tessera'];
const names={charcoal:'Charcoal',oxblood:'Oxblood',parchment:'Parchment',umber:'Umber',tessera:'Tessera'};
function build(){
 fs.mkdirSync(out,{recursive:true});
 const host=process.env.TESSERA_PREVIEW_HOST||'/tmp/tessera-decks-host';
 const result=spawnSync(host,['economy'],{input:'',encoding:'utf8',maxBuffer:16*1024*1024});
 if(result.error||result.status!==0)throw new Error(`Build tests/decks_host.cpp as ${host} first. ${result.error||result.stderr}`);
 const packet=JSON.parse(result.stdout.trim());
 let css=fs.readFileSync(path.join(source,'themes.css'),'utf8');
 const isolation=`<script>const previewStorage=new Map();Object.defineProperty(window,'localStorage',{value:{getItem:k=>previewStorage.get(k)??null,setItem:(k,v)=>previewStorage.set(k,String(v)),removeItem:k=>previewStorage.delete(k)}});window.ttcgCommand=()=>{};</script>`;
 const script='const previewFixture='+JSON.stringify(packet).replaceAll('<','\\u003c')+';\n'+fs.readFileSync(path.join(source,'frame.js'),'utf8');
 let html=fs.readFileSync(path.join(root,'TesseraUI/index.html'),'utf8');
 html=html.replace('<head>','<head><base href="../../../TesseraUI/">'+isolation)
  .replace('</head>','<style>'+css+'</style></head>').replace('</body>','<script>'+script+'</script></body>');
 fs.writeFileSync(path.join(out,'frame.html'),html);
 for(const file of ['index.html','review.css','review.js'])fs.copyFileSync(path.join(source,file),path.join(out,file));
}
async function ready(page){
 await page.waitForFunction(()=>window.previewReady===true);
 await page.waitForFunction(()=>[...document.images].filter(i=>i.getClientRects().length&&i.getAttribute('src')).every(i=>i.complete&&i.naturalWidth>0),{},{timeout:20000});
 await page.waitForTimeout(140);
}
function sheet(title,subtitle,tiles,columns=2){return `<!doctype html><html><meta charset="utf-8"><style>*{box-sizing:border-box}body{margin:0;padding:38px;background:#101211;color:#e0d6c3;font:14px Arial,sans-serif}header{display:flex;align-items:baseline;gap:28px;padding:0 8px 30px}h1{font:28px Georgia,serif;letter-spacing:2px;margin:0}header span{color:#92998f;font-size:14px}main{display:grid;grid-template-columns:repeat(${columns},minmax(0,1fr));gap:24px}figure{margin:0;border:1px solid #c9b98a30;background:#191c18}figcaption{display:flex;justify-content:space-between;align-items:center;padding:16px 20px;font:22px Georgia,serif}small{font:12px Arial,sans-serif;color:#a39d8c}img{display:block;width:100%;height:auto}footer{padding:25px 8px 0;color:#92998f;font-size:12px}</style><header><h1>TESSERA</h1><span>${title} · ${subtitle}</span></header><main>${tiles.map(t=>`<figure><figcaption>${t.title}<small>${t.detail}</small></figcaption><img src="${t.src}"></figure>`).join('')}</main><footer>Design studies · Card art, rarity, and ownership treatments are shared across themes.</footer></html>`;}
async function capture(){
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({viewport:{width:1600,height:1056},deviceScaleFactor:1,reducedMotion:'reduce'}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto(pathToFileURL(path.join(out,'frame.html')).href);await ready(page);
  for(const theme of themes){
   for(const scene of ['match','album','lobby','settings','board']){
    await page.evaluate(p=>showPreview(p),{theme,scene,surface:'theme',empty:false,fullscreen:false,grain:10});await ready(page);
    for(const selector of scene==='match'?['#board','#player-hand','#rival-hand','#match-footer']:scene==='album'?['#album-grid','#album .album-inspector']:scene==='settings'?['#settings-panel']:[]){
     const box=await page.locator(selector).boundingBox();assert(box&&box.x>=0&&box.y>=0&&box.x+box.width<=1601&&box.y+box.height<=1057,`${theme}/${scene}: ${selector} fits the window`);
    }
    await page.screenshot({path:path.join(out,`${theme}-${scene}.png`)});
   }
   console.log('Captured '+theme+' in match, album, setup, settings, and board detail.');
  }
  const surfaces=['slate','felt','parchment','leather','walnut','plain'];
  for(const surface of surfaces){
   await page.evaluate(p=>showPreview(p),{theme:'tessera',scene:'board',surface,empty:true,grain:10});await ready(page);
   await page.locator('#board').screenshot({path:path.join(out,'surface-'+surface+'.png')});
  }
  // Compare real hover, focus and open-dropdown states, not just static canvases.
  for(const theme of themes){
   await page.evaluate(theme=>showPreview({theme,scene:'album'}),theme);await ready(page);
   await page.locator('#album-cards-tab').click();
   await page.locator('#album-sort-trigger').click();assert(await page.locator('#album-sort-options').isVisible());
   await page.keyboard.press('Escape');await page.locator('#album-settings-tab').click();
   await page.locator('#settings-section-controls').hover();assert(await page.locator('#settings-panel').isVisible());
  }
  assert.deepEqual(errors,[],'No renderer exceptions');
  const details={charcoal:'Slate · Silver',oxblood:'Leather · Oxblood',parchment:'Parchment · Ink',umber:'Walnut · Brass'};
  for(const scene of ['match','album']){
   const tiles=themes.slice(0,4).map(theme=>({title:names[theme],detail:details[theme],src:`${theme}-${scene}.png`}));
   const basename=scene==='match'?'comparison':'album-comparison';
   fs.writeFileSync(path.join(out,basename+'.html'),sheet(scene==='match'?'Window & board studies':'Album studies','Four directions',tiles));
   await page.setViewportSize({width:2000,height:1200});await page.goto(pathToFileURL(path.join(out,basename+'.html')).href);
   await page.locator('img').evaluateAll(imgs=>Promise.all(imgs.map(i=>i.decode())));
   await page.screenshot({path:path.join(out,basename+'.png'),fullPage:true});
  }
  fs.writeFileSync(path.join(out,'surfaces.html'),sheet('Board surfaces','Independent of the window theme',surfaces.map(s=>({title:s[0].toUpperCase()+s.slice(1),detail:'Texture 10%',src:'surface-'+s+'.png'})),3));
  await page.setViewportSize({width:1600,height:1200});await page.goto(pathToFileURL(path.join(out,'surfaces.html')).href);
  await page.locator('img').evaluateAll(imgs=>Promise.all(imgs.map(i=>i.decode())));await page.screenshot({path:path.join(out,'surfaces.png'),fullPage:true});
  // Review controls must work through postMessage even for local file:// iframes.
  await page.setViewportSize({width:1600,height:1056});await page.goto(pathToFileURL(path.join(out,'index.html')).href);
  const frame=page.frameLocator('#preview');await frame.locator('#board').waitFor({state:'visible'});
  assert.equal(await page.locator('#grain').inputValue(),'10');
  assert.equal(await frame.locator('#theme-corners .scratches').count(),0);
  await page.locator('[data-theme=parchment]').click();await frame.locator('html[data-theme=parchment]').waitFor({state:'attached'});
  await page.locator('#surface').selectOption('felt');await frame.locator('html[data-surface=felt]').waitFor({state:'attached'});
  await page.locator('[data-scene=album]').click();await frame.locator('#album-grid').waitFor({state:'visible'});
  await page.locator('[data-scene=settings]').click();await frame.locator('#settings-panel').waitFor({state:'visible'});
  for(const [id,value] of [['interface-font','3'],['card-name-font','2'],['card-value-font','4']]){
   await frame.locator('#'+id+'-trigger').click();await frame.locator('#'+id+'-options [data-value="'+value+'"]').click();
   assert.equal(await frame.locator('#'+id).inputValue(),value);
  }
  assert((await frame.locator('#settings-preview-card .rank').first().evaluate(e=>getComputedStyle(e).fontFamily)).startsWith('Verdana'));
  await page.locator('#board-toggle').click();await frame.locator('html[data-scene=board]').waitFor({state:'attached'});
  await page.locator('#empty-board').check();await page.waitForTimeout(150);assert.equal(await frame.locator('#board .card').count(),0);
  await page.locator('#large-window').check();await frame.locator('html[data-fullscreen=true]').waitFor({state:'attached'});
  assert.deepEqual(errors,[],'No gallery exceptions');
  console.log('Verified image loading, panel bounds, dropdowns, Settings navigation, theme/surface controls, empty board and fullscreen.');
 }finally{await browser.close();}
}
(async()=>{build();if(!process.argv.includes('--no-capture'))await capture();console.log(path.join(out,'index.html'));})().catch(error=>{console.error(error);process.exitCode=1;});
