import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {createGame, modifiers, affinityCounts, score, play, chooseMove, legalMoves} from '../tools/rule-lab/engine.mjs';

const card = (group = 'Nord', affinities = [], sides = [5, 5, 5, 5]) => ({id: group + affinities.join(''), name: group, groups: [group], affinities, sides});
const nord = card(), orc = card('Orc'), neutral = card('Construct'), strength = card('Orc', ['Strength']);
const hands = () => [Array(5).fill(nord), Array(5).fill(orc)];
function board(cells, rules = {}) {
  const game = createGame(hands(), {affinity: 'off', dominion: false, ...rules});
  for (const [q, card, owner = 1] of cells) game.board[q] = {card, owner};
  return game;
}

// Affinity counts include either owner, exclude hands, cap at three, and never
// stack multiple affinities on a single card. Neutral still has a creature group.
for (const affinity of ['ascension', 'descension']) {
  const sign = affinity === 'ascension' ? 1 : -1;
  const game = board([[0, strength, 0], [1, strength, 1]], {affinity});
  assert.equal(affinityCounts(game).Strength, 2);
  assert.equal(modifiers(game, strength).total, sign * 2);
  assert.equal(modifiers(game, neutral).total, 0);
  game.board[2] = {card: strength, owner: 0}; game.board[3] = {card: strength, owner: 1};
  game.board[4] = {card: card('Nord', ['Willpower']), owner: 0};
  assert.equal(modifiers(game, strength).total, sign * 3);
  assert.equal(modifiers(game, card('Nord', ['Strength', 'Willpower'])).total, sign * 3);
}

// Delaying the count preserves a lone affinity and weakens repeated affinities.
// The triggering card counts immediately; Neutral and multi-affinity behavior
// remain consistent at either cap.
for (const affinity of ['ascension', 'descension']) for (const affinityCap of [2, 3]) {
  const sign = affinity === 'ascension' ? 1 : -1;
  const game = board([], {affinity, affinityCap, affinityStart: 2});
  const dual = card('Nord', ['Strength', 'Willpower']);
  for (let n = 0; n <= 5; n++) {
    const expected = sign * Math.min(affinityCap, Math.max(0, n - 1)) || 0;
    assert.equal(modifiers(game, strength).total, expected);
    assert.equal(modifiers(game, dual).total, expected);
    assert.equal(modifiers(game, neutral).total, 0);
    if (n < 5) game.board[n] = {card: strength, owner: n % 2};
  }
}
{
  const game = board([[4, strength]], {affinity: 'descension', affinityStart: 2, affinityCap: 2});
  game.hands[0][0] = neutral;
  assert.deepEqual(play(game, 0, 3).captured, [], 'One matching affinity gets no penalty');
  game.board[8] = {card: strength, owner: 0};
  assert.deepEqual(play(game, 0, 3).captured, [4], 'Two matching affinities give the defender −1');
  game.board[8] = null;
  game.hands[0][0] = strength;
  const result = play(game, 0, 3);
  assert.deepEqual(result.captured, [], 'Second matching card updates both sides before comparison');
  assert.equal(modifiers(result.game, strength, 3).total, -1);
}

// Two adjacent members of ONE other group weaken only the outsider. Ownership
// does not matter, and no group needs three cards or a global activation first.
let game = board([[3, nord, 0], [4, orc, 1], [5, nord, 1]], {dominion: true});
assert.deepEqual([3, 4, 5].map(q => modifiers(game, game.board[q].card, q).total), [0, -1, 0]);
assert.deepEqual(modifiers(game, orc, 4).dominionGroups, ['Nord']);
game.board[5] = null;
assert.equal(modifiers(game, orc, 4).total, 0, 'One other-group neighbor has no effect');
game.board[5] = {card: nord, owner: 1};
game.board[5].card = card('Khajiit');
assert.equal(modifiers(game, orc, 4).total, 0, 'Two unrelated groups do not combine');
game.board[1] = {card: card('Beast'), owner: 0}; game.board[7] = {card: neutral, owner: 0};
assert.equal(modifiers(game, orc, 4).total, 0, 'Four distinct groups still do not combine');
game.board[1].card = nord; game.board[7].card = card('Khajiit');
assert.equal(modifiers(game, orc, 4).total, -1, 'Two qualifying groups apply one penalty');
assert.deepEqual(modifiers(game, orc, 4).dominionGroups.sort(), ['Khajiit', 'Nord']);
game.board[5].card = nord; game.board[7].card = nord;
assert.equal(modifiers(game, orc, 4).total, -1, 'Four matching neighbors still apply one penalty');
assert.equal(modifiers(game, orc).total, 0);
assert.equal(modifiers(board([[0, nord], [2, nord], [4, orc]], {dominion: true}), orc, 4).total, 0, 'Diagonals do not count');
assert.equal(modifiers(board([[1, nord], [3, nord], [0, orc]], {dominion: true}), orc, 0).total, -1, 'Corner cards can be weakened');
const multi = {...nord, groups: ['Nord', 'Undead']};
assert.equal(modifiers(board([[3, nord], [4, multi], [5, nord]], {dominion: true}), multi, 4).total, 0, 'Own groups never weaken a card');
const orcUndead = {...orc, groups: ['Orc', 'Undead']};
assert.equal(modifiers(board([[3, orcUndead], [4, multi], [5, orcUndead]], {dominion: true}), multi, 4).total, -1, 'Another group can weaken a multi-group card');
const repeatedGroup = {...nord, groups: ['Nord', 'Nord']};
assert.equal(modifiers(board([[3, repeatedGroup], [4, orc]], {dominion: true}), orc, 4).total, 0, 'Count distinct adjacent cards');
assert.equal(modifiers(board([[3, nord], [4, orc], [5, nord]], {dominion: false}), orc, 4).total, 0);

// One neighbor does not break an equal comparison; the second matching neighbor
// weakens the defender BEFORE the triggering capture. It stays weakened even
// after its owner changes.
game = board([[4, orc]], {dominion: true});
assert.deepEqual(play(game, 0, 3).captured, []);
game.board[3] = {card: nord, owner: 0};
const before = JSON.stringify(game), result = play(game, 0, 5);
assert.deepEqual(result.captured, [4]);
assert.equal(result.game.board[4].owner, 0);
assert.equal(modifiers(result.game, orc, 4).total, -1);
assert.equal(JSON.stringify(game), before, 'Previews must not mutate the match');

// The incoming card can itself be weakened, including under Reverse. Existing
// facing pairs are not re-captured just because Dominion changed their values.
game = board([[3, orc], [5, orc]], {dominion: true});
assert.deepEqual(play(game, 0, 4).captured, []);
assert.equal(modifiers(play(game, 0, 4).game, nord, 4).total, -1);
game.rules.reverse = true;
assert.deepEqual(play(game, 0, 4).captured.sort(), [3, 5]);
game = board([[3, nord, 0], [4, orc, 1]], {dominion: true});
game.hands[0][0] = card('Nord', [], [1, 1, 1, 1]);
assert.deepEqual(play(game, 0, 1).captured, []);
assert.equal(modifiers(play(game, 0, 1).game, orc, 4).total, -1);
assert.equal(play(game, 0, 1).game.board[4].owner, 1);

// The arriving affinity updates defenders too. Counting only the attacker would
// incorrectly capture this equally strong card of the same affinity.
game = board([[4, strength]], {affinity: 'ascension'}); game.hands[0][0] = strength;
assert.deepEqual(play(game, 0, 3).captured, []);
assert.equal(modifiers(play(game, 0, 3).game, strength, 3).total, 2);

// A modifier change alone never recaptures an unrelated pair already in play.
game = board([[0, strength, 0], [1, card('Nord', [], [5, 5, 5, 6]), 1]], {affinity: 'ascension'});
game.hands[0][0] = strength;
assert.equal(play(game, 0, 8).game.board[1].owner, 1);

// Reverse uses the final effective numbers. Same still uses printed values even
// when an affinity modifier has made those facing numbers unequal.
game = board([[4, orc]], {dominion: false, reverse: true}); game.hands[0][0] = card('Nord', [], [2, 2, 2, 2]);
assert.deepEqual(play(game, 0, 3).captured, [4]);
game = board([[1, card('Nord', [], [1, 1, 5, 1])], [5, card('Orc', ['Strength'], [1, 1, 1, 3])]], {affinity: 'ascension', same: true});
game.hands[0][0] = card('Construct', [], [5, 3, 1, 1]);
const same = play(game, 0, 4);
assert.equal(same.same, true); assert.deepEqual(same.captured.sort(), [1, 5]);
assert.deepEqual(play({...game, rules: {...game.rules, same: false}}, 0, 4).captured, []);

// Plus starts a Combo, whose subsequent comparisons use modified values.
game = board([[1, card('Nord', [], [1, 1, 4, 1])], [5, card('Orc', [], [1, 1, 7, 2])], [8, card('Beast', [], [6, 1, 1, 1])]], {plus: true});
game.hands[0][0] = card('Nord', [], [4, 6, 1, 1]);
const plus = play(game, 0, 4);
assert.equal(plus.plus, true); assert(plus.combo.includes(8));

// Combined modifiers are additive and are not clamped to the printed scale.
game = board([[1, strength], [3, strength], [4, card('Nord', ['Strength'], [1, 1, 1, 1])]], {affinity: 'descension', dominion: true});
assert.equal(modifiers(game, game.board[4].card, 4).total, -4);
assert.equal(1 + modifiers(game, game.board[4].card, 4).total, -3);
assert.equal(play(game, -1, 0), null); assert.equal(play(game, 0, 4), null); assert.equal(play(game, 0, 9), null);

const context = {window: {}};
vm.runInNewContext(readFileSync(new URL('../build/rule-lab/card-data.js', import.meta.url), 'utf8'), context);
const {cards, presets} = JSON.parse(JSON.stringify(context.window.ruleLabData));
const byId = new Map(cards.map(card => [card.id, card]));
let random = 744;
const roll = max => {random = (Math.imul(random, 1664525) + 1013904223) >>> 0; return random % max;};
let matches = 0;
for (const affinity of ['off', 'ascension', 'descension']) for (const dominion of [false, true]) for (const reverse of [false, true]) {
  for (let n = 0; n < 6; n++) {
    let match = createGame([presets[roll(presets.length)], presets[roll(presets.length)]].map(p => p.cards.map(id => byId.get(id))), {affinity, dominion, reverse, same: n % 2 === 0, plus: n % 3 === 0}, n % 2);
    const names = match.hands.flat().map(card => card.id).sort();
    while (match.moves < 9) {
      const legal = legalMoves(match), move = n === 0 ? chooseMove(match, 2200) : legal[roll(legal.length)];
      assert(legal.some(candidate => candidate.hand === move.hand && candidate.square === move.square));
      match = play(match, move.hand, move.square).game;
      assert.equal(score(match).reduce((a, b) => a + b), 10);
      assert.equal(match.board.filter(Boolean).length, match.moves);
      assert.deepEqual([...match.hands.flat().filter(Boolean), ...match.board.filter(Boolean).map(cell => cell.card)].map(card => card.id).sort(), names);
    }
    assert.equal(legalMoves(match).length, 0); assert.equal(play(match, 0, 0), null); matches++;
  }
}
console.log(`Rule table: affinity timing, caps, Neutral, Dominion adjacency, Reverse, Same/Plus/Combo and ${matches} complete games passed.`);
