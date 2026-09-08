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
        -> embedded Lua policy/configuration/prompt rendering
        -> llama.cpp
        -> local G9v3-3B GGUF
        -> JSON response
```

Version 0.1 includes:

- C++20 application core;
- embedded Lua for configuration and agent behavior;
- G9v3-specific prompt rendering implemented in Lua from the upstream template semantics;
- direct `libllama` integration, not a `llama-cli` subprocess;
- local GGUF model loading only;
- thinking/non-thinking prompt selection in Lua;
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
- a generic chat-template engine for arbitrary models;
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

The implementation targets the C API available at that revision, including:

- `llama_model_load_from_file`;
- `llama_model_get_vocab`;
- `llama_init_from_model`;
- `llama_tokenize`;
- `llama_decode`;
- `llama_token_to_piece`;
- `llama_vocab_is_eog`;
- `llama_sampler_chain_init`;
- `llama_sampler_init_penalties`;
- `llama_sampler_init_top_k`;
- `llama_sampler_init_top_p`;
- `llama_sampler_init_min_p`;
- `llama_sampler_init_temp`;
- `llama_sampler_init_dist`;
- `llama_sampler_sample`.

The revision is pinned because `llama.cpp` evolves quickly. A newer revision may work but is not the v0.1 reproducibility baseline.

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

The runtime default context is 8192 tokens rather than attempting the model's theoretical maximum. Users may raise it in Lua based on available memory and the capabilities of the chosen llama.cpp build.

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
    ├── llama.cpp/
    │   └── .gitkeep
    └── lua/
        └── .gitkeep
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
4. run Lua request hooks;
5. ask Lua for the semantic message list;
6. ask Lua to render the G9v3 prompt text;
7. obtain effective generation settings from Lua;
8. initialize/load the local llama.cpp runtime;
9. tokenize/evaluate the rendered prompt;
10. generate the completion;
11. run Lua response postprocessing;
12. serialize exactly one JSON response to stdout.

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

It supports:

- null;
- booleans;
- finite JSON numbers;
- strings with escaping and Unicode escape decoding, including UTF-16 surrogate pairs;
- arrays;
- objects.

The parser rejects malformed input, duplicate syntax errors, invalid number forms, trailing garbage, invalid escape sequences, invalid surrogate pairs, and structurally invalid contract requests. No external JSON package is introduced in v0.1.

### `lua_agent.*`

Owns Lua VM lifecycle and the C++/Lua boundary.

It loads `config/agent.lua`, validates the returned table, reads typed configuration, converts request/context data into Lua values, invokes supported hooks, and converts Lua-produced structures back to C++.

Lua errors are captured with useful stack/error text and returned as application errors without crashing the process.

### `llama_runtime.*`

Owns all llama.cpp resources using RAII and exposes a small C++ interface independent from Lua.

Responsibilities:

- backend initialization;
- local model loading;
- vocabulary access;
- context creation;
- tokenization of the already-rendered G9v3 prompt;
- prompt evaluation;
- sampler construction;
- token generation;
- EOG detection;
- token-to-text conversion;
- prompt/completion token accounting;
- cleanup.

It does not contain G9v3 prompt syntax, system-prompt policy, or other agent personality decisions.

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
    thinking = false,
    max_tokens = 512,
    temperature = 0.7,
    top_p = 0.95,
    top_k = 40,
    min_p = 0.0,
    repeat_penalty = 1.0,
    repeat_last_n = 64,
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

local function append_message(parts, message)
    local role = message.role
    local content = message.content or ""

    table.insert(parts, "<|im_start|>" .. role .. "\n")

    if role == "assistant" and
       not string.find(content, "<think>", 1, true) and
       not string.find(content, "</think>", 1, true) then
        table.insert(parts, "<think>\n\n</think>\n\n")
    end

    table.insert(parts, content)
    table.insert(parts, "<|im_end|>\n")
end

function agent.render_prompt(messages)
    local parts = {}

    for _, message in ipairs(messages) do
        append_message(parts, message)
    end

    table.insert(parts, "<|im_start|>assistant\n")

    if agent.generation.thinking then
        table.insert(parts, "<think>\n")
    else
        table.insert(parts, "<think>\n\n</think>\n\n")
    end

    return table.concat(parts)
end

function agent.generation_settings(request)
    return agent.generation
end

function agent.after_response(response)
    return response
end

return agent
```

The default renderer is intentionally a model-specific subset of the upstream G9v3 chat template. v0.1 supports normal system/user/assistant conversational messages but not tool-call branches from the upstream template.

### Supported hooks

#### `before_prompt(request)`

Optional. Receives a Lua request table and may return a replacement request table.

#### `build_messages(request)`

Required in the default policy. Returns an ordered array of chat messages. Accepted v0.1 roles are `system`, `user`, and `assistant`.

#### `render_prompt(messages)`

Required. Converts semantic messages into the exact text tokenized by llama.cpp. This is where model-specific prompt syntax lives.

The default G9v3 renderer mirrors the relevant upstream template behavior:

- ChatML-style `<|im_start|>` / `<|im_end|>` message boundaries;
- assistant generation begins at `<|im_start|>assistant\n`;
- non-thinking mode inserts an empty `<think>\n\n</think>\n\n` block;
- thinking mode starts an open `<think>\n` block;
- historical assistant messages without explicit think markup receive an empty think block before their visible content.

#### `generation_settings(request)`

Required by the default policy. Returns the effective sampling/runtime generation table, allowing future request-aware settings without recompiling C++.

The upstream recommendations are:

```text
thinking=false: temperature=0.7, top_p=0.95
thinking=true:  temperature=0.9, top_p=0.95
```

The shipped default is non-thinking. If a user switches `thinking=true`, the README instructs them to use the upstream 0.9 temperature recommendation unless they deliberately want a different experiment.

#### `after_response(response)`

Optional. Receives a Lua table containing generated text plus metadata and may return a replacement response table. The C++ layer still enforces the final sandbox output contract.

### Restricted Lua environment

Lua is an agent policy language, not an escape hatch around the offline design.

The host explicitly opens only:

- base library;
- table library;
- string library;
- math library;
- UTF-8 library.

It does not open:

- `io`;
- `os`;
- `package`;
- `debug`.

After opening the base library, the host removes `print`, `dofile`, `loadfile`, and `load` from the global environment. `require` is unavailable because `package` is not opened.

The host exposes one custom function:

```lua
log("message")
```

which writes a prefixed line to stderr only. Lua cannot write contract-breaking text to stdout through the shipped API.

This is capability reduction, not an OS security boundary. Hard process sandboxing remains outside v0.1.

## 8. G9v3 prompt fidelity and thinking mode

The upstream `chat_template.jinja` contains model-specific tool branches and thinking behavior. In particular, for a new assistant generation it conditionally emits:

```text
thinking=false -> <think>\n\n</think>\n\n
thinking=true  -> <think>\n
```

after the assistant message prefix.

The public `llama_chat_apply_template` C interface at the pinned llama.cpp revision does not expose a generic arbitrary-Jinja-variable map equivalent to Transformers' `enable_thinking` argument. Using it without controlling that variable would make the intended non-thinking semantics ambiguous.

Therefore the first agent deliberately renders its G9v3 prompt in Lua rather than relying on generic llama.cpp chat-template application.

This design has two advantages:

1. the G9v3-specific behavior is explicit and testable;
2. model policy remains in Lua, matching the project's goal that configurable agent behavior live outside the C++ inference engine.

The renderer is not advertised as a complete clone of the upstream tool-calling template. Tool branches are deferred until tool calling itself becomes an experiment goal.

## 9. llama.cpp generation design

The implementation follows the direct C API pattern demonstrated by current llama.cpp examples:

1. call `ggml_backend_load_all()`;
2. obtain `llama_model_default_params()`;
3. set `n_gpu_layers` from Lua;
4. call `llama_model_load_from_file()` with the local GGUF path;
5. obtain vocabulary using `llama_model_get_vocab()`;
6. tokenize the Lua-rendered prompt with `llama_tokenize()`;
7. create `llama_context_params` using Lua context/batch settings;
8. create context with `llama_init_from_model()`;
9. evaluate the prompt with `llama_decode()`;
10. sample one token at a time through a `llama_sampler` chain;
11. stop at EOG or `max_tokens`;
12. convert sampled tokens with `llama_token_to_piece()`;
13. free sampler, context, and model through RAII wrappers.

The initial sampler chain is explicitly based on APIs present at the pinned revision:

```text
llama_sampler_init_penalties(...)
llama_sampler_init_top_k(top_k)
llama_sampler_init_top_p(top_p, 1)
llama_sampler_init_min_p(min_p, 1)
llama_sampler_init_temp(temperature)
llama_sampler_init_dist(seed)
```

`seed < 0` maps to `LLAMA_DEFAULT_SEED`; non-negative seeds are converted to the unsigned seed type expected by the sampler.

The penalties sampler uses:

```text
n_vocab = llama_vocab_n_tokens(vocab)
penalty_last_n = repeat_last_n
penalty_repeat = repeat_penalty
penalty_freq = 0.0
penalty_present = 0.0
```

Samplers whose configured values disable their effect may still be added when llama.cpp defines them as no-ops, or may be omitted; this must not alter output semantics.

## 10. Runtime paths

All default paths resolve relative to the agent directory, not the caller's current working directory.

`run.sh` determines its own directory and executes the built binary from there. It passes the absolute agent root to the executable using a command-line argument reserved for the launcher, not an environment dependency.

The C++ application resolves:

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

Lua is compiled into the agent build as a static library from the supplied Lua 5.4 source directory, excluding the standalone interpreter/compiler entrypoints (`lua.c` and `luac.c`). llama.cpp is included using its CMake project and linked directly to the agent executable.

Baseline build requirements:

- CMake 3.24+;
- a C++20 compiler;
- POSIX shell for provided convenience scripts on macOS/Linux;
- optional platform GPU toolchain depending on the llama.cpp backend chosen by the user.

Initial CMake presets:

- `cpu-release` — release build with GPU backends disabled where practical through llama.cpp CMake options;
- `native-release` — release build allowing the platform's default/local llama.cpp backend selection and native optimization.

GPU-specific experiments beyond those presets remain explicit CMake cache settings rather than hidden downloads or runtime detection services.

## 12. Model and dependency provisioning

`models/` never contains a tracked GGUF weight file. `third_party/llama.cpp/` and `third_party/lua/` likewise do not vendor full external source trees into this repository by default.

The agent-local `.gitignore` ignores:

```text
build/
models/*.gguf
third_party/llama.cpp/*
third_party/lua/*
```

while preserving the `.gitkeep` placeholders and documentation.

### Connected provisioning phase

Before entering an air-gapped environment, obtain:

```text
llama.cpp commit f3f1a8f2760f28325a5ec20c05b171e5b7c83a29
Lua 5.4.9 source
bartowski/ai9stars_G9v3-3B-GGUF / ai9stars_G9v3-3B-Q4_K_M.gguf
```

Then either place sources under the agent's `third_party/` directories or keep them in arbitrary local paths and pass those paths to CMake.

Place the model at:

```text
models/ai9stars_G9v3-3B-Q4_K_M.gguf
```

Documentation may provide example download commands, but no download command is executed by the agent or its build scripts.

### Offline build/runtime phase

Once dependencies and model weights are present, configuring, compiling, unit testing, model smoke testing, and running the agent require no internet access.

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

`context` is exposed to Lua as JSON-derived values. The shipped `build_messages` uses context items that contain string `role` and string `content` fields. Other context shapes remain available to custom Lua policy.

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
    "contract_version": 1,
    "thinking": false
  }
}
```

`stdout` contains only the final JSON object and a trailing newline.

Diagnostics, Lua `log()` output, model-loading progress, timing, and errors go to `stderr`.

llama.cpp logging is redirected/configured so library logs cannot contaminate stdout.

## 14. Errors and exit codes

The application uses explicit categories rather than aborting on recoverable configuration/input failures.

Baseline exit codes:

```text
0  success
2  invalid stdin JSON or invalid contract request
3  Lua configuration/hook/prompt-render failure
4  model file missing or model-load failure
5  tokenization/context failure
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

Tests require the locally provisioned source dependencies but do not require model weights or network access.

`test_json.cpp` covers:

- all JSON primitive types;
- nested arrays/objects;
- escaping;
- Unicode escapes and surrogate pairs;
- malformed/truncated input;
- malformed numbers;
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
- exact non-thinking G9v3 prompt rendering;
- exact thinking G9v3 prompt rendering;
- assistant-history rendering;
- `generation_settings` values;
- `after_response` transformation;
- Lua syntax/runtime errors;
- malformed configuration tables;
- absence of `io`, `os`, `package`, `debug`, `print`, `dofile`, `loadfile`, and `load`;
- `log()` writing only through the host's stderr callback.

### Build integration test

CMake/CTest builds the executable and unit-test binary against locally supplied Lua and llama.cpp source trees.

### Model smoke test

A separate opt-in test is documented for environments where the GGUF is present. It sends a tiny contract request and validates:

- successful local model load;
- G9v3 prompt tokenization;
- generation of at least one completion token;
- parseable contract-v1 JSON output;
- no non-JSON stdout contamination.

The smoke test is not part of the default unit suite because the repository does not store the ~1.90 GB model.

### Offline verification

Deployment/testing documentation includes a final manual check: run configure, build, unit tests, and the model smoke test with network disabled after all provisioning is complete. No step is allowed to attempt automatic fetching.

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

The shipped agent contains no networking implementation and requires no network service. It does not bind ports, open HTTP clients, fetch remote files, call cloud APIs, or expose process-execution helpers to Lua.

This is distinct from claiming that the process is cryptographically or OS-enforced incapable of networking. C/C++ executes as the current OS user, and an OS-level air-gap/firewall/sandbox remains an external deployment property.

For this experiment, the concrete offline criterion is: with dependency sources and the GGUF already provisioned, disabling network access does not change configure/build/test/runtime behavior.

## 19. Documentation deliverables

The agent README explains:

1. architecture and Lua/C++ split;
2. exact dependency baselines;
3. how to provision llama.cpp, Lua, and the GGUF;
4. offline build commands;
5. CPU build first-run commands;
6. optional GPU configuration pointers without assuming one platform;
7. Lua configuration and hook reference;
8. G9v3 prompt renderer and thinking toggle;
9. sandbox stdin/stdout examples;
10. test commands;
11. copying the directory outside the repository;
12. known v0.1 limitations.

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
- stdout contains no llama.cpp or Lua logs or non-JSON chatter;
- Lua can change system prompt, message construction, prompt rendering, thinking mode, and generation parameters without recompiling C++;
- direct llama.cpp C API inference is used rather than a CLI subprocess;
- copying only the agent directory does not introduce references to repository sibling directories;
- network disconnection does not affect normal build/test/run after provisioning.

## 21. Deferred follow-up experiments

Potential later agents or versions may explore:

- complete G9v3 tool-calling template support and a Lua-managed tool loop;
- persistent model process / interactive REPL;
- structured/grammar-constrained outputs;
- generic model chat-template abstraction;
- model/runtime benchmarking adapters;
- Metal, CUDA, Vulkan, SYCL, or other backend-specific presets;
- memory/RAG;
- multiple local models or router agents;
- embedded Lua tools with explicit capability controls.

These are deliberately not required to prove v0.1.

## 22. Reference material used for the design

- G9v3-3B model: `https://huggingface.co/ai9stars/G9v3-3B`
- G9v3-3B upstream prompt template: `https://huggingface.co/ai9stars/G9v3-3B/blob/main/chat_template.jinja`
- G9v3-3B GGUF quantizations: `https://huggingface.co/bartowski/ai9stars_G9v3-3B-GGUF`
- llama.cpp: `https://github.com/ggml-org/llama.cpp`
- pinned llama.cpp revision: `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`
- Lua: `https://www.lua.org/`
- Lua v0.1 baseline: `5.4.9`
