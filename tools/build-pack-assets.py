#!/usr/bin/env python3
"""Rebuild Tessera's authored pack meshes and textures; optional NIF-based previews."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
NIFLY_REVISION='cca0a770094bb962fb28ea1fec5ea903e68fda8e'


def run(*args):
    subprocess.run([str(x) for x in args],cwd=ROOT,check=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview',action='store_true')
    args=parser.parse_args()
    source=ROOT/'build/tools/nifly';built=ROOT/'build/tools/nifly-out'
    if not source.is_dir():
        source.parent.mkdir(parents=True,exist_ok=True)
        run('git','clone','https://github.com/ousnius/nifly.git',source)
        run('git','-C',source,'checkout','--detach',NIFLY_REVISION)
    actual=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    if actual!=NIFLY_REVISION:raise SystemExit(f'Expected nifly {NIFLY_REVISION}; found {actual}')
    if subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True).strip():
        raise SystemExit('nifly checkout has local edits; preserve or move them before rebuilding')
    run('cmake','-S',source,'-B',built,'-G','Ninja','-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release')
    run('cmake','--build',built,'--parallel','6')
    includes=ROOT/'build/vcpkg_installed/x64-windows-clangcl/include'
    if not (includes/'nlohmann/json.hpp').is_file():
        includes=ROOT/'build/tools/pack-json'
        header=includes/'nlohmann/json.hpp';header.parent.mkdir(parents=True,exist_ok=True)
        if not header.exists():
            urllib.request.urlretrieve('https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp',header)
    tool=ROOT/'build/tools/nif-export'
    run('c++','-std=c++17','-O2',f'-I{source}/include',f'-I{source}/external',f'-I{includes}',
        ROOT/'tools/pack-assets/nif-export.cpp',built/'src/libnifly.a','-o',tool)
    run(sys.executable,'tools/pack-assets/textures.py')
    run('blender','--background','--factory-startup','--python','tools/pack-assets/model.py','--','build')
    meshes=ROOT/'meshes/Tessera/packs';meshes.mkdir(parents=True,exist_ok=True)
    for name in ('standard','tournament'):
        run(tool,'write',ROOT/f'build/pack-assets/{name}.json',meshes/f'{name}.nif')
        run(tool,'inspect',meshes/f'{name}.nif',ROOT/f'build/pack-assets/{name}-nif.json')
    run(sys.executable,'tools/pack-assets/verify.py')
    run('blender','--background','--factory-startup','--threads','10','--python','tools/pack-assets/model.py','--','ui')
    if args.preview:
        run('blender','--background','--factory-startup','--threads','10','--python','tools/pack-assets/model.py','--','preview')
    print('Pack assets rebuilt. Plugin integration: tools/build-plugin.py; deployment: tools/install-mod.py.')


if __name__=='__main__':main()
