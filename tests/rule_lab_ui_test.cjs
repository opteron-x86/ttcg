const assert = require('node:assert/strict');
const fs = require('node:fs');
const {chromium} = require('playwright');
const url = process.env.TESSERA_RULE_LAB_URL || 'http://127.0.0.1:8137/tools/rule-lab/';

(async () => {
  fs.mkdirSync('build/rule-lab/review', {recursive: true});
  const browser = await chromium.launch({headless: true, args: ['--no-sandbox']});
  const errors = [], page = await browser.newPage({viewport: {width: 1600, height: 1000}});
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => {if (message.type() === 'error') errors.push(message.text());});
  const snapshot = () => page.evaluate(() => window.ruleLab.snapshot());
  const screenshot = options => page.screenshot({...options, animations: 'disabled'});
  const load = async () => {await page.goto(url); await page.waitForFunction(() => window.ruleLab);};
  const images = async () => page.locator('.portrait img').evaluateAll(images => Promise.all(images.map(image => image.decode())));
  try {
    await load(); await images();
    assert.equal(await page.locator('#player-hand .card').count(), 5);
    assert.equal(await page.locator('#rival-hand .card').count(), 5);
    // Pending setup does not change an existing board; replay restores the same deal.
    await page.locator('#mode').selectOption('both'); await page.locator('#deal').click();
    await page.locator('#affinity').selectOption('descension');
    assert.equal((await snapshot()).game.rules.affinity, 'ascension');
    assert(await page.locator('#pending').isVisible());
    await page.locator('#deal').click();
    assert.equal((await snapshot()).game.rules.affinity, 'descension');
    assert(await page.locator('#pending').isHidden());
    await page.locator('#hand-0-0').click(); await page.locator('#square-4').hover();
    await page.waitForFunction(() => document.querySelector('#square-4.ghost'));
    assert.equal((await snapshot()).game.moves, 0, 'Hover never changes the match');
    assert.match(await page.locator('#readout-detail').innerText(), /Descension −1/);
    const previewRanks = await page.locator('#square-4 .rank').allTextContents();
    await page.locator('#square-4').click();
    assert.equal((await snapshot()).game.moves, 1);
    assert.deepEqual(await page.locator('#square-4 .rank').allTextContents(), previewRanks, 'Committed values equal the preview');
    await page.locator('#undo').click(); assert.equal((await snapshot()).game.moves, 0);
    // Actual browser drag-and-drop uses the same placement path as clicking.
    await page.locator('#hand-0-1').dragTo(page.locator('#square-0'));
    assert.equal((await snapshot()).game.moves, 1);
    await page.locator('#replay').click(); assert.equal((await snapshot()).game.moves, 0);
    assert.equal((await snapshot()).game.rules.affinity, 'descension');

    // Neutral deck and all three matchup shortcuts are playable.
    await page.locator('[data-scenario="neutral"]').click();
    assert((await snapshot()).game.hands[0].every(card => card.groups.includes('Construct') && card.affinities.length === 0));
    await page.locator('#hand-0-0').click(); await page.locator('#square-3').click();
    await page.locator('#hand-1-0').click(); await page.locator('#square-4').click();
    assert.equal(await page.locator('#square-3 .affinity-modifier').count(), 0);
    assert.equal(await page.locator('#square-4 .affinity-modifier').innerText(), '−1');
    await page.locator('#square-3').click({button: 'right'});
    assert(await page.locator('#inspect-dialog').isVisible());
    assert.equal(await page.locator('#inspect-affinity').innerText(), 'Neutral');
    await images(); await screenshot({path: 'build/rule-lab/review/inspect.png'});
    await page.keyboard.press('Escape');
    await page.locator('[data-scenario="dominion"]').click();
    assert.equal((await snapshot()).game.rules.affinity, 'off');
    assert.equal((await snapshot()).game.rules.dominion, true);
    await page.locator('#hand-0-0').click(); await page.locator('#square-3').click();
    await page.locator('#hand-1-0').click(); await page.locator('#square-4').click();
    assert.equal(await page.locator('#square-3 .affinity-modifier, #square-4 .affinity-modifier').count(), 0, 'A lone neighbor applies no Dominion');
    await page.locator('#hand-0-1').click(); await page.locator('#square-5').hover();
    await page.waitForFunction(() => document.querySelector('#square-5.ghost'));
    assert.equal(await page.locator('#square-4 .affinity-modifier').innerText(), '−1');
    assert.equal(await page.locator('#square-3 .affinity-modifier, #square-5 .affinity-modifier').count(), 0, 'Only the outsider is weakened');
    assert.equal((await snapshot()).game.moves, 2, 'Dominion preview does not commit the third card');
    await images(); await screenshot({path: 'build/rule-lab/review/dominion-preview.png'});
    await page.locator('#square-5').click();
    await page.locator('#square-4').click({button: 'right'});
    assert.match(await page.locator('#inspect-effects').innerText(), /Dominion −1/);
    assert.match(await page.locator('#inspect-effects').innerText(), /Nord neighbors apply −1 once/);
    await page.keyboard.press('Escape');
    await page.locator('#undo').click();
    assert.equal(await page.locator('#square-4 .affinity-modifier').count(), 0, 'Undo removes the second neighbor and its penalty');
    await page.locator('#hand-0-1').click(); await page.locator('#square-5').click();

    // Every theme and a range of window sizes: no missing art or overlapping hands.
    for (const theme of ['tessera', 'charcoal', 'oxblood', 'parchment', 'umber']) {
      await page.locator('#theme').selectOption(theme); await images();
      await screenshot({path: `build/rule-lab/review/${theme}.png`});
    }
    for (const [width, height] of [[1920, 1080], [1600, 1000], [1280, 800], [1024, 768], [760, 900]]) {
      await page.setViewportSize({width, height});
      const bounds = await page.evaluate(() => {
        const box = id => document.getElementById(id).getBoundingClientRect().toJSON();
        return {overflow: document.documentElement.scrollWidth, height: document.documentElement.scrollHeight, board: box('board'), player: box('player-hand'), rival: box('rival-hand')};
      });
      assert(bounds.overflow <= width + 1, `${width}: no horizontal page overflow`);
      assert(bounds.board.x >= 0 && bounds.board.right <= width + 1, `${width}: board fits`);
      if (width > 860) {
        assert(bounds.player.right <= bounds.board.left + 1, `${width}: player hand does not overlap board`);
        assert(bounds.rival.left >= bounds.board.right - 1, `${width}: rival hand does not overlap board`);
        assert(bounds.height <= height + 2, `${width}×${height}: table fits vertically (${bounds.height})`);
      }
      await screenshot({path: `build/rule-lab/review/size-${width}.png`, fullPage: width <= 860});
    }
    await page.setViewportSize({width: 1600, height: 1000});
    await page.locator('#theme').selectOption('tessera');
    // Complete matches against the real asynchronous opponent, including repeated
    // deals and an opponent-first round. Undo during thinking cancels stale work.
    for (const [affinity, reverse, first] of [['ascension', false, '0'], ['descension', true, '0'], ['off', false, '1']]) {
      await page.locator('#mode').selectOption('ai'); await page.locator('#affinity').selectOption(affinity);
      await page.locator('#dominion').check(); await page.locator('#reverse').setChecked(reverse); await page.locator('#first').selectOption(first);
      await page.locator('#deal').click();
      await page.waitForFunction(() => !window.ruleLab.snapshot().thinking, null, {timeout: 20000});
      if (first === '0') {
        await page.locator('#hand-0-0').click(); await page.locator('#square-0').click(); await page.locator('#undo').click();
        await page.waitForTimeout(600); assert.equal((await snapshot()).game.moves, 0, 'Canceled opponent cannot place a stale move');
      }
      for (let round = 0; round < 5; round++) {
        const current = (await snapshot()).game;
        if (current.moves === 9) break;
        assert.equal(current.turn, 0);
        const hand = current.hands[0].findIndex(Boolean), square = current.board.findIndex(cell => !cell);
        await page.locator(`#hand-0-${hand}`).click(); await page.locator(`#square-${square}`).click();
        await page.waitForFunction(() => !window.ruleLab.snapshot().thinking, null, {timeout: 20000});
      }
      assert.equal((await snapshot()).game.moves, 9);
      assert.equal(await page.locator('#turn').innerText(), 'Game complete');
      assert.equal(Number(await page.locator('#player-score').innerText()) + Number(await page.locator('#rival-score').innerText()), 10);
    }
    await page.locator('#help').click(); assert(await page.locator('#help-dialog').isVisible());
    await page.keyboard.press('Escape'); assert(await page.locator('#help-dialog').isHidden());
    await page.locator('[data-scenario="affinity"]').click();
    await page.waitForFunction(() => !window.ruleLab.snapshot().thinking);
    await page.locator('#first').selectOption('0'); await page.locator('#dominion').check(); await page.locator('#deal').click();
    await images(); await screenshot({path: 'build/rule-lab/review/table.png'});
    assert.deepEqual(errors, []);
    console.log('Rule table browser: previews, drag/drop, Neutral, Dominion, inspection, all themes, responsive layout, undo/replay and three AI matches passed.');
  } finally {await browser.close();}
})().catch(error => {console.error(error); process.exitCode = 1;});
