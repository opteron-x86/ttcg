#!/usr/bin/env python3
"""Save command output and attach failures to the GitHub check."""
from collections import deque
import os
from pathlib import Path
import subprocess
import sys

name, *command = sys.argv[1:]
output = Path('build/test-results')
output.mkdir(parents=True, exist_ok=True)
tail = deque(maxlen=60)
with (output / f'{name}.log').open('w', encoding='utf-8') as log:
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding='utf-8', errors='replace')
    for line in process.stdout:
        print(line, end='', flush=True)
        log.write(line)
        tail.append(line)
    code = process.wait()
if code and os.environ.get('GITHUB_ACTIONS'):
    message = ''.join(tail)[-12000:].replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
    print(f'::error title={name}::{message}')
raise SystemExit(code)
