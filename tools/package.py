#!/usr/bin/env python3
"""Build separate runtime and card-art downloads, with no development artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import zipfile

from release_files import collect_release, safe_relative
from runtime_files import ROOT, hashes


def content_revision(manifest):
    return hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode()).hexdigest()[:12]


def write_archive(files, expected, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, suffix='.zip.tmp', delete=False) as stream:
        temporary = Path(stream.name)
    try:
        with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as archive:
            for name, path in sorted(files.items()):
                safe_relative(name)
                info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = 0o644 << 16
                archive.writestr(info, path.read_bytes())
        with zipfile.ZipFile(temporary) as archive:
            if set(archive.namelist()) != set(expected):
                raise RuntimeError('Archive file list verification failed')
            for name, digest in expected.items():
                if hashlib.sha256(archive.read(name)).hexdigest() != digest:
                    raise RuntimeError(f'Archive content verification failed: {name}')
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def package(main, art, version, output, manifests):
    if main.keys() & art.keys():
        raise ValueError('Main and art downloads overlap')
    expected = {'main': hashes(main), 'art': hashes(art)}
    revision = content_revision(expected['art'])
    paths = {'main': output / f'Tessera-{version}-Main.zip',
             'art': output / f'Tessera-Card-Art-{revision}.zip'}
    for kind, files in (('main', main), ('art', art)):
        print(f'Packaging {kind}: {len(files)} files', flush=True)
        write_archive(files, expected[kind], paths[kind])
        print(f'Verified {paths[kind]} ({paths[kind].stat().st_size:,} bytes)', flush=True)
    manifests.mkdir(parents=True, exist_ok=True)
    report = {'version': version, 'artRevision': revision, 'archives': {}}
    for kind, path in paths.items():
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        report['archives'][kind] = {'file': path.name, 'bytes': path.stat().st_size,
                                  'sha256': digest, 'files': expected[kind]}
    (manifests / 'release-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    runtime, art = collect_release()
    version = json.loads((ROOT / 'vcpkg.json').read_text())['version-semver']
    package(runtime, art, version, args.output, ROOT / 'build/package')


if __name__ == '__main__':
    main()
