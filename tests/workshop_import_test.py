#!/usr/bin/env python3
"""Exercise workshop imports and failure recovery against disposable catalogs."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import workshop_import as importer
from card_catalog import card_outputs, creature_groups_output


class WorkshopImportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.audit = json.loads((ROOT / 'docs/catalog-audit-data.json').read_text())
        cls.live = json.loads((ROOT / 'data/cards.json').read_text())

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.catalog = copy.deepcopy(self.live)
        self.catalog['cards'] = [copy.deepcopy(c) for c in self.live['cards'] if c['tier'] == 1][:18]
        self.first, self.second = self.catalog['cards'][:2]
        self.registry = {c['id']: c['form_id'] for c in self.catalog['cards']}
        self.write('data/cards.json', self.catalog)
        self.write('data/released-card-ids.json', self.registry)
        self.write('data/opponents.json', [{'name': 'Hulda', 'skillName': 'Beginner'}])
        self.write('data/opponent-collections.json', {'opponents': [{'name': 'Hulda', 'cards': list(self.registry.values())}]})
        self.write('data/rank-policy.json', importer.POLICY)
        self.write('data/workshop-roster.json', {})
        for path in ('include/Album.h', 'include/Tournaments.h', 'include/Match.h', 'include/Campaign.h', 'include/Shop.h'):
            self.write(path, '// fixture\n')
        for path, text in card_outputs(self.catalog, self.registry).items():
            self.write(path, text)
        self.write('include/CreatureGroups.h', creature_groups_output(self.catalog['cards']))
        data = copy.deepcopy(self.audit)
        data['sources'] = {p: importer.file_hash(self.root / p) for p in data['sources']}
        data['fingerprint'] = hashlib.sha256(json.dumps(data['sources'], sort_keys=True).encode()).hexdigest()
        data['templates'] = [t for t in data['templates'] if t['group'] in ('Dark Elf', 'Argonian')]
        data['cards'] = [c for c in data['cards'] if c['id'] in self.registry]
        self.data = data
        self.write('docs/catalog-template-builder.html', '<meta name="workshop-save-api" content="">'
                   '<script type="application/json" id="builder-data">' + json.dumps(data) + '</script>')
        self.group = self.make_group('Dark Elf')
        self.group['slots'] = self.group['slots'][:1]
        self.group['slots'][0]['assignment'] = {'cardId': self.first['id'], 'name': 'Renamed Mudcrab'}
        self.draft = self.root / 'draft.json'
        self.save_draft()
        for c in (self.first, self.second):
            self.art(str(importer.ART / 'cards' / (c['name'] + '.webp')), 'red')
        self.art('source/Fresh Illustration.png', 'blue')
        self.art(str(importer.ART / 'backs/mosaic.webp'), 'darkgreen')

    def write(self, relative, value):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value if isinstance(value, bytes) else value.encode() if isinstance(value, str) else importer.json_bytes(value))
        return path

    def art(self, relative, color):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        Image.new('RGB', (48, 72), color).save(path)
        return path

    def make_group(self, name):
        t = next(t for t in self.data['templates'] if t['group'] == name)
        return {'kind': 'tessera-catalog-template', 'schema': 2, 'catalogFingerprint': self.data['fingerprint'],
                'sideOrder': self.data['policy']['side_order'],
                'template': {k: t[k] for k in ('id', 'revision', 'group', 'label')},
                'slots': [dict(copy.deepcopy(s), assignment=None, confirmed=False) for s in t['slots']]}

    def save_draft(self, doc=None):
        self.write('draft.json', doc or self.group)

    def new_card(self, name='Fresh Illustration', tier=1):
        t = next(t for t in self.data['templates'] if t['group'] == 'Dark Elf')
        slot = next(s for s in t['slots'] if s['tier'] == tier)
        self.group['slots'] = [dict(copy.deepcopy(slot), confirmed=False,
                                    assignment={'kind': 'new', 'cardId': 'new-' + 'a' * 32, 'name': name})]
        self.save_draft()

    def plan(self, **kwargs):
        return importer.Plan(self.root, self.draft, **({'accept_unconfirmed': True} | kwargs))

    def snapshot(self):
        return {str(p.relative_to(self.root)): p.read_bytes() for d in ('data', 'include', 'TesseraUI', 'reference')
                for p in (self.root / d).rglob('*') if p.is_file()}

    def test_preview_preserves_every_source_and_reports_diff(self):
        before = self.snapshot()
        plan = self.plan()
        plan.write_report(self.root / 'preview')
        self.assertEqual(before, self.snapshot())
        self.assertFalse(plan.errors)
        self.assertEqual(plan.report['summary']['updated'], 1)
        self.assertEqual(len(plan.report['unplaced']), 17)
        c = plan.catalog['cards'][0]
        self.assertEqual((c['id'], c['form_id']), (self.first['id'], self.first['form_id']))
        self.assertEqual(c['acquisition'], self.first['acquisition'])
        self.assertEqual(c['groups'], ['Dark Elf'])
        self.assertEqual(plan.catalog['cards'][1:], self.catalog['cards'][1:])
        self.assertEqual(plan.report['changes'][0]['art']['status'], 'rename')

    def test_new_card_apply_reimport_and_restore(self):
        self.new_card()
        before = self.snapshot()
        plan = self.plan(art_sources=[self.root / 'source'])
        self.assertFalse(plan.errors)
        backup = importer.apply_plan(plan)
        card = json.loads((self.root / 'data/cards.json').read_text())['cards'][-1]
        self.assertEqual(card['id'], 'workshop-' + 'a' * 32)
        self.assertGreaterEqual(card['form_id'], 0x400000)
        self.assertEqual(card['acquisition'], {'starter': False, 'available': True, 'special': False, 'opponents': [], 'route': 'Standard play'})
        self.assertTrue((self.root / importer.ART / 'cards/Fresh Illustration.webp').exists())
        self.assertTrue((self.root / importer.ART / 'thumbs/Fresh Illustration.webp').exists())
        self.assertEqual((self.root / card['source_art']).read_bytes(), (self.root / 'source/Fresh Illustration.png').read_bytes())
        for path, text in card_outputs(plan.catalog, plan.registry).items():
            self.assertEqual((self.root / path).read_text(), text)
        self.assertEqual((self.root / 'include/CreatureGroups.h').read_text(), creature_groups_output(plan.catalog['cards']))
        again = self.plan(art_sources=[self.root / 'source'])
        self.assertFalse(again.errors)
        self.assertIsNone(importer.apply_plan(again))
        self.assertEqual(again.report['summary']['new'], 0)
        importer.restore_import(self.root, backup)
        self.assertEqual(before, self.snapshot())
        self.assertFalse((self.root / importer.STATE).exists())

    def test_new_unique_defaults_to_reserved_reward(self):
        self.new_card(tier=8)
        plan = self.plan(art_sources=[self.root / 'source'])
        self.assertFalse(plan.errors)
        card = plan.catalog['cards'][-1]
        self.assertTrue(card['unique'])
        self.assertFalse(card['acquisition']['available'])

    def test_explicit_placeholder_apply_repeat_and_later_source_replacement(self):
        self.new_card('Unpainted Card')
        self.assertTrue(self.plan().errors)
        plan = self.plan(placeholder_missing_art=True)
        self.assertFalse(plan.errors)
        self.assertEqual(plan.report['summary']['placeholders'], 1)
        importer.apply_plan(plan)
        card = json.loads((self.root / 'data/cards.json').read_text())['cards'][-1]
        face = self.root / importer.ART / 'cards/Unpainted Card.webp'
        self.assertEqual(card['art_status'], 'placeholder')
        self.assertEqual(card['placeholder_art_sha256'], importer.file_hash(face))
        self.assertIsNone(importer.apply_plan(self.plan()))
        self.art('source/Unpainted Card.png', 'purple')
        replacement = self.plan(art_sources=[self.root / 'source'])
        self.assertFalse(replacement.errors)
        importer.apply_plan(replacement)
        painted = json.loads((self.root / 'data/cards.json').read_text())['cards'][-1]
        self.assertEqual(painted['form_id'], card['form_id'])
        self.assertNotIn('art_status', painted)
        self.assertNotIn('placeholder_art_sha256', painted)
        self.assertNotEqual(importer.file_hash(face), card['placeholder_art_sha256'])

    def test_external_importer_replacement_clears_placeholder_metadata(self):
        self.new_card('Unpainted Card')
        importer.apply_plan(self.plan(placeholder_missing_art=True))
        self.art(str(importer.ART / 'cards/Unpainted Card.webp'), 'purple')
        replacement = self.plan()
        self.assertFalse(replacement.errors)
        self.assertEqual(replacement.report['changes'][0]['art']['status'], 'adopt-installed')
        importer.apply_plan(replacement)
        painted = json.loads((self.root / 'data/cards.json').read_text())['cards'][-1]
        self.assertNotIn('art_status', painted)
        self.assertIsNone(importer.apply_plan(self.plan()))

    def test_placeholder_opt_in_does_not_hide_ambiguous_or_invalid_art(self):
        self.new_card()
        self.art('source/Fresh Illustration.jpg', 'green')
        self.assertIn('multiple source images', ' '.join(self.plan(art_sources=[self.root / 'source'], placeholder_missing_art=True).errors))
        mapping = self.write('map.json', {'Fresh Illustration': 'source/broken.png'})
        self.write('source/broken.png', b'broken image')
        self.assertIn('unreadable artwork', ' '.join(self.plan(art_map=mapping, placeholder_missing_art=True).errors))

    def test_rename_preserves_custom_runtime_art_over_source_match(self):
        self.art('source/Renamed Mudcrab.png', 'blue')
        self.art(str(importer.ART / 'cards' / (self.first['name'] + '.png')), 'green')
        plan = self.plan(art_sources=[self.root / 'source'])
        self.assertTrue(plan.report['changes'][0]['art']['source'].endswith('.png'))
        importer.apply_plan(plan)
        with Image.open(self.root / importer.ART / 'cards/Renamed Mudcrab.webp') as im:
            pixel = im.getpixel((10, 10))
            self.assertGreater(pixel[1], pixel[0] + 80)
        self.assertTrue((self.root / importer.ART / 'cards' / (self.first['name'] + '.png')).exists())

    def test_art_mapping_replaces_all_shadowing_formats_and_undo_restores_them(self):
        self.group['slots'][0]['assignment']['name'] = self.first['name']
        self.save_draft()
        self.art(str(importer.ART / 'cards' / (self.first['name'] + '.png')), 'green')
        mapping = self.write('map.json', {self.first['id']: 'source/Fresh Illustration.png'})
        before = self.snapshot()
        plan = self.plan(art_map=mapping)
        backup = importer.apply_plan(plan)
        self.assertFalse((self.root / importer.ART / 'cards' / (self.first['name'] + '.png')).exists())
        self.assertIsNone(importer.apply_plan(self.plan(art_map=mapping)))
        importer.restore_import(self.root, backup)
        self.assertEqual(before, self.snapshot())

    def test_missing_or_ambiguous_art_blocks_all_writes(self):
        self.new_card()
        before = self.snapshot()
        plan = self.plan()
        self.assertIn('missing artwork', ' '.join(plan.errors))
        with self.assertRaises(ValueError):
            importer.apply_plan(plan)
        self.assertEqual(before, self.snapshot())
        self.art('source/nested/Fresh Illustration.jpg', 'red')
        plan = self.plan(art_sources=[self.root / 'source'])
        self.assertEqual(plan.report['changes'][0]['art']['status'], 'ambiguous')
        mapping = self.write('map.json', {'new-' + 'a' * 32: 'source/Fresh Illustration.png'})
        self.assertFalse(self.plan(art_sources=[self.root / 'source'], art_map=mapping).errors)

    def test_invalid_image_blocks_import(self):
        self.new_card()
        self.write('source/Fresh Illustration.png', b'broken image')
        plan = self.plan(art_sources=[self.root / 'source'])
        self.assertIn('unreadable artwork', ' '.join(plan.errors))

    def test_partial_groups_and_unconfirmed_need_explicit_flags(self):
        t = next(t for t in self.data['templates'] if t['group'] == 'Dark Elf')
        self.group['slots'].append(dict(copy.deepcopy(t['slots'][1]), assignment=None, confirmed=False))
        self.save_draft()
        errors = self.plan(accept_unconfirmed=False).errors
        self.assertTrue(any('empty slots' in e for e in errors))
        self.assertTrue(any('unconfirmed cards' in e for e in errors))
        self.assertFalse(self.plan(allow_incomplete=True).errors)

    def test_group_selection_shared_save_and_derived_export_fields(self):
        other = self.make_group('Argonian')
        other['slots'] = other['slots'][:1]
        other['slots'][0]['assignment'] = {'cardId': self.second['id'], 'name': self.second['name']}
        doc = {'kind': 'tessera-roster-workshop', 'schema': 2, 'catalogFingerprint': self.data['fingerprint'],
               'sideOrder': self.data['policy']['side_order'], 'groups': [self.group, other],
               'currentGroup': self.group['template']['id'], 'sharedSave': {'revision': 'saved-revision'}}
        # Computed export summaries are untrusted; only the editable slots are applied.
        self.group['entries'] = [{'after': {'name': 'Wrong name', 'sides': [10] * 4}}]
        self.save_draft(doc)
        plan = self.plan(groups=['Dark Elf'])
        self.assertFalse(plan.errors)
        self.assertEqual(plan.report['summary']['selected'], 1)
        self.assertEqual(plan.report['sharedSave']['revision'], 'saved-revision')
        self.assertEqual(plan.catalog['cards'][1], self.second)
        self.assertEqual(plan.catalog['cards'][0]['name'], 'Renamed Mudcrab')
        with self.assertRaisesRegex(ValueError, 'Unknown group'):
            self.plan(groups=['Dwemer'])

    def test_conflicting_and_unsafe_names_and_side_values_are_blocked(self):
        for name in (self.second['name'].upper(), '../unsafe', 'CON', 'Bad:Name'):
            with self.subTest(name=name):
                self.group['slots'][0]['assignment']['name'] = name
                self.save_draft()
                self.assertTrue(self.plan().errors)
        self.group['slots'][0]['assignment']['name'] = 'Okay Name'
        self.group['slots'][0]['sides'] = [10, 10, 10, 10]
        self.save_draft()
        self.assertTrue(any('side' in e for e in self.plan().errors))

    def test_bad_schema_fingerprint_side_order_and_registry(self):
        for key, value in (('schema', 1), ('catalogFingerprint', 'old'), ('sideOrder', ['left', 'top', 'right', 'bottom'])):
            doc = copy.deepcopy(self.group)
            doc[key] = value
            self.save_draft(doc)
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.plan()
        self.save_draft()
        registry = dict(self.registry)
        registry[self.first['id']] += 100
        self.write('data/released-card-ids.json', registry)
        with self.assertRaisesRegex(ValueError, 'registry mismatch'):
            self.plan()

    def test_stale_draft_or_concurrent_source_edit_is_rejected(self):
        plan = self.plan()
        self.write('include/Album.h', '// changed after preview\n')
        with self.assertRaisesRegex(ValueError, 'Input changed'):
            importer.apply_plan(plan)
        with self.assertRaisesRegex(ValueError, 'Catalog changed'):
            self.plan()

    def test_unexpected_new_art_variant_after_preview_blocks_apply(self):
        plan = self.plan()
        self.art(str(importer.ART / 'cards/Renamed Mudcrab.png'), 'blue')
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'Artwork variants changed'):
            importer.apply_plan(plan)
        self.assertEqual(before, self.snapshot())

    def test_successive_edits_keep_card_identity_and_detect_external_catalog_edits(self):
        self.new_card()
        importer.apply_plan(self.plan(art_sources=[self.root / 'source']))
        first = json.loads((self.root / 'data/cards.json').read_text())['cards'][-1]
        self.group['slots'][0]['assignment']['name'] = 'New Name'
        self.save_draft()
        importer.apply_plan(self.plan())
        catalog = json.loads((self.root / 'data/cards.json').read_text())
        second = catalog['cards'][-1]
        self.assertEqual((first['id'], first['form_id']), (second['id'], second['form_id']))
        self.assertEqual(second['name'], 'New Name')
        self.assertEqual(len(catalog['cards']), 19)
        catalog['cards'][0]['focus'] = '50% 50%'
        self.write('data/cards.json', catalog)
        with self.assertRaisesRegex(ValueError, 'Catalog changed'):
            self.plan()

    def test_edit_during_art_conversion_is_rejected(self):
        plan = self.plan()
        before = self.snapshot()
        convert = importer.prepare_images
        def change_draft(*args):
            convert(*args)
            self.write('draft.json', '{}')
        with patch.object(importer, 'prepare_images', side_effect=change_draft), self.assertRaisesRegex(ValueError, 'Input changed'):
            importer.apply_plan(plan)
        self.assertEqual(before, self.snapshot())

    def test_failed_install_rolls_back_catalog_and_images(self):
        plan = self.plan()
        before = self.snapshot()
        replace = importer.os.replace
        failed = False
        def fail_thumbnail(source, destination):
            nonlocal failed
            if not failed and Path(destination) == self.root / importer.ART / 'thumbs/Renamed Mudcrab.webp':
                failed = True
                raise OSError('simulated disk failure')
            return replace(source, destination)
        with patch.object(importer.os, 'replace', side_effect=fail_thumbnail), self.assertRaisesRegex(OSError, 'simulated'):
            importer.apply_plan(plan)
        self.assertTrue(failed)
        self.assertEqual(before, self.snapshot())
        manifest = next((self.root / importer.HISTORY).glob('*/manifest.json'))
        self.assertEqual(json.loads(manifest.read_text())['status'], 'rolled-back')

    def test_restore_refuses_changed_outputs_or_corrupt_backup(self):
        backup = importer.apply_plan(self.plan())
        after = self.snapshot()
        original_header = (self.root / 'include/Cards.h').read_bytes()
        self.write('include/Cards.h', '// somebody edited this\n')
        with self.assertRaisesRegex(ValueError, 'File changed since import'):
            importer.restore_import(self.root, backup)
        self.write('include/Cards.h', original_header)
        (backup / 'originals/data/cards.json').write_text('broken backup')
        with self.assertRaisesRegex(ValueError, 'Backup missing or damaged'):
            importer.restore_import(self.root, backup)
        self.assertEqual(after, self.snapshot())

    def test_interrupted_import_must_be_restored_before_new_apply(self):
        before = self.snapshot()
        backup = importer.apply_plan(self.plan())
        manifest = json.loads((backup / 'manifest.json').read_text())
        manifest['status'] = 'installing'
        (backup / 'manifest.json').write_bytes(importer.json_bytes(manifest))
        with self.assertRaisesRegex(ValueError, 'Interrupted import'):
            importer.apply_plan(self.plan())
        result = subprocess.run([sys.executable, str(ROOT / 'tools/import-roster-workshop.py'),
                                 str(self.draft), '--root', str(self.root)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('--restore', result.stderr)
        importer.restore_import(self.root, backup)
        self.assertEqual(before, self.snapshot())

    def test_linked_destination_and_unknown_art_map_do_not_write(self):
        plan = self.plan()
        with tempfile.TemporaryDirectory() as outside:
            target = self.root / 'include/Cards.h'
            target.unlink()
            other = Path(outside) / 'keep.h'
            other.write_text('keep')
            target.symlink_to(other)
            with self.assertRaisesRegex(ValueError, 'Linked destination|Input changed'):
                importer.apply_plan(plan)
            self.assertEqual(other.read_text(), 'keep')
        mapping = self.write('map.json', {'Typo card': 'source/Fresh Illustration.png'})
        with self.assertRaisesRegex(ValueError, 'keys outside this import'):
            self.plan(art_map=mapping)

    def test_report_cannot_overwrite_its_input_map(self):
        path = self.write('preview/missing-art-map.json', {self.first['id']: '../source/Fresh Illustration.png'})
        plan = self.plan(art_map=path)
        before = path.read_bytes()
        with self.assertRaisesRegex(ValueError, 'overwrite an input'):
            plan.write_report(path.parent)
        self.assertEqual(before, path.read_bytes())

    def test_cli_preview_and_apply_failure_exit_codes(self):
        command = [sys.executable, str(ROOT / 'tools/import-roster-workshop.py'), str(self.draft), '--root', str(self.root)]
        before = self.snapshot()
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        result = subprocess.run(command + ['--accept-unconfirmed'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(before, self.snapshot())
        self.new_card('Missing Art')
        result = subprocess.run(command + ['--accept-unconfirmed', '--apply'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(before, self.snapshot())

    def test_cli_successful_apply_and_restore(self):
        before = self.snapshot()
        command = [sys.executable, str(ROOT / 'tools/import-roster-workshop.py'), '--root', str(self.root)]
        result = subprocess.run(command + [str(self.draft), '--accept-unconfirmed', '--apply'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        backup = next((self.root / importer.HISTORY).glob('*/manifest.json')).parent
        result = subprocess.run(command + ['--restore', str(backup)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(before, self.snapshot())

    def test_generators_remain_byte_identical_for_current_catalog(self):
        registry = json.loads((ROOT / 'data/released-card-ids.json').read_text())
        outputs = card_outputs(self.live, registry)
        outputs['include/CreatureGroups.h'] = creature_groups_output(self.live['cards'])
        for path, text in outputs.items():
            self.assertEqual((ROOT / path).read_text(), text, path)


if __name__ == '__main__':
    unittest.main()
