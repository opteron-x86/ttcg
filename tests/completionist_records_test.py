"""Check the generated plugin against Completionist's actual INI/page contract."""
import configparser
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from plugin_records import walk, parse_fields
spec = importlib.util.spec_from_file_location('completionist_builder', ROOT / 'tools/build-completionist-addon.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class RecordsTest(unittest.TestCase):
    def validate(self, folder, mapping):
        blob = (folder / mapping['plugin']).read_bytes()
        records, groups = walk(blob)
        def fields(form):
            a, b, _ = records[form]
            return parse_fields(blob[a:b])[1]
        self.assertTrue(struct.unpack_from('<I', blob, 8)[0] & 0x200)
        masters = [v.rstrip(b'\0').decode() for k, v in fields(0) if k == b'MAST']
        self.assertEqual(masters[:3], builder.MASTERS)
        own = len(masters) << 24
        used = set()
        pairs = {}
        for page in mapping['pages']:
            ini = configparser.ConfigParser(interpolation=None)
            ini.optionxform = str
            stem = Path(page['translation']).stem
            ini.read(folder / 'SKSE/Plugins/CompletionistData/Addons' / (stem + '.ini'))
            settings = ini['Completionist Patch Data']
            self.assertEqual(settings['PluginFileName'], mapping['plugin'])
            self.assertEqual(settings['MCMPageName'], page['nameKey'])
            self.assertIn(f'PageName{page["number"]}', settings)
            sections = [ini[s] for s in ini.sections()[1:] if ini[s].get('DisplayOnPage') == str(page['number'])]
            self.assertEqual(len(sections), 1)
            section = sections[0]
            self.assertEqual(section['Type'], '0')
            self.assertLessEqual(len(page['entries']), 126)
            formids = [int(value, 16) for value in section['FormIDs'].split(',')]
            self.assertEqual(formids, [e['form'] for e in page['entries']])
            lines = (folder / 'SKSE/Plugins/CompletionistData/Translations' / page['translation']).read_text().splitlines()
            translations = {line.split('\t', 1)[0]: line.split('|', 1)[1] for line in lines}
            self.assertIn(page['nameKey'], translations)
            for entry in page['entries']:
                form = own | entry['form']
                self.assertNotIn(form, used)
                used.add(form)
                self.assertEqual(blob[records[form][0]:records[form][0] + 4], b'MISC')
                self.assertEqual(dict(fields(form))[b'DATA'], bytes(8))
                self.assertIn(section[f'0x{entry["form"]:06X}_DisplayName'], translations)
                pairs.setdefault(entry['key'], set()).add(entry['foil'])
        self.assertTrue(all(flags == {False, True} for flags in pairs.values()))
        for form, (a, _, _) in records.items():
            if not form: continue
            self.assertEqual(form >> 24, len(masters), 'Must not override another mod')
            self.assertTrue(0x800 <= (form & 0xFFFFFF) < 0x1000)
            self.assertIn(blob[a:a + 4], (b'MISC', b'QUST'), 'No placed/inventory/world records')
        return records, fields, pairs

    def test_core_catalog_and_player_alias(self):
        folder = builder.OUT
        mapping = json.loads((folder / 'SKSE/Plugins/Tessera/Completionist/tessera.json').read_text())
        records, fields, pairs = self.validate(folder, mapping)
        catalog = json.loads((ROOT / 'data/cards.json').read_text())['cards']
        self.assertEqual(set(pairs), {'tessera:' + c['id'] for c in catalog if c['roster_status'] != 'pending'})
        self.assertEqual(len(pairs), 674)
        self.assertEqual(len(mapping['pages']), 20)
        self.assertEqual(len(records), len(pairs) * 2 + 2)
        quest = len(builder.MASTERS) << 24 | 0x800
        f = dict(fields(quest))
        self.assertEqual(struct.unpack_from('<H', f[b'DNAM'])[0], 1)
        self.assertEqual(f[b'ALFR'], struct.pack('<I', 0x14))
        self.assertEqual((folder / 'Seq/Tessera Completionist.seq').read_bytes(), struct.pack('<I', quest))
        self.assertIn(b'Tessera_CompletionistQuest', f[b'VMAD'])
        self.assertIn(b'Tessera_CompletionistPlayer', f[b'VMAD'])

    def test_expansion_and_stable_identity(self):
        manifest = json.loads((ROOT / 'examples/my-cool-cards/SKSE/Plugins/Tessera/Expansions/my-cool-cards.json').read_text())
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            mapping = builder.build(manifest['cards'], manifest['setId'], manifest['name'], folder, core=False)
            self.validate(folder, mapping)
            before = json.loads((folder / 'card-ids.json').read_text())
            modified = [dict(c, name='Renamed ' + c['name'], tier=10) for c in reversed(manifest['cards'])]
            modified = modified[1:]
            modified.append(dict(manifest['cards'][0], id='brand-new-card'))
            builder.build(modified, manifest['setId'], manifest['name'], folder, core=False)
            after = json.loads((folder / 'card-ids.json').read_text())
            self.assertTrue(all(after[k] == v for k, v in before.items()))
            self.assertGreater(after['my-cool-cards:brand-new-card'], max(before.values()))

    def test_overflow_tier_splits_at_126(self):
        cards = [dict(id=f'card-{i}', name=f'Card {i}', tier=1) for i in range(130)]
        with tempfile.TemporaryDirectory() as d:
            mapping = builder.build(cards, 'big-set', 'Big Set', Path(d), core=False)
            self.validate(Path(d), mapping)
            self.assertEqual([len(p['entries']) for p in mapping['pages']], [126, 4, 126, 4])

    def test_mapping_does_not_reuse_retired_or_pending_ids(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'ids.json'
            original = builder.stable_ids(['set:old-card', 'set:pending-card'], path)
            current = builder.stable_ids(['set:new-card', 'set:pending-card'], path)
            self.assertEqual(current['set:old-card'], original['set:old-card'])
            self.assertEqual(current['set:pending-card'], original['set:pending-card'])
            self.assertGreater(current['set:new-card'], max(original.values()))

if __name__ == '__main__': unittest.main()
