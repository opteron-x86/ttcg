#!/usr/bin/env python3
"""Exercise install ownership, user configuration and failure preflight."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
spec = importlib.util.spec_from_file_location('install_mod', Path(sys.path[0]) / 'install-mod.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class InstallerTests(unittest.TestCase):
    def test_install_update_and_preserve_user_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source'
            source.write_text('first build')
            seed = root / 'seed.ini'
            seed.write_text('Enabled=0')
            target, manifest = root / 'mod', root / 'manifest.json'
            module.install({'old.bin': source, 'modified.bin': source}, target, manifest, seed)
            (target / 'SKSE/Plugins/TTCG.ini').write_text('Enabled=1')
            (target / 'custom.webp').write_text('user artwork')
            (target / 'modified.bin').write_text('user edit')
            source.write_text('second build')
            module.install({'new.bin': source}, target, manifest, seed)
            self.assertFalse((target / 'old.bin').exists())
            self.assertEqual((target / 'modified.bin').read_text(), 'user edit')
            self.assertEqual((target / 'new.bin').read_text(), 'second build')
            self.assertEqual((target / 'custom.webp').read_text(), 'user artwork')
            self.assertEqual((target / 'SKSE/Plugins/TTCG.ini').read_text(), 'Enabled=1')

    def test_missing_input_does_not_change_runtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / 'mod'
            target.mkdir()
            (target / 'current.bin').write_text('installed')
            source = root / 'source'
            source.write_text('replacement')
            with self.assertRaises(FileNotFoundError):
                module.install({'current.bin': source, 'missing.bin': root / 'missing'},
                               target, root / 'manifest.json')
            self.assertEqual((target / 'current.bin').read_text(), 'installed')

    def test_supported_art_formats_and_thumbnail_fallback(self):
        from runtime_files import validate_runtime_art
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            faces = root / 'TesseraUI/art/cards'
            backs = root / 'TesseraUI/art/backs'
            faces.mkdir(parents=True)
            backs.mkdir()
            (backs / 'MOSAIC.WebP').write_bytes(b'fixture')
            for name, extension in [('Nocturnal', 'JPG'), ('Azura', 'png'),
                                    ('Namira', 'jpeg'), ('Mara', 'webp')]:
                (faces / f'{name.lower()}.{extension}').write_bytes(b'fixture')
            cards = [{'name': name} for name in ['Nocturnal', 'Azura', 'Namira', 'Mara']]
            validate_runtime_art(root, cards)  # No thumbnails: use the full faces.
            (faces / 'nocturnal.JPG').unlink()
            (faces / 'Nocturnal.gif').write_bytes(b'unsupported')
            with self.assertRaisesRegex(FileNotFoundError, 'Nocturnal'):
                validate_runtime_art(root, cards)

    def test_path_escape_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source'
            source.write_text('build')
            with self.assertRaises(ValueError):
                module.install({'../escape': source}, root / 'mod', root / 'manifest.json')
            self.assertFalse((root / 'escape').exists())


if __name__ == '__main__':
    unittest.main()
