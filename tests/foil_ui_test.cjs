const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {spawnSync}=require('node:child_process'),{pathToFileURL}=require('node:url');
const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const output=path.resolve('build/prismatic-runtime');fs.mkdirSync(output,{recursive:true});
const fixture=JSON.parse(spawnSync('/tmp/ttcg-ui-host',{input:'',encoding:'utf8'}).stdout.trim());
const foil=fixture.cards.findIndex(c=>c.foil&&c.name==='Nocturnal');assert(foil>=0);
fixture.screen='album';fixture.matchContext=false;fixture.opponent='';
Object.assign(fixture.collection,{owned:fixture.cards.map((_,i)=>i<5||i===foil?1:0),known:fixture.cards.map((_,i)=>i<5||i===foil),deck:[foil,1,2,3,4],holdID:1,regions:[{id:1,name:'Whiterun',rules:0,trade:1}],packs:[2,0],revealed:[],revealID:0});
const pageURL=pathToFileURL(path.resolve('TesseraUI/index.html')).href;
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({viewport:{width:1600,height:1050}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto(pageURL);await page.evaluate(s=>ttcgState(s),fixture);
  await page.locator('#album-card-'+foil).click();
  await page.waitForFunction(()=>document.querySelector('#inspect-card .material-ready'));
  assert.equal(await page.locator('#album-grid .foil-material').count(),1,'Standard cards do not allocate foil canvases');
  assert.equal(await page.evaluate(()=>foilMaterial.diagnostics().contexts),1);
  const card=page.locator('#inspect-card .foil'),canvas=card.locator('.foil-material');
  assert.equal(await canvas.evaluate(c=>getComputedStyle(c).opacity),'0.72');
  const before=await canvas.evaluate(c=>c.toDataURL());
  await card.hover({position:{x:35,y:60}});await page.waitForFunction(()=>foilMotion.frame===0);
  const after=await canvas.evaluate(c=>c.toDataURL());assert.notEqual(after,before,'Pointer light changes the rendered foil');
  assert(await card.evaluate(c=>{const copy=foilMaterial.clone(c),second=foilMaterial.clone(copy);return c.querySelector('canvas').toDataURL()===second.querySelector('canvas').toDataURL();}),'Trading and dragging preserve foil pixels through nested card clones');
  assert.equal(await card.evaluate(c=>c.style.transform),'','Foil does not override card placement/flip transforms');
  assert.equal(await card.locator('.rank').count(),4);
  await page.mouse.move(5,5);await page.waitForFunction(()=>foilMotion.frame===0);
  const idle=await page.evaluate(()=>foilMaterial.diagnostics().draws);await page.waitForTimeout(140);
  assert.equal(await page.evaluate(()=>foilMaterial.diagnostics().draws),idle,'Idle cards consume no render loop');
  await page.screenshot({path:path.join(output,'album.png')});
  await page.locator('#inspect-card').click();
  await page.waitForFunction(()=>document.querySelector('.art-front.material-ready'));
  assert.equal(await page.locator('.art-front .rank,.art-front .tier,.art-front .card-name').count(),0);
  const scene=page.locator('.art-scene'),art=page.locator('.art-front .foil-material');
  const initial=await art.evaluate(c=>c.toDataURL());await scene.press('ArrowRight');
  assert.notEqual(await art.evaluate(c=>c.toDataURL()),initial,'Art rotation changes the same Prismatic material');
  await page.locator('#inspect-flip').click();assert.equal(await scene.getAttribute('data-side'),'back');
  await page.locator('#inspect-reset').click();assert.equal(await scene.getAttribute('data-side'),'front');
  await scene.press('ArrowRight');await page.screenshot({path:path.join(output,'art-viewer.png')});
  await page.keyboard.press('Escape');
  await page.emulateMedia({reducedMotion:'reduce'});
  await card.hover({position:{x:40,y:70}});await page.waitForTimeout(120);
  assert.equal(await page.evaluate(()=>foilMotion.frame),0,'Reduced motion disables pointer animation');
  // The same sized material at the same light must be pixel-identical in game and library.
  const renderMaterial=async(p,runtime)=>{
   await p.evaluate(([id,isRuntime])=>{
    const c=isRuntime?cardElement(id,0):cardArt(cards.find(c=>c.name==='Nocturnal'),true,'current');
    c.id='parity';Object.assign(c.style,{position:'fixed',zIndex:'99999',left:'10px',top:'10px',width:'300px',height:'400px',border:'0',transform:'none'});document.body.append(c);
   },[foil,runtime]);
   await p.waitForFunction(()=>document.getElementById('parity').classList.contains('material-ready'));
   return p.evaluate(()=>{const c=document.getElementById('parity');foilMaterial.draw(c,{x:.27,y:.68});return c.querySelector('.foil-material').toDataURL();});
  };
  const gamePixels=await renderMaterial(page,true);
  const library=await browser.newPage({viewport:{width:1600,height:1050},reducedMotion:'reduce'});
  await library.goto(pathToFileURL(path.resolve('docs/card-library.html')).href);
  assert.equal(await renderMaterial(library,false),gamePixels,'Library and game render the approved material identically');
  await library.close();await page.evaluate(()=>document.getElementById('parity').remove());
  // State refreshes retire removed cards, including their observers and backing buffers.
  for(let i=0;i<8;i++)await page.evaluate(([s,i])=>ttcgState({...s,revision:s.revision+100+i}),[fixture,i]);
  assert(await page.evaluate(()=>foilMaterial.diagnostics().cards<=document.querySelectorAll('.foil-material').length),'Replaced card renderers are released');
  await page.evaluate(()=>ttcgReset());await page.waitForTimeout(80);
  assert.equal(await page.evaluate(()=>foilMotion.frame),0);
  assert(await page.locator('.foil-material').evaluateAll(es=>es.every(c=>c.width===1&&c.height===1)),'Hidden UI releases foil backing buffers');
  // WebGL loss switches to CSS and restores the material when the device returns.
  await page.evaluate(s=>ttcgState({...s,session:s.session+1}),fixture);await page.locator('#album-card-'+foil).click();
  await page.waitForFunction(()=>document.querySelector('#inspect-card .material-ready'));
  await page.evaluate(()=>{window.testLoss=foilMaterial.context.getExtension('WEBGL_lose_context');testLoss.loseContext();});
  await page.waitForFunction(()=>!document.querySelector('.material-ready'));
  assert.equal(await card.locator('.foil-film').evaluate(e=>getComputedStyle(e).display),'block');
  await page.evaluate(()=>testLoss.restoreContext());await page.waitForFunction(()=>document.querySelector('#inspect-card .material-ready'));
  assert.deepEqual(errors,[]);
  const fallback=await browser.newPage({viewport:{width:1440,height:900}});
  fallback.on('pageerror',error=>errors.push(error.message));
  await fallback.addInitScript(()=>{const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type==='webgl'?null:get.call(this,type,...args);};});
  await fallback.goto(pageURL);await fallback.evaluate(s=>ttcgState(s),fixture);await fallback.locator('#album-card-'+foil).click();
  assert.equal(await fallback.evaluate(()=>foilMaterial.diagnostics().backend),'css');
  const film=fallback.locator('#inspect-card .foil-film'),old=await film.evaluate(e=>getComputedStyle(e).backgroundPosition);
  await film.locator('..').hover({position:{x:50,y:80}});await fallback.waitForFunction(()=>foilMotion.frame===0);
  assert.notEqual(await film.evaluate(e=>getComputedStyle(e).backgroundPosition),old,'Fallback remains responsive');
  assert.deepEqual(errors,[]);
  console.log('Runtime Prismatic: game/library pixel parity, 72% strength, hover and full rotation, unchanged card transforms, idle cleanup, reduced motion, reset buffers, GPU recovery and CSS fallback passed');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exit(1);});
