#!/usr/bin/env python3
"""Package and optionally install only the files owned by the Missives addon."""
import argparse
import configparser
import hashlib
import importlib.util
from pathlib import Path
import shutil
from datetime import datetime, timezone
from runtime_files import ROOT, hashes

spec=importlib.util.spec_from_file_location('release_package',ROOT/'tools/package.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
source=ROOT/'addons/missives'

def collect():
    names=['Tessera Missives.esp','Seq/Tessera Missives.seq','README.md','meta.ini']
    for p in sorted((source/'Scripts/Source').glob('*.psc')):
        names += [f'Scripts/{p.stem}.pex',f'Scripts/Source/{p.name}']
    return {name:source/name for name in names}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--install',type=Path);args=parser.parse_args()
    files=collect();manifest=hashes(files)
    metadata=configparser.ConfigParser();metadata.read(source/'meta.ini')
    version='.'.join(metadata['General']['version'].split('.')[:3])
    archive=ROOT/f'dist/Tessera-Missives-{version}.zip'
    package.write_archive(files,manifest,archive);print(f'Verified {archive} ({len(files)} files)')
    if args.install:
        target=args.install.resolve();target.mkdir(parents=True,exist_ok=True)
        backup=ROOT/'build/install-backups'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-missives')
        for name,p in files.items():
            dest=target/name
            if dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest()!=manifest[name]:
                saved=backup/name;saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dest,saved)
            dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
        assert hashes({name:target/name for name in files})==manifest
        print(f'Installed and verified {len(files)} files in {target}')
        if backup.exists():print(f'Previous files: {backup}')

if __name__=='__main__':main()
