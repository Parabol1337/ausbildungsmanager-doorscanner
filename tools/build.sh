#!/usr/bin/env bash
# Builds the sketch of this folder with arduino-cli and records exactly what was used, so that the same build can
# be reproduced years later:  BUILD-INFO.txt (commit, arduino-cli, board package versions, checksum of the result).
#
#   tools/build.sh                 build, write BUILD-INFO.txt
#   tools/build.sh --check-deps    only verify the checksums of the files in this folder (DEPENDENCIES.sha256)
#
# FQBN: the board. Check it once with `arduino-cli board listall rp2040` and write the right one into FQBN below
# (or pass FQBN=... in the environment).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

FQBN="${FQBN:-rp2040:rp2040:waveshare_rp2040_eth}"

if [ -f DEPENDENCIES.sha256 ]; then
    sha256sum --quiet -c DEPENDENCIES.sha256 || { echo "A vendored file differs from DEPENDENCIES.sha256 - see DEPENDENCIES.md" >&2; exit 1; }
    echo "Dependencies: all files match DEPENDENCIES.sha256"
fi
[ "${1:-}" = "--check-deps" ] && exit 0

sketch="$(ls ./*.ino | head -1)"
out="build/out"
rm -rf "$out"
arduino-cli compile --fqbn "$FQBN" --output-dir "$out" --warnings default .

{
    echo "built      $(date -u +%FT%TZ)"
    echo "sketch     $sketch"
    echo "commit     $(git rev-parse HEAD 2>/dev/null || echo unknown)$(git diff --quiet 2>/dev/null || echo ' (uncommitted changes)')"
    echo "fqbn       $FQBN"
    echo "cli        $(arduino-cli version | head -1)"
    echo "packages:"
    arduino-cli core list | sed 's/^/  /'
    echo "AMLink     $(head -2 src/AMLink/VERSION | tr '\n' ' ')"
    echo "result:"
    (cd "$out" && sha256sum ./*.uf2 ./*.bin 2>/dev/null | sed 's/^/  /')
} > BUILD-INFO.txt
cat BUILD-INFO.txt
