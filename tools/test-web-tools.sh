#!/usr/bin/env bash
# Browser tests for the developer tools that are web pages: the card library, rule lab,
# expansion editor, catalog audit and workshop server. They need `npm install` (Playwright)
# and are not part of ctest.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -eq 0 ]]; then
  for task_test in tests/*_test.cjs; do node "$task_test"; done
else
  for check in "$@"; do
    [[ "$check" =~ ^[a-z_]+$ ]] || exit 2
    node "tests/${check}_test.cjs"
  done
fi
