# Local validation checklist

Run this checklist only after Lua, llama.cpp, and the GGUF have been provisioned locally. It intentionally does not rely on GitHub Actions or any other GitHub CI.

## 1. Verify pins

```bash
lua_dir=/path/to/lua-5.4.9/src
llama_dir=/path/to/llama.cpp
model=/path/to/ai9stars_G9v3-3B-Q4_K_M.gguf

test -f "$lua_dir/lua.h"
test -f "$llama_dir/include/llama.h"
git -C "$llama_dir" rev-parse HEAD
# expected: f3f1a8f2760f28325a5ec20c05b171e5b7c83a29

test -f "$model"
```

## 2. Clean configure/build

```bash
rm -rf build/cpu-release
./build.sh cpu-release \
  -DLUA_DIR="$lua_dir" \
  -DLLAMA_CPP_DIR="$llama_dir" \
  -DG9_MODEL_PATH="$model"
```

Expected: configure and link complete without network access or fetch steps.

## 3. Unit tests

```bash
ctest --preset cpu-release --output-on-failure
```

All unit tests should pass before running a real coding session.

## 4. Doctor

```bash
./run.sh doctor \
  --workspace /path/to/sacrificial/test-repo \
  --model "$model" \
  --command-sandbox strict
```

Confirm model/config visibility and sandbox capability. On Linux, strict command execution requires the Landlock capabilities reported by the doctor path.

## 5. Model smoke

```bash
ctest --preset cpu-release -L model --output-on-failure
```

Then make one direct interactive request:

```bash
./run.sh chat \
  --workspace /path/to/sacrificial/test-repo \
  --model "$model" \
  --profile review \
  --command-sandbox strict
```

Ask the model to inspect a harmless source file and summarize it before testing edits.

## 6. Tool-calling smoke

Using a disposable Git repository/workspace, verify at least:

- `list_files` / `find_files`;
- `search_text` / `read_file`;
- a small `replace_text`;
- a multi-file `apply_patch`;
- a known-safe build/test command;
- an unknown executable requiring approval;
- a destructive file operation requiring approval;
- a forbidden network command being denied;
- `git reset --hard` being denied;
- a path/symlink escape being rejected.

Inspect `/changes` and the audit log if enabled.

## 7. Workspace boundary

Create a workspace with a sibling directory containing a sentinel file. Verify file tools cannot read/write the sibling and a strict sandboxed subprocess cannot modify it.

Also test nested scopes, for example:

```text
.:rw
.git:none
vendor:r
secrets:none
```

## 8. Cancellation

Start a long generation and press Ctrl-C. Confirm the current generation is aborted cleanly and the process/terminal state remains usable or exits predictably.

## 9. Offline execution

Disable networking at the host/container level and repeat build tests that do not require provisioning plus a model session. The agent should not attempt network access.

## 10. Performance baseline

Record at least:

- host CPU/GPU;
- RAM/VRAM;
- GGUF quantization and file size;
- context size;
- prompt-eval tokens/sec;
- generation tokens/sec;
- peak memory;
- latency for one read-only task and one edit/test task.

Do not treat performance numbers from another host as deployment guarantees.
