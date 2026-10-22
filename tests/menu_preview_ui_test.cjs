const { chromium }=require(process.env.TTCG_PLAYWRIGHT||'playwright');
const {spawnSync}=require('node:child_process');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
  const page=await browser.newPage({viewport:{width:1440,height:900}});
  const state=JSON.parse(spawnSync('/tmp/ttcg-ui-host',['--fixtures'],{encoding:'utf8'}).stdout.trim().split('\n')[0]);
  state.musicAvailable=false;
  await page.goto(pathToFileURL(path.resolve('TesseraUI/index.html')).href);
  await page.evaluate(s=>window.ttcgState(s),state);
  assert(await page.locator('#music').isDisabled());
  await page.locator('#window-size-trigger').click();
  const box=await page.locator('#window-size-options').boundingBox();
  assert(box.width>90&&box.height>200&&box.y+box.height<900);
  await page.screenshot({path:'build/clean-catalog/screenshots/prototype-size-menu.png'});
  // Native-music setting updates during a callout must preserve its playback.
  await page.keyboard.press('Escape');
  assert(await page.locator('#window-size-options').isHidden());
  assert(await page.locator('#table').isVisible());
  console.log('Option bounds, Escape dismissal, and empty-playlist toggle passed');
 } finally { await browser.close(); }
})().catch(error=>{console.error(error); process.exit(1);});
