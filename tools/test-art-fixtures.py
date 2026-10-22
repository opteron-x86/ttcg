#!/usr/bin/env python3
"""Create geometric image fixtures for tests in an artwork-free checkout."""
import io
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def main():
    image = Image.new('RGB', (192, 256), '#25282e')
    draw = ImageDraw.Draw(image)
    draw.rectangle((8, 8, 183, 247), outline='#b3a68a', width=3)
    draw.polygon([(96, 60), (150, 128), (96, 196), (42, 128)], outline='#b3a68a', width=3)
    files = [f'TesseraUI/art/{folder}/{card["name"]}.webp'
             for card in json.loads((ROOT / 'data/cards.json').read_text(encoding='utf-8'))['cards']
             for folder in ('cards', 'thumbs')]
    encoded = io.BytesIO()
    image.save(encoded, 'WEBP', lossless=True)
    count = 0
    for name in files:
        path = ROOT / name
        if path.is_file():
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(encoded.getvalue())
        count += 1
    print(f'Created {count} test images')


if __name__ == '__main__':
    main()
