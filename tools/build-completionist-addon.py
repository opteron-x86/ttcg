#!/usr/bin/env python3
"""Build Completionist checklists and an ESL plugin with unplaced markers."""
import argparse
import json
from pathlib import Path
import re
import struct
from plugin_records import field, walk

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'addons/completionist'
MASTERS = ['Skyrim.esm', 'Completionist.esp', 'Tessera TCG.esp']
ROMAN = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X']

def u32(value): return struct.pack('<I', value)
def ztext(value): return value.encode('utf-8') + b'\0'
def text(value):
    value = value.encode('utf-8')
    return struct.pack('<H', len(value)) + value
def script(name): return text(name) + b'\0' + struct.pack('<H', 0)
def record(tag, form, fields, flags=0):
    body = b''.join(field(k.encode(), v) for k, v in fields)
    return tag.encode() + struct.pack('<IIIIHH', len(body), flags, form, 0, 44, 0) + body
def group(tag, records):
    body = b''.join(records)
    return b'GRUP' + u32(len(body) + 24) + tag.encode() + bytes(12) + body

def stable_ids(keys, path):
    """Append only, including unreleased cards. Never renumber a saved form."""
    ids = json.loads(path.read_text()) if path.exists() else {}
    values = list(ids.values())
    if len(values) != len(set(values)) or any(not isinstance(v, int) or v < 0x810 or v >= 0xFFF or v % 2 for v in values):
        raise ValueError('Invalid saved card mapping')
    next_id = max(values, default=0x80E) + 2
    for key in sorted(keys):
        if key not in ids:
            if next_id + 1 >= 0x1000:
                raise ValueError('This set needs more than one ESL checklist plugin')
            ids[key] = next_id
            next_id += 2
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(ids, indent=2, ensure_ascii=False) + '\n')
    return ids

def build(cards, set_id='tessera', set_name='Tessera', output=OUT, id_file=None, core=True):
    if not re.fullmatch(r'[a-z][a-z0-9_-]{0,63}', set_id) or (set_id == 'tessera' and not core):
        raise ValueError('Set ID must match the expansion manifest; tessera is reserved for the main set')
    if not set_name.strip() or any(c in set_name for c in '\n\r\t|[]'):
        raise ValueError('Invalid set name')
    cards = [dict(card) for card in cards]
    keys = []
    for card in cards:
        if not re.fullmatch(r'[a-z][a-z0-9_-]{0,63}', card['id']): raise ValueError('Invalid card ID')
        if type(card['tier']) is not int or not 1 <= card['tier'] <= 10 or not card['name'].strip(): raise ValueError('Invalid card name or tier')
        if any(c in card['name'] for c in '\n\r\t|'): raise ValueError('Card names must be single-line text')
        keys.append(set_id + ':' + card['id'])
    if len(keys) != len(set(keys)): raise ValueError('Duplicate card IDs')
    output.mkdir(parents=True, exist_ok=True)
    id_file = id_file or output / 'card-ids.json'
    ids = stable_ids(keys, id_file)
    released = [card for card in cards if card.get('roster_status', 'ready') != 'pending']
    if not released: raise ValueError('No released cards in this set')
    masters = MASTERS if core else MASTERS + ['Tessera Completionist.esp']
    own = len(masters) << 24
    plugin = 'Tessera Completionist.esp' if core else f'Tessera Completionist - {set_id}.esp'
    stem = 'Tessera' if core else f'Tessera_{set_id}'
    records = []
    mapping = {'version': 1, 'plugin': plugin, 'set': set_id, 'pages': []}
    for card in released:
        key = set_id + ':' + card['id']
        for foil in (False, True):
            local = ids[key] + int(foil)
            records.append(record('MISC', own | local, [
                ('EDID', ztext(f'TesseraChecklist_{set_id.replace("-", "_")}_{local:03X}')),
                ('OBND', bytes(12)),
                ('FULL', ztext(card['name'] + (' (Foil)' if foil else ''))),
                ('DATA', bytes(8)),
            ]))
    content = group('MISC', records)
    if core:
        quest = own | 0x800
        name = 'Tessera_CompletionistQuest'
        vmad = struct.pack('<HHH', 5, 2, 1) + script(name)
        vmad += b'\x02' + struct.pack('<H', 0) + text(name) + struct.pack('<H', 1)
        vmad += struct.pack('<HhIHHH', 0, 0, quest, 5, 2, 1) + script('Tessera_CompletionistPlayer')
        content += group('QUST', [record('QUST', quest, [
            ('EDID', ztext('TesseraCompletionistTracker')), ('VMAD', vmad),
            ('FULL', ztext('Tessera collection tracking')),
            ('DNAM', struct.pack('<HBBII', 1, 0, 255, 0, 0)),
            ('NEXT', b''), ('ANAM', u32(1)),
            ('ALST', u32(0)), ('ALID', ztext('Player')), ('FNAM', u32(0x200)),
            ('ALFR', u32(0x14)), ('VTCK', u32(0)), ('ALED', b''),
        ])])
        (output / 'Seq').mkdir(exist_ok=True)
        (output / 'Seq' / plugin.replace('.esp', '.seq')).write_bytes(u32(quest))
    all_records, groups = walk(content)
    header = [('HEDR', struct.pack('<fII', 1.7, len(all_records) + len(groups), max(ids.values()) + 2)),
              ('CNAM', ztext('Tessera')), ('SNAM', ztext('Card collection checklists for Completionist.'))]
    for master in masters: header += [('MAST', ztext(master)), ('DATA', bytes(8))]
    (output / plugin).write_bytes(record('TES4', 0, header, 0x200) + content)
    for foil in (False, True):
        kind = 'Foils' if foil else 'Cards'
        file_stem = stem + '_' + kind
        title_key = file_stem + '_Page'
        label = ('Tessera' if core else set_name) + ' ' + kind
        ini = ['[Completionist Patch Data]', 'axPlt = 0', 'Log = False', 'IsVanillaTracking = False',
               'Priority = 50', 'Header = Tessera_Header', 'Enabled = True', f'PluginFileName = {plugin}',
               f'MCMPageName = {title_key}', 'PrependPageNumber = False']
        translations = {'Tessera_Header': 'Tessera', title_key: label, 'Tessera_Collected': 'Collected',
                        'Tessera_Help': 'Collected at least once. Foils also count toward the main card checklist.'}
        sections = []
        number = 0
        for tier in range(1, 11):
            tier_cards = sorted([c for c in released if c['tier'] == tier], key=lambda c: (c['name'].casefold(), c['id']))
            for start in range(0, len(tier_cards), 126):
                number += 1
                if number > 127: raise ValueError('Too many checklist pages')
                chunk = tier_cards[start:start + 126]
                tier_key = f'{file_stem}_Tier{tier}_{start // 126 + 1}'
                translations[tier_key] = f'Tier {ROMAN[tier]}' + (f' ({start // 126 + 1})' if len(tier_cards) > 126 else '')
                ini.append(f'PageName{number} = {tier_key}')
                entries = [{'key': set_id + ':' + c['id'], 'form': ids[set_id + ':' + c['id']] + int(foil), 'foil': foil} for c in chunk]
                sections += ['', f'[Tier {tier} Part {start // 126 + 1}]', 'Enabled = True', f'DisplayOnPage = {number}',
                             f'SectionHeader_L = {tier_key}', 'SectionHeader_R = Tessera_Collected', 'Type = 0',
                             'FormIDs = ' + ','.join(f'0x{e["form"]:06X}' for e in entries)]
                for card, entry in zip(chunk, entries):
                    card_key = f'{file_stem}_Card{entry["form"]:03X}'
                    translations[card_key] = card['name']
                    sections.append(f'0x{entry["form"]:06X}_DisplayName = {card_key}')
                    sections.append(f'0x{entry["form"]:06X}_Highlight = Tessera_Help')
                mapping['pages'].append({'nameKey': title_key, 'translation': file_stem + '.txt', 'number': number, 'entries': entries})
        path = output / 'SKSE/Plugins/CompletionistData/Addons' / (file_stem + '.ini')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('\n'.join(ini + sections) + '\n', encoding='utf-8')
        path = output / 'SKSE/Plugins/CompletionistData/Translations' / (file_stem + '.txt')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(''.join(f'{key}\t|{value}\n' for key, value in translations.items()), encoding='utf-8')
    path = output / 'SKSE/Plugins/Tessera/Completionist' / (set_id + '.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(mapping, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'{plugin}: {len(released)} cards, {len(released)} foils, {len(mapping["pages"])} pages; no world references or overrides')
    return mapping

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expansion', type=Path, help='Expansion manifest to create extra checklists for')
    parser.add_argument('--output', type=Path, help='Separate output mod folder')
    args = parser.parse_args()
    if args.expansion:
        if not args.output: parser.error('--expansion needs --output')
        manifest = json.loads(args.expansion.read_text(encoding='utf-8-sig'))
        if manifest.get('schemaVersion') != 1: parser.error('Unsupported expansion schemaVersion')
        set_id = manifest['setId']
        build(manifest['cards'], set_id, manifest['name'], args.output, core=False)
        (args.output / 'README.md').write_text(f'# {manifest["name"]} — Completionist checklist\n\nInstall alongside Tessera Completionist and the {manifest["name"]} expansion. Enable the included ESP.\n\nKeep `card-ids.json` when rebuilding this checklist so existing saves keep their checkmarks.\n')
    else:
        build(json.loads((ROOT / 'data/cards.json').read_text())['cards'], output=args.output or OUT,
              id_file=ROOT / 'data/completionist-card-ids.json')

if __name__ == '__main__': main()
