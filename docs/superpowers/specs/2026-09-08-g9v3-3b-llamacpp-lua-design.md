# G9v3-3B llama.cpp + Lua Agent Design

Date: 2026-09-08
Status: Approved design, implementation pending spec review
Repository: `wsmontes/local-model-sandbox`
Target: `agents/g9v3-3b-llamacpp-lua/`

## 1. Purpose

Create the repository's first real LLM agent experiment: a fully local, portable agent using `ai9stars/G9v3-3B`, `llama.cpp`, an embedded Lua runtime, and a C++ application core.

The agent is designed to be copied out of `local-model-sandbox` and remain independently understandable, buildable, and runnable without depending on any file elsewhere in the repository.

Runtime operation must require no network access. Model inference, configuration, prompt construction, and execution all use local files and local processes only.

## 2. Scope of v0.1

Version 0.1 proves the complete local stack:

```text
sandbox JSON contract
        -> C++ application
        -> embedded Lua policy/configuration
        -> llama.cpp
        -> local G9v3-3B GGUF
        -> JSON response
```

Version 0.1 includes:

- C++20 application core;
- embedded Lua for configuration and agent behavior;
- direct `libllama` integration, not a `llama-cli` subprocess;
- local GGUF model loading only;
- chat prompt formatting using the chat template embedded in the GGUF when supported by `llama.cpp`;
- sampling controlled by Lua configuration;
- repository contract v1 over stdin/stdout;
- deterministic error handling and structured diagnostics;
- unit tests that do not require downloading or loading the full model;
- build/run scripts contained inside the agent directory;
- documentation for provisioning dependencies and the model before entering an air-gapped environment.

Version 0.1 explicitly does not include:

- HTTP servers or HTTP clients;
- model downloads at runtime;
- dependency downloads during normal configure/build;
- Hugging Face API integration;
- telemetry or update checks;
- tool execution or autonomous tool loops;
- vector databases, RAG, persistence, or memory stores;
- a network sandbox implementation;
- generic switching between thinking and non-thinking prompt templates;
- a shared repository runtime library.

## 3. Non-negotiable isolation rule

Everything required by this agent at runtime must be inside the copied agent directory or be an explicitly documented external system dependency installed/provisioned on the target machine.

The agent must never reference:

```text
../../evaluation/
../../scripts/
../another-agent/
repository-root shared code
```

The agent may be built against a local checkout of `llama.cpp` and local Lua source tree supplied either under its own `third_party/` directory or via explicit CMake paths. Those dependencies are agent-local/external build inputs, never dependencies on sibling repository directories.

## 4. External versions and reproducibility

### llama.cpp

Baseline revision:

```text
repository: https://github.com/ggml-org/llama.cpp
commit: f3f1a8f2760f28325a5ec20c05b171e5b7c83a29
```

The implementation targets the C API available at that revision, including the current model/context/sampler APIs such as:

- `llama_model_load_from_file`;
- `llama_model_get_vocab`;
- `llama_init_from_model`;
- `llama_decode`;
- `llama_sampler_*`;
- `llama_model_chat_template`;
- `llama_chat_apply_template`.

The revision is pinned in documentation because `llama.cpp` evolves quickly. A newer revision may work but is not the v0.1 reproducibility baseline.

### Lua

Baseline source release:

```text
Lua 5.4.9
```

Lua 5.4 is chosen deliberately as the conservative embedded API baseline. The agent does not depend on Lua modules from LuaRocks.

### Model

Logical model identity:

```text
ai9stars/G9v3-3B
```

The upstream model is a ~3B-parameter `LlamaForCausalLM` model with a declared 131,072-token maximum context and Apache-2.0 model license.

The default llama.cpp artifact for v0.1 is:

```text
repository: bartowski/ai9stars_G9v3-3B-GGUF
file: ai9stars_G9v3-3B-Q4_K_M.gguf
approximate size: 1.90 GB
```

The GGUF repository describes Q4_K_M as a recommended/default-size quantization. The binary model file is not committed to this repository.

The runtime default context remains 8192 tokens rather than attempting the model's theoretical maximum. Users may raise it in Lua based on available memory and the capabilities of the chosen llama.cpp build.

## 5. Target file structure

```text
agents/g9v3-3b-llamacpp-lua/
├── README.md
├── THIRD_PARTY.md
├── agent.yaml
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── build.sh
├── run.sh
│
├── config/
│   └── agent.lua
│
├── src/
│   ├── main.cpp
│   ├── application.cpp
│   ├── application.hpp
│   ├── contract.cpp
│   ├── contract.hpp
│   ├── error.hpp
│   ├── json.cpp
│   ├── json.hpp
│   ├── llama_runtime.cpp
│   ├── llama_runtime.hpp
│   ├── lua_agent.cpp
│   └── lua_agent.hpp
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_contract.cpp
│   ├── test_json.cpp
│   └── test_lua_agent.cpp
│
├── models/
│   ├── README.md
│   └── .gitkeep
│
└── third_party/
    ├── README.md
    ├── llama.cpp/        # optional local source checkout, gitignored contents
    └── lua/              # optional local Lua source tree, gitignored contents
```

The project will not assign a license to the user's new agent source code implicitly. `THIRD_PARTY.md` records the licenses and source identities of external dependencies and model artifacts. A project `LICENSE` can be added later if the repository owner chooses one explicitly.

## 6. Component responsibilities

### `main.cpp`

Thin process boundary. It calls the application entrypoint, maps exceptions/errors to exit codes, and ensures stdout remains reserved for the contract response.

### `application.*`

Coordinates one request lifecycle:

1. resolve agent-local paths;
2. load Lua policy/configuration;
3. parse one JSON request from stdin;
4. run Lua request hooks and message construction;
5. initialize/load the local llama.cpp runtime;
6. build the chat prompt;
7. generate the completion;
8. run Lua response postprocessing;
9. serialize exactly one JSON response to stdout.

### `contract.*`

Maps the sandbox contract v1 to typed C++ structures. It has no dependency on repository-level schemas at runtime.

Baseline types:

```cpp
struct AgentRequest {
    std::string prompt;
    JsonValue context;
    JsonValue options;
};

struct Usage {
    std::int64_t prompt_tokens = 0;
    std::int64_t completion_tokens = 0;
};

struct AgentResponse {
    std::string output;
    std::string model;
    Usage usage;
    JsonValue metadata;
};
```

### `json.*`

A small self-contained JSON parser/serializer for the contract boundary.

It must support the complete JSON value set needed by the contract:

- null;
- booleans;
- numbers;
- strings with escaping and Unicode escape decoding;
- arrays;
- objects.

The parser must reject malformed input, trailing garbage, invalid escape sequences, and structurally invalid contract requests. No external JSON package is introduced in v0.1.

### `lua_agent.*`

Owns Lua VM lifecycle and the C++/Lua boundary.

It loads `config/agent.lua`, validates the returned table, reads typed configuration, converts request/context data into Lua values, invokes supported hooks, and converts Lua-produced message/response structures back to C++.

Lua errors are captured with useful stack/error text and returned as application errors without crashing the process.

### `llama_runtime.*`

Owns all llama.cpp resources using RAII and exposes a small C++ interface independent from Lua.

Responsibilities:

- backend initialization;
- local model loading;
- vocabulary access;
- context creation;
- chat-template application;
- tokenization;
- prompt evaluation;
- sampler construction;
- token generation;
- EOG detection;
- token-to-text conversion;
- prompt/completion token accounting;
- cleanup.

It must not contain agent personality/policy decisions.

## 7. Lua contract

`config/agent.lua` returns one table.

Baseline shape:

```lua
local agent = {}

agent.model = {
    path = "models/ai9stars_G9v3-3B-Q4_K_M.gguf",
    context_size = 8192,
    batch_size = 512,
    gpu_layers = 0,
    threads = 0
}

agent.generation = {
    max_tokens = 512,
    temperature = 0.7,
    top_p = 0.95,
    top_k = 40,
    min_p = 0.0,
    repeat_penalty = 1.0,
    seed = -1
}

agent.system_prompt = [[
You are a helpful assistant running entirely on the local machine.
]]

function agent.before_prompt(request)
    return request
end

function agent.build_messages(request)
    local messages = {}

    if agent.system_prompt ~= "" then
        table.insert(messages, {
            role = "system",
            content = agent.system_prompt
        })
    end

    if request.context then
        for _, message in ipairs(request.context) do
            if type(message) == "table" and message.role and message.content then
                table.insert(messages, {
                    role = message.role,
                    content = message.content
                })
            end
        end
    end

    table.insert(messages, {
        role = "user",
        content = request.prompt
    })

    return messages
end

function agent.after_response(response)
    return response
end

return agent
```

### Supported hooks

#### `before_prompt(request)`

Optional. Receives a Lua request table and may return a replacement request table. It is intended for local normalization/policy, not I/O.

#### `build_messages(request)`

Required in the default policy. Returns an ordered array of chat messages:

```lua
{
    { role = "system", content = "..." },
    { role = "user", content = "..." }
}
```

For v0.1, accepted roles are `system`, `user`, and `assistant`.

#### `after_response(response)`

Optional. Receives a Lua table containing generated text plus metadata and may return a replacement response table. The C++ layer still enforces the final sandbox output contract.

### Deliberately unavailable Lua capabilities

The C++ host will open only the standard Lua libraries needed for configuration and pure computation. Runtime design must not expose custom socket, HTTP, process-execution, or filesystem helper APIs.

Lua's normal standard-library capabilities are not claimed to be an operating-system security boundary. The project's offline guarantee is architectural: the shipped C++ application contains no networking code and the provided policy performs no network I/O. Hard OS-level network sandboxing is outside v0.1.

## 8. Prompt formatting and thinking mode

Lua owns the semantic list of messages. C++ owns conversion of those messages into the model-specific text prompt.

Preferred flow:

```text
Lua messages
    -> llama_chat_message[]
    -> llama_model_chat_template(model, nullptr)
    -> llama_chat_apply_template(..., add_assistant=true)
    -> formatted prompt
```

If the loaded model has no usable chat template, generation fails with a clear error in v0.1 rather than silently applying a generic template that may be wrong for the model.

The upstream Transformers example exposes an `enable_thinking` template variable, but the baseline `llama_chat_apply_template` C API does not provide a generic mechanism for passing arbitrary Jinja variables. Therefore v0.1 does not expose a Lua `thinking=true/false` switch that might be semantically ineffective.

The default generation parameters follow the upstream non-thinking recommendation where applicable:

```text
temperature = 0.7
top_p = 0.95
```

Thinking-mode support is a later experiment after inspecting and validating the exact GGUF template behavior with the pinned llama.cpp revision.

## 9. llama.cpp generation design

The implementation follows the direct C API pattern demonstrated by current llama.cpp examples:

1. call `ggml_backend_load_all()`;
2. obtain `llama_model_default_params()`;
3. set `n_gpu_layers` from Lua;
4. call `llama_model_load_from_file()` with the local GGUF path;
5. obtain vocabulary using `llama_model_get_vocab()`;
6. format and tokenize the prompt;
7. create `llama_context_params` using Lua context/batch settings;
8. create context with `llama_init_from_model()`;
9. evaluate the prompt with `llama_decode()`;
10. sample one token at a time through a `llama_sampler` chain;
11. stop at EOG or `max_tokens`;
12. convert sampled tokens to output text;
13. free sampler, context, and model through RAII wrappers.

The initial sampler chain supports:

- temperature;
- top-k;
- top-p;
- min-p when available at the pinned revision;
- repeat penalty;
- deterministic/random seed behavior.

The implementation plan must confirm exact sampler function signatures against the pinned `llama.h` before code is written.

## 10. Runtime paths

All default paths resolve relative to the agent directory, not the caller's current working directory.

`run.sh` determines its own directory and executes the built binary from there. The C++ application receives or derives the agent root and resolves:

```text
config/agent.lua
models/*.gguf
```

from that root.

A copied agent directory therefore works from any parent filesystem location.

## 11. Build strategy

CMake is the build system.

Normal offline configuration searches in this order:

### llama.cpp

1. `-DLLAMA_CPP_DIR=/absolute/local/path` when supplied;
2. `third_party/llama.cpp` inside the agent directory.

### Lua

1. `-DLUA_DIR=/absolute/local/path` when supplied;
2. `third_party/lua` inside the agent directory.

If a required source tree is absent, CMake fails with a clear provisioning message. It never downloads the dependency automatically.

Lua is compiled into the agent build as a static library from the supplied source tree. llama.cpp is included using its CMake project and linked directly to the agent executable.

Baseline build requirements:

- CMake 3.24+;
- a C++20 compiler;
- POSIX shell for provided convenience scripts on macOS/Linux;
- optional platform GPU toolchain depending on the llama.cpp backend chosen by the user.

Initial CMake presets:

- `cpu-release` — portable CPU-oriented release build;
- `native-release` — release build that allows local/native compiler optimization;
- GPU-specific build flags remain explicit CMake cache options rather than being silently enabled.

## 12. Model provisioning

`models/` never contains a tracked GGUF weight file.

`models/README.md` documents two phases:

### Connected provisioning phase

On a machine that has internet access, obtain:

```text
bartowski/ai9stars_G9v3-3B-GGUF
ai9stars_G9v3-3B-Q4_K_M.gguf
```

and copy it to:

```text
models/ai9stars_G9v3-3B-Q4_K_M.gguf
```

The documentation may provide example download commands for convenience, but no download command is executed by the agent or its build scripts.

### Offline runtime phase

Once dependencies and model weights are present, configuring, compiling, testing, and running the agent must not require internet access.

## 13. Sandbox contract behavior

Input is exactly one JSON object read from stdin.

Required:

```json
{
  "prompt": "Explain closures in Lua"
}
```

Accepted optional fields:

```json
{
  "prompt": "Continue",
  "context": [
    {"role": "user", "content": "What is a closure?"},
    {"role": "assistant", "content": "..."}
  ],
  "options": {}
}
```

In v0.1, `options` is preserved and exposed to Lua but does not directly override native generation settings unless Lua explicitly chooses to use it.

Success output:

```json
{
  "output": "...",
  "model": "ai9stars/G9v3-3B",
  "usage": {
    "prompt_tokens": 42,
    "completion_tokens": 187
  },
  "metadata": {
    "agent": "g9v3-3b-llamacpp-lua",
    "runtime": "llama.cpp",
    "contract_version": 1
  }
}
```

`stdout` contains only the final JSON object and a trailing newline.

Diagnostics, model-loading progress, timing, and errors go to `stderr`.

## 14. Errors and exit codes

The application uses explicit categories rather than aborting on recoverable configuration/input failures.

Baseline exit codes:

```text
0  success
2  invalid stdin JSON or invalid contract request
3  Lua configuration/hook failure
4  model file missing or model-load failure
5  chat-template/tokenization/context failure
6  inference/generation failure
10 internal/unexpected failure
```

When possible, failures emit a JSON error object to stdout before exiting non-zero:

```json
{
  "error": "model file not found: models/ai9stars_G9v3-3B-Q4_K_M.gguf",
  "metadata": {
    "agent": "g9v3-3b-llamacpp-lua"
  }
}
```

Fatal failures that occur before contract serialization is possible may write only to stderr and return non-zero, which remains compatible with the repository contract.

## 15. Resource ownership

All C APIs with explicit allocation/free lifetimes are wrapped using C++ RAII.

This includes:

- `lua_State`;
- `llama_model`;
- `llama_context`;
- `llama_sampler`.

No long-lived global mutable runtime is required for v0.1. One process handles one stdin request, produces one response, and exits.

This deliberately favors correctness and testability over amortized model-loading performance. Persistent/repl/server modes can be separate later experiments.

## 16. Tests

### Pure unit tests

Tests must run without model weights and without network access.

`test_json.cpp` covers:

- all JSON primitive types;
- nested arrays/objects;
- escaping;
- Unicode escapes;
- malformed/truncated input;
- trailing garbage;
- serialization round-trips.

`test_contract.cpp` covers:

- minimal valid request;
- request with context/options;
- missing prompt;
- empty prompt;
- incorrect field types;
- success-response serialization;
- error-response serialization.

`test_lua_agent.cpp` covers:

- loading the default Lua configuration from a fixture/copy;
- typed model/generation settings;
- `before_prompt` transformation;
- context-to-message construction;
- `after_response` transformation;
- Lua syntax/runtime errors;
- malformed configuration tables.

### Build integration test

CMake/CTest must build the executable and unit-test binary against locally supplied Lua and llama.cpp source trees.

### Model smoke test

A separate opt-in test is documented for environments where the GGUF is present. It sends a tiny contract request and validates:

- successful local model load;
- valid chat formatting/tokenization;
- generation of at least one completion token;
- parseable contract-v1 JSON output;
- zero network dependency.

The smoke test is not part of the default unit suite because the repository does not store the ~1.90 GB model.

## 17. Agent metadata

`agent.yaml` identifies this experiment without adding runtime coupling:

```yaml
schema_version: 1
name: g9v3-3b-llamacpp-lua
version: 0.1.0
execution_mode: local
runtime: llama.cpp+lua
entrypoint: ./run.sh
contract_version: 1
model:
  provider: ai9stars
  name: G9v3-3B
environment: []
tags:
  - cpp
  - lua
  - llama.cpp
  - gguf
  - offline
```

No API key or network-related environment variable is required.

## 18. Security/offline interpretation

The agent itself contains no networking implementation and requires no network service. It does not bind ports, open HTTP clients, fetch remote files, or call cloud APIs.

This is distinct from claiming that the process is cryptographically or OS-enforced incapable of networking. C/C++ and Lua execute as the current OS user, and an OS-level air-gap/firewall/sandbox remains an external deployment property.

For the project's intended offline tests, the relevant success criterion is stronger and simpler: with dependencies and GGUF already provisioned, unplugging network access must not change build/test/runtime behavior.

## 19. Documentation deliverables

The agent README must explain:

1. architecture and Lua/C++ split;
2. exact dependency baselines;
3. how to provision llama.cpp, Lua, and the GGUF;
4. offline build commands;
5. CPU build first-run commands;
6. optional GPU configuration pointers without assuming one platform;
7. Lua configuration reference;
8. sandbox stdin/stdout examples;
9. test commands;
10. copying the directory outside the repository;
11. known v0.1 limitations.

`THIRD_PARTY.md` records at least:

- llama.cpp source/revision and license identity;
- Lua source/release and license identity;
- G9v3-3B logical model source and model license;
- GGUF quantization source.

## 20. Success criteria

The implementation is successful when:

- all new agent code is contained under `agents/g9v3-3b-llamacpp-lua/`;
- repository validation finds no cross-directory runtime dependency;
- no build/run path performs automatic network access;
- a clean local dependency/model provisioning can be performed before going offline;
- the project builds using the pinned/compatible llama.cpp and Lua source trees;
- unit tests pass without the GGUF;
- the opt-in smoke test works when the GGUF is locally present;
- the executable consumes contract-v1 JSON on stdin and emits contract-v1 JSON on stdout;
- stdout contains no llama.cpp logs or non-JSON chatter;
- Lua can change system prompt, message construction, and generation parameters without recompiling C++;
- direct llama.cpp C API inference is used rather than a CLI subprocess;
- copying only the agent directory does not introduce references to repository sibling directories;
- network disconnection does not affect normal build/test/run after provisioning.

## 21. Deferred follow-up experiments

Potential later agents or versions may explore:

- tool-calling and a Lua-managed tool loop;
- persistent model process / interactive REPL;
- structured/grammar-constrained outputs;
- validated thinking-mode control;
- model/runtime benchmarking adapters;
- Metal, CUDA, Vulkan, SYCL, or other backend-specific presets;
- memory/RAG;
- multiple local models or router agents;
- embedded Lua tools with explicit capability controls.

These are deliberately not required to prove v0.1.

## 22. Reference material used for the design

- G9v3-3B model: `https://huggingface.co/ai9stars/G9v3-3B`
- G9v3-3B GGUF quantizations: `https://huggingface.co/bartowski/ai9stars_G9v3-3B-GGUF`
- llama.cpp: `https://github.com/ggml-org/llama.cpp`
- pinned llama.cpp revision: `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`
- Lua: `https://www.lua.org/`
- Lua v0.1 baseline: `5.4.9`
