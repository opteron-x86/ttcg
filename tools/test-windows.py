#!/usr/bin/env python3
"""Run DLL metadata, loader, lifetime and DirectX probes."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from release_support import ROOT, digest, source_state, version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    dll = build / 'TTCG.dll'
    report = {'version': version(), 'source': source_state(), 'dll_sha256': digest(dll),
              'passed': False, 'cases': {}}
    report_path = output / 'windows-tests.json'

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def command(name, *arguments):
        executable = str(build / name)
        return ([executable] if os.name == 'nt' else ['wine', executable]) + list(arguments)

    dll_arg = str(dll) if os.name == 'nt' else 'Z:' + str(dll).replace('/', '\\')
    cases = {
        'metadata': [sys.executable, str(ROOT / 'tests/native_build_test.py'), '--dll', str(dll), '--build-dir', str(build)],
        'plugin': command('tessera_plugin_probe.exe', dll_arg),
        'lifetime': command('tessera_commonlib_lifetime_test.exe'),
        'dx_state': command('tessera_dx_state_test.exe'),
    }
    save()
    for name, argv in cases.items():
        with (output / f'{name}.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(argv, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        report['cases'][name] = {'returncode': result.returncode}
        save()
        if result.returncode:
            raise SystemExit(f'FAIL: {name}; see {log.name}')
        print(f'PASS: {name}', flush=True)
    report['passed'] = True
    save()


if __name__ == '__main__':
    main()
