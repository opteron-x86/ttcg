"""The implemented roster must match the approved workshop, including retirements."""
from collections import Counter
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
read = lambda p: json.loads((root / p).read_text())
policy = read('data/roster-policy.json')
cards = read('data/cards.json')['cards']
by_id = {c['id']: c for c in cards}
retired = read('data/retired-cards.json')['cards']
registry = read('data/released-card-ids.json')
assert hashlib.sha256((root / policy['workshop']).read_bytes()).hexdigest() == policy['workshopSHA256']
assigned = set(policy['assignedIds'])
pending = set(policy['pendingIds'])
assert len(assigned) == 674 and len(pending) == 29 and len(retired) == 197
assert assigned.isdisjoint(pending)
assert set(by_id) == assigned | pending == set(registry)
assert set(policy['retiredIds']) == {c['id'] for c in retired}
assert not set(policy['retiredIds']) & set(by_id)
assert not {c['form_id'] for c in retired} & set(registry.values())
assert set(g for c in cards for g in c['groups']) == set(policy['standardGroups'] + policy['pendingGroups'])
assert all(len(c['groups']) == 1 for c in cards)
for group in policy['standardGroups']:
    roster = [c for c in cards if c['groups'] == [group]]
    for tier in policy['standardTiers']:
        row = [c for c in roster if c['tier'] == tier]
        assert Counter(c['rarity'] for c in row) == policy['standardRarities'], (group, tier)
        assert all(not c['unique'] and c['acquisition']['available'] for c in row)
    assert all(c['tier'] in (8, 9) and c['rarity'] == 'Legendary' and c['unique'] for c in roster if c['tier'] > 7)
for c in cards:
    assert registry[c['id']] == c['form_id']
    if c['id'] in pending:
        assert c['roster_status'] == 'pending' and c['groups'][0] in policy['pendingGroups']
        assert not c['acquisition']['available'] and not c['acquisition']['starter']
    if set(c['subtypes']) & {'Divine', 'Daedric Prince'}:
        assert c['groups'] == ['God'] and c['id'] in assigned
        assert c['tier'] == 10 and c['rarity'] == 'Legendary' and c['unique']
        assert c['acquisition']['special'] and not c['acquisition']['available']
    if c['acquisition']['starter']:
        assert c['tier'] == 1 and c['rarity'] == 'Common' and c['id'] in assigned
assert sum(c['acquisition']['starter'] for c in cards) == 8
assert sum('Divine' in c['subtypes'] for c in cards) == 9
assert sum('Daedric Prince' in c['subtypes'] for c in cards) == 17
lesser_gods = [c for c in cards if c['groups'] == ['God'] and c['id'] in pending]
assert {c['name'] for c in lesser_gods} == {'Almalexia', 'Vivec', 'Sotha Sil', 'Dagoth Ur', 'Morihaus'}
assert all(c['tier'] == 9 for c in lesser_gods)
library = json.loads(re.search(r'<script id="card-data" type="application/json">(.*?)</script>',
                              (root / 'docs/card-library.html').read_text(), re.S)[1])
assert {c['id'] for c in library if c['obtainable']} == assigned
assert {c['id'] for c in library if c['pending']} == pending
# Exact art/name associations, side order, rarity, affinities and user-added slots survive.
for group in read(policy['workshop'])['groups']:
    for slot in group['slots']:
        a = slot['assignment']
        if not a:
            continue
        ident = 'workshop-' + a['cardId'][4:] if a.get('kind') == 'new' else a['cardId']
        c = by_id[ident]
        assert c['name'] == a['name'] and c['groups'] == [group['template']['group']]
        for field in ('tier', 'rarity', 'unique', 'sides', 'affinities'):
            assert c[field] == slot[field], (c['name'], field)
pools = read('data/opponent-collections.json')['opponents']
for c in cards:
    owners = [p['name'] for p in pools if c['form_id'] in p['cards']]
    assert c['acquisition']['opponents'] == owners, c['name']
    if c['id'] in pending:
        assert not owners
print('Roster: exact workshop values, 16 balanced standard groups, 26 Tier X Gods, 29 pending superior cards, 197 retirements and eight starters verified')
