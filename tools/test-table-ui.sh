#!/usr/bin/env bash
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
