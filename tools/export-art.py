#!/usr/bin/env python3
"""Export WebP artwork from preserved originals and generate the native catalog."""
from pathlib import Path
import argparse
import json
import hashlib
from PIL import Image, ImageOps
from card_catalog import card_outputs
from card_art import render_art, webp_bytes

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=Path.home() / 'Pictures/TT')
parser.add_argument("--metadata-only", action="store_true", help="Regenerate the card header without rewriting unchanged images")
parser.add_argument('--missing-only', action='store_true', help='Export only missing runtime images')
args = parser.parse_args()
catalog = json.loads((root / 'data/cards.json').read_text())
cards = catalog['cards']
registry=json.loads((root/'data/released-card-ids.json').read_text())
outputs = card_outputs(catalog, registry)
destination = root / 'TesseraUI/art/cards'
destination.mkdir(parents=True, exist_ok=True)
thumbs = root / 'TesseraUI/art/thumbs'
thumbs.mkdir(parents=True, exist_ok=True)
for card in cards:
    source = root/card['source_art'] if card.get('source_art') else args.source / card['source']
    if not args.metadata_only:
        assert source.is_file(), source
    if not args.metadata_only and card.get('source_sha1'):
        assert hashlib.sha1(source.read_bytes()).hexdigest()==card['source_sha1']
    filename = card['name'] + '.webp'
    if not args.metadata_only and (not args.missing_only or not (destination/filename).exists() or not (thumbs/(card['name']+'.webp')).exists()):
        face, thumbnail = render_art(source.read_bytes())
        (destination/filename).write_bytes(face)
        (thumbs/(card['name']+'.webp')).write_bytes(thumbnail)
if not args.metadata_only:
    for name in ('mosaic', 'dragon', 'celestial'):
        source = root / 'reference/card-backs' / (name + '.png')
        output = root / 'TesseraUI/art/backs' / (name + '.webp')
        if not args.missing_only or not output.exists():
            with Image.open(source) as image:
                output.write_bytes(webp_bytes(ImageOps.exif_transpose(image).convert('RGBA'), lossless=True))
for path, text in outputs.items():
    (root / path).write_text(text)
print(f'Exported {len(cards)} Tessera cards, filename index and include/Cards.h')
