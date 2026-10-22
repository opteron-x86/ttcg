const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const output=path.resolve('build/card-library-finishes');fs.mkdirSync(output,{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:1050}}),errors=[],external=[];
  page.on('pageerror',e=>errors.push(e.message));
  page.on('request',r=>{if(/^https?:/.test(r.url()))external.push(r.url());});
  await page.goto('file://'+path.resolve('docs/card-library.html'));
  await page.locator('#search').fill('Nocturnal');
  assert.equal(await page.locator('#gallery .entry').count(),1);
  await page.locator('#gallery .entry').click();
  const ids=['none','current','satin','engraved','holo','reverse','cosmos','rainbow','texture','relief','secret','radiant'];
  assert.equal(await page.locator('.finish-sample').count(),ids.length);
  const images=new Set();
  for(let i=0;i<ids.length;i++){
   const id=ids[i];await page.locator('.finish-sample').nth(i).click();
   assert.equal(await page.locator('#finish').inputValue(),id);
   assert.equal(await page.locator('.finish-sample[aria-pressed=true]').count(),1);
   assert.equal(await page.locator('#detail-art .rank').count(),4);
   await page.locator('#detail-art .card').scrollIntoViewIfNeeded();
   await page.locator('#detail-art .portrait>img').first().evaluate(i=>i.decode());
   const card=page.locator('#detail-art .card');
   if(id!=='none'){
    // Labels in the comparison tiles must not pad the material spans.
    assert.equal(await page.locator('.finish-sample .finish-'+id+' .foil-film').evaluate(e=>getComputedStyle(e).padding),'0px');
    const sample=()=>card.evaluate(e=>{const s=getComputedStyle(e);return [s.getPropertyValue('--mx'),s.getPropertyValue('--my'),s.transform];});
    const before=await sample();
    await card.hover({position:{x:55,y:100}});await page.waitForTimeout(400);
    assert.notDeepEqual(await sample(),before,id+' responds to the pointer');
    await card.hover({position:{x:250,y:280}});await page.waitForTimeout(400);
   }
   const png=await card.screenshot({path:path.join(output,id+'.png')});images.add(png.toString('base64'));
   await page.mouse.move(1400,20);await page.waitForFunction(()=>typeof foilFrame!=='undefined'&&foilFrame===0);
   assert.equal(await page.locator('.foil-active').count(),0,'Leaving a card releases its tilt');
  }
  assert.equal(images.size,ids.length,'All treatments render differently');
  // Use the existing page, including its swatches, for the review screenshot.
  await page.locator('.finish-sample').filter({has:page.locator('.finish-holo')}).click();
  await page.locator('#detail-art .card').hover({position:{x:95,y:145}});await page.waitForTimeout(400);
  await page.screenshot({path:path.join(output,'library-desktop.png')});
  await page.locator('#close').click();await page.reload();
  assert.equal(await page.locator('#finish').inputValue(),'holo','Finish preference survives reload');
  await page.emulateMedia({reducedMotion:'reduce'});
  await page.locator('#search').fill('Azura');await page.locator('#gallery .entry').click();
  const still=await page.locator('#detail-art .card').getAttribute('style');
  await page.locator('#detail-art .card').hover({position:{x:80,y:120}});await page.waitForTimeout(100);
  assert.equal(await page.locator('#detail-art .card').getAttribute('style'),still,'Reduced motion remains static');
  await page.setViewportSize({width:390,height:844});
  await page.screenshot({path:path.join(output,'library-mobile.png')});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'No horizontal overflow on mobile');
  assert.deepEqual(errors,[]);assert.deepEqual(external,[],'Materials and artwork work offline');
  console.log('Library finishes: all twelve choices, distinct renders, pointer motion and release including the game material, reduced motion, persistence, mobile layout and offline loading passed');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
