#!/usr/bin/env python3
"""Install the complete built mod into a chosen mod directory."""
import argparse
import json
from pathlib import Path
import shutil
import os
import tempfile
from datetime import datetime, timezone

from runtime_files import ROOT, collect_runtime, hashes


def install(files, target, manifest_path, seed_ini=None):
    target = target.resolve()
    expected = hashes(files)
    previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    owned = previous.get(str(target), {})
    for name in set(files) | set(owned):
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not (target / path).resolve().is_relative_to(target):
            raise ValueError(f'Unsafe runtime path: {name}')
    backup = manifest_path.parent / 'backups' / ('install-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    changes = [(name, source) for name, source in files.items()
               if not (target/name).is_file() or hashes({name:target/name})[name] != expected[name]]
    removals = [name for name in owned.keys()-files.keys()
                if (target/name).is_file() and hashes({name:target/name})[name] == owned[name]]
    touched=[]
    try:
        for name, source in changes + [(name,None) for name in removals]:
            destination=target/name
            if destination.exists():
                saved=backup/name;saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(destination,saved)
            touched.append(name)
            if source is None:destination.unlink();continue
            destination.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=destination.parent,delete=False) as f:temp=Path(f.name)
            try:
                shutil.copy2(source,temp);temp.replace(destination)
            finally:temp.unlink(missing_ok=True)
    except Exception:
        for name in reversed(touched):
            destination=target/name;saved=backup/name
            if saved.exists():shutil.copy2(saved,destination)
            else:destination.unlink(missing_ok=True)
        raise
    ini = target / 'SKSE/Plugins/TTCG.ini'
    if seed_ini and seed_ini.is_file() and not ini.exists():
        ini.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(seed_ini, ini)
    actual = hashes({name: target / name for name in files})
    if actual != expected:
        raise RuntimeError('Installed files do not match the build')
    previous[str(target)] = expected
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    temporary=manifest_path.with_suffix('.tmp')
    temporary.write_text(json.dumps(previous, indent=2) + '\n')
    temporary.replace(manifest_path)
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, required=True)
    args = parser.parse_args()
    target = args.target.resolve()
    if target == ROOT or target in ROOT.parents:
        parser.error('Choose a runtime directory separate from the source')
    files = collect_runtime()
    count = install(files, target, ROOT / 'build/install-manifest.json', ROOT / 'SKSE/Plugins/TTCG.ini')
    print(f'Installed and hash-verified {count} files: {target}')


if __name__ == '__main__':
    main()
