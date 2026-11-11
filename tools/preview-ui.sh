#!/usr/bin/env bash
# Render one screen of the native UI to a PNG for review.
#   tools/preview-ui.sh NAME [preview args...]
# Environment: THEME (default oathvein), SIZE (1920x1080), FR (frames, default 12; use 300-400 to let
# animations settle), OUT (output folder, default build/previews), BACKDROP (a screenshot drawn behind
# the UI), ICONS (an ImGui Icons folder for keycaps).
# Example: tools/preview-ui.sh match --snapshot build/test-fixtures/match.json
set -euo pipefail
cd "$(dirname "$0")/.."
name=$1; shift
out=${OUT:-build/previews}; mkdir -p "$out"
args=(--hidden --frames "${FR:-12}" --size "${SIZE:-1920x1080}" --theme "${THEME:-oathvein}" --screenshot "$out/$name.bmp")
[ -n "${BACKDROP:-}" ] && args+=(--backdrop "$BACKDROP")
[ -n "${ICONS:-}" ] && args+=(--key-icons "$ICONS")
build/native/native/TesseraNativeUI "${args[@]}" "$@" | grep -v '^Screenshot\|^Native frames' || true
magick "$out/$name.bmp" "$out/$name.png" && rm -f "$out/$name.bmp"
echo "$out/$name.png"
