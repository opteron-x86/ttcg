// Run with: node tools/rule-lab/simulate.mjs
// Reuses the browser table's rules and opponent. Outputs go under build/.
import {Worker, isMainThread, parentPort, workerData} from 'node:worker_threads';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {AFFINITIES, createGame, affinityCounts, modifiers, score, play, chooseMove} from './engine.mjs';

const ROOT = fileURLToPath(new URL('../../', import.meta.url));
const ARCHETYPES = ['affinity', 'group', 'construct', 'mixed'];
const LABELS = {affinity: 'One affinity', group: 'One group', construct: 'Neutral Constructs', mixed: 'Mixed'};

function rng(seed) {
  return max => {seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed % max;};
}
function shuffle(values, roll) {
  const result = [...values];
  for (let i = result.length - 1; i > 0; i--) {const j = roll(i + 1); [result[i], result[j]] = [result[j], result[i]];}
  return result;
}
function makeCases(cards, count, seed, controlled = false) {
  const cases = [], groups = [...new Set(cards.filter(c => c.tier === 3).flatMap(c => c.groups))].filter(g => g !== 'Construct').sort();
  for (let sample = 0; sample < count; sample++) {
    const tier = [3, 5, 7][sample % 3], affinity = AFFINITIES[sample % AFFINITIES.length], group = groups[sample % groups.length];
    const roll = rng(seed + sample * 7919), pool = cards.filter(c => c.tier === tier);
    const affinityHand = [], usedGroups = new Set();
    for (const c of shuffle(pool.filter(c => c.affinities.length === 1 && c.affinities[0] === affinity), roll)) {
      if (c.groups.some(g => usedGroups.has(g))) continue;
      affinityHand.push(c); c.groups.forEach(g => usedGroups.add(g));
      if (affinityHand.length === 5) break;
    }
    assert.equal(affinityHand.length, 5, `Five distinct groups for ${affinity}, tier ${tier}`);
    // Give Mixed five distinct groups and five distinct affinities, counting
    // Neutral as one possible affinity. Retry the small matching problem if a
    // greedy draw runs out of available groups.
    let mixed;
    for (let attempt = 0; attempt < 100 && !mixed; attempt++) {
      const chosen = [], occupied = new Set();
      for (const a of shuffle([...AFFINITIES, 'Neutral'], roll).slice(0, 5)) {
        const options = shuffle(pool.filter(c => (c.affinities[0] || 'Neutral') === a && c.affinities.length <= 1 && !c.groups.some(g => occupied.has(g))), roll);
        if (!options.length) break;
        chosen.push(options[0]); options[0].groups.forEach(g => occupied.add(g));
      }
      if (chosen.length === 5) mixed = chosen;
    }
    assert(mixed, 'Mixed hand');
    const decks = {
      affinity: shuffle(affinityHand, roll),
      group: shuffle(pool.filter(c => c.groups.includes(group)), roll).slice(0, 5),
      construct: shuffle(pool.filter(c => c.groups.includes('Construct') && !c.affinities.length), roll).slice(0, 5),
      mixed: shuffle(mixed, roll),
    };
    for (const hand of Object.values(decks)) assert.equal(hand.length, 5);
    if (controlled) {
      // Isolate the rules from roster balance: both sides receive identical
      // printed values, with only their group/affinity labels changed.
      const printed = shuffle(pool, roll).slice(0, 5);
      for (const kind of ARCHETYPES) decks[kind] = printed.map((c, index) => ({...c,
        groups: [kind === 'construct' ? 'Construct' : kind === 'group' ? 'Nord' : `Mixed group ${index + 1}`],
        affinities: kind === 'construct' ? [] : kind === 'affinity' ? [affinity] :
          kind === 'mixed' && index === 4 ? [] : [AFFINITIES[(index + sample) % AFFINITIES.length]],
      }));
    }
    for (let a = 0; a < ARCHETYPES.length; a++) for (let b = a + 1; b < ARCHETYPES.length; b++) {
      const archetypes = [ARCHETYPES[a], ARCHETYPES[b]];
      cases.push({id: cases.length, sample, tier, affinity, group, archetypes,
        hands: archetypes.map((kind, owner) => decks[kind].map((card, index) => ({...card, instance: `${owner}:${index}`})))});
    }
  }
  return cases;
}
function allRules(affinityCap, affinityStart, constructStudy) {
  const rules = [];
  for (const affinity of ['off', 'ascension', 'descension']) for (const dominion of [false, true]) for (const reverse of [false, true]) for (const same of [false, true]) for (const plus of [false, true]) {
    if (constructStudy && !((affinity === 'descension' && !reverse) || (affinity === 'ascension' && reverse))) continue;
    const names = [affinity !== 'off' && (affinity === 'ascension' ? 'Ascension' : 'Descension'), dominion && 'Dominion', reverse && 'Reverse', same && 'Same', plus && 'Plus'].filter(Boolean);
    rules.push({id: rules.length, name: names.join(' + ') || 'Standard', affinity, affinityCap, affinityStart, dominion, reverse, same, plus});
  }
  return rules;
}
function runMatch(deal, rules, first, budget) {
  let game = createGame(deal.hands, rules, first);
  const moves = [], metrics = {captures: 0, sameTriggers: 0, plusTriggers: 0, comboCaptures: 0, maxMoveCaptures: 0,
    dominionFirstMove: 0, dominionCaptureChanges: 0, affinityCaptureChanges: 0, finalDominionCards: 0,
    minValue: Infinity, maxValue: -Infinity, finalCappedAffinityCards: 0};
  while (game.moves < 9) {
    const move = chooseMove(game, budget), result = play(game, move.hand, move.square);
    assert(result, 'AI chose a legal move');
    const captureKey = result.captured.toSorted().join(',');
    if (rules.dominion) {
      const other = play({...game, rules: {...game.rules, dominion: false}}, move.hand, move.square);
      if (other.captured.toSorted().join(',') !== captureKey) metrics.dominionCaptureChanges++;
    }
    if (rules.affinity !== 'off') {
      const other = play({...game, rules: {...game.rules, affinity: 'off'}}, move.hand, move.square);
      if (other.captured.toSorted().join(',') !== captureKey) metrics.affinityCaptureChanges++;
    }
    game = result.game;
    metrics.captures += result.captured.length;
    metrics.sameTriggers += Number(result.same); metrics.plusTriggers += Number(result.plus);
    metrics.comboCaptures += result.combo.length;
    metrics.maxMoveCaptures = Math.max(metrics.maxMoveCaptures, result.captured.length);
    const counts = affinityCounts(game);
    let dominionCards = 0, capped = 0;
    for (let q = 0; q < 9; q++) if (game.board[q]) {
      const c = game.board[q].card, effect = modifiers(game, c, q, counts);
      dominionCards += Number(effect.dominion !== 0); capped += Number(Math.abs(effect.affinity) === (rules.affinityCap ?? 3));
      for (const rank of c.sides) {
        metrics.minValue = Math.min(metrics.minValue, rank + effect.total);
        metrics.maxValue = Math.max(metrics.maxValue, rank + effect.total);
      }
    }
    for (const c of game.hands.flat().filter(Boolean)) {
      const effect = modifiers(game, c, -1, counts);
      for (const rank of c.sides) {
        metrics.minValue = Math.min(metrics.minValue, rank + effect.total);
        metrics.maxValue = Math.max(metrics.maxValue, rank + effect.total);
      }
    }
    if (dominionCards && !metrics.dominionFirstMove) metrics.dominionFirstMove = game.moves;
    metrics.finalDominionCards = dominionCards; metrics.finalCappedAffinityCards = capped;
    moves.push({hand: move.hand, square: move.square, captured: result.captured, same: result.same, plus: result.plus, combo: result.combo});
    assert.equal(score(game).reduce((a, b) => a + b), 10, 'Ten total cards');
    assert.equal(new Set([...game.hands.flat().filter(Boolean), ...game.board.filter(Boolean).map(c => c.card)].map(c => c.instance)).size, 10, 'No cards lost or duplicated');
  }
  return {case: deal.id, rule: rules.id, first, scores: score(game), ...metrics, moves};
}

function summarize(rows, rules, cases) {
  return rules.map(rule => {
    const games = rows.filter(r => r.rule === rule.id), n = games.length;
    const mean = key => games.reduce((sum, game) => sum + game[key], 0) / n;
    const rate = test => games.filter(test).length / n;
    const performance = {};
    for (const archetype of ARCHETYPES) {
      const gamesFor = games.filter(g => cases[g.case].archetypes.includes(archetype));
      let wins = 0, draws = 0;
      for (const game of gamesFor) {
        const owner = cases[game.case].archetypes.indexOf(archetype);
        wins += Number(game.scores[owner] > game.scores[1 - owner]); draws += Number(game.scores[0] === game.scores[1]);
      }
      performance[archetype] = {games: gamesFor.length, wins, draws, losses: gamesFor.length - wins - draws, points: (wins + draws / 2) / gamesFor.length};
    }
    return {...rule, games: n, drawRate: rate(g => g.scores[0] === g.scores[1]),
      starterPoints: games.reduce((sum, g) => sum + (g.scores[g.first] > g.scores[1 - g.first] ? 1 : g.scores[0] === g.scores[1] ? .5 : 0), 0) / n,
      meanMargin: games.reduce((sum, g) => sum + Math.abs(g.scores[0] - g.scores[1]), 0) / n,
      captures: mean('captures'), dominionRate: rate(g => g.dominionFirstMove > 0), dominionCards: mean('finalDominionCards'),
      dominionCaptureChanges: mean('dominionCaptureChanges'), affinityCaptureChanges: mean('affinityCaptureChanges'),
      sameTriggers: mean('sameTriggers'), plusTriggers: mean('plusTriggers'), comboCaptures: mean('comboCaptures'),
      maxMoveCaptures: Math.max(...games.map(g => g.maxMoveCaptures)),
      belowOneRate: rate(g => g.minValue < 1), aboveTenRate: rate(g => g.maxValue > 10),
      minValue: Math.min(...games.map(g => g.minValue)), maxValue: Math.max(...games.map(g => g.maxValue)), performance};
  });
}

if (!isMainThread) {
  parentPort.on('message', ({task, caseId, ruleId, first}) => {
    try {parentPort.postMessage({task, result: runMatch(workerData.cases[caseId], workerData.rules[ruleId], first, workerData.budget)});}
    catch (error) {parentPort.postMessage({task, error: error.stack});}
  });
} else {
  const args = process.argv.slice(2), option = (name, fallback) => {const i = args.indexOf('--' + name); return i < 0 ? fallback : args[i + 1];};
  const count = Number(option('cases', 15)), seed = Number(option('seed', 20261010)), budget = Number(option('budget', 18000));
  const workers = Math.min(16, Math.max(1, Number(option('workers', 8))));
  const affinityCap = Number(option('affinity-cap', 3)), affinityStart = Number(option('affinity-start', 1));
  assert(Number.isInteger(affinityCap) && affinityCap >= 1 && affinityCap <= 9, 'Affinity cap must be 1–9');
  assert(Number.isInteger(affinityStart) && affinityStart >= 1 && affinityStart <= 9, 'Affinity start must be 1–9');
  const constructStudy = args.includes('--construct-study');
  const out = resolve(ROOT, option('out', 'build/rule-lab/simulations'));
  const context = {window: {}}, dataSource = readFileSync(resolve(ROOT, 'build/rule-lab/card-data.js'), 'utf8');
  vm.runInNewContext(dataSource, context);
  const {cards} = JSON.parse(JSON.stringify(context.window.ruleLabData));
  const controlled = args.includes('--controlled');
  const cases = makeCases(cards, count, seed, controlled).filter(deal => !constructStudy || deal.archetypes.includes('construct'))
    .map((deal, id) => ({...deal, originalCase: deal.id, id}));
  const rules = allRules(affinityCap, affinityStart, constructStudy), tasks = [];
  for (const rule of rules) for (const deal of cases) for (const first of [0, 1]) tasks.push({task: tasks.length, caseId: deal.id, ruleId: rule.id, first});
  mkdirSync(out, {recursive: true});
  const hash = source => createHash('sha256').update(source).digest('hex');
  const metadata = {created: new Date().toISOString(), seed, budget, workers, sampleBatches: count, games: tasks.length, controlled,
    affinityCap, affinityStart, constructStudy,
    engineSha256: hash(readFileSync(new URL('./engine.mjs', import.meta.url))), rosterSha256: hash(dataSource), archetypes: LABELS,
    method: 'Same matched deals across every rule set; each deal played with each side starting. Rule-aware deterministic browser AI, capped at depth three early and searching to the end when five placements remain, subject to the node budget. All hands visible. Wins=1, draws=0.5 for points rates.' +
      (controlled ? ' Controlled suite: identical printed sides for both players; group and affinity labels assigned to isolate their effects. These are artificial decks, not roster cards.' : ' Roster suite: unmodified real cards from tiers III, V and VII; every match uses one tier.') +
      (constructStudy ? ' Construct study: each game pairs Neutral Constructs against one-affinity, one-group or mixed cards. Only Descension and Ascension + Reverse, with every combination of Dominion, Same and Plus.' : '')};
  writeFileSync(resolve(out, 'setup.json'), JSON.stringify({metadata, rules, cases}, null, 2));
  console.log(`${tasks.length} games; ${rules.length} rule sets; ${cases.length} deals; ${workers} workers; budget ${budget}.`);
  const rows = [], started = Date.now(); let next = 0;
  const heartbeat = setInterval(() => {
    const seconds = (Date.now() - started) / 1000;
    console.log(`${rows.length}/${tasks.length} complete (${seconds.toFixed(0)}s; ${(rows.length / seconds).toFixed(1)} games/s)`);
  }, 15000);
  await Promise.all(Array.from({length: workers}, () => new Promise((resolveWorker, reject) => {
    const worker = new Worker(new URL(import.meta.url), {workerData: {cases, rules, budget}});
    const dispatch = () => {
      if (next >= tasks.length) {worker.terminate().then(resolveWorker); return;}
      worker.postMessage(tasks[next++]);
    };
    worker.on('error', reject);
    worker.on('message', message => {
      if (message.error) {worker.terminate(); reject(new Error(message.error)); return;}
      rows.push(message.result);
      if (rows.length % 240 === 0) writeFileSync(resolve(out, 'progress.json'), JSON.stringify({completed: rows.length, total: tasks.length, lastRule: message.result.rule}));
      dispatch();
    });
    dispatch();
  }))).finally(() => clearInterval(heartbeat));
  rows.sort((a, b) => a.rule - b.rule || a.case - b.case || a.first - b.first);
  metadata.seconds = (Date.now() - started) / 1000;
  const summary = summarize(rows, rules, cases);
  writeFileSync(resolve(out, 'results.json'), JSON.stringify({metadata, rules, cases, games: rows, summary}));
  writeFileSync(resolve(out, 'summary.json'), JSON.stringify({metadata, summary}, null, 2));
  const columns = ['name', 'games', 'drawRate', 'starterPoints', 'meanMargin', 'captures', 'dominionRate', 'dominionCards', 'dominionCaptureChanges', 'affinityCaptureChanges', 'sameTriggers', 'plusTriggers', 'comboCaptures', 'belowOneRate', 'aboveTenRate', 'minValue', 'maxValue'];
  const csv = [columns.join(',')];
  for (const row of summary) csv.push(columns.map(key => typeof row[key] === 'number' ? Number(row[key].toFixed(5)) : JSON.stringify(row[key])).join(','));
  writeFileSync(resolve(out, 'summary.csv'), csv.join('\n') + '\n');
  console.log(`Saved ${rows.length} completed games to ${out} (${metadata.seconds.toFixed(1)}s).`);
}
