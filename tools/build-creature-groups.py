#!/usr/bin/env python3
"""Generate group masks in the same catalog order used by the native cards."""
from pathlib import Path
import json,sys
from card_catalog import creature_groups_output
root=Path(__file__).resolve().parents[1]
cards=json.loads((root/'data/cards.json').read_text())['cards'];groups=sorted({g for c in cards for g in c['groups']})
text=creature_groups_output(cards)
p=root/'include/CreatureGroups.h'
if '--check' in sys.argv:assert p.read_text()==text
else:p.write_text(text)
print(f'{len(groups)} creature groups verified across {len(cards)} cards')
