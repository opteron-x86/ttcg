#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/build-env.sh
: "${TESSERA_GAME_DATA:?Set the game Data directory}"
: "${TESSERA_PAPYRUS_COMPILER:?Set the PapyrusCompiler.exe path}"
: "${TESSERA_SKSE_SOURCES:?Set the SKSE script source directory}"
export TESSERA_PAPYRUS_FLAGS="${TESSERA_PAPYRUS_FLAGS:-$TESSERA_GAME_DATA/scripts/source/TESV_Papyrus_Flags.flg}"
export WINEPREFIX="${TESSERA_PAPYRUS_PREFIX:-$PWD/build/toolchain/papyrus-wine}"
export WINEDEBUG="${WINEDEBUG:--all}"
mkdir -p "$WINEPREFIX"
task_sources="${TESSERA_PAPYRUS_SOURCES:-$PWD/Scripts/Source}"
task_output="${TTCG_PAPYRUS_OUTPUT:-$PWD/Scripts}"
mkdir -p "$task_output"
winpath(){ winepath -w "$1"; }
task_imports="$(winpath "$task_sources");$(winpath "$PWD/Scripts/Source");$(winpath "$PWD/tools/papyrus-headers");$(winpath "$TESSERA_SKSE_SOURCES");$(winpath "$TESSERA_GAME_DATA/scripts/source")"
if [[ -n "${TESSERA_PAPYRUS_EXTRA_IMPORTS:-}" ]]; then
  task_imports="$task_imports;$(winpath "$TESSERA_PAPYRUS_EXTRA_IMPORTS")"
fi
for task_source in "$task_sources"/*.psc; do
  task_log=$(mktemp /tmp/tessera-papyrus-log.XXXXXX)
  wine "$TESSERA_PAPYRUS_COMPILER" "$(winpath "$task_source")" \
    "-i=$task_imports" "-o=$(winpath "$task_output")" "-f=$(winpath "$TESSERA_PAPYRUS_FLAGS")" | tee "$task_log"
  if ! rg -q '1 succeeded, 0 failed' "$task_log"; then exit 1; fi
done
# PEX embeds a build timestamp; normalize it for reproducible release artifacts.
python3 - "$task_output" "$task_sources" <<'PY'
import os,struct,sys
from pathlib import Path
for source in Path(sys.argv[2]).glob('*.psc'):
 p=Path(sys.argv[1])/(source.stem+'.pex')
 b=bytearray(p.read_bytes())
 if b[:4]!=bytes.fromhex('fa57c0de'):raise ValueError(p)
 struct.pack_into('>Q',b,8,int(os.environ['SOURCE_DATE_EPOCH']))
 p.write_bytes(b)
PY
