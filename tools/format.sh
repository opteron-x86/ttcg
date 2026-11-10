#!/usr/bin/env bash
# Format the C++ sources, or verify them with --check. CI pins clang-format 23.1.4.
# Generated headers are listed in .clang-format-ignore.
set -euo pipefail
cd "$(dirname "$0")/.."
mapfile -t files < <(git ls-files 'include/*' 'src/*' 'native/src/*' 'native/skse/*' 'native/tests/*' 'tests/*' 'tools/*' |
    grep -E '\.(h|hpp|inc|c|cpp)$')
if [[ ${1-} == --check ]]; then
    clang-format --dry-run -Werror "${files[@]}"
else
    clang-format -i "${files[@]}"
fi
