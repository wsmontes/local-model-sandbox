#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PRESET=${1:-cpu-release}
if [ "$#" -gt 0 ]; then shift; fi
cd "$ROOT"
cmake --preset "$PRESET" "$@"
cmake --build --preset "$PRESET"
