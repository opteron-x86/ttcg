#!/usr/bin/env python3
"""Validate the built DLL's actual SKSE declaration and locked native inputs."""
from pathlib import Path
import argparse
import json
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--dll', type=Path, default=root / 'build/runtime-tessera/SKSE/Plugins/TTCG.dll')
parser.add_argument('--build-dir', type=Path, default=root / 'build/linux-clangcl')
args = parser.parse_args()
lock = json.loads((root / 'cmake/dependencies.json').read_text())
binary = args.dll
b = binary.read_bytes()
u16 = lambda at: struct.unpack_from('<H', b, at)[0]
u32 = lambda at: struct.unpack_from('<I', b, at)[0]
pe = u32(0x3c)
assert b[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x8664
optional = pe + 24
assert u16(optional) == 0x20b
sections = optional + u16(pe+20)

def offset(rva):
    for i in range(u16(pe+6)):
        at = sections + i*40
        size, va, raw_size, raw = struct.unpack_from('<4I', b, at+8)
        if va <= rva < va + max(size, raw_size):
            return raw + rva - va
    raise ValueError(f'Unmapped RVA: {rva:x}')

def string(at):
    return b[at:b.index(b'\0', at)].decode()

exports = offset(u32(optional+112))
functions, names, ordinals = (offset(u32(exports+at)) for at in [28, 32, 36])
export_map = {string(offset(u32(names+4*i))): offset(u32(functions+4*u16(ordinals+2*i)))
              for i in range(u32(exports+24))}
assert {'SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version', 'Tessera_GetCollectionAPI'} <= export_map.keys()
version = export_map['SKSEPlugin_Version']
assert u32(version) == 1 and string(version+8) == 'TTCG'
major, minor, patch = map(int, json.loads((root/'vcpkg.json').read_text())['version-semver'].split('.'))
assert u32(version+4) == (major << 24 | minor << 16 | patch << 4), 'DLL and project versions differ'
assert u32(version+0x304) == 3, 'Require dynamic struct layouts and Address Library v5 support'
assert u32(version+0x308) == 1, 'Require Address Library, without a fixed post-629 layout'
assert not any(b[version+0x30c:version+0x34c]), 'No single-runtime whitelist'
assert lock['commonlib']['commit'].encode() in b
imports = []
import_table = offset(u32(optional+120))
while u32(import_table+12):
    imports.append(string(offset(u32(import_table+12))).lower())
    import_table += 20
cache = (args.build_dir / 'CMakeCache.txt').read_text()
assert 'ENABLE_SKYRIM_SE:BOOL=ON' in cache
assert 'ENABLE_SKYRIM_AE:BOOL=ON' in cache
assert 'ENABLE_SKYRIM_VR:BOOL=ON' in cache
for name in ['commonlib', 'vcpkg', 'minhook']:
    source = root / 'build/deps' / name
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    assert actual == lock[name]['commit']
    assert not subprocess.check_output(['git', 'diff', 'HEAD', '--'], cwd=source)
assert not any(any(token in name for token in ('meridian', 'cef', 'chrome')) for name in imports), imports
assert b'MeridianUI_CreateView' not in b
assert b'Data/MeridianUI/' not in b
assert b'Native Tessera menu registered' in b
assert b'bsthashmap-lifetime-v1' in b, 'Missing CommonLib lifetime correction identifier'
print('Native build: Windows x64, exports, dynamic layouts, Address Library and locked dependencies passed')
