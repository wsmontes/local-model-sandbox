#!/bin/sh
# Black-box execution battery for g9-agent.
#
# Part 1 (deterministic, no model): CLI surface, exit codes, tool listing.
# Part 2 (model-dependent): JSON contract generation + tool-calling.
#   Skipped when the GGUF is missing or when G9_SKIP_MODEL=1.
#
# Usage:
#   tests/exec_suite.sh                 # uses cpu-release (or $G9_PRESET)
#   G9_PRESET=native-release tests/exec_suite.sh
#   G9_SKIP_MODEL=1 tests/exec_suite.sh # CLI-only

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export G9_PRESET=${G9_PRESET:-cpu-release}
RUN="$ROOT/run.sh"
BIN="$ROOT/build/$G9_PRESET/g9-agent"
MODEL="$ROOT/models/ai9stars_G9v3-3B-Q4_K_M.gguf"

if [ ! -x "$BIN" ]; then
    echo "g9-agent binary not found: $BIN" >&2
    echo "Build first: ./build.sh $G9_PRESET -DLUA_DIR=... -DLLAMA_CPP_DIR=..." >&2
    exit 1
fi

FAILS=0
TOTAL=0
ok(){ TOTAL=$((TOTAL+1)); echo "ok   - $1"; }
fail(){ TOTAL=$((TOTAL+1)); FAILS=$((FAILS+1)); echo "FAIL - $1" >&2; }

expect_ok(){ desc=$1; shift; o=$(mktemp); e=$(mktemp)
    if "$@" >"$o" 2>"$e"; then ok "$desc"; else fail "$desc (exit $?)"; sed 's/^/       | /' "$e" >&2; fi
    rm -f "$o" "$e"; }

expect_fail(){ desc=$1; shift; o=$(mktemp); e=$(mktemp)
    if "$@" >"$o" 2>"$e"; then fail "$desc (expected non-zero exit)"; else ok "$desc"; fi
    rm -f "$o" "$e"; }

expect_contains(){ desc=$1; needle=$2; shift 2; o=$(mktemp); e=$(mktemp)
    if "$@" >"$o" 2>"$e"; then
        if grep -q -- "$needle" "$o"; then ok "$desc"; else fail "$desc (missing '$needle')"; sed 's/^/       | /' "$o" >&2; fi
    else fail "$desc (exit $?)"; sed 's/^/       | /' "$e" >&2; fi
    rm -f "$o" "$e"; }

# ---- temp workspace + sibling sentinel outside the root ----
PARENT=$(mktemp -d "${TMPDIR:-/tmp}/g9-exec.XXXXXX")
WS="$PARENT/ws"
mkdir -p "$WS/src"
printf 'alpha\nbeta\nalpha\n' > "$WS/src/a.cpp"
printf 'hello\n' > "$WS/README.md"
printf 'outside\n' > "$PARENT/sentinel.txt"
trap 'rm -rf "$PARENT"' EXIT HUP INT TERM

echo "== CLI =="
expect_contains "version"            "g9-agent 0.1.0"   "$RUN" version
expect_contains "version pin"        "llama.cpp f3f1a8f" "$RUN" version
expect_contains "verbose flag parses" "g9-agent 0.1.0"   "$RUN" version --verbose
expect_contains "help"               "Options"           "$RUN" help

for t in list_files find_files search_text read_file write_file replace_text apply_patch delete_file workspace_status run_command; do
    expect_contains "tool listed: $t" "$t" "$RUN" tools --workspace "$WS"
done

expect_fail "unknown option rejected"          "$RUN" --definitely-not-an-option
expect_fail "run requires --json"             "$RUN" run --workspace "$WS"
expect_fail "--json only valid with run"      "$RUN" chat --json --workspace "$WS"
expect_fail "invalid --max-steps rejected"    "$RUN" --max-steps 0 --workspace "$WS"
expect_fail "invalid --profile rejected"      "$RUN" --profile nope --workspace "$WS"

echo "== doctor =="
o=$(mktemp); e=$(mktemp)
if "$RUN" doctor --workspace "$WS" >"$o" 2>"$e"; then
    ok "doctor exit 0"
    grep -q -- "model" "$o" && ok "doctor shows model" || fail "doctor missing model line"
    grep -q -- "config" "$o" && ok "doctor shows config" || fail "doctor missing config line"
else
    fail "doctor (exit $?)"; sed 's/^/       | /' "$e" >&2; sed 's/^/       | /' "$o" >&2
fi
rm -f "$o" "$e"

echo "== JSON contract (model) =="
if [ -f "$MODEL" ] && [ "${G9_SKIP_MODEL:-0}" != "1" ]; then

    resp=$(printf '%s\n' '{"prompt":"Reply with exactly OK","context":[],"options":{}}' | "$RUN" run --json --workspace "$WS" 2>/dev/null)
    rc=$?
    if [ "$rc" = 0 ] && printf '%s' "$resp" | python3 -c 'import json,sys; r=json.load(sys.stdin); assert r.get("model") and r.get("output") and "usage" in r' 2>/dev/null; then
        ok "json basic generation"
    else
        fail "json basic generation (exit $rc)"; printf '%s' "$resp" | head -c 400 >&2; echo >&2
    fi

    resp=$(printf '%s\n' '{"prompt":"Use the list_files tool to list files in the workspace root, then tell me what files are present.","context":[],"options":{}}' | "$RUN" run --json --workspace "$WS" --thinking on 2>/dev/null)
    rc=$?
    if [ "$rc" = 0 ] && printf '%s' "$resp" | python3 -c 'import json,sys; r=json.load(sys.stdin); assert r.get("metadata",{}).get("tool_steps",0) >= 1' 2>/dev/null; then
        ok "json tool-calling (tool_steps >= 1)"
    else
        fail "json tool-calling (exit $rc)"; printf '%s' "$resp" | head -c 400 >&2; echo >&2
    fi

else
    echo "skip - model not found (or G9_SKIP_MODEL=1): $MODEL"
fi

echo
echo "== summary =="
echo "passed: $((TOTAL-FAILS))/$TOTAL"
if [ "$FAILS" = 0 ]; then echo "ALL OK"; else echo "FAILURES: $FAILS" >&2; exit 1; fi
