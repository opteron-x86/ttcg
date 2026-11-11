#!/usr/bin/env python3
"""Download and verify an existing GitHub release for the manual Nexus workflow."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import uuid
import zipfile


def release_version(tag):
    if not re.fullmatch(r'v\d+\.\d+\.\d+', tag):
        raise ValueError('Choose a standard release tag, such as v0.33.0')
    return tag[1:]


def validate_release(tag, release, build, commit, run):
    version = release_version(tag)
    if release.get('tagName') != tag or release.get('isDraft') is not False or release.get('isPrerelease') is not False:
        raise ValueError('Nexus uploads require a published standard release')
    if build.get('version') != version or build.get('package_version') != version:
        raise ValueError('Release package version does not match the requested tag')
    source = build.get('source', {})
    if not re.fullmatch(r'[0-9a-f]{40}', commit) or source.get('commit') != commit or source.get('dirty') is not False:
        raise ValueError('Release package does not match the clean tagged source')
    tests = build.get('windows_tests', {})
    if (tests.get('passed') is not True or tests.get('source') != source or tests.get('version') != version
            or tests.get('dll_sha256') != build.get('dll_sha256')):
        raise ValueError('Windows validation does not match the release package')
    if (str(run.get('id')) != str(build.get('workflow_run')) or run.get('path') != '.github/workflows/build.yml'
            or run.get('event') != 'push' or run.get('head_sha') != commit or run.get('head_branch') != tag
            or run.get('status') != 'completed' or run.get('conclusion') != 'success'):
        raise ValueError('The tagged build and test workflow has not completed successfully')


def verify_downloads(directory, version, build):
    filename = f'Tessera-{version}-Main.zip'
    sums = {}
    for line in (directory / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
        checksum, name = line.split('  ', 1)
        if name in sums or not re.fullmatch(r'[0-9a-f]{64}', checksum):
            raise ValueError('Malformed or duplicate release checksum')
        sums[name] = checksum
    for name in (filename, 'build-info.json', 'release-notes.md'):
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        if sums.get(name) != actual:
            raise ValueError(f'Release checksum mismatch: {name}')
    package = directory / filename
    if build.get('archives', {}).get(filename, {}).get('sha256') != sums[filename]:
        raise ValueError('Runtime ZIP differs from the recorded build artifact')
    with zipfile.ZipFile(package) as archive:
        if archive.testzip() is not None:
            raise ValueError('Runtime ZIP is corrupt')
        dll = archive.read('SKSE/Plugins/TTCG.dll')
        if hashlib.sha256(dll).hexdigest() != build.get('dll_sha256'):
            raise ValueError('Runtime ZIP does not contain the tested DLL')
    return package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--repo', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    version = release_version(args.tag)
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repo):
        raise ValueError('Invalid GitHub repository')
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError('Download directory must be empty')
    args.output.mkdir(parents=True, exist_ok=True)

    def gh_json(*arguments):
        return json.loads(subprocess.check_output(['gh', *arguments], text=True))

    release = gh_json('release', 'view', args.tag, '--repo', args.repo, '--json', 'tagName,isDraft,isPrerelease')
    if release.get('tagName') != args.tag or release.get('isDraft') is not False or release.get('isPrerelease') is not False:
        raise ValueError('Nexus uploads require a published standard release')
    commit = gh_json('api', f'repos/{args.repo}/commits/{args.tag}')['sha']
    subprocess.run(['gh', 'release', 'download', args.tag, '--repo', args.repo, '--dir', str(args.output),
                    '--pattern', f'Tessera-{version}-Main.zip', '--pattern', 'SHA256SUMS.txt',
                    '--pattern', 'build-info.json', '--pattern', 'release-notes.md'], check=True)
    build = json.loads((args.output / 'build-info.json').read_text(encoding='utf-8'))
    run_id = str(build.get('workflow_run', ''))
    if not re.fullmatch(r'[0-9]+', run_id):
        raise ValueError('Release is missing its GitHub build run ID')
    run = gh_json('api', f'repos/{args.repo}/actions/runs/{run_id}')
    validate_release(args.tag, release, build, commit, run)
    package = verify_downloads(args.output, version, build)
    notes = (args.output / 'release-notes.md').read_text(encoding='utf-8').strip()
    if not notes:
        raise ValueError('Release changelog is empty')
    output = os.environ.get('GITHUB_OUTPUT')
    if output:
        delimiter = 'changelog_' + uuid.uuid4().hex
        with open(output, 'a', encoding='utf-8') as stream:
            stream.write(f'version={version}\nfilename={package}\nchangelog<<{delimiter}\n{notes}\n{delimiter}\n')
    print(f'Verified {package}: published tag, successful CI, release checksums and tested DLL')


if __name__ == '__main__':
    main()
