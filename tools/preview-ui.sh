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
temporary=$(mktemp -d "$out/.preview-XXXXXX")
trap 'rm -f "$temporary/frame.bmp" "$temporary/frame.png"; rmdir "$temporary"' EXIT
args=(--hidden --frames "${FR:-12}" --size "${SIZE:-1920x1080}" --theme "${THEME:-oathvein}" --screenshot "$temporary/frame.bmp")
[ -n "${BACKDROP:-}" ] && args+=(--backdrop "$BACKDROP")
[ -n "${ICONS:-}" ] && args+=(--key-icons "$ICONS")
build/native/native/TesseraNativeUI "${args[@]}" "$@"
magick "$temporary/frame.bmp" "$temporary/frame.png"
mv "$temporary/frame.png" "$out/$name.png"
echo "$out/$name.png"
