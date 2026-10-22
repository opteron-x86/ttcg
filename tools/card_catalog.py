"""Validate catalog identities and render the generated native metadata."""
import json
import re
import unicodedata

from card_rank_policy import valid_profile

ATTRIBUTES = ['Strength', 'Intelligence', 'Willpower', 'Agility', 'Endurance']
RARITIES = {'Common', 'Rare', 'Epic', 'Legendary'}
SIDE_ORDER = ['top', 'right', 'bottom', 'left']


def name_key(name):
    return unicodedata.normalize('NFC', name).casefold()


def safe_name(name):
    return (isinstance(name, str) and bool(name.strip()) and len(name) <= 120
            and name == name.strip() and not re.search(r'[<>:"/\\|?*\x00-\x1f\x7f]', name)
            and not name.endswith('.')
            and not re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?', name, re.I))


def validate_catalog(catalog, registry):
    cards = catalog['cards']
    if not cards or catalog['side_order'] != SIDE_ORDER:
        raise ValueError('Empty catalog or unexpected side order')
    ids, forms, names = set(), set(), set()
    for c in cards:
        ident, form, name = c['id'], c['form_id'], c['name']
        if (not isinstance(ident, str) or not re.fullmatch(r'[a-z0-9-]+', ident) or ident in ids
                or type(form) is not int or not 0 < form < 0x80000000 or form in forms):
            raise ValueError(f'Invalid or repeated card identity: {ident}')
        if not safe_name(name) or name_key(name) in names:
            raise ValueError(f'Unsafe or duplicate card filename: {name}')
        ids.add(ident)
        forms.add(form)
        names.add(name_key(name))
        if type(registry.get(ident)) is not int or registry[ident] != form:
            raise ValueError(f'Card registry mismatch: {ident}')
        if (type(c['tier']) is not int or c['tier'] not in range(1, 11)
                or c['rarity'] not in RARITIES or not valid_profile(c['sides'], c['tier'], c['rarity'])):
            raise ValueError(f'Invalid side profile: {name}')
        for key in ('groups', 'subtypes', 'affinities'):
            if (not isinstance(c[key], list) or any(not isinstance(v, str) or not v for v in c[key])
                    or len(c[key]) != len(set(c[key]))):
                raise ValueError(f'Invalid {key}: {name}')
        if any(a not in ATTRIBUTES for a in c['affinities']):
            raise ValueError(f'Invalid affinities: {name}')
        if (type(c['unique']) is not bool or not isinstance(c['focus'], str)
                or any(type(c['acquisition'][k]) is not bool for k in ('starter', 'available'))):
            raise ValueError(f'Invalid acquisition or display metadata: {name}')
        if c.get('roster_status', 'ready') not in ('ready', 'pending'):
            raise ValueError(f'Invalid roster status: {name}')
        if c.get('roster_status') == 'pending' and (c['acquisition']['available'] or c['acquisition']['starter']):
            raise ValueError(f'Unfinished card cannot enter normal acquisition: {name}')
    if set(registry) != ids:
        raise ValueError('Card registry must match the catalog')


def card_outputs(catalog, registry):
    validate_catalog(catalog, registry)
    cards = catalog['cards']
    lines = ['#pragma once', '#include <array>', '#include "RuntimeCatalog.h"', 'namespace ttcg {',
             'inline constexpr std::array<const char*,5> attributeNames{"Strength","Intelligence","Willpower","Agility","Endurance"};',
             'struct Card { const char* name; const char* art; std::array<int,4> sides; int tier; const char* focus; const char* id; unsigned form; const char* rarity; const char* groups; const char* subtypes; const char* affinities; unsigned affinityMask; bool starter; bool unique; bool available; bool pending; };',
             f'inline constexpr std::array<Card,{len(cards)}> baseCards{{{{']
    for c in cards:
        sides = ','.join(map(str, c['sides']))
        mask = sum(1 << ATTRIBUTES.index(a) for a in c['affinities'])
        metadata = ','.join(json.dumps(json.dumps(c[k], ensure_ascii=False)) for k in ('groups', 'subtypes', 'affinities'))
        a = c['acquisition']
        lines.append(' {' + f'{json.dumps(c["name"])},{json.dumps("cards/" + c["name"] + ".webp")},{{{sides}}},{c["tier"]},{json.dumps(c["focus"])},{json.dumps(c["id"])},{c["form_id"]},{json.dumps(c["rarity"])},{metadata},{mask},{str(a["starter"]).lower()},{str(c["unique"]).lower()},{str(a["available"]).lower()},{str(c.get("roster_status") == "pending").lower()}' + '},')
    lines += ['}};', 'inline constexpr RuntimeCatalog<baseCards> cards;', '}']
    index = {c['name']: {'id': c['id'], 'card': c['name'] + '.webp', 'thumbnail': c['name'] + '.webp'} for c in cards}
    return {'include/Cards.h': '\n'.join(lines) + '\n',
            'data/card-art-filenames.json': json.dumps(index, indent=2, ensure_ascii=False) + '\n'}


def creature_groups_output(cards):
    groups = sorted({g for c in cards for g in c['groups']})
    if len(groups) > 64:
        raise ValueError('Group masks support up to 64 creature groups')
    return ('#pragma once\n#include <array>\n#include "RuntimeCatalog.h"\n#include <cstdint>\n#include <string_view>\nnamespace ttcg {\n'
            f'inline constexpr std::array<std::string_view,{len(groups)}> creatureGroups{{'
            + ','.join(json.dumps(g) for g in groups) + '};\n'
            f'inline constexpr std::array<std::uint64_t,{len(cards)}> baseCardGroups{{'
            + ','.join(str(sum(1 << groups.index(g) for g in c['groups'])) + 'ull' for c in cards) + '};\ninline constexpr RuntimeCatalog<baseCardGroups> cardGroups;\n}\n')
