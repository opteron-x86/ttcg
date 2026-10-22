#!/usr/bin/env python3
"""Build the offline card-library document from the current catalog and artwork."""
import argparse
import csv
import io
import json
from pathlib import Path
from urllib.parse import quote
from card_art import ART_FORMATS

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'TesseraUI/art'


def images(folder, formats):
    selected = {}
    for path in (ART / folder).iterdir():
        extension = path.suffix.lower()
        if not path.is_file() or extension not in formats:
            continue
        key = path.stem.lower()
        candidate = (formats.index(extension), f'{folder}/{path.name}')
        if key not in selected or candidate < selected[key]:
            selected[key] = candidate
    return {key: '../TesseraUI/art/' + quote(value[1], safe='/')
            for key, value in selected.items()}


def build():
    catalog = json.loads((ROOT / 'data/cards.json').read_text())
    assert catalog['side_order'] == ['top', 'right', 'bottom', 'left']
    faces = images('cards', ART_FORMATS)
    thumbs = images('thumbs', ART_FORMATS)
    opponents = {}
    for row in json.loads((ROOT / 'data/opponent-collections.json').read_text())['opponents']:
        for form in row['cards']:
            opponents.setdefault(form, set()).add(row['name'])
    cards = []
    for card in sorted(catalog['cards'], key=lambda c: (c['tier'], c['name'])):
        key = card['name'].lower()
        if key not in faces:
            raise FileNotFoundError(f'Missing artwork: {card["name"]}')
        acquisition = card['acquisition']
        pending = card.get('roster_status') == 'pending'
        pack_route = acquisition['available'] and not card['unique'] and (
            1 <= card['tier'] <= 6 or 3 <= card['tier'] <= 7 and card['rarity'] in ('Rare', 'Epic'))
        trophy_route = not pending and card['unique'] and not acquisition['available'] and 8 <= card['tier'] <= 10
        cards.append({
            'id': card['id'], 'name': card['name'], 'tier': card['tier'],
            'rarity': card['rarity'], 'unique': card['unique'], 'sides': card['sides'],
            'groups': card['groups'], 'affinities': card['affinities'],
            'subtypes': card['subtypes'], 'focus': card.get('focus', '50% 50%'),
            'available': acquisition['available'], 'starter': acquisition['starter'],
            'obtainable': not pending and (acquisition['starter'] or card['form_id'] in opponents or pack_route or trophy_route),
            'packs': acquisition['available'] and not card['unique'] and card['tier'] <= 6,
            'tournamentPacks': not pending and acquisition['available'] and not card['unique'] and 3 <= card['tier'] <= 7 and card['rarity'] in ('Rare', 'Epic'),
            'plannedRoute': acquisition.get('route', ''),
            'artStatus': card.get('art_status', 'source'),
            'pending': pending,
            'opponents': sorted(opponents.get(card['form_id'], [])),
            'art': faces[key], 'thumbnail': thumbs.get(key, faces[key]),
        })
    payload = json.dumps(cards, ensure_ascii=False, separators=(',', ':')).replace('<', '\\u003c').replace('&', '\\u0026')
    template = (ROOT / 'tools/card-library.html').read_text()
    # Embed the actual runtime material so the in-game reference cannot drift.
    # Scope it to its swatch; other review finishes have their own material layers.
    runtime_foil = (ROOT / 'TesseraUI/foil.css').read_text()
    grain = quote((ROOT / 'TesseraUI/foil-grain.svg').read_text(), safe='')
    runtime_foil = runtime_foil.replace("url('foil-grain.svg')", 'url("data:image/svg+xml,' + grain + '")')
    template = template.replace('/* RUNTIME_FOIL_CSS */', '@scope (.finish-current) {\n' + runtime_foil + '\n}')
    template = template.replace('/* FOIL_MATERIAL_JS */', (ROOT / 'TesseraUI/foil-material.js').read_text())
    for marker, filename in (
        ('/* LIBRARY_FINISHES_CSS */', 'finishes.css'),
        ('/* FOIL_REVIEW_CSS */', 'review.css'),
        ('<!-- FOIL_REVIEW_HTML -->', 'review.html'),
        ('/* FOIL_MOTION_JS */', 'motion.js'),
        ('/* FOIL_REVIEW_JS */', 'review.js'),
    ):
        template = template.replace(marker, (ROOT / 'tools/card-library' / filename).read_text())
    return template.replace('<!-- CARD_DATA -->', payload).replace('<!-- CARD_COUNT -->', str(len(cards))).replace('<!-- AVAILABLE_COUNT -->', str(sum(c['obtainable'] for c in cards))).replace('<!-- RESERVED_COUNT -->', str(sum(not c['available'] and not c['pending'] for c in cards)))


def build_csv():
    """Keep the library's download in sync with the same catalog as its HTML."""
    cards = json.loads((ROOT / 'data/cards.json').read_text())['cards']
    output = io.StringIO(newline='')
    writer = csv.writer(output, lineterminator='\n')
    writer.writerow(['Name', 'Tier', 'Rarity', 'Unique', 'Top', 'Right', 'Bottom', 'Left',
                     'Groups', 'Affinities', 'Available', 'Artwork', 'Roster status'])
    for card in sorted(cards, key=lambda c: (c['tier'], c['name'])):
        writer.writerow([card['name'], card['tier'], card['rarity'], card['unique'], *card['sides'],
                         '; '.join(card['groups']), '; '.join(card['affinities']),
                         card['acquisition']['available'], card.get('art_status', 'source'), card.get('roster_status', 'ready')])
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check that the document matches the current catalog and art')
    args = parser.parse_args()
    results = {'docs/card-library.html': build(), 'docs/card-library.csv': build_csv()}
    for path, text in results.items():
        output = ROOT / path
        content = text.encode('utf-8')
        if args.check:
            if not output.is_file() or output.read_bytes() != content:
                raise SystemExit('Card library is out of date. Run python3 tools/build-card-library.py')
        else:
            output.write_bytes(content)
            print(f'Built {path}')
    if args.check:
        print('HTML and CSV card libraries match the current catalog, opponent collections and artwork.')


if __name__ == '__main__':
    main()
