#!/usr/bin/env python3
"""Check the real CommonLib header with an unpatched negative control."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
output = ROOT / 'build/validation/commonlib-lifetime'
overlay = output / 'include'
output.mkdir(parents=True, exist_ok=True)
subprocess.run(['cmake', f'-DTTCG_COMMONLIB_ROOT={ROOT / "build/deps/commonlib"}',
                f'-DTTCG_COMMONLIB_OVERLAY={overlay}', '-P',
                str(ROOT / 'cmake/CommonLibHashMapPatch.cmake')], check=True)
compiler = shlex.split(os.environ.get('CXX', 'clang++'))
for patched in (False, True):
    binary = output / ('patched' if patched else 'original')
    includes = ['-I' + str(ROOT / 'tests/commonlib_stubs')]
    if patched:
        includes += ['-I' + str(overlay), '-DTESSERA_REQUIRE_HASHMAP_PATCH=1']
    includes += ['-I' + str(ROOT / 'build/deps/commonlib/include')]
    subprocess.run(compiler + ['-std=c++23', '-O2', '-DNDEBUG'] + includes +
                   [str(ROOT / 'tests/commonlib_lifetime_test.cpp'), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], text=True, capture_output=True)
    if patched:
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode == 1, result.stdout + result.stderr
        assert 'Menu registration released an existing menu prematurely' in result.stderr, result.stderr
    print(f'{binary.name}: {(result.stdout + result.stderr).strip()}')
