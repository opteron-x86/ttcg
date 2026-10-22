// Browser-only rule study. The UI, move preview and opponent use this same engine.
export const AFFINITIES = ['Strength', 'Intelligence', 'Willpower', 'Agility', 'Endurance'];
export const DEFAULT_RULES = Object.freeze({affinity: 'ascension', affinityCap: 3, affinityStart: 1, dominion: true, reverse: false, same: false, plus: false});

export function createGame(hands, rules = {}, first = 0) {
  if (hands.length !== 2 || hands.some(hand => hand.length !== 5 || hand.some(card => !card))) throw new Error('Choose five cards for each hand.');
  return {hands: hands.map(hand => [...hand]), board: Array(9).fill(null), rules: {...DEFAULT_RULES, ...rules}, turn: first, moves: 0};
}

export function neighbor(square, side) {
  return [square >= 3 ? square - 3 : -1, square % 3 < 2 ? square + 1 : -1,
    square < 6 ? square + 3 : -1, square % 3 > 0 ? square - 1 : -1][side];
}

export function affinityCounts(game) {
  const counts = Object.fromEntries(AFFINITIES.map(name => [name, 0]));
  for (const cell of game.board) if (cell) for (const affinity of new Set(cell.card.affinities)) {
    if (affinity in counts) counts[affinity]++;
  }
  return counts;
}

export function modifiers(game, card, square = -1, counts = affinityCounts(game)) {
  const sign = game.rules.affinity === 'ascension' ? 1 : game.rules.affinity === 'descension' ? -1 : 0;
  const count = Math.max(0, ...card.affinities.map(name => counts[name] || 0));
  // Optional study settings retain the browser table's original defaults.
  // Starting on card two means 0, 1, 2... as matching cards enter the board.
  const level = Math.max(0, count - (game.rules.affinityStart ?? 1) + 1);
  const affinity = sign * Math.min(game.rules.affinityCap ?? 3, level) || 0;
  const neighboringGroups = new Map();
  if (game.rules.dominion && square >= 0) for (let side = 0; side < 4; side++) {
    const position = neighbor(square, side), cell = game.board[position];
    if (!cell) continue;
    for (const group of new Set(cell.card.groups)) if (!card.groups.includes(group)) {
      neighboringGroups.set(group, (neighboringGroups.get(group) || 0) + 1);
    }
  }
  // Any group can exert Dominion throughout the match. Two adjacent members of
  // one other group are required; more neighbors or groups never stack penalties.
  const dominionGroups = [...neighboringGroups].filter(([, count]) => count >= 2).map(([group]) => group);
  const dominion = dominionGroups.length ? -1 : 0;
  return {affinity, dominion, total: affinity + dominion, dominionGroups};
}

export function score(game) {
  const scores = game.hands.map(hand => hand.filter(Boolean).length);
  for (const cell of game.board) if (cell) scores[cell.owner]++;
  return scores;
}

export function legalMoves(game) {
  const moves = [];
  if (game.moves === 9) return moves;
  game.hands[game.turn].forEach((card, hand) => {
    if (card) game.board.forEach((cell, square) => { if (!cell) moves.push({hand, square}); });
  });
  return moves;
}

export function play(game, hand, square) {
  if (game.moves >= 9 || !Number.isInteger(hand) || !Number.isInteger(square) || square < 0 || square > 8 ||
      hand < 0 || hand > 4 || game.board[square] || !game.hands[game.turn][hand]) return null;
  const next = {...game, hands: game.hands.map(cards => [...cards]), board: game.board.map(cell => cell && {...cell})};
  const player = game.turn, card = game.hands[player][hand];
  next.board[square] = {card, owner: player};
  next.hands[player][hand] = null;
  next.moves++;
  // Count the new card and update BOTH sides before any captures. Ownership changes
  // cannot change affinity counts or Dominion, so these values hold through Combo.
  const counts = affinityCounts(next);
  const changes = next.board.map((cell, q) => cell ? modifiers(next, cell.card, q, counts).total : 0);
  const beats = (from, to, side) => {
    const a = next.board[from].card.sides[side] + changes[from];
    const b = next.board[to].card.sides[(side + 2) % 4] + changes[to];
    return next.rules.reverse ? a < b : a > b;
  };
  const equal = [], sums = new Map(), ordinary = new Set(), special = new Set();
  for (let side = 0; side < 4; side++) {
    const q = neighbor(square, side), cell = next.board[q];
    if (!cell) continue;
    const a = card.sides[side], b = cell.card.sides[(side + 2) % 4];
    if (a === b) equal.push(q);
    const sum = a + b;
    sums.set(sum, [...(sums.get(sum) || []), q]);
    if (cell.owner !== player && beats(square, q, side)) ordinary.add(q);
  }
  let same = false, plus = false;
  if (next.rules.same && equal.length >= 2) for (const q of equal) if (next.board[q].owner !== player) {special.add(q); same = true;}
  if (next.rules.plus) for (const group of sums.values()) if (group.length >= 2) for (const q of group) {
    if (next.board[q].owner !== player) {special.add(q); plus = true;}
  }
  const captured = new Set([...ordinary, ...special]), queue = [...special], combo = [];
  for (const q of captured) next.board[q].owner = player;
  for (let index = 0; index < queue.length; index++) for (let side = 0; side < 4; side++) {
    const from = queue[index], q = neighbor(from, side), cell = next.board[q];
    if (cell && cell.owner !== player && beats(from, q, side)) {
      cell.owner = player; captured.add(q); queue.push(q); combo.push(q);
    }
  }
  next.turn = 1 - player;
  return {game: next, player, hand, square, card, captured: [...captured], same, plus, combo};
}

// Limited, deterministic search runs in a Web Worker; it never blocks the table.
export function chooseMove(game, budget = 18000) {
  const player = game.turn;
  let visited = 0;
  const staticValue = position => {
    const points = score(position), lead = points[player] - points[1 - player];
    if (position.moves === 9) return lead === 0 ? 0 : Math.sign(lead) * 10000 + lead;
    let defense = 0;
    const counts = affinityCounts(position);
    position.board.forEach((cell, q) => {
      if (!cell) return;
      const change = modifiers(position, cell.card, q, counts).total;
      let weakest = 15, exposed = false;
      for (let side = 0; side < 4; side++) {
        const n = neighbor(q, side);
        if (n >= 0 && !position.board[n]) {
          exposed = true;
          const rank = cell.card.sides[side] + change;
          weakest = Math.min(weakest, position.rules.reverse ? 11 - rank : rank);
        }
      }
      defense += (cell.owner === player ? 1 : -1) * (exposed ? weakest : 12);
    });
    return lead * 100 + defense;
  };
  function search(position, depth, alpha, beta) {
    if (++visited > budget) throw new Error('budget');
    if (!depth || position.moves === 9) return staticValue(position);
    const maximizing = position.turn === player;
    const moves = legalMoves(position).map(move => ({...move, next: play(position, move.hand, move.square).game}));
    moves.sort((a, b) => (maximizing ? -1 : 1) * (staticValue(a.next) - staticValue(b.next)));
    let best = maximizing ? -Infinity : Infinity;
    for (const move of moves) {
      const value = search(move.next, depth - 1, alpha, beta);
      best = maximizing ? Math.max(best, value) : Math.min(best, value);
      if (maximizing) alpha = Math.max(alpha, best); else beta = Math.min(beta, best);
      if (beta <= alpha) break;
    }
    return best;
  }
  const moves = legalMoves(game).map(move => ({...move, next: play(game, move.hand, move.square).game}));
  if (!moves.length) return null;
  moves.sort((a, b) => staticValue(b.next) - staticValue(a.next));
  let chosen = {hand: moves[0].hand, square: moves[0].square};
  const maxDepth = game.moves >= 4 ? 9 - game.moves : 3;
  for (let depth = 1; depth <= maxDepth; depth++) {
    let best = -Infinity, candidate = chosen;
    try {
      for (const move of moves) {
        const value = search(move.next, depth - 1, best, Infinity);
        if (value > best) {best = value; candidate = {hand: move.hand, square: move.square};}
      }
      chosen = candidate;
    } catch (error) {if (error.message !== 'budget') throw error; break;}
  }
  return chosen;
}
