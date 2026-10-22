#!/usr/bin/env python3
"""Build the rule table's roster, then serve it locally. No runtime mod edits."""
import argparse
import functools
import http.server
import json
from pathlib import Path
import webbrowser

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/rule-lab'


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    roster = json.loads((ROOT / 'data/cards.json').read_text())['cards']
    filenames = json.loads((ROOT / 'data/card-art-filenames.json').read_text())
    cards = []
    for source in roster:
        if source.get('roster_status') != 'ready':
            continue
        filename = filenames.get(source['name'], {}).get('card')
        if not filename or not (ROOT / 'TesseraUI/art/cards' / filename).is_file():
            continue
        thumb = filenames[source['name']].get('thumbnail', filename)
        card = {key: source[key] for key in ('id', 'name', 'tier', 'sides', 'groups', 'affinities', 'rarity')}
        card.update(index=len(cards), art='cards/' + filename, thumbnail='thumbs/' + thumb,
                    focus=source.get('focus', '50% 40%'))
        if not (ROOT / 'TesseraUI/art' / card['thumbnail']).is_file():
            card['thumbnail'] = card['art']
        cards.append(card)
    presets = [
        {'id': 'strength', 'name': 'Strength company', 'cards': ['kvatch-soldier', 'brutal-ashlander', 'legends-209203', 'legends-208811', 'legends-291244']},
        {'id': 'willpower', 'name': 'Willpower company', 'cards': ['sharpshooter-scout', 'legends-228305', 'legends-209155', 'legends-233891', 'legends-291257']},
        {'id': 'agility', 'name': 'Agility company', 'cards': ['legends-253868', 'legends-234220', 'legends-299187', 'legends-220780', 'legends-209126']},
        {'id': 'constructs', 'name': 'Neutral Constructs', 'cards': ['legends-246465', 'legends-246783', 'legends-246690', 'legends-245458', 'legends-245672']},
        {'id': 'nords', 'name': 'Nord company', 'cards': ['legends-209155', 'legends-234365', 'legends-208811', 'legends-209433', 'legends-233487']},
        {'id': 'orcs', 'name': 'Orc company', 'cards': ['orc-clan-captain', 'legends-209203', 'legends-209126', 'legends-209404', 'workshop-baa65a0298c246f0a85937396cce7691']},
        {'id': 'mixed', 'name': 'Mixed company', 'cards': ['legends-246465', 'legends-209155', 'legends-209126', 'brutal-ashlander', 'legends-291244']},
    ]
    ids = {card['id'] for card in cards}
    for preset in presets:
        assert len(preset['cards']) == 5 and all(card in ids for card in preset['cards']), preset['name']
    (OUT / 'card-data.js').write_text('window.ruleLabData = ' + json.dumps({'cards': cards, 'presets': presets}, ensure_ascii=False).replace('<', '\\u003c') + ';\n')

    # Reuse the actual browser renderer and its heraldic icons without loading the
    # old application's game bridge, settings handlers or album lifecycle.
    source = (ROOT / 'TesseraUI/app.js').read_text()
    start, end = source.index('const affinityNames='), source.index('function description(id)')
    renderer = source[start:end]
    assert 'function cardElement(' in renderer and 'function affinityIcon(' in renderer
    adapter = ''''use strict';
const state = {cards: window.ruleLabData.cards};
function setCardImage(image, card, thumbnail = false) {
  const path = thumbnail ? card.thumbnail : card.art;
  image.src = '../../TesseraUI/art/' + path.split('/').map(encodeURIComponent).join('/');
}
function cardBackElement(owner = 1) {
  const card = document.createElement('div'); card.className = 'card card-back owner-' + owner; return card;
}
'''
    (OUT / 'card-renderer.js').write_text(adapter + renderer + '\nwindow.ruleLabRenderer = {cardElement, affinityIcon};\n')
    print(f'Rule table ready: {len(cards)} cards, {len(presets)} decks.', flush=True)


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def do_GET(self):
        if self.path == '/':
            self.send_response(302)
            self.send_header('Location', '/tools/rule-lab/')
            self.end_headers()
            return
        super().do_GET()

    def log_message(self, format, *args):
        if len(args) < 2 or str(args[1]) not in ('200', '304'):
            super().log_message(format, *args)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Open the Tessera rule table in a local browser.')
    parser.add_argument('--port', type=int, default=8137)
    parser.add_argument('--open', action='store_true', help='Open your browser when the server starts')
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    build()
    if not args.build_only:
        handler = functools.partial(Handler, directory=str(ROOT))
        with http.server.ThreadingHTTPServer(('127.0.0.1', args.port), handler) as server:
            url = f'http://127.0.0.1:{args.port}/tools/rule-lab/'
            print(url, flush=True)
            if args.open:
                webbrowser.open(url)
            server.serve_forever()
