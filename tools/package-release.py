#!/usr/bin/env python3
"""Package a tested Main download, source, symbols and collection API for GitHub."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile
from release_files import collect_release
from release_support import (ROOT, check_public_files, digest, package_version, release_notes,
                             source_files, source_state, verify_dependencies, verify_runtime_assets, version)


def validate_tests(report, dll, state, release):
    if report.get('passed') is not True or set(report.get('cases', {})) != {'metadata', 'plugin', 'lifetime', 'dx_state'}:
        raise ValueError('All Windows probes must pass before packaging')
    if any(case.get('returncode') != 0 for case in report['cases'].values()):
        raise ValueError('Windows probe failed')
    if report.get('dll_sha256') != digest(dll):
        raise ValueError('DLL differs from the tested binary')
    if report.get('source') != state or report.get('version') != release:
        raise ValueError('Validation belongs to a different source revision or version')


def archive(path, files, epoch):
    check_public_files(files)
    if path.exists():
        raise ValueError(f'Archive already exists: {path}')
    date = datetime.datetime.fromtimestamp(max(epoch, 315532800), datetime.timezone.utc)
    with zipfile.ZipFile(path, 'x', zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for name, file in sorted(files.items()):
            info = zipfile.ZipInfo(name, date.timetuple()[:6])
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (0o100755 if name.endswith('.sh') else 0o100644) << 16
            zipped.writestr(info, file.read_bytes())
    with zipfile.ZipFile(path) as zipped:
        if zipped.testzip() is not None or set(zipped.namelist()) != set(files):
            raise ValueError(f'Archive verification failed: {path}')
        for name, file in files.items():
            if hashlib.sha256(zipped.read(name)).hexdigest() != digest(file):
                raise ValueError(f'Archive file differs: {name}')
    return {'sha256': digest(path), 'bytes': path.stat().st_size, 'file_count': len(files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist/github')
    parser.add_argument('--validation', type=Path, required=True)
    parser.add_argument('--tag')
    args = parser.parse_args()
    build, output = args.build_dir.resolve(), args.output_dir.resolve()
    state, release = source_state(), version()
    label = package_version(release, state['commit'], args.tag)
    if state['dirty']:
        raise ValueError('Commit source changes before packaging')
    verify_dependencies()
    verify_runtime_assets()
    report = json.loads(args.validation.read_text(encoding='utf-8'))
    validate_tests(report, build / 'TTCG.dll', state, release)
    if not (build / 'TTCG.pdb').is_file():
        raise ValueError('Matching debug symbols are required: TTCG.pdb')
    if output.exists() and any(output.iterdir()):
        raise ValueError(f'Output directory must be empty: {output}')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='tessera-package-') as temporary:
        info = Path(temporary) / 'SOURCE-INFO.json'
        info.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')
        runtime, art = collect_release(artwork=False, dll=build / 'TTCG.dll')
        if art:
            raise ValueError('Card art is distributed through Nexus')
        runtime['TesseraUI/INSTALL.txt'] = ROOT / 'docs/INSTALL.txt'
        runtime['licenses/Tessera/LICENSE'] = ROOT / 'LICENSE'
        public_source = source_files()
        file_list = Path(temporary) / 'SOURCE-FILES.json'
        file_list.write_text(json.dumps(sorted(public_source), indent=2) + '\n', encoding='utf-8')
        sources = {f'Tessera-{label}-source/{name}': path for name, path in public_source.items()}
        sources[f'Tessera-{label}-source/SOURCE-INFO.json'] = info
        sources[f'Tessera-{label}-source/SOURCE-FILES.json'] = file_list
        api = {name: ROOT / name for name in (
            'docs/collection-api.md', 'docs/exchange-api.md', 'include/Tessera/CollectionAPI.h',
            'Scripts/Source/Tessera_Collection.psc', 'Scripts/Source/Tessera_Exchange.psc',
            'examples/collection-api/CollectionExample.cpp',
            'examples/collection-api/TesseraCollectionExample.psc', 'LICENSE')}
        sets = {f'Tessera-{label}-Main.zip': runtime,
                f'Tessera-{label}-source.zip': sources,
                f'Tessera-{label}-symbols.zip': {'TTCG.pdb': build / 'TTCG.pdb'},
                f'Tessera-{label}-api.zip': api}
        records = {}
        for name, files in sets.items():
            records[name] = archive(output / name, files, state['timestamp'])
            print(f'Verified {name}: {len(files)} files', flush=True)
    manifest = {'version': release, 'package_version': label, 'source': state,
                'workflow_run': os.environ.get('GITHUB_RUN_ID'), 'dll_sha256': report['dll_sha256'],
                'windows_tests': report, 'archives': records}
    (output / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    notes = release_notes(release)
    notes += '\nInstall Main and the separate [Card Art download on Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/193442?tab=files).\n'
    notes += '\nDownloads: Main, source, debug symbols, collection API and SHA-256 checksums.\n'
    (output / 'release-notes.md').write_text(notes, encoding='utf-8')
    sums = ''.join(f'{digest(p)}  {p.name}\n' for p in sorted(output.iterdir()) if p.is_file())
    (output / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')


if __name__ == '__main__':
    main()
