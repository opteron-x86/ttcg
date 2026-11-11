"""Check the boundary between tested GitHub artifacts and Nexus publishing."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('nexus_upload', ROOT / 'tools/prepare-nexus-upload.py')
nexus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nexus)


class NexusUpload(unittest.TestCase):
    def setUp(self):
        self.commit = 'a' * 40
        self.tag = 'v0.33.0'
        self.release = {'tagName': self.tag, 'isDraft': False, 'isPrerelease': False}
        self.build = {
            'version': '0.33.0', 'package_version': '0.33.0', 'workflow_run': '123',
            'source': {'commit': self.commit, 'dirty': False, 'timestamp': 100},
            'dll_sha256': hashlib.sha256(b'tested DLL').hexdigest(),
        }
        self.build['windows_tests'] = {
            'passed': True, 'source': copy.deepcopy(self.build['source']), 'version': '0.33.0',
            'dll_sha256': self.build['dll_sha256'],
        }
        self.run = {
            'id': 123, 'path': '.github/workflows/build.yml', 'event': 'push',
            'head_sha': self.commit, 'head_branch': self.tag, 'status': 'completed', 'conclusion': 'success',
        }

    def validate(self, release=None, build=None, run=None):
        nexus.validate_release(self.tag, release or self.release, build or self.build, self.commit, run or self.run)

    def test_accepts_successful_standard_release(self):
        self.validate()

    def test_rejects_development_and_prerelease_tags(self):
        for tag in ('master', 'v0.33.0-rc.1', 'v0.33.0-dev.abc', '--help', 'v0.33.0\n'):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                nexus.release_version(tag)

    def test_requires_published_matching_release(self):
        for change in ({'isDraft': True}, {'isPrerelease': True}, {'tagName': 'v0.32.0'}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(release={**self.release, **change})

    def test_requires_successful_tagged_build_workflow(self):
        changes = ({'id': 999}, {'path': '.github/workflows/nexus.yml'}, {'event': 'workflow_dispatch'},
                   {'head_sha': 'b' * 40}, {'head_branch': 'master'}, {'status': 'in_progress'},
                   {'conclusion': 'failure'})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(run={**self.run, **change})

    def test_requires_matching_source_and_windows_validation(self):
        changes = ({'version': '0.32.0'}, {'package_version': '0.33.0-dev.abc'},
                   {'source': {**self.build['source'], 'dirty': True}},
                   {'source': {**self.build['source'], 'commit': 'b' * 40}},
                   {'dll_sha256': 'wrong'}, {'windows_tests': {}})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(build={**self.build, **change})
        for change in ({'passed': False}, {'version': '0.32.0'}, {'source': {}}, {'dll_sha256': 'wrong'}):
            build = copy.deepcopy(self.build)
            build['windows_tests'].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(build=build)

    def download_fixture(self, directory, dll=b'tested DLL'):
        filename = 'Tessera-0.33.0-Main.zip'
        with zipfile.ZipFile(directory / filename, 'w') as archive:
            archive.writestr('SKSE/Plugins/TTCG.dll', dll)
        self.build['archives'] = {filename: {'sha256': hashlib.sha256((directory / filename).read_bytes()).hexdigest()}}
        (directory / 'build-info.json').write_text(json.dumps(self.build), encoding='utf-8')
        (directory / 'release-notes.md').write_text('Release notes\n', encoding='utf-8')
        sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(directory.iterdir()))
        (directory / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')
        return directory / filename

    def test_verifies_runtime_zip_and_metadata_checksums(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            package = self.download_fixture(directory)
            self.assertEqual(nexus.verify_downloads(directory, '0.33.0', self.build), package)
            for name in (package.name, 'build-info.json', 'release-notes.md'):
                path = directory / name
                original = path.read_bytes()
                path.write_bytes(original + b'changed')
                with self.subTest(file=name), self.assertRaises(ValueError):
                    nexus.verify_downloads(directory, '0.33.0', self.build)
                path.write_bytes(original)

    def test_rejects_zip_with_a_different_dll_even_when_zip_checksum_matches(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.download_fixture(directory, b'untested DLL')
            with self.assertRaisesRegex(ValueError, 'tested DLL'):
                nexus.verify_downloads(directory, '0.33.0', self.build)

    def test_rejects_zip_not_recorded_in_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.download_fixture(directory)
            self.build['archives']['Tessera-0.33.0-Main.zip']['sha256'] = 'wrong'
            with self.assertRaisesRegex(ValueError, 'recorded build artifact'):
                nexus.verify_downloads(directory, '0.33.0', self.build)


if __name__ == '__main__':
    unittest.main()
