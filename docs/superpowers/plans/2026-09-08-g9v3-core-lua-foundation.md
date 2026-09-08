# G9 Coding Agent Core + Lua Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the self-contained C++20 project foundation, JSON/contract layer, restricted embedded Lua policy runtime, and exact G9v3 prompt renderer for `agents/g9v3-3b-llamacpp-lua/`.

**Architecture:** The agent is split into small C++ libraries with no repository-root runtime dependency. `g9_core` owns JSON and contract types; `g9_lua` owns the Lua VM and model-specific policy/prompt rendering. Full llama.cpp inference is deliberately left to the later runtime plan, but this plan produces independently testable parsing, configuration, and prompt-rendering behavior.

**Tech Stack:** C++20, CMake 3.24+, Lua 5.4.9 source supplied locally, POSIX shell scripts, CTest. No package manager, no FetchContent, no network fetches.

**Spec:** `docs/superpowers/specs/2026-09-08-g9v3-3b-llamacpp-lua-design.md`

## Global Constraints

- All runtime code for this agent lives under `agents/g9v3-3b-llamacpp-lua/`.
- The project must remain copyable outside `local-model-sandbox` without sibling-directory dependencies.
- C++ standard is C++20.
- CMake minimum version is 3.24.
- Lua baseline is 5.4.9 and is supplied locally through `-DLUA_DIR=...` or `third_party/lua/`.
- No CMake `FetchContent`, package-manager install, curl, git clone, or remote lookup may run automatically.
- Runtime/build code must not contain HTTP clients, model downloaders, telemetry, or update checks.
- Contract-mode stdout is reserved for exactly one final JSON object plus newline.
- Lua opens only base/table/string/math/UTF-8 libraries; `io`, `os`, `package`, and `debug` remain unavailable.
- After base-library initialization, `print`, `dofile`, `loadfile`, and `load` are removed.
- G9v3 prompt rendering is performed in Lua and starts with exactly one `<s>` BOS token text.
- Default model path is `models/ai9stars_G9v3-3B-Q4_K_M.gguf` and model weights are gitignored.

---

## File map

Create the following foundation files:

```text
agents/g9v3-3b-llamacpp-lua/
├── .gitignore
├── agent.yaml
├── CMakeLists.txt
├── CMakePresets.json
├── build.sh
├── run.sh
├── config/agent.lua
├── src/error.hpp
├── src/json.hpp
├── src/json.cpp
├── src/contract.hpp
├── src/contract.cpp
├── src/lua_agent.hpp
├── src/lua_agent.cpp
├── src/main.cpp
├── tests/CMakeLists.txt
├── tests/test_json.cpp
├── tests/test_contract.cpp
├── tests/test_lua_agent.cpp
├── models/.gitkeep
├── models/README.md
├── third_party/lua/.gitkeep
├── third_party/llama.cpp/.gitkeep
└── third_party/README.md
```

Responsibilities are intentionally narrow: JSON and contract code do not include Lua; Lua code does not know llama.cpp; `main.cpp` is only a temporary thin process entrypoint until the later CLI/runtime plan replaces its orchestration.

---

### Task 1: Project scaffold and offline CMake boundary

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/.gitignore`
- Create: `agents/g9v3-3b-llamacpp-lua/agent.yaml`
- Create: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Create: `agents/g9v3-3b-llamacpp-lua/CMakePresets.json`
- Create: `agents/g9v3-3b-llamacpp-lua/build.sh`
- Create: `agents/g9v3-3b-llamacpp-lua/run.sh`
- Create: `agents/g9v3-3b-llamacpp-lua/src/error.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/main.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/CMakeLists.txt`
- Create: `agents/g9v3-3b-llamacpp-lua/models/.gitkeep`
- Create: `agents/g9v3-3b-llamacpp-lua/models/README.md`
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/lua/.gitkeep`
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/llama.cpp/.gitkeep`
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/README.md`

**Interfaces:**
- Consumes: repository metadata contract only as documentation; no runtime import.
- Produces: CMake targets `g9_core`, `g9_lua`, `g9-agent`, and a CTest-enabled test tree; `g9::Error` and `g9::ExitCode` declarations for later tasks.

- [ ] **Step 1: Add metadata and ignore rules**

Create `agent.yaml` exactly with the repository-required keys:

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
  - coding-agent
```

Create `.gitignore` with:

```gitignore
/build/
/models/*.gguf
/third_party/llama.cpp/*
!/third_party/llama.cpp/.gitkeep
/third_party/lua/*
!/third_party/lua/.gitkeep
*.log
```

- [ ] **Step 2: Define error categories**

Create `src/error.hpp` with these exact public types:

```cpp
#pragma once
#include <stdexcept>
#include <string>

namespace g9 {

enum class ExitCode : int {
    success = 0,
    invalid_input = 2,
    lua_failure = 3,
    model_failure = 4,
    context_failure = 5,
    inference_failure = 6,
    tool_protocol_failure = 7,
    policy_denied = 8,
    process_failure = 9,
    internal_failure = 10,
};

class Error final : public std::runtime_error {
public:
    Error(ExitCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}
    [[nodiscard]] ExitCode code() const noexcept { return code_; }
private:
    ExitCode code_;
};

} // namespace g9
```

- [ ] **Step 3: Define CMake dependency discovery without downloads**

The root `CMakeLists.txt` must:

```cmake
cmake_minimum_required(VERSION 3.24)
project(g9_coding_agent VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(G9_BUILD_TESTS "Build unit tests" ON)
set(LUA_DIR "" CACHE PATH "Path to Lua 5.4 source directory")
set(LLAMA_CPP_DIR "" CACHE PATH "Path to llama.cpp source directory")

if(NOT LUA_DIR)
  set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua")
endif()

if(NOT EXISTS "${LUA_DIR}/lua.h")
  message(FATAL_ERROR "Lua 5.4 source not found. Set -DLUA_DIR=/path/to/lua-5.4.9/src or populate third_party/lua.")
endif()
```

Build Lua as a static target from the official source files while excluding `lua.c` and `luac.c`. Do not call `find_package(Lua)` because the portability baseline is the supplied source tree.

Declare empty `g9_core`/`g9_lua` targets only after their source files exist in Tasks 2-4. The full llama.cpp target is intentionally not added in this plan; preserve the `LLAMA_CPP_DIR` cache variable for the runtime plan.

- [ ] **Step 4: Add reproducible presets and scripts**

`CMakePresets.json` must contain configure presets `cpu-release` and `native-release`, both using an out-of-tree `build/<presetName>` directory and never enabling network features.

`build.sh` must resolve its own directory and run:

```sh
cmake --preset "${1:-cpu-release}"
cmake --build --preset "${1:-cpu-release}"
```

`run.sh` must resolve its own directory and execute the built `g9-agent` path from the selected/default preset without changing to a repository parent directory.

- [ ] **Step 5: Verify provisioning failure is explicit**

Run from the agent directory with no Lua source populated:

```bash
cmake --preset cpu-release
```

Expected: configure exits non-zero with the exact provisioning message beginning `Lua 5.4 source not found.` and performs no network access.

- [ ] **Step 6: Run repository isolation validator**

From repository root:

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed`.

- [ ] **Step 7: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua
git commit -m "feat: scaffold portable G9 coding agent"
```

---

### Task 2: Self-contained JSON parser and serializer

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/json.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/json.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_json.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: standard library only.
- Produces: `g9::JsonValue`, `g9::parse_json(std::string_view)`, `g9::serialize_json(const JsonValue&)`.

- [ ] **Step 1: Define the JSON value API**

Create `json.hpp` with this public shape:

```cpp
namespace g9 {

struct JsonValue {
    using array = std::vector<JsonValue>;
    using object = std::map<std::string, JsonValue, std::less<>>;
    using storage = std::variant<std::nullptr_t, bool, double, std::string, array, object>;
    storage value;

    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] const object& as_object() const;
    [[nodiscard]] const array& as_array() const;
    [[nodiscard]] const std::string& as_string() const;
};

JsonValue parse_json(std::string_view text);
std::string serialize_json(const JsonValue& value);

} // namespace g9
```

- [ ] **Step 2: Write failing parser tests**

`test_json.cpp` must assert at least:

```cpp
CHECK(parse_json("null").value == JsonValue::storage{nullptr});
CHECK(parse_json("true").value == JsonValue::storage{true});
CHECK(parse_json("-12.5e2").value == JsonValue::storage{-1250.0});
CHECK(parse_json("\"a\\n\\u00e9\"").as_string() == "a\né");
CHECK(parse_json("\"\\uD83D\\uDE80\"").as_string() == "🚀");
CHECK_THROWS(parse_json("{\"a\":1,\"a\":2}"));
CHECK_THROWS(parse_json("01"));
CHECK_THROWS(parse_json("true false"));
CHECK_THROWS(parse_json("\"\\uD800\""));
```

Use the project-local minimal test harness defined in `tests/test_support.hpp` rather than introducing Catch2/GoogleTest. `test_support.hpp` should provide `CHECK`, `CHECK_EQ`, and `CHECK_THROWS` macros plus a test count/failure count.

- [ ] **Step 3: Run tests and confirm red state**

```bash
ctest --test-dir build/cpu-release -R json --output-on-failure
```

Expected: compile or link failure because JSON functions are not implemented yet.

- [ ] **Step 4: Implement strict parser**

Implement recursive descent in `json.cpp` with explicit functions for value/object/array/string/number. Requirements:

```text
- reject duplicate object keys
- reject non-finite numbers
- reject leading-zero integer forms except literal 0
- decode \uXXXX, including valid UTF-16 surrogate pairs to UTF-8
- reject lone/misordered surrogates
- reject unescaped control bytes in strings
- reject trailing non-whitespace bytes
- nesting depth hard cap: 256
```

- [ ] **Step 5: Implement canonical serializer**

Objects serialize in `std::map` key order. Strings escape control bytes, quotes, and backslashes. NaN/Inf are rejected with `g9::Error(ExitCode::invalid_input, ...)`.

- [ ] **Step 6: Run JSON tests**

```bash
ctest --test-dir build/cpu-release -R json --output-on-failure
```

Expected: all JSON tests pass.

- [ ] **Step 7: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/json.* agents/g9v3-3b-llamacpp-lua/tests
git commit -m "feat: add self-contained JSON codec"
```

---

### Task 3: Sandbox contract v1 typed boundary

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/contract.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/contract.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_contract.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `JsonValue` from Task 2.
- Produces: `AgentRequest`, `Usage`, `AgentResponse`, `parse_request`, `response_to_json`, `error_to_json`.

- [ ] **Step 1: Define typed contract API**

Create `contract.hpp`:

```cpp
namespace g9 {

struct AgentRequest {
    std::string prompt;
    JsonValue context{JsonValue::array{}};
    JsonValue options{JsonValue::object{}};
};

struct Usage {
    std::int64_t prompt_tokens = 0;
    std::int64_t completion_tokens = 0;
};

struct AgentResponse {
    std::string output;
    std::string model = "ai9stars/G9v3-3B";
    Usage usage;
    JsonValue metadata{JsonValue::object{}};
};

AgentRequest parse_request(const JsonValue& root);
JsonValue response_to_json(const AgentResponse& response);
JsonValue error_to_json(std::string message, JsonValue metadata = JsonValue::object{});

} // namespace g9
```

- [ ] **Step 2: Write failing validation tests**

Cover:

```cpp
CHECK_EQ(parse_request(parse_json(R"({"prompt":"fix it"})")).prompt, "fix it");
CHECK_THROWS(parse_request(parse_json(R"({})")));
CHECK_THROWS(parse_request(parse_json(R"({"prompt":""})")));
CHECK_THROWS(parse_request(parse_json(R"({"prompt":12})")));
CHECK_THROWS(parse_request(parse_json(R"({"prompt":"x","context":{}})")));
CHECK_THROWS(parse_request(parse_json(R"({"prompt":"x","options":[]})")));
```

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R contract --output-on-failure
```

Expected: contract target fails before implementation.

- [ ] **Step 4: Implement parser and serializers**

`parse_request` requires non-empty string `prompt`, array `context` when present, and object `options` when present. Additional top-level fields are preserved only in the raw Lua conversion later; they do not alter native workspace grants.

`response_to_json` must produce exactly `output`, `model`, `usage`, and `metadata` keys. `usage` contains integer-valued JSON numbers for `prompt_tokens` and `completion_tokens`.

- [ ] **Step 5: Run contract tests**

```bash
ctest --test-dir build/cpu-release -R contract --output-on-failure
```

Expected: all contract tests pass.

- [ ] **Step 6: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/contract.* agents/g9v3-3b-llamacpp-lua/tests/test_contract.cpp
git commit -m "feat: implement sandbox contract v1"
```

---

### Task 4: Restricted embedded Lua runtime and typed configuration

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/lua_agent.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/lua_agent.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/config/agent.lua`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_lua_agent.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Lua 5.4 C API, `JsonValue`, `AgentRequest`.
- Produces: `LuaAgent`, `ModelConfig`, `GenerationConfig`, `LoopConfig`, `WorkspacePolicyConfig`, and exact prompt-rendering hooks.

- [ ] **Step 1: Define typed config interfaces**

`lua_agent.hpp` must expose:

```cpp
struct ModelConfig {
    std::filesystem::path path;
    std::uint32_t context_size = 8192;
    std::uint32_t batch_size = 512;
    int gpu_layers = 0;
    int threads = 0;
};

struct GenerationConfig {
    bool thinking = false;
    std::uint32_t max_tokens = 768;
    float temperature = 0.7F;
    float top_p = 0.95F;
    int top_k = 40;
    float min_p = 0.0F;
    float repeat_penalty = 1.0F;
    int repeat_last_n = 64;
    std::int64_t seed = -1;
};

struct LoopConfig {
    std::uint32_t max_steps = 32;
    std::uint32_t max_tool_calls_per_step = 8;
    std::size_t max_tool_result_bytes = 65536;
};

struct ChatMessage {
    std::string role;
    std::string content;
};

class LuaAgent {
public:
    explicit LuaAgent(const std::filesystem::path& config_path,
                      std::function<void(std::string_view)> log_sink);
    ~LuaAgent();
    LuaAgent(LuaAgent&&) noexcept;
    LuaAgent& operator=(LuaAgent&&) noexcept;
    LuaAgent(const LuaAgent&) = delete;
    LuaAgent& operator=(const LuaAgent&) = delete;

    [[nodiscard]] const ModelConfig& model() const noexcept;
    [[nodiscard]] const GenerationConfig& generation() const noexcept;
    [[nodiscard]] const LoopConfig& loop() const noexcept;
    AgentRequest before_prompt(const AgentRequest& request);
    std::vector<ChatMessage> build_messages(const AgentRequest& request);
    std::string render_prompt(const std::vector<ChatMessage>& messages,
                              const GenerationConfig& generation,
                              const JsonValue& tool_schemas,
                              const JsonValue& tool_history);
    GenerationConfig generation_settings(const AgentRequest& request);
    AgentResponse after_response(const AgentResponse& response);
};
```

- [ ] **Step 2: Write failing Lua capability tests**

Create fixture Lua snippets from C++ temporary files and assert:

```text
io == nil
os == nil
package == nil
debug == nil
print == nil
dofile == nil
loadfile == nil
load == nil
```

Also test that `log("hello")` invokes the supplied C++ sink with `hello` and produces no stdout.

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R lua_agent --output-on-failure
```

Expected: failure because `LuaAgent` is not implemented.

- [ ] **Step 4: Implement Lua VM initialization**

Use `luaL_newstate`, then explicitly open only:

```cpp
luaL_requiref(L, "_G", luaopen_base, 1);
luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
```

Set forbidden globals to nil after opening base. Register only host-controlled helpers required by this plan (`log`). Store the returned config table in the Lua registry using a private registry key rather than a mutable global.

- [ ] **Step 5: Implement JSON↔Lua conversion with bounds**

Convert null→nil only when used as scalar value, booleans, finite numbers, strings, arrays as 1-based dense tables, and objects as string-keyed tables. Reject mixed/sparse tables when converting Lua output to JSON. Add recursion depth cap 128.

- [ ] **Step 6: Implement typed config validation**

Reject wrong types and out-of-range settings. Required constraints:

```text
context_size: 256..131072
batch_size: 1..context_size
max_tokens: 1..16384
temperature: 0.0..5.0
top_p/min_p: 0.0..1.0
top_k: 0..100000
repeat_penalty: >0.0..10.0
repeat_last_n: 0..context_size
max_steps: 1..256
max_tool_calls_per_step: 1..64
max_tool_result_bytes: 1024..1048576
```

Model path remains relative to agent root by default; absolute paths may be accepted only when supplied through explicit CLI/model override in the later plan.

- [ ] **Step 7: Implement the shipped `config/agent.lua`**

The default config must include:

```lua
agent.model = {
  path = "models/ai9stars_G9v3-3B-Q4_K_M.gguf",
  context_size = 8192,
  batch_size = 512,
  gpu_layers = 0,
  threads = 0,
}
agent.generation = {
  thinking = false,
  max_tokens = 768,
  temperature = 0.7,
  top_p = 0.95,
  top_k = 40,
  min_p = 0.0,
  repeat_penalty = 1.0,
  repeat_last_n = 64,
  seed = -1,
}
agent.loop = {
  max_steps = 32,
  max_tool_calls_per_step = 8,
  max_tool_result_bytes = 65536,
}
```

It must also include the coding-oriented system prompt from the spec and default workspace/tool/guardrail tables even though later plans consume those fields.

- [ ] **Step 8: Implement exact G9v3 normal-message renderer**

For no tools, `render_prompt` must generate:

```text
<s><|im_start|>system\nSYSTEM<|im_end|>\n<|im_start|>user\nUSER<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n
```

for non-thinking mode, and the same prefix ending in:

```text
<|im_start|>assistant\n<think>\n
```

for thinking mode.

Historical assistant messages without explicit `<think>` tags receive an empty `<think>\n\n</think>\n\n` block before their visible content.

- [ ] **Step 9: Test request hooks and renderer byte-for-byte**

Tests must compare the complete rendered string, including newlines and exactly one leading `<s>`. Also verify a custom fixture can transform prompt text in `before_prompt`, modify generation settings, and alter visible output in `after_response`.

- [ ] **Step 10: Run Lua suite**

```bash
ctest --test-dir build/cpu-release -R lua_agent --output-on-failure
```

Expected: all Lua tests pass.

- [ ] **Step 11: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/config agents/g9v3-3b-llamacpp-lua/src/lua_agent.* agents/g9v3-3b-llamacpp-lua/tests/test_lua_agent.cpp
git commit -m "feat: embed restricted Lua agent policy"
```

---

### Task 5: Foundation verification and documentation stub

**Files:**
- Modify: `agents/g9v3-3b-llamacpp-lua/src/main.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/README.md`
- Create: `agents/g9v3-3b-llamacpp-lua/THIRD_PARTY.md`

**Interfaces:**
- Consumes: Tasks 1-4.
- Produces: a diagnostic foundation executable and exact dependency/provisioning documentation that later plans extend.

- [ ] **Step 1: Make `g9-agent version` and `g9-agent doctor` minimally usable**

Until the later CLI plan replaces the parser, `main.cpp` accepts exactly:

```text
g9-agent version
g9-agent doctor --agent-root PATH
```

`version` prints `g9-agent 0.1.0` to stdout. `doctor` validates that `config/agent.lua` loads and reports the configured model path to stderr/stdout in a simple human-readable form; it must not attempt model loading or network access.

- [ ] **Step 2: Document provisioning boundaries**

`README.md` must already state:

```text
llama.cpp baseline: f3f1a8f2760f28325a5ec20c05b171e5b7c83a29
Lua baseline: 5.4.9
model: bartowski/ai9stars_G9v3-3B-GGUF / ai9stars_G9v3-3B-Q4_K_M.gguf
```

Document that dependencies/model are provisioned before offline use and that build scripts never download them.

`THIRD_PARTY.md` records source URL/revision/version and license identity for llama.cpp, Lua, G9v3-3B, and the GGUF quantization source; do not assign a license to the user's new source code.

- [ ] **Step 3: Run all available foundation tests**

```bash
ctest --test-dir build/cpu-release --output-on-failure
```

Expected: JSON, contract, and Lua tests all pass.

- [ ] **Step 4: Run repo isolation validation**

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed`.

- [ ] **Step 5: Scan for prohibited automatic network behavior**

Run:

```bash
grep -RInE 'FetchContent|ExternalProject_Add|curl |wget |https?://.*download|git clone' agents/g9v3-3b-llamacpp-lua --exclude=README.md --exclude=THIRD_PARTY.md
```

Expected: no build/runtime implementation matches.

- [ ] **Step 6: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua
git commit -m "docs: document G9 agent foundation"
```

---

## Plan 1 completion gate

Before moving to the tools/guardrails plan, verify all of the following:

```text
[ ] project config never fetches dependencies
[ ] JSON codec tests are green
[ ] contract tests are green
[ ] restricted Lua capability tests are green
[ ] G9v3 prompt renderer tests compare exact bytes
[ ] default config loads from an agent-local path
[ ] repository isolation validator is green
[ ] README/THIRD_PARTY identify all external baselines
```

Do not start llama.cpp inference, filesystem tools, command execution, or the interactive CLI in this plan.