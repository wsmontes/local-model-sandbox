# G9v3-3B llama.cpp + Lua Coding Agent

Experimental, fully local coding agent built in C++20, embedding Lua 5.4.9 for policy/prompt configuration and `llama.cpp` for inference with `ai9stars/G9v3-3B`.

The agent is designed to be copied out of this monorepo. It does not import or depend on root repository code. The only required external build/runtime artifacts are provisioned locally: Lua sources, a pinned `llama.cpp` checkout, and a GGUF model file.

## Design goals

- local inference only; no cloud/API fallback;
- no automatic downloads at configure, build, or runtime;
- C++20 core with a small embedded Lua configuration layer;
- coding-oriented file/search/edit/patch/command tools;
- explicit workspace boundary with `r`, `rw`, and `none` scopes;
- configurable destructive-action guardrails;
- subprocess execution without a shell;
- strict Linux command sandbox using Landlock when the host supports filesystem and network restrictions;
- clean JSON mode for the repository evaluation contract;
- interactive terminal mode for human coding sessions.

## External artifacts

Provision these before entering an air-gapped/offline environment:

1. **Lua 5.4.9** source tree. `LUA_DIR` must point to the directory containing `lua.h` (normally `lua-5.4.9/src`).
2. **llama.cpp** checkout at commit `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`.
3. A **G9v3-3B GGUF**. The default configuration expects `models/ai9stars_G9v3-3B-Q4_K_M.gguf`.

See `THIRD_PARTY.md` and `models/README.md`.

Nothing in this project runs `curl`, `wget`, package-manager downloads, `FetchContent`, Git fetches, or model downloads.

## Provisioning layouts

You can keep dependencies inside the copied agent directory:

```text
third_party/
├── lua/          # contents of lua-5.4.9/src
└── llama.cpp/    # pinned llama.cpp checkout
models/
└── ai9stars_G9v3-3B-Q4_K_M.gguf
```

or keep them elsewhere and provide CMake paths:

```bash
./build.sh cpu-release \
  -DLUA_DIR=/opt/src/lua-5.4.9/src \
  -DLLAMA_CPP_DIR=/opt/src/llama.cpp
```

CMake intentionally fails if either source tree is absent. It never downloads them.

## Build

Requirements for the default preset:

- CMake 3.24+
- Ninja
- a C++20 compiler
- local Lua 5.4.9 sources
- local pinned llama.cpp sources

Portable CPU build:

```bash
./build.sh cpu-release \
  -DLUA_DIR=/path/to/lua-5.4.9/src \
  -DLLAMA_CPP_DIR=/path/to/llama.cpp
```

Native CPU build:

```bash
./build.sh native-release \
  -DLUA_DIR=/path/to/lua-5.4.9/src \
  -DLLAMA_CPP_DIR=/path/to/llama.cpp
```

The CMake configuration explicitly disables llama.cpp tools, examples, server, app, tests, CURL support, and GGML RPC.

Run unit tests after a real local build:

```bash
ctest --preset cpu-release
```

The optional real-model smoke test is only created when `G9_MODEL_PATH` points to an existing GGUF at configure time:

```bash
./build.sh cpu-release \
  -DLUA_DIR=/path/to/lua-5.4.9/src \
  -DLLAMA_CPP_DIR=/path/to/llama.cpp \
  -DG9_MODEL_PATH=/path/to/ai9stars_G9v3-3B-Q4_K_M.gguf

ctest --preset cpu-release -L model
```

## Tests

Deterministic unit/integration tests (fast, no model required):

```bash
ctest --preset cpu-release
```

Black-box execution battery (runs the built binary):

```bash
tests/exec_suite.sh
```

It exercises the CLI surface (`version`, `tools`, `doctor`, `help`, invalid options and exit codes), lists all ten tools, and — when the GGUF is present — runs JSON-contract generation and a tool-calling round trip through the real model. Set `G9_SKIP_MODEL=1` for the CLI-only path, and `G9_PRESET=native-release` to target the native build.

## CLI

After building, `run.sh` resolves the binary relative to this agent directory:

```bash
./run.sh chat --workspace /path/to/project --model /path/to/model.gguf
```

Useful commands:

```bash
./run.sh doctor --workspace /path/to/project --model /path/to/model.gguf
./run.sh tools --workspace /path/to/project
./run.sh version
```

Important options:

```text
--workspace PATH
--config PATH
--model PATH
--profile review|balanced|autonomous
--scope PATH:r|rw|none
--deny GLOB
--command-sandbox strict|best-effort|off
--verbose
--max-steps N
--thinking on|off
--history-file PATH
--audit-log PATH
--color auto|always|never
```

Scopes are repeatable. The initial `--workspace` is the hard root; scopes can reduce or subdivide permissions inside it but approvals cannot expand access outside it.

By default output is quiet: the interactive banner shows only the agent name, and model/runtime logs are hidden. Pass `--verbose` to see the model/workspace/policy/sandbox banner and full runtime logs.

Example:

```bash
./run.sh chat \
  --workspace "$PWD" \
  --model /models/ai9stars_G9v3-3B-Q4_K_M.gguf \
  --profile balanced \
  --scope .:rw \
  --scope .git:none \
  --deny 'models/**' \
  --command-sandbox strict
```

Interactive slash commands include `/status`, `/scope`, `/policy`, `/tools`, `/changes`, `/thinking`, `/clear`, and `/exit`.

History is disabled unless `--history-file` is explicitly supplied.

## JSON contract mode

`run --json` is intended for the shared evaluation harness. Stdout is reserved for machine-readable JSON; diagnostics go to stderr.

```bash
printf '%s\n' '{"prompt":"Inspect this repository and explain the build system","context":[],"options":{}}' \
  | ./run.sh run --json --workspace /path/to/project --model /path/to/model.gguf
```

The model remains local; JSON mode does not enable network access.

## Coding tools

The v0.1 tool set is intentionally compact:

- `list_files`
- `find_files`
- `search_text`
- `read_file`
- `write_file`
- `replace_text`
- `apply_patch`
- `delete_file`
- `run_command`
- `workspace_status`

`apply_patch` is the preferred multi-file editor. Patch validation occurs before writes. Exact replacement requires an expected occurrence count to avoid ambiguous edits.

File reads/searches are bounded so a small local model does not flood its context.

## Guardrails and command execution

Three profiles are available:

- `review`: reads are automatic; writes/commands normally ask.
- `balanced`: ordinary workspace writes and known-safe build/test commands can proceed; destructive or unusual operations ask.
- `autonomous`: broader workspace operations are automatic, while destructive filesystem actions still require approval and hard-denied actions stay denied.

Hard-denied examples include network-oriented commands and destructive Git operations. Unknown executables are not treated like known build/test tools: they require approval.

`run_command` executes an argv vector directly. There is no `/bin/sh -c`, pipe interpretation, redirection, command substitution, or `&&` parsing by the agent.

### Filesystem scope vs subprocess sandbox

File tools enforce the complete scope tree (`r` / `rw` / `none`) and deny globs.

On Linux, the strict subprocess sandbox uses Landlock to enforce the hard workspace boundary and, where the kernel ABI supports it, network restrictions. This is a coarser subprocess boundary than the file-tool scope tree; command policy remains responsible for fine-grained tool decisions.

`strict` refuses command execution when the required filesystem+network confinement is unavailable. `best-effort` and `off` are weaker modes and should be selected deliberately.

When the configured default (`strict`) is unavailable on the host (for example macOS, where the strict sandbox is not implemented in this version), the CLI automatically falls back to `best-effort` and prints a one-line notice, unless `--command-sandbox` is passed explicitly.

The current implementation does **not** claim a strict macOS sandbox. Use Linux for the intended strict execution model.

## Lua configuration

`config/agent.lua` controls model defaults, generation settings, system behavior, tools, workspace policy, and guardrails.

The Lua environment is intentionally reduced. It is not a general-purpose local shell: dangerous standard libraries/functions such as `io`, `os`, `package`, `debug`, `dofile`, `loadfile`, and dynamic `load` are not exposed to the agent configuration environment. Logging is routed through the host.

The G9v3 prompt/tool format is rendered in Lua so model-specific behavior stays configurable rather than being hardcoded throughout the C++ runtime.

## Offline boundary

The project is meant to be **provisioned while connected, then built/run offline**. A network-disabled host/container remains the strongest assurance that no process can communicate externally.

The application itself contains no HTTP client, cloud provider integration, telemetry, model downloader, updater, remote Git operation tool, or browser tool.

## Local validation handoff

The repository-side implementation can be reviewed without CI, but the following validations require a real local environment and have not been claimed as completed here:

1. configure/build against the actual Lua 5.4.9 sources;
2. configure/build against the exact pinned llama.cpp checkout;
3. run all CTest targets against those real libraries;
4. load the actual G9v3 GGUF;
5. run model smoke inference;
6. verify native G9v3 tool calls against the parser/loop;
7. exercise interactive approvals and Ctrl-C during real inference;
8. validate Landlock behavior on the deployment kernel;
9. benchmark RAM/VRAM, tokens/sec, context limits, and coding quality.

No GitHub Actions workflow is required or provided for this agent.
