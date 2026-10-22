const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), {pathToFileURL} = require('node:url');
const {chromium} = require(process.env.TTCG_PLAYWRIGHT || 'playwright');
const data=JSON.parse(fs.readFileSync('docs/catalog-audit-data.json','utf8'));
const trophy=data.cards.find(c=>c.routes.includes('trophy')),premium=data.cards.find(c=>c.routes.includes('tournament'));
const output = path.resolve('build/catalog-audit');
fs.mkdirSync(output, {recursive:true});
(async () => {
 const browser = await chromium.launch({headless:true,args:['--no-sandbox']});
 try {
  const page = await browser.newPage({viewport:{width:1440,height:1100},reducedMotion:'reduce'});
  const errors = [], external = [], failed = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('request', r => {if (/^https?:/.test(r.url())) external.push(r.url());});
  page.on('requestfailed', r => failed.push(r.url()));
  await page.goto(pathToFileURL(path.resolve('docs/catalog-audit.html')).href);
  assert.match(await page.locator('#stats').innerText(), /703/);
  assert.match(await page.locator('#pack-report').innerText(), /Every eligible pool/);
  assert.equal(await page.locator('#group-matrix tbody tr').count(), 18, 'Eighteen gameplay groups');
  await page.screenshot({path:path.join(output,'overview.png'),fullPage:true});

  // A zero heatmap cell remains inspectable; populated cells drill into exact filters.
  await page.getByRole('button',{name:'God · Tier I: 0 cards',exact:true}).click();
  assert(await page.locator('#empty').isVisible());
  assert.equal(await page.locator('#group').inputValue(),'God');
  await page.locator('#tier').selectOption('');
  assert.equal(await page.locator('.catalog-card').count(),30);
  await page.locator('.catalog-card img').first().evaluate(i => i.decode());
  await page.screenshot({path:path.join(output,'falmer.png'),fullPage:true});
  await page.locator('.pin-button').nth(0).click();
  await page.locator('.pin-button').nth(1).click();
  await page.locator('#open-compare').click();
  assert.equal(await page.locator('#comparison>section').count(),2);
  await page.locator('#comparison img').evaluateAll(imgs => Promise.all(imgs.map(i => i.decode())));
  await page.screenshot({path:path.join(output,'comparison.png')});
  await page.keyboard.press('Escape');

  // Re-review a real card. Proposals must survive reload and never alter baseline charts.
  await page.locator('.card-open').first().click();
  const title = await page.locator('#detail-title').innerText();
  await page.locator('#proposal-decision').selectOption('adjust');
  await page.locator('#proposal-tier').selectOption('3');
  await page.locator('#proposal-rarity').selectOption('Epic');
  await page.locator('#proposal-note').fill('Review the corner profile. <script>throw Error("unsafe")</script>');
  await page.locator('#side-0').fill('8');
  assert.match(await page.locator('#draft-cost').innerText(),/weighted points/);
  await page.getByRole('button',{name:'Save to review',exact:true}).click();
  assert.equal(await page.locator('#review-count').innerText(),'1');
  await page.locator('#detail-art img').evaluate(i => i.decode());
  await page.screenshot({path:path.join(output,'card-review.png')});
  await page.keyboard.press('Escape');
  await page.locator('[data-view=notebook]').click();
  assert.match(await page.locator('#review-list').innerText(),/tier: (9|10) → 3/);
  assert.match(await page.locator('#review-list').innerText(),/<script>/);
  await page.reload();
  assert.match(await page.locator('#review-list').innerText(),new RegExp(title));
  const downloadPromise = page.waitForEvent('download');
  await page.locator('#export-review').click();
  const download = await downloadPromise, reviewFile = path.join(output,'review.json');
  assert.equal(download.suggestedFilename(),'tessera-catalog-review.json');
  await download.saveAs(reviewFile);
  const review = JSON.parse(fs.readFileSync(reviewFile,'utf8'));
  assert.equal(review.entries.length,1);
  assert([9,10].includes(review.entries[0].before.tier));
  assert.equal(review.entries[0].after.tier,3);

  await page.getByRole('button',{name:'Review card',exact:true}).click();
  await page.locator('#remove-proposal').click();
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#review-count').innerText(),'0');
  await page.locator('#import-file').setInputFiles(reviewFile);
  await page.waitForFunction(() => document.getElementById('review-count').textContent === '1');
  const bad = {...review,fingerprint:'wrong-snapshot'};
  await page.locator('#import-file').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(bad))});
  await page.waitForFunction(() => document.getElementById('status').textContent.startsWith('Import failed:'));
  assert.equal(await page.locator('#review-count').innerText(),'1');

  // Shared filter state, actual trophy eligibility, tier VII tournament acquisition and export.
  await page.locator('[data-view=explorer]').click();
  await page.locator('#reset-filters').click();
  await page.locator('#scope').selectOption('none');
  assert.equal(await page.locator('.catalog-card').count(),data.summary.noRoute);
  await page.locator('#scope').selectOption('pending');
  assert.match(await page.locator('#explorer-summary').innerText(),/^29 cards/);
  await page.locator('#scope').selectOption('trophy');
  assert.match(await page.locator('#explorer-summary').innerText(),new RegExp('^'+data.summary.trophies+' cards'));
  await page.locator('#search').fill(trophy.name);
  assert.equal(await page.locator('.catalog-card').count(),1);
  const csvPromise = page.waitForEvent('download');
  await page.locator('#export-csv').click();
  const csv = await csvPromise, csvPath = path.join(output,'selection.csv');await csv.saveAs(csvPath);
  assert.equal(fs.readFileSync(csvPath,'utf8').split('\r\n').length,2);
  await page.locator('#reset-filters').click();
  await page.locator('#scope').selectOption('tournament');
  await page.locator('#search').fill(premium.name);
  assert.equal(await page.locator('.catalog-card').count(),1);
  await page.reload();
  assert.equal(await page.locator('#search').inputValue(),premium.name);
  assert.equal(await page.locator('#scope').inputValue(),'tournament');
  await page.locator('#reset-filters').click();
  const first = await page.locator('.card-name').first().innerText();
  await page.locator('#next-page').click();
  assert.notEqual(await page.locator('.card-name').first().innerText(),first);
  await page.locator('#previous-page').click();
  assert.equal(await page.locator('.card-name').first().innerText(),first);
  await page.locator('#show-ranks').uncheck();
  assert(await page.locator('.catalog-grid .rank').first().isHidden());
  await page.locator('#show-ranks').check();
  await page.locator('#rarity').selectOption('Rare');
  assert.equal(await page.locator('.tier-badge').first().evaluate(n=>getComputedStyle(n).color),'rgb(86, 223, 255)');
  await page.locator('#rarity').selectOption('Epic');
  assert.equal(await page.locator('.tier-badge').first().evaluate(n=>getComputedStyle(n).color),'rgb(214, 132, 250)');
  await page.screenshot({path:path.join(output,'explorer.png'),fullPage:true});

  for(const width of [390,768,1920]){
   await page.setViewportSize({width,height:900});
   for(const view of ['overview','explorer','notebook','method']){
    await page.locator('[data-view='+view+']').click();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),width+' '+view+' should not overflow');
   }
   if(width===390){
    await page.locator('[data-view=overview]').click();
    await page.screenshot({path:path.join(output,'mobile.png'),fullPage:true});
    await page.locator('[data-view=explorer]').click();
    await page.locator('.card-open').first().click();
    assert(await page.locator('#detail-dialog').evaluate(d=>d.scrollWidth<=d.clientWidth),'Mobile card review fits');
    await page.keyboard.press('Escape');
   }
  }
  // Proposed groups are a separate, inspectable overlay, including unresolved cards.
  await page.setViewportSize({width:1440,height:1100});
  await page.locator('[data-view=grouping]').click();
  assert.equal(await page.locator('#grouping-matrix tbody tr').count(),19);
  assert.match(await page.locator('#grouping-evidence').innerText(),/703 proposed assignments · 0 unassigned · 0 retirement candidates/);
  assert(await page.locator('.scope-label').isHidden(),'Proposal always describes the full catalog');
  await page.locator('#grouping-matrix').getByRole('button',{name:'God',exact:true}).click();
  assert.match(await page.locator('#grouping-summary').innerText(),/^31 cards/);
  await page.reload();
  assert.equal(await page.locator('#grouping-group').inputValue(),'God');
  assert.equal(await page.locator('#grouping-action').inputValue(),'','All-cards drilldown survives reload');
  await page.locator('#grouping-search').fill('Azura');
  await page.locator('#grouping-grid .card-open').click();
  assert.match(await page.locator('.grouping-detail').innerText(),/God → God/);
  assert.equal(await page.locator('#proposal-groups').inputValue(),'God','Built-in draft never overwrites a user review');
  await page.keyboard.press('Escape');
  await page.locator('#grouping-reset').click();
  await page.locator('#grouping-action').selectOption('retire');
  assert.equal(await page.locator('#grouping-grid .catalog-card').count(),0);
  const groupDownloadPromise=page.waitForEvent('download');
  await page.locator('#grouping-export').click();
  const groupDownload=await groupDownloadPromise,groupCSV=path.join(output,'grouping.csv');await groupDownload.saveAs(groupCSV);
  assert.equal(fs.readFileSync(groupCSV,'utf8').split('\r\n').length,1);
  assert.match(fs.readFileSync(groupCSV,'utf8'),/current_groups.*proposed_group/);
  await page.locator('#grouping-reset').click();
  await page.locator('#grouping-matrix').getByRole('button',{name:'Dragon proposed · current tier I: 0 cards',exact:true}).click();
  assert(await page.locator('#grouping-empty').isVisible());
  await page.locator('#grouping-reset').click();
  await page.locator('#grouping-group').selectOption('Spirit');
  await page.locator('#grouping-grid img').evaluateAll(imgs=>Promise.all(imgs.map(i=>i.decode())));
  await page.screenshot({path:path.join(output,'grouping-proposal.png'),fullPage:true});
  await page.locator('.grouping-curves summary').click();
  assert.equal(await page.locator('#ordinary-curve tbody tr').count(),9);
  assert.equal(await page.locator('#dragon-curve tbody tr').count(),4);
  for(const width of [390,768,1920]){
   await page.setViewportSize({width,height:900});
   assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),width+' proposal should not overflow');
  }
  assert.equal(await page.locator('#review-count').innerText(),'1','Proposal browsing preserves the notebook');
  assert.deepEqual(errors,[]);assert.deepEqual(external,[],'Audit loads without external requests');
  assert.deepEqual(failed,[],'All requested local assets exist');
  console.log('Catalog audit UI: heatmap drilldowns, comparisons, review persistence/export/import, scope accuracy, filtering, pagination, colors, mobile layout and offline loading passed.');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
