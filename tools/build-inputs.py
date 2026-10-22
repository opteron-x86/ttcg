#!/usr/bin/env python3
"""Record compiler versions, pinned dependencies and public source hashes."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path
from release_support import ROOT, digest, source_files, source_state


def capture():
    tools = {}
    for name in ('clang-cl', 'lld-link', 'cmake', 'ninja', 'python3', 'git'):
        path = shutil.which(name)
        if path:
            result = subprocess.run([path, '--version'], text=True, capture_output=True)
            lines = (result.stdout + result.stderr).splitlines()
            tools[name] = {'sha256': digest(Path(path).resolve()), 'version': lines[0] if lines else ''}
    return {'source': source_state(), 'tools': tools,
            'dependencies': json.loads((ROOT / 'cmake/dependencies.json').read_text(encoding='utf-8')),
            'files': {name: digest(path) for name, path in source_files().items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/build-inputs.json')
    parser.add_argument('--against', type=Path)
    args = parser.parse_args()
    current = capture()
    if args.against:
        if json.loads(args.against.read_text(encoding='utf-8')) != current:
            raise SystemExit('Build inputs differ')
        print('Build inputs match')
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(current, indent=2) + '\n', encoding='utf-8')
        print(args.output)


if __name__ == '__main__':
    main()
