#!/usr/bin/env python3
"""Check release inputs and print CI version metadata."""
import argparse
import os
from release_support import package_version, release_notes, source_files, source_state, verify_dependencies, verify_runtime_assets, version

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--github-output', action='store_true')
args = parser.parse_args()
release, state = version(), source_state()
tag = os.environ.get('GITHUB_REF_NAME') if os.environ.get('GITHUB_REF_TYPE') == 'tag' else None
if tag and state['dirty']:
    raise SystemExit('Tagged releases require a clean working tree')
label = package_version(release, state['commit'], tag)
release_notes(release)
source_files()
count = verify_dependencies()
assets = verify_runtime_assets()
print(f'Verified {count} dependency files and {assets} runtime inputs; package {label}')
if args.github_output:
    values = {'version': release, 'package_version': label, 'commit': state['commit'],
              'prerelease': str('-' in release).lower()}
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
        for key, value in values.items():
            stream.write(f'{key}={value}\n')
