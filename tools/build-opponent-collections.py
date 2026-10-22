#!/usr/bin/env python3
"""Rebuild authored playing pools from rank, theme and the current catalog."""
import argparse
import json
import random
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STOP = {'and', 'the', 'of', 'a', 'an', 'to', 'for', 'with', 'in', 'on', 'or', 'at'}
ROMAN = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X']
SIGNATURES = {'Ysolda': [1339365],
    'Nazeem': [1302469],
    'Kodlak Whitemane': [2982026],
    'Runil': [1339714],
    'Viarmo': [1257912],
    'Taarie': [1257661],
    'Angi': [1257688],
    'Frida': [1294604],
    'Moth gro-Bagol': [1257788],
    'Neloth': [1302521],
    'Frea': [1332445],
    'Wuunferth the Unliving': [1347457],
    'Teldryn Sero': [1302514],
    'Brunwulf Free-Winter': [1347395],
    'Urag gro-Shub': [1339160],
    'Nelacar': [1302468],
    'Madesi': [1257919],
    'Mjoll the Lioness': [1271949],
    'Teeba-Ei': [1263447],
    'Captain Aldis': [1302480],
    'Sybille Stentor': [4194340],
    'Falion': [4194345],
    'Madena': [2534491],
    'Calcelmo': [4194327],
    'Delvin Mallory': [1266825],
    'Adril Arano': [2318608],
    'Niranye': [1339364],
    'Bothela': [4194374],
    'Froki Whetted-Blade': [2864470],
    'Idgrod Ravencrone': [4194335],
    "M'aiq the Liar": [4194417]}
ALL_SIGNATURES = {form: name for name, forms in SIGNATURES.items() for form in forms}
# Inclusive pool sizes by skill. Beginner and Learner stay compact; everyone else lands in 24-32.
SIZES = {
    'Beginner': (18, 18),
    'Learner': (22, 22),
    'Regular': (24, 26),
    'Experienced': (26, 28),
    'Expert': (28, 30),
    'Master': (30, 32),
}


def tokens(text):
    return {w for w in re.findall(r'[a-z]+', text.lower()) if w not in STOP and len(w) > 2}


def card_tokens(card):
    return tokens(' '.join([card['name'], *card['groups'], *card['subtypes']]))


def pool_label(ids, catalog):
    counts = Counter(catalog[i]['tier'] for i in ids)
    return ' · '.join(f'{ROMAN[t]} × {n}' for t, n in sorted(counts.items()))


def valid_pool(skill, ids, catalog, traveller=False):
    lo, hi = SIZES[skill]
    if not lo <= len(ids) <= hi or len(set(ids)) != len(ids):
        return False
    tiers = Counter(catalog[i]['tier'] for i in ids)
    extra = sum(n for t, n in tiers.items() if t not in range(1, 11))
    if extra:
        return False
    if traveller:
        return all(catalog[i]['tier'] in (6, 7) or i in SIGNATURES["M'aiq the Liar"] for i in ids)
    if skill == 'Beginner':
        return tiers[1] == len(ids)
    if skill == 'Learner':
        return 2 <= tiers[2] <= 3 and tiers[1] + tiers[2] == len(ids)
    if skill == 'Regular':
        return tiers[1] >= 6 and tiers[2] >= 8 and 2 <= tiers[3] <= 6 and sum(tiers[t] for t in range(4, 11)) == 0
    if skill == 'Experienced':
        return tiers[1] + tiers[2] <= 6 and tiers[3] >= 12 and 3 <= tiers[4] <= 8 and sum(tiers[t] for t in range(5, 11)) == 0
    if skill == 'Expert':
        return (tiers[1] + tiers[2] == 0 and tiers[3] >= 6 and tiers[4] >= 6 and tiers[5] >= 6
                and 2 <= tiers[6] <= 5 and 1 <= tiers[7] <= 4 and sum(tiers[t] for t in range(8, 11)) <= 2)
    if skill == 'Master':
        return (sum(tiers[t] for t in range(1, 5)) == 0 and tiers[5] >= 10 and tiers[6] >= 8
                and 2 <= tiers[7] <= 6 and sum(tiers[t] for t in range(8, 11)) <= 2)
    return False


def allocate(skill, size, rng, reserved):
    left = size - sum(reserved.values())
    if left < 0:
        raise ValueError('reserved cards exceed pool size')
    if skill == 'Beginner':
        plan = {1: left}
    elif skill == 'Learner':
        t2 = min(3, max(0, 2 + rng.randrange(2) - reserved[2]))
        t2 = min(t2, left)
        plan = {2: t2, 1: left - t2}
    elif skill == 'Regular':
        t3 = min(4, max(2, rng.randint(2, 4) - reserved[3]))
        t3 = min(t3, max(0, left - 14))
        t2 = min(14, max(8, rng.randint(10, 14) - reserved[2]))
        t2 = min(t2, max(0, left - t3 - 6))
        plan = {3: t3, 2: t2, 1: left - t2 - t3}
    elif skill == 'Experienced':
        t1 = min(2, max(0, rng.randint(0, 2) - reserved[1]))
        t2 = min(4, max(2, rng.randint(2, 4) - reserved[2]))
        t4 = min(6, max(4, rng.randint(4, 6) - reserved[4]))
        t1, t2, t4 = min(t1, left), min(t2, max(0, left - t1)), min(t4, max(0, left - t1 - t2 - 12))
        plan = {1: t1, 2: t2, 4: t4, 3: left - t1 - t2 - t4}
    elif skill == 'Expert':
        t7 = min(2, max(1, rng.randint(1, 2) - reserved[7]))
        t6 = min(4, max(2, rng.randint(2, 4) - reserved[6]))
        t5 = min(9, max(6, rng.randint(6, 9) - reserved[5]))
        t4 = min(10, max(7, rng.randint(7, 10) - reserved[4]))
        t7 = min(t7, left)
        t6 = min(t6, max(0, left - t7 - 19))
        t5 = min(t5, max(0, left - t7 - t6 - 13))
        t4 = min(t4, max(0, left - t7 - t6 - t5 - 6))
        plan = {7: t7, 6: t6, 5: t5, 4: t4, 3: left - t7 - t6 - t5 - t4}
    elif skill == 'Master':
        t7 = min(4, max(2, rng.randint(2, 4) - reserved[7]))
        t6 = min(14, max(10, rng.randint(10, 14) - reserved[6]))
        t7 = min(t7, left)
        t6 = min(t6, max(0, left - t7 - 10))
        plan = {7: t7, 6: t6, 5: left - t7 - t6}
    else:
        raise ValueError(skill)
    return {t: n for t, n in plan.items() if n > 0}


def score(card, theme, name):
    want = tokens(theme) | tokens(name)
    overlap = len(want & card_tokens(card))
    return overlap * 8 + (2 if card['rarity'] in ('Common', 'Rare') else 0)


def take(pool, n, rng, used, usage, theme, name):
    choices = [c for c in pool if c['form_id'] not in used]
    picked = []
    for _ in range(n):
        if not choices:
            break
        weights = [max(1, score(c, theme, name) * 4 + 6) / (1 + usage[c['form_id']]) for c in choices]
        card = rng.choices(choices, weights=weights, k=1)[0]
        picked.append(card)
        choices.remove(card)
        used.add(card['form_id'])
        usage[card['form_id']] += 1
    return picked


def ensure_basics(chosen, by_tier, rng, used, usage, theme, name, need=6):
    basics = [c for c in chosen if c['rarity'] in ('Common', 'Rare') and not c['unique']]
    if len(basics) >= need:
        return chosen
    wanted = need - len(basics)
    replaceable = [c for c in chosen if c['rarity'] not in ('Common', 'Rare') and not c['unique']]
    rng.shuffle(replaceable)
    for victim in replaceable:
        options = [c for c in by_tier[victim['tier']]
                   if c['form_id'] not in used and c['rarity'] in ('Common', 'Rare') and not c['unique']]
        if not options:
            continue
        weights = [max(1, score(c, theme, name) * 4 + 6) / (1 + usage[c['form_id']]) for c in options]
        card = rng.choices(options, weights=weights, k=1)[0]
        chosen = [card if c is victim else c for c in chosen]
        used.remove(victim['form_id'])
        used.add(card['form_id'])
        usage[victim['form_id']] -= 1
        usage[card['form_id']] += 1
        wanted -= 1
        if wanted <= 0:
            break
    return chosen


def build_pool(profile, current, catalog, by_tier, rng, usage, exclusive):
    skill = profile['skillName']
    lo, hi = SIZES[skill]
    size = lo if lo == hi else rng.randint(lo, hi)
    theme = current.get('theme', '')
    signatures = SIGNATURES.get(profile['name'], [])
    ordinary_size = size - len(signatures)
    used, chosen = set(signatures), []
    exclusive.update(signatures)
    for form in current.get('cards', []):
        card = catalog.get(form)
        if not card or form in used or not card['acquisition']['available']:
            continue
        if card['unique']:
            continue
        chosen.append(card)
        used.add(form)
    plan = ({6: ordinary_size // 2, 7: ordinary_size - ordinary_size // 2}
            if profile['name']=="M'aiq the Liar" else allocate(skill, ordinary_size, rng, Counter()))
    kept = []
    have = Counter()
    for card in chosen:
        if have[card['tier']] < plan.get(card['tier'], 0):
            kept.append(card)
            have[card['tier']] += 1
        else:
            used.discard(card['form_id'])
    chosen = kept
    for tier, need in plan.items():
        missing = need - have[tier]
        if missing > 0:
            added = take(by_tier[tier], missing, rng, used, usage, theme, profile['name'])
            chosen.extend(added)
            have[tier] += len(added)
    if len(chosen) < ordinary_size:
        fallback = [c for t in range(1, 11) if plan.get(t, 0) for c in by_tier[t]]
        chosen.extend(take(fallback, ordinary_size - len(chosen), rng, used, usage, theme, profile['name']))
    chosen = ensure_basics(chosen, by_tier, rng, used, usage, theme, profile['name'])
    chosen = [catalog[form] for form in signatures] + [c for c in chosen if c['form_id'] not in set(signatures)]
    order = {form: i for i, form in enumerate(current.get('cards', []))}
    owned = set(signatures)
    chosen.sort(key=lambda c: (0 if c['form_id'] in owned else 1, order.get(c['form_id'], 10_000), c['name']))
    ids = [c['form_id'] for c in chosen]
    if not valid_pool(skill, ids, catalog, profile['name']=="M'aiq the Liar"):
        raise SystemExit(f'{profile["name"]} failed pool policy: {skill} {pool_label(ids, catalog)} ({len(ids)})')
    return ids, [catalog[i]['name'] for i in ids]


def generate(catalog, profiles, current):
    cards = {c['form_id']: c for c in catalog}
    by_tier = {t: [c for c in catalog if c['acquisition']['available'] and not c['unique'] and c['tier'] == t]
               for t in range(1, 11)}
    current_by_name = {row['name']: row for row in current}
    usage, exclusive, rows = Counter(), set(ALL_SIGNATURES), []
    for profile in profiles:
        rng = random.Random(profile['base'] ^ 0x54544347)
        ids, names = build_pool(profile, current_by_name[profile['name']], cards, by_tier, rng, usage, exclusive)
        rows.append({
            'name': profile['name'],
            'theme': current_by_name[profile['name']].get('theme', ''),
            'cards': ids,
            'card_names': names,
        })
    # Tier VII Commons have no pack route. Give each one an authored holder,
    # replacing a same-tier duplicate or a card that also comes from packs.
    ownership = Counter(form for row in rows for form in row['cards'])
    missing = sorted((c for c in catalog if c['acquisition']['available'] and not c['unique']
                      and c['tier'] == 7 and c['rarity'] == 'Common' and not ownership[c['form_id']]),
                     key=lambda c: c['id'])
    for card in missing:
        choices = [(score(card, row['theme'], row['name']) - score(cards[form], row['theme'], row['name']),
                    ownership[form], -i, -slot, i, slot, form)
                   for i, row in enumerate(rows) for slot, form in enumerate(row['cards'])
                   if cards[form]['tier'] == 7 and not cards[form]['unique']
                   and (ownership[form] > 1 or cards[form]['rarity'] in ('Rare', 'Epic'))]
        if not choices:
            raise ValueError(f'No rank-appropriate collection can supply {card["name"]}')
        *_, i, slot, old = max(choices)
        rows[i]['cards'][slot] = card['form_id']
        rows[i]['card_names'][slot] = card['name']
        ownership[old] -= 1
        ownership[card['form_id']] += 1
    for row in rows:
        for form in row['cards']:
            owner = ALL_SIGNATURES.get(form)
            if owner and owner != row['name']:
                raise SystemExit(f'{row["name"]} must not hold {cards[form]["name"]}')
        for form in SIGNATURES.get(row['name'], []):
            if form not in row['cards']:
                raise SystemExit(f'{row["name"]} is missing {cards[form]["name"]}')
    return {'version': 3, 'opponents': rows}


def update_hold_doc(text, profiles, pools, catalog):
    text = text.replace('18-card pool', 'Card pool')
    text = text.replace(
        '- **Card pool:** tier counts in the authored starting collection. A match draws five cards from the persistent collection; wins, losses and replenishment can change its contents.',
        '- **Card pool:** tier counts in the authored starting collection. Beginners hold 18 Tier I cards, Learners 22 with a couple of Tier II, and everyone else 24–32 cards weighted toward their rank. A match draws five cards from the persistent collection; wins, losses and replenishment can change its contents.')
    text = text.replace('its owner’s 18-card playing collection', 'its owner’s playing collection')
    text = text.replace('from their persistent 18-card collections', 'from their persistent playing collections')
    text = text.replace('counted from its 18 card IDs', 'counted from its authored card IDs')
    text = text.replace(
        'Only the reputation title appears in the journal. The numbers below are reference information, not in-game progress displays.',
        'The journal shows the hold’s reputation title and a Wins total. The numbers below are reference information, not in-game progress displays.')
    by_name = {row['name']: row['cards'] for row in pools}
    profile_by_name = {p['name']: p for p in profiles}
    lines = []
    for line in text.splitlines(keepends=True):
        if line.startswith('| ') and ' | ' in line and not line.startswith('| Opponent') and not line.startswith('|---'):
            cells = [c.strip() for c in line.strip().strip('|').split('|')]
            if len(cells) >= 4 and cells[0] in by_name:
                cells[3] = pool_label(by_name[cells[0]], catalog)
                cells[2] = profile_by_name[cells[0]]['skillName']
                if len(cells) >= 6:
                    cells[4] = str(profile_by_name[cells[0]]['wager'])
                notes = []
                for form in SIGNATURES.get(cells[0], []):
                    label = f"{catalog[form]['name']} ({ROMAN[catalog[form]['tier']]})"
                    if catalog[form]['name'] not in cells[-1]:
                        notes.append(label)
                if notes and len(cells) >= 6:
                    cells[-1] = cells[-1] + '; ' + '; '.join(notes)
                line = '| ' + ' | '.join(cells) + ' |\n'
                if not line.endswith('\n') and text.endswith('\n'):
                    pass
        lines.append(line)
    return ''.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    catalog = json.loads((ROOT / 'data/cards.json').read_text())['cards']
    cards = {c['form_id']: c for c in catalog}
    profiles = json.loads((ROOT / 'data/opponents.json').read_text())
    current = json.loads((ROOT / 'data/opponent-collections.json').read_text())['opponents']
    payload = generate(catalog, profiles, current)
    text = json.dumps(payload, indent=2, ensure_ascii=False) + '\n'
    path = ROOT / 'data/opponent-collections.json'
    if args.check:
        existing = json.loads(path.read_text())
        held = {form for row in existing['opponents'] for form in row['cards']}
        assert all(c['form_id'] in held for c in catalog if c['acquisition']['available']
                   and c['tier'] == 7 and c['rarity'] == 'Common'), 'Tier VII Commons need an NPC acquisition route'
        for row, profile in zip(existing['opponents'], profiles, strict=True):
            assert row['name'] == profile['name']
            assert valid_pool(profile['skillName'], row['cards'], cards, profile['name']=="M'aiq the Liar"), (profile['name'], profile['skillName'], pool_label(row['cards'], cards))
            for form in SIGNATURES.get(profile['name'], []):
                assert form in row['cards'], (profile['name'], cards[form]['name'])
        print(f'{len(payload["opponents"])} opponent collections match the rank-weighted pool policy')
        return
    path.write_text(text)
    document = json.loads((ROOT / 'data/cards.json').read_text())
    for card in document['cards']:
        acquisition = card['acquisition']
        acquisition['opponents'] = [row['name'] for row in payload['opponents'] if card['form_id'] in row['cards']]
        if card['unique'] and card.get('roster_status') != 'pending':
            acquisition['available'] = bool(acquisition['opponents'])
            acquisition['route'] = 'Signature opponent' if acquisition['available'] else 'Rewards and tournaments'
    (ROOT / 'data/cards.json').write_text(json.dumps(document, indent=2, ensure_ascii=False) + '\n')
    hold = ROOT / 'docs/opponents-by-hold.md'
    hold.write_text(update_hold_doc(hold.read_text(), profiles, payload['opponents'], cards))
    print(f'{len(payload["opponents"])} opponent collections written')


if __name__ == '__main__':
    main()
