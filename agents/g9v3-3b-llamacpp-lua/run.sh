#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PRESET=${G9_PRESET:-cpu-release}
BIN="$ROOT/build/$PRESET/g9-agent"
if [ ! -x "$BIN" ]; then
  echo "g9-agent binary not found: $BIN" >&2
  echo "Build first: ./build.sh $PRESET -DLUA_DIR=/path/to/lua-5.4.9/src -DLLAMA_CPP_DIR=/path/to/llama.cpp" >&2
  exit 127
fi
exec "$BIN" "$@" --agent-root "$ROOT"
