#!/usr/bin/env python3
"""Fetch the locked native dependencies and independent Linux cross-build tools."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import shutil
import tarfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'cmake/dependencies.json').read_text())


def run(*args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def checkout(name):
    item = LOCK[name]
    path = ROOT / 'build/deps' / name
    if not (path / '.git').exists():
        path.mkdir(parents=True, exist_ok=True)
        run('git', 'init', path)
        run('git', 'remote', 'add', 'origin', item['repository'], cwd=path)
        run('git', 'fetch', '--depth', '1', 'origin', item['commit'], cwd=path)
        run('git', 'checkout', '--detach', item['commit'], cwd=path)
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=path, text=True).strip()
    if subprocess.check_output(['git', 'diff', 'HEAD', '--'], cwd=path):
        raise RuntimeError(f'{name}: dependency has local source edits')
    if actual != item['commit']:
        run('git', 'fetch', item['repository'], item['commit'], cwd=path)
        run('git', 'checkout', '--detach', item['commit'], cwd=path)
    if name == 'vcpkg' and subprocess.check_output(['git', 'rev-parse', '--is-shallow-repository'], cwd=path, text=True).strip() == 'true':
        run('git', 'fetch', '--unshallow', 'origin', item['commit'], cwd=path)
    if (path / '.gitmodules').exists():
        run('git', 'submodule', 'update', '--init', '--recursive', '--depth', '1', cwd=path)
    return path


def main():
    host = ROOT / 'build/toolchain/host'
    host.mkdir(parents=True, exist_ok=True)
    os.environ['PATH'] = str(host / 'usr/bin') + os.pathsep + os.environ['PATH']
    if not shutil.which('zip'):
        archive = host / 'zip.pkg.tar.zst'
        if not archive.exists():
            run('curl', '-fL', '--retry', '3', LOCK['host_zip']['url'], '-o', archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != LOCK['host_zip']['sha256']:
            raise RuntimeError('Host zip archive checksum mismatch')
        with tarfile.open(archive) as tar:
            tar.extractall(host, filter='data')
    checkout('commonlib')
    checkout('minhook')
    vcpkg = checkout('vcpkg')
    if not (vcpkg / 'vcpkg').is_file():
        run('bash', vcpkg / 'bootstrap-vcpkg.sh', '-disableMetrics')
    toolchain = ROOT / 'build/toolchain'
    toolchain.mkdir(parents=True, exist_ok=True)
    sdk = Path(os.environ.get('XWIN_SYSROOT', toolchain / 'xwin'))
    marker = sdk / 'tessera-sdk.json'
    if not marker.exists() and 'XWIN_SYSROOT' not in os.environ:
        run('xwin', '--accept-license', '--cache-dir', toolchain / 'xwin-cache',
            '--sdk-version', LOCK['sdk']['sdk_version'], '--crt-version', LOCK['sdk']['crt_version'],
            'splat', '--output', sdk, '--include-debug-libs', '--use-winsysroot-style', '--preserve-ms-arch-notation')
        marker.write_text(json.dumps(LOCK['sdk'], indent=2) + '\n')
    if not (sdk / 'Windows Kits/10/Include').is_dir():
        raise RuntimeError(f'Missing Windows SDK in winsysroot layout: {sdk}')
    if marker.exists() and json.loads(marker.read_text()) != LOCK['sdk']:
        raise RuntimeError('SDK version differs from cmake/dependencies.json')
    mingw = toolchain / LOCK['llvm_mingw']['directory']
    compiler = Path(os.environ.get('LLVM_MINGW_BIN', mingw / 'bin')) / 'x86_64-w64-mingw32-clang++'
    if not compiler.exists():
        if 'LLVM_MINGW_BIN' in os.environ:
            raise RuntimeError(f'Missing compiler: {compiler}')
        archive = toolchain / 'llvm-mingw.tar.xz'
        if not archive.exists():
            run('curl', '-fL', '--retry', '3', LOCK['llvm_mingw']['url'], '-o', archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != LOCK['llvm_mingw']['sha256']:
            raise RuntimeError('LLVM-MinGW archive checksum mismatch')
        with tarfile.open(archive) as tar:
            tar.extractall(toolchain, filter='data')
    print(f'Native dependencies ready; CommonLibSSE-NG {LOCK["commonlib"]["version"]}: {LOCK["commonlib"]["commit"]}', flush=True)


if __name__ == '__main__':
    main()
