"""Release boundaries, stable art revisions and verified archive writes."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from release_files import collect_release, ui_files, safe_relative
from runtime_files import hashes
from pack_assets import pack_runtime_files
spec = importlib.util.spec_from_file_location('package_release', ROOT / 'tools/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class ReleaseTests(unittest.TestCase):
    def test_actual_release_is_runtime_only(self):
        main, art = collect_release()
        self.assertFalse(main.keys() & art.keys())
        self.assertIn('SKSE/Plugins/TTCG.dll', main)
        self.assertIn('Tessera TCG.esp', main)
        self.assertEqual({name for name in main if name.startswith('Music/')}, {'Music/TTCG/RPG Medieval Animated Music.xwm'})
        self.assertIn('Scripts/TTCG_CardExpansion.pex', main)
        self.assertIn('Scripts/Tessera_Collection.pex', main)
        self.assertFalse(any('meridian' in name.lower() for name in main.keys() | art.keys()))
        self.assertTrue(pack_runtime_files(ROOT).keys() <= main.keys())
        self.assertFalse(pack_runtime_files(ROOT).keys() & art.keys())
        self.assertIn('TesseraUI/rules.json', main)
        self.assertIn('TesseraUI/fonts/LiberationSerif-Regular.ttf', main)
        self.assertIn('TesseraUI/icons/affinity-4.png', main)
        self.assertFalse(any(Path(n).suffix in {'.html', '.js', '.css'} for n in main))
        for back in ('mosaic', 'dragon', 'celestial'):
            self.assertIn(f'TesseraUI/art/backs/{back}.webp', main)
        self.assertTrue(all(Path(name).suffix == '.webp' for name in main if name.startswith('TesseraUI/art/')))
        for name in tuple(f'assets/themes/{t}.png' for t in ('fine', 'paper', 'leather', 'wood', 'slate')):
            self.assertIn('TesseraUI/'+name, main)
            self.assertNotIn('TesseraUI/'+name, art)
        self.assertNotIn('TesseraUI/pack-models.js', main)
        for name in ('assets/packs/standard.png','assets/packs/tournament.png'):
            self.assertIn('TesseraUI/'+name, main)
            self.assertNotIn('TesseraUI/'+name, art)
        self.assertNotIn('SKSE/Plugins/TTCG.ini', main)
        self.assertNotIn('SKSE/Plugins/TTCG.ini.example', main)
        for name in main:
            self.assertNotIn(Path(name).parts[0], {'docs', 'data', 'tests', 'tools', 'build'})
            self.assertNotIn(Path(name).suffix, {'.psc', '.py', '.map'})
            self.assertNotIn('prompt', name.lower())
        self.assertTrue(any(name.startswith('licenses/') for name in main))
        voices = json.loads((ROOT / 'data/voice-manifest.json').read_text())
        self.assertEqual({v['destination'] for v in voices}, {name for name in main if name.startswith('Sound/Voice/')})
        cards = json.loads((ROOT / 'data/cards.json').read_text())['cards']
        self.assertEqual(set(art), {f'TesseraUI/art/{folder}/{c["name"]}.{ext}'
                                   for c in cards for folder, ext in [('cards', 'webp'), ('thumbs', 'webp')]})

    def test_native_resource_selection_and_failures(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ui_files(ROOT):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'resource')
            (root / 'TesseraUI/authoring.html').write_text('do not ship')
            self.assertEqual(set(ui_files(root)), set(ui_files(ROOT)))
            (root / 'TesseraUI/icons/affinity-0.png').unlink()
            with self.assertRaisesRegex(FileNotFoundError, 'Missing native UI resource'):
                ui_files(root)

    def test_art_revision_is_independent_and_archives_repeatable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, illustration = root / 'code', root / 'art'
            source.write_bytes(b'plugin v1')
            illustration.write_bytes(b'illustration')
            main, art = {'SKSE/Plugins/TTCG.dll': source}, {'TesseraUI/art/cards/Test.png': illustration}
            first = package.package(main, art, '1.0', root / 'dist', root / 'manifests')
            again = package.package(main, art, '1.0', root / 'dist', root / 'manifests')
            self.assertEqual(first, again)
            source.write_bytes(b'plugin v2')
            second = package.package(main, art, '2.0', root / 'dist', root / 'manifests')
            self.assertEqual(first['artRevision'], second['artRevision'])
            self.assertEqual(first['archives']['art'], second['archives']['art'])
            self.assertNotEqual(first['archives']['main']['sha256'], second['archives']['main']['sha256'])
            illustration.write_bytes(b'new illustration')
            self.assertNotEqual(first['artRevision'], package.content_revision(hashes(art)))
            for kind in ['main', 'art']:
                with zipfile.ZipFile(root / 'dist' / second['archives'][kind]['file']) as archive:
                    self.assertEqual(set(archive.namelist()), set(second['archives'][kind]['files']))
                    for name in archive.namelist():
                        self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(), second['archives'][kind]['files'][name])

    def test_failed_verification_keeps_previous_archive(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, target = root / 'source', root / 'release.zip'
            source.write_bytes(b'old build')
            inputs = {'runtime.bin': source}
            expected = hashes(inputs)
            target.write_bytes(b'previous release')
            source.write_bytes(b'changed while packaging')
            with self.assertRaisesRegex(RuntimeError, 'verification failed'):
                package.write_archive(inputs, expected, target)
            self.assertEqual(target.read_bytes(), b'previous release')
            self.assertFalse(list(root.glob('*.tmp')))
            for name in ('../escape', '/absolute', 'folder/../../escape', 'folder\\escape'):
                with self.assertRaises(ValueError):
                    safe_relative(name)
            with self.assertRaises(ValueError):
                package.package(inputs, inputs, '1', root, root)


if __name__ == '__main__':
    unittest.main()
