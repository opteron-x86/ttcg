#!/usr/bin/env python3
"""Keep packaged native dependency notices aligned with the locked build."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
licenses = root / 'docs/licenses'
(licenses / 'native').mkdir(parents=True, exist_ok=True)
commonlib = root / 'build/deps/commonlib'
for source, name in [
    (commonlib/'COPYING.txt', 'CommonLibSSE-NG-COPYING.txt'),
    (commonlib/'EXCEPTIONS.md', 'CommonLibSSE-NG-EXCEPTIONS.md'),
    (commonlib/'licenses/LICENSE-MIT.txt', 'CommonLibSSE-NG-MIT.txt'),
    (root/'build/linux-clangcl/_deps/hde64-src/LICENSE.txt', 'native/hde64-Minhook.txt'),
]:
    shutil.copy2(source, licenses/name)
for name in ['directxtk', 'directxmath', 'spdlog', 'fmt', 'rapidcsv']:
    shutil.copy2(root/'build/vcpkg_installed/x64-windows-clangcl/share'/name/'copyright',
                 licenses/'native'/(name+'.txt'))

for path in (root/'native/licenses').glob('*'):
    if path.is_file():shutil.copy2(path, licenses/'native'/path.name)
shutil.copy2(root/'native/third_party/imgui/LICENSE.txt',licenses/'native/imgui.txt')
shutil.copy2(root/'native/third_party/libwebp/COPYING',licenses/'native/libwebp.txt')
shutil.copy2(root/'native/third_party/libwebp/PATENTS',licenses/'native/libwebp-patents.txt')
