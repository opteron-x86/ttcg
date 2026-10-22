"""Boundary examples and whole-roster checks for the card rank policy."""
import csv
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / 'tools'))
from card_rank_policy import valid_profile, weighted_cost

cases = [
    (1, 'Common', [6, 2, 2, 2], True),
    (1, 'Common', [6, 3, 1, 1], False),
    (2, 'Common', [7, 3, 2, 2], True),
    (2, 'Rare', [7, 4, 1, 1], False),
    (3, 'Common', [7, 4, 4, 3], True),
    (3, 'Common', [7, 5, 3, 3], False),
    (3, 'Rare', [7, 5, 3, 3], True),
    (3, 'Epic', [7, 5, 4, 1], False),
    (3, 'Legendary', [7, 1, 7, 1], False),
    (4, 'Common', [7, 7, 4, 2], True),
    (4, 'Rare', [8, 3, 4, 3], True),
    (4, 'Rare', [8, 4, 3, 3], False),
    (4, 'Legendary', [8, 1, 5, 1], False),
    (5, 'Common', [8, 4, 5, 4], True),
    (5, 'Epic', [8, 5, 4, 4], False),
    (5, 'Legendary', [8, 1, 6, 1], False),
    (6, 'Common', [9, 4, 5, 4], True),
    (6, 'Rare', [9, 5, 4, 4], False),
    (6, 'Legendary', [9, 1, 6, 1], False),
]
for tier, rarity, sides, expected in cases:
    for rotation in range(4):
        p = sides[rotation:] + sides[:rotation]
        assert valid_profile(p, tier, rarity) == expected, (tier, rarity, p)
assert weighted_cost([7, 1, 1, 1]) == 12
assert weighted_cost([8, 1, 1, 1]) == 16
assert weighted_cost([9, 1, 1, 1]) == 21
assert weighted_cost([10, 1, 1, 1]) == 27

catalog = json.loads((root / 'data/cards.json').read_text())['cards']
assert len(catalog) == 703
for card in catalog:
    assert valid_profile(card['sides'], card['tier'], card['rarity']), card['name']
assert sum(c.get('roster_status') == 'pending' for c in catalog) == 29
assert all(c['unique'] or c.get('roster_status') == 'pending' for c in catalog if not c['acquisition']['available'])
print('Card ranks: 76 rotated boundary cases and all 703 current profiles passed.')
