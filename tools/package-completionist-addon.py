#!/usr/bin/env python3
"""Package the optional Completionist addon without changing the main mod."""
import argparse
import configparser
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import shutil
from package import write_archive
from runtime_files import ROOT, hashes

SOURCE = ROOT / 'addons/completionist'

def collect():
    names = ['Tessera Completionist.esp', 'Seq/Tessera Completionist.seq',
             'SKSE/Plugins/TesseraCompletionist.dll', 'README.md', 'meta.ini']
    for source in sorted((SOURCE / 'Scripts/Source').glob('*.psc')):
        names += [f'Scripts/{source.stem}.pex', f'Scripts/Source/{source.name}']
    for folder in ('SKSE/Plugins/CompletionistData', 'SKSE/Plugins/Tessera/Completionist'):
        names += [str(path.relative_to(SOURCE)) for path in sorted((SOURCE / folder).rglob('*')) if path.is_file()]
    files = {name: SOURCE / name for name in names}
    licenses = ['CommonLibSSE-NG-COPYING.txt', 'CommonLibSSE-NG-EXCEPTIONS.md', 'CommonLibSSE-NG-MIT.txt']
    licenses += ['native/' + name + '.txt' for name in ('nlohmann-json', 'spdlog', 'fmt', 'rapidcsv', 'hde64-Minhook', 'directxmath', 'directxtk')]
    for name in licenses:
        files['licenses/TesseraCompletionist/' + name] = ROOT / 'docs/licenses' / name
    return files

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', type=Path)
    args = parser.parse_args()
    files = collect()
    manifest = hashes(files)
    metadata = configparser.ConfigParser()
    metadata.read(SOURCE / 'meta.ini')
    version = '.'.join(metadata['General']['version'].split('.')[:3])
    archive = ROOT / f'dist/Tessera-Completionist-{version}.zip'
    write_archive(files, manifest, archive)
    print(f'Verified {archive} ({len(files)} files, {archive.stat().st_size:,} bytes)')
    if args.install:
        # Refuse to overwrite a module that the game may still have loaded.
        for command in Path('/proc').glob('[0-9]*/cmdline'):
            try: name = command.read_bytes().split(b'\0')[0].replace(b'\\', b'/').rsplit(b'/', 1)[-1].lower()
            except OSError: continue
            if name in (b'skyrimse.exe', b'skyrimvr.exe'): raise SystemExit('Close Skyrim before installing.')
        target = args.install.resolve()
        backup = ROOT / 'build/install-backups' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-completionist')
        for name, path in files.items():
            dest = target / name
            if dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest() != manifest[name]:
                saved = backup / name
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(dest, saved)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
        assert hashes({name: target / name for name in files}) == manifest
        print(f'Installed and verified {len(files)} files in {target}')
        if backup.exists(): print(f'Previous files: {backup}')

if __name__ == '__main__': main()
