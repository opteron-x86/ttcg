const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process'),path=require('node:path'),fs=require('node:fs'),{pathToFileURL}=require('node:url'),assert=require('node:assert/strict');
function packets(binary,args=[]){const p=spawnSync(binary,args,{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(p.status,0,p.stderr);return p.stdout.trim().split('\n').map(JSON.parse);}
const regions=packets('/tmp/ttcg-regional_rules-test',['--ui']),base=packets('/tmp/ttcg-ui-host')[0];
const out=path.resolve('build/regional-rules/previews');fs.mkdirSync(out,{recursive:true});
(async()=>{const browser=await chromium.launch({headless:true,args:['--no-sandbox']});try{
 for(const [width,height] of [[1280,720],[1920,1080],[3440,1440]]){
  const page=await browser.newPage({viewport:{width,height},reducedMotion:'reduce'}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  const s=structuredClone(base);Object.assign(s,{screen:'album',matchContext:false,session:900,revision:0,settings:{theme:0}});
  await page.evaluate(()=>{albumTab='opponents';journalHold=8;});
  async function update(region){s.revision++;Object.assign(s.collection,{regions:[region],holdID:8,roster:[]});await page.evaluate(s=>ttcgState(s),s);}
  async function fits(){
   const box=await page.locator('.journal-main').boundingBox(),title=await page.locator('#journal-title').boundingBox();
   for(const r of await page.locator('.journal-rule').evaluateAll(es=>es.map(e=>e.getBoundingClientRect().toJSON()))){
    assert(r.x>=box.x-1&&r.x+r.width<=box.x+box.width+1&&r.y>=box.y-1&&r.y+r.height<=box.y+box.height+1,`${width}: rule stays inside the Journal`);
    assert(r.y>=title.y+title.height-1||r.x>=title.x+title.width-1,`${width}: rule does not overlap the hold title`);
   }
  }
  for(const [theme,name] of ['tessera','charcoal','oxblood','parchment','umber'].entries()){
   s.settings.theme=theme;await update(regions[0]);
   assert.equal(await page.locator('html').getAttribute('data-theme'),name);
   assert.deepEqual(await page.locator('.journal-rule.rule-added').allTextContents(),['+Decimation','+Direct']);
   assert.deepEqual(await page.locator('.journal-rule.rule-removed').allTextContents(),['−Legion','−Diff']);
   const plus=page.locator('.journal-rule[data-rule="256"]'),minus=page.locator('.journal-rule[data-rule="128"]');
   assert.match(await plus.getAttribute('aria-label'),/Protected from removal until .*4E 201/);
   assert.match(await minus.getAttribute('title'),/Cannot return until .*4E 201/);
   assert.equal(await minus.locator('span').last().evaluate(e=>getComputedStyle(e).textDecorationLine),'line-through');
   assert.notEqual(await plus.evaluate(e=>getComputedStyle(e).color),await minus.evaluate(e=>getComputedStyle(e).color));
   await fits();
   if(width===1920)await page.screenshot({path:path.join(out,name+'-journal.png')});
   await update(regions[1]);
   assert.equal(await page.locator('.journal-rule[data-rule="128"]').count(),0,'Expired abolition mark disappears');
   assert.equal(await page.locator('.journal-rule[data-rule="256"]').textContent(),'Decimation','Added rule remains active after its mark expires');
   assert.deepEqual(await page.locator('.journal-rule.rule-added').allTextContents(),['+Direct']);
   assert.deepEqual(await page.locator('.journal-rule.rule-removed').allTextContents(),['−Diff']);
   await update(regions[2]);
   assert.equal(await page.locator('.rule-change-mark').count(),0,'Trade marks expire with their longer protection');
   assert.deepEqual(await page.locator('.journal-rule').allTextContents(),['Open','Decimation','Direct']);
   await update(regions[3]);await fits();
   if(width===1920&&theme===0)await page.screenshot({path:path.join(out,'many-rules-journal.png')});
  }
  assert.deepEqual(errors,[]);await page.close();console.log(`${width}x${height}: Journal additions/removals, trade changes, native expiry dates, all themes and long rule lists passed`);
 }
}finally{await browser.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
