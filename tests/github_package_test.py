"""GitHub packages keep card illustrations in the Nexus download."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from release_support import check_public_files, package_version, source_files, verify_runtime_assets
spec = importlib.util.spec_from_file_location('github_package', ROOT / 'tools/package-release.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class GitHubPackageTests(unittest.TestCase):
    def test_art_boundary(self):
        for name in ('TesseraUI/art/cards/Card.webp', 'TesseraUI/art/thumbs/Card.webp',
                     'source/examples/card-expansion/TesseraUI/art/expansions/sample/a.png',
                     'source/docs/card-art-prompts/output/image.png'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                check_public_files([name])
        check_public_files(['TesseraUI/art/backs/mosaic.webp', 'TesseraUI/assets/themes/paper.png'])
        check_public_files(source_files())

    def test_archive_rejects_art_before_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            file = root / 'file'
            file.write_bytes(b'fixture')
            output = root / 'release.zip'
            with self.assertRaises(ValueError):
                package.archive(output, {'TesseraUI/art/cards/Card.webp': file}, 0)
            self.assertFalse(output.exists())
            package.archive(output, {'TesseraUI/art/backs/mosaic.webp': file}, 0)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read('TesseraUI/art/backs/mosaic.webp'), b'fixture')

    def test_main_contains_backs_and_runtime(self):
        from release_files import collect_release
        with tempfile.TemporaryDirectory() as temporary:
            dll = Path(temporary) / 'TTCG.dll'
            dll.write_bytes(b'package selection fixture')
            main, art = collect_release(artwork=False, dll=dll)
            self.assertFalse(art)
            self.assertIn('Tessera TCG.esp', main)
            self.assertIn('TesseraUI/art/backs/mosaic.webp', main)
            self.assertIs(main['SKSE/Plugins/TTCG.dll'], dll)
            check_public_files(main)

    def test_version_and_assets(self):
        self.assertEqual(package_version('0.32.6', 'a' * 40, 'v0.32.6'), '0.32.6')
        with self.assertRaises(ValueError):
            package_version('0.32.6', 'a' * 40, 'v0.32.5')
        self.assertGreater(verify_runtime_assets(), 20)


if __name__ == '__main__':
    unittest.main()
