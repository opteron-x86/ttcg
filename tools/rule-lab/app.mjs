import {AFFINITIES, createGame, affinityCounts, modifiers, score, play, legalMoves} from './engine.mjs';

const $ = id => document.getElementById(id);
const {cards, presets} = window.ruleLabData;
const {cardElement, affinityIcon} = window.ruleLabRenderer;
const byId = new Map(cards.map(card => [card.id, card]));
const fields = ['affinity', 'dominion', 'reverse', 'same', 'plus', 'your-deck', 'rival-deck', 'mode', 'first'];
const checkboxFields = new Set(['dominion', 'reverse', 'same', 'plus']);
let game, initialGame, configuration, selected = -1, previewSquare = -1, lastMove = null;
let history = [], worker = null, replyTimer = null, epoch = 0, thinking = false, dragSource = null;
let renderPreviewFrame = null;

const signed = value => value > 0 ? '+' + value : value < 0 ? '−' + Math.abs(value) : '0';
const rankName = value => value === 10 ? 'A' : String(value);
const squareName = q => ['Top left', 'Top middle', 'Top right', 'Middle left', 'Center', 'Middle right', 'Bottom left', 'Bottom middle', 'Bottom right'][q];
const ruleName = value => value === 'ascension' ? 'Ascension' : value === 'descension' ? 'Descension' : '';
const canPlay = () => game.moves < 9 && !thinking && (configuration.mode === 'both' || game.turn === 0);

function settings() {
  return Object.fromEntries(fields.map(id => [id, checkboxFields.has(id) ? $(id).checked : $(id).value]));
}

function stopOpponent() {
  epoch++;
  worker?.terminate(); worker = null;
  clearTimeout(replyTimer); replyTimer = null;
  thinking = false;
}

function handFor(id) {
  const preset = presets.find(item => item.id === id);
  return preset.cards.map(card => byId.get(card));
}

function newGame(replay = false) {
  stopOpponent();
  if (!replay) {
    configuration = settings();
    const first = configuration.first === 'random' ? (crypto.getRandomValues(new Uint32Array(1))[0] & 1) : Number(configuration.first);
    initialGame = createGame([handFor(configuration['your-deck']), handFor(configuration['rival-deck'])], configuration, first);
  }
  game = initialGame; history = []; selected = -1; previewSquare = -1; lastMove = null;
  $('pending').hidden = JSON.stringify(settings()) === JSON.stringify(configuration);
  render(); scheduleOpponent();
}

function chooseCard(player, hand) {
  if (!game.hands[player][hand]) return;
  if (!canPlay() || player !== game.turn) {inspect(game.hands[player][hand], player, -1); return;}
  selected = selected === hand ? -1 : hand; previewSquare = -1; lastMove = null;
  render();
}

function setPreview(square) {
  const target = canPlay() && selected >= 0 && !game.board[square] ? square : -1;
  if (previewSquare === target) return;
  previewSquare = target;
  cancelAnimationFrame(renderPreviewFrame);
  renderPreviewFrame = requestAnimationFrame(() => {renderTable(false); renderReadout();});
}

function makeCard(card, owner, position, square = -1) {
  const change = modifiers(position, card, square);
  const element = cardElement(card.index, owner, change.total, true);
  element.removeAttribute('title');
  const group = document.createElement('span'); group.className = 'card-group'; group.textContent = card.groups.join(' / '); element.append(group);
  if ($('values').value === 'effective') {
    element.querySelectorAll('.rank').forEach((node, side) => {
      node.textContent = rankName(card.sides[side] + change.total);
      node.classList.toggle('modified-up', change.total > 0);
      node.classList.toggle('modified-down', change.total < 0);
    });
  }
  return element;
}

function cardLabel(card, position, square = -1) {
  const change = modifiers(position, card, square);
  return `${card.name}. ${card.groups.join(', ')}. ${card.affinities.join(', ') || 'Neutral'}. Values ${card.sides.map(n => rankName(n + change.total)).join(', ')}. Modifier ${signed(change.total)}.`;
}

function renderHand(player, position) {
  const root = $(player ? 'rival-hand' : 'player-hand');
  root.classList.toggle('inactive', player !== game.turn || game.moves === 9);
  game.hands[player].forEach((card, hand) => {
    let button = root.children[hand];
    if (!button) {
      button = document.createElement('button'); button.id = `hand-${player}-${hand}`; button.dataset.player = player; button.dataset.hand = hand;
      button.className = 'hand-slot'; button.onclick = () => chooseCard(player, hand);
      button.oncontextmenu = event => {event.preventDefault(); const current = game.hands[player][hand]; if (current) inspect(current, player, -1);};
      button.ondragstart = event => {
        if (!canPlay() || game.turn !== player) {event.preventDefault(); return;}
        selected = hand; dragSource = hand; previewSquare = -1;
        event.dataTransfer.setData('text/plain', String(hand)); event.dataTransfer.effectAllowed = 'move';
        root.querySelectorAll('.hand-slot').forEach((node, index) => node.classList.toggle('selected', index === hand));
        document.querySelectorAll('#board .square.empty').forEach(node => node.classList.add('available'));
      };
      button.ondragend = () => {dragSource = null; setPreview(-1);};
      root.append(button);
    }
    button.className = 'hand-slot' + (!card ? ' spent' : '') + (player === game.turn && hand === selected ? ' selected' : '');
    button.disabled = !card;
    button.draggable = Boolean(card && canPlay() && game.turn === player);
    button.setAttribute('aria-pressed', String(player === game.turn && hand === selected));
    button.setAttribute('aria-label', card ? cardLabel(card, position) : 'Card already played');
    const change = card ? modifiers(position, card).total : 0;
    const key = `${card?.id || ''}:${player}:${change}:${$('values').value}`;
    if (button.dataset.renderKey !== key) {button.replaceChildren(...(card ? [makeCard(card, player, position)] : [])); button.dataset.renderKey = key;}
  });
}

function renderBoard(position, prediction, animate) {
  const root = $('board');
  for (let q = 0; q < 9; q++) {
    let button = root.children[q];
    if (!button) {
      button = document.createElement('button'); button.id = 'square-' + q; button.dataset.square = q;
      button.onclick = () => {
        if (game.board[q]) inspect(game.board[q].card, game.board[q].owner, q);
        else if (canPlay() && selected >= 0) commit(selected, q);
      };
      button.onmouseenter = () => setPreview(q);
      button.onmouseleave = () => {if (previewSquare === q) setPreview(-1);};
      button.onfocus = () => setPreview(q);
      button.onblur = () => {if (previewSquare === q) setPreview(-1);};
      button.ondragover = event => {if (dragSource !== null && !game.board[q]) {event.preventDefault(); setPreview(q);}};
      button.ondrop = event => {event.preventDefault(); if (dragSource !== null && canPlay()) {const hand = dragSource; dragSource = null; commit(hand, q);}};
      button.oncontextmenu = event => {event.preventDefault(); const cell = game.board[q]; if (cell) inspect(cell.card, cell.owner, q);};
      root.append(button);
    }
    const original = game.board[q], cell = position.board[q], ghost = prediction && q === previewSquare;
    const owner = ghost ? game.turn : original?.owner;
    const change = cell ? modifiers(position, cell.card, q).total : 0;
    const before = original ? modifiers(game, original.card, q).total : 0;
    button.className = 'square' + (!original ? ' empty' : '') + (!original && selected >= 0 && canPlay() ? ' available' : '') +
      (ghost ? ' preview-target ghost' : '') + (prediction?.captured.includes(q) ? ' would-capture' : '') +
      (original && change !== before ? ' changed' : '') + (animate && lastMove?.square === q ? ' just-placed' : '') +
      (animate && lastMove?.captured.includes(q) ? ' just-captured' : '');
    button.setAttribute('aria-label', `${squareName(q)}. ${cell ? cardLabel(cell.card, position, q) : 'Empty square'}`);
    const key = `${cell?.card.id || ''}:${owner}:${change}:${$('values').value}`;
    if (button.dataset.renderKey !== key) {button.replaceChildren(...(cell ? [makeCard(cell.card, owner, position, q)] : [])); button.dataset.renderKey = key;}
  }
}

function prediction() {
  return canPlay() && selected >= 0 && previewSquare >= 0 ? play(game, selected, previewSquare) : null;
}

function renderCounts(position) {
  const root = $('affinity-counts'); root.replaceChildren();
  if (game.rules.affinity === 'off') {
    const label = document.createElement('span'); label.className = 'affinity-off'; label.textContent = game.rules.dominion ? 'Dominion · two of one other group → −1' : 'Standard captures'; root.append(label); return;
  }
  const counts = affinityCounts(position), sign = game.rules.affinity === 'descension' ? -1 : 1;
  for (const affinity of AFFINITIES) {
    const tally = document.createElement('span'); tally.className = 'affinity-tally' + (!counts[affinity] ? ' zero' : '');
    const amount = sign * Math.min(3, counts[affinity]);
    tally.title = `${affinity}: ${counts[affinity]} placed, ${signed(amount)} to matching cards`;
    tally.setAttribute('aria-label', tally.title);
    const value = document.createElement('b'); value.textContent = signed(amount);
    const count = document.createElement('small'); count.textContent = `(${counts[affinity]})`;
    tally.append(affinityIcon(affinity), value, count); root.append(tally);
  }
}

function renderTable(animate = false) {
  const preview = prediction(), position = preview?.game || game;
  renderHand(0, position); renderHand(1, position); renderBoard(position, preview, animate); renderCounts(position);
}

function effectText(position, card, square) {
  const effect = modifiers(position, card, square), parts = [];
  if (position.rules.affinity !== 'off') parts.push(card.affinities.length ? `${ruleName(position.rules.affinity)} ${signed(effect.affinity)}` : 'Neutral · no affinity modifier');
  if (position.rules.dominion && square >= 0) parts.push(`Dominion ${signed(effect.dominion)}`);
  return parts.join(' · ');
}

function renderReadout() {
  const title = $('readout-title'), detail = $('readout-detail');
  $('inspect').disabled = selected < 0 || !game.hands[game.turn][selected];
  const preview = prediction();
  if (preview) {
    const taken = preview.captured.length;
    title.textContent = `${squareName(previewSquare)} · ${taken ? `captures ${taken}` : 'no capture'}`;
    detail.textContent = `${preview.card.name} · ${effectText(preview.game, preview.card, previewSquare) || 'No modifier'}${preview.same ? ' · Same' : ''}${preview.plus ? ' · Plus' : ''}${preview.combo.length ? ' · Combo' : ''}`;
  } else if (game.moves === 9) {
    const points = score(game); title.textContent = points[0] === points[1] ? 'Draw' : points[0] > points[1] ? 'You win' : 'Opponent wins';
    detail.textContent = `${points[0]}–${points[1]} · The unplayed card counts. Replay this deal or try another matchup.`;
  } else if (selected >= 0 && game.hands[game.turn][selected]) {
    const card = game.hands[game.turn][selected]; title.textContent = card.name;
    detail.textContent = `${card.groups.join(' / ')} · ${card.affinities.join(' / ') || 'Neutral'} · ${effectText(game, card, -1) || 'No modifier'} · Hover over a square to preview.`;
  } else if (lastMove) {
    title.textContent = `${lastMove.player ? 'Opponent' : 'You'} played ${lastMove.card.name}`;
    detail.textContent = `${squareName(lastMove.square)} · ${lastMove.captured.length ? `${lastMove.captured.length} captured` : 'no capture'}${lastMove.same ? ' · Same' : ''}${lastMove.plus ? ' · Plus' : ''}${lastMove.combo.length ? ' · Combo' : ''}`;
  } else {
    title.textContent = thinking ? 'Opponent is choosing a card' : 'Choose a card';
    detail.textContent = 'Select a card, then a square. Right-click any card to inspect it.';
  }
}

function render(animate = false) {
  const points = score(game);
  $('player-score').textContent = points[0]; $('rival-score').textContent = points[1];
  $('turn').textContent = game.moves === 9 ? 'Game complete' : thinking ? 'Opponent’s turn…' : game.turn ? 'Opponent’s turn' : 'Your turn';
  $('progress').textContent = `${game.moves} / 9`;
  document.querySelector('.score-panel.player').classList.toggle('active', game.turn === 0 && game.moves < 9);
  document.querySelector('.score-panel.rival').classList.toggle('active', game.turn === 1 && game.moves < 9);
  const names = ['Open', ruleName(game.rules.affinity), game.rules.dominion && 'Dominion', game.rules.reverse && 'Reverse', game.rules.same && 'Same', game.rules.plus && 'Plus'].filter(Boolean);
  $('active-rules').replaceChildren(...names.map(name => {const item = document.createElement('span'); item.textContent = name; return item;}));
  $('undo').disabled = !history.length;
  renderTable(animate); renderReadout();
  $('history-count').textContent = history.length;
  $('history').replaceChildren(...history.map(({move}) => {
    const item = document.createElement('li');
    item.textContent = `${move.player ? 'Opponent' : 'You'} · ${move.card.name} → ${squareName(move.square)} · ${move.captured.length} captured${move.same ? ' · Same' : ''}${move.plus ? ' · Plus' : ''}${move.combo.length ? ' · Combo' : ''}`;
    return item;
  }));
}

function commit(hand, square, fromOpponent = false) {
  if (fromOpponent ? game.turn !== 1 || !thinking : !canPlay()) return;
  const result = play(game, hand, square); if (!result) return;
  history.push({game, move: result}); game = result.game; lastMove = result;
  thinking = false; selected = -1; previewSquare = -1; dragSource = null;
  render(true); scheduleOpponent();
}

function scheduleOpponent() {
  if (game.moves >= 9 || game.turn !== 1 || configuration.mode !== 'ai') return;
  thinking = true; renderReadout(); $('turn').textContent = 'Opponent’s turn…';
  const ticket = ++epoch, started = performance.now();
  worker = new Worker(new URL('./opponent.mjs', import.meta.url), {type: 'module'});
  const reply = move => {
    if (ticket !== epoch) return;
    worker?.terminate(); worker = null;
    replyTimer = setTimeout(() => {if (ticket === epoch) commit(move.hand, move.square, true);}, Math.max(0, 430 - (performance.now() - started)));
  };
  worker.onmessage = ({data}) => {
    if (ticket !== epoch) return;
    if (data.error) {console.error('Opponent search:', data.error); reply(legalMoves(game)[0]);}
    else if (data.id === ticket && data.move) reply(data.move);
  };
  worker.onerror = event => {console.error('Opponent worker:', event.message); reply(legalMoves(game)[0]);};
  worker.postMessage({id: ticket, game});
}

function undo() {
  if (!history.length) return;
  stopOpponent();
  game = history.pop().game;
  if (configuration.mode === 'ai') while (game.turn !== 0 && history.length) game = history.pop().game;
  lastMove = history.at(-1)?.move || null; selected = -1; previewSquare = -1;
  render(); scheduleOpponent();
}

function inspect(card, player, square) {
  setPreview(-1);
  const effect = modifiers(game, card, square);
  $('inspect-name').textContent = card.name;
  $('inspect-art').replaceChildren(makeCard(card, player, game, square));
  $('inspect-group').textContent = card.groups.join(' / ');
  $('inspect-affinity').replaceChildren();
  if (!card.affinities.length) $('inspect-affinity').textContent = 'Neutral';
  for (const affinity of card.affinities) $('inspect-affinity').append(affinityIcon(affinity), document.createTextNode(' ' + affinity + ' '));
  $('inspect-tier').textContent = `Tier ${['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X'][card.tier]} · ${card.rarity}`;
  $('inspect-ranks').replaceChildren(...['Top', 'Right', 'Bottom', 'Left'].map((side, index) => {
    const row = document.createElement('tr');
    for (const value of [side, rankName(card.sides[index]), rankName(card.sides[index] + effect.total)]) {const cell = document.createElement('td'); cell.textContent = value; row.append(cell);}
    return row;
  }));
  $('inspect-effects').textContent = effectText(game, card, square) || 'No modifier';
  if (game.rules.dominion) {
    const explanation = document.createElement('p');
    explanation.textContent = square < 0 ? 'Dominion applies after placement. Preview a square to see its effect.' : effect.dominionGroups.length ? effect.dominionGroups.join(' and ') + ' neighbors apply −1 once.' : 'Needs two adjacent cards of the same other group.';
    $('inspect-effects').append(explanation);
  }
  $('inspect-dialog').showModal();
}

function theme(value) {
  const themes = ['tessera', 'charcoal', 'oxblood', 'parchment', 'umber'];
  if (!themes.includes(value)) value = 'tessera';
  document.documentElement.dataset.theme = value;
  document.documentElement.classList.toggle('alternate-theme', value !== 'tessera');
  document.documentElement.dataset.surface = {tessera: 'felt', charcoal: 'slate', oxblood: 'leather', parchment: 'parchment', umber: 'walnut'}[value];
  $('theme').value = value;
  try {localStorage.setItem('tessera.rule-lab.theme', value);} catch {}
}

for (const id of ['your-deck', 'rival-deck']) for (const preset of presets) $(id).add(new Option(preset.name, preset.id));
$('your-deck').value = 'strength'; $('rival-deck').value = 'nords';
for (const id of fields) $(id).addEventListener('change', () => {$('pending').hidden = JSON.stringify(settings()) === JSON.stringify(configuration);});
$('deal').onclick = () => newGame(); $('replay').onclick = () => newGame(true); $('undo').onclick = undo;
$('values').onchange = () => {renderTable(); renderReadout();};
$('inspect').onclick = () => {const card = game.hands[game.turn][selected]; if (card) inspect(card, game.turn, -1);};
$('help').onclick = () => {setPreview(-1); $('help-dialog').showModal();};
for (const button of document.querySelectorAll('[data-close]')) button.onclick = () => $(button.dataset.close).close();
for (const dialog of document.querySelectorAll('dialog')) dialog.addEventListener('click', event => {
  if (event.target !== dialog) return;
  const box = dialog.getBoundingClientRect();
  if (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom) dialog.close();
});
$('theme').onchange = () => theme($('theme').value);
try {theme(localStorage.getItem('tessera.rule-lab.theme') || 'tessera');} catch {theme('tessera');}
for (const button of document.querySelectorAll('[data-scenario]')) button.onclick = () => {
  const scenario = button.dataset.scenario;
  $('affinity').value = scenario === 'neutral' ? 'descension' : scenario === 'dominion' ? 'off' : 'ascension';
  $('your-deck').value = scenario === 'neutral' ? 'constructs' : scenario === 'dominion' ? 'nords' : 'strength';
  $('rival-deck').value = scenario === 'dominion' ? 'orcs' : scenario === 'neutral' ? 'strength' : 'willpower';
  $('dominion').checked = scenario === 'dominion';
  for (const id of ['reverse', 'same', 'plus']) $(id).checked = false;
  newGame();
};
document.addEventListener('keydown', event => {
  if (document.querySelector('dialog[open]') || ['INPUT', 'SELECT', 'TEXTAREA'].includes(event.target.tagName) || event.ctrlKey || event.metaKey || event.altKey) return;
  if (event.key === 'Escape') {selected = -1; previewSquare = -1; render();}
  if (event.key.toLowerCase() === 'i' && selected >= 0) {$('inspect').click(); event.preventDefault();}
  if (event.key.toLowerCase() === 'u') {undo(); event.preventDefault();}
  if (/^[1-5]$/.test(event.key) && canPlay()) {chooseCard(game.turn, Number(event.key) - 1); event.preventDefault();}
});
// Read-only state is useful for recording and reproducing a browser playtest.
window.ruleLab = {snapshot: () => structuredClone({game, configuration, selected, previewSquare, thinking, history: history.map(item => ({hand: item.move.hand, square: item.move.square}))})};
newGame();
