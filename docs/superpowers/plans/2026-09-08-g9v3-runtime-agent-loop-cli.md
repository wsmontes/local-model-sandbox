# G9 Runtime + Agent Loop + CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the local coding agent by integrating pinned llama.cpp inference, G9v3 tool-call protocol, session/tool loop, robust interactive CLI, terminal approvals, doctor diagnostics, contract mode, and end-to-end documentation.

**Architecture:** A single process owns one loaded model and one task/session. Lua renders the complete G9v3 conversation and tool schemas; C++ performs llama.cpp inference, parses model tool calls, routes them through the policy-aware ToolRegistry from Plan 2, appends structured tool results, and repeats. Human interaction is a terminal frontend only; contract mode uses the same core without interactive prompts.

**Tech Stack:** C++20, CMake 3.24+, pinned llama.cpp commit `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`, Lua 5.4.9, vendored antirez/linenoise commit `a473823d74b93eab2ba83480df16ed37617493f2`, ANSI/VT100 rendering, CTest.

**Spec:** `docs/superpowers/specs/2026-09-08-g9v3-3b-llamacpp-lua-design.md`

## Global Constraints

- Runtime is fully local and does not require network access after provisioning.
- Direct `libllama` C API is used; no `llama-cli` subprocess.
- llama.cpp baseline commit is exactly `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`.
- Model identity is `ai9stars/G9v3-3B`; default GGUF is `ai9stars_G9v3-3B-Q4_K_M.gguf` from `bartowski/ai9stars_G9v3-3B-GGUF`.
- G9v3 prompt/tool syntax is rendered in Lua and begins with exactly one `<s>`.
- C++ tokenizes the complete rendered prompt with `add_special=false` and `parse_special=true`.
- The model never directly executes a tool; every call is parsed, schema-validated, scope-checked, guardrail-checked, then executed.
- Interactive approval cannot expand startup workspace or bypass hard deny invariants.
- Contract-mode stdout contains one final JSON object and newline only.
- Human-facing terminal formatting never enters model-facing tool results.
- History persistence is opt-in.
- No ncurses/readline/Boost/full UI framework.

---

## File map

```text
src/tool_protocol.hpp
src/tool_protocol.cpp
src/llama_runtime.hpp
src/llama_runtime.cpp
src/session.hpp
src/session.cpp
src/agent_loop.hpp
src/agent_loop.cpp
src/cli.hpp
src/cli.cpp
src/terminal.hpp
src/terminal.cpp
src/application.hpp
src/application.cpp
src/main.cpp
tests/test_tool_protocol.cpp
tests/test_llama_runtime.cpp
tests/test_agent_loop.cpp
tests/test_cli.cpp
third_party/linenoise/LICENSE
third_party/linenoise/linenoise.c
third_party/linenoise/linenoise.h
README.md
THIRD_PARTY.md
```

---

### Task 1: Vendor linenoise and finalize CMake dependency graph

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/linenoise/LICENSE`
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/linenoise/linenoise.c`
- Create: `agents/g9v3-3b-llamacpp-lua/third_party/linenoise/linenoise.h`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakePresets.json`
- Modify: `agents/g9v3-3b-llamacpp-lua/THIRD_PARTY.md`

**Interfaces:**
- Consumes: official linenoise source at pinned commit; locally supplied llama.cpp tree.
- Produces: targets `g9_linenoise`, `llama` integration, final `g9-agent` link graph.

- [ ] **Step 1: Vendor exact linenoise files**

Copy `linenoise.c`, `linenoise.h`, and license text from `antirez/linenoise` commit:

```text
a473823d74b93eab2ba83480df16ed37617493f2
```

Do not modify vendored source except build-warning fixes proven necessary for the supported compiler; any modification must be documented in `THIRD_PARTY.md`.

- [ ] **Step 2: Add pinned provenance check documentation**

`THIRD_PARTY.md` records repository, commit SHA, BSD license, and exact vendored files. A helper comment in CMake states the pin but does not perform a network lookup.

- [ ] **Step 3: Add llama.cpp local-source validation**

CMake must resolve:

```cmake
if(NOT LLAMA_CPP_DIR)
  set(LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp")
endif()
if(NOT EXISTS "${LLAMA_CPP_DIR}/CMakeLists.txt" OR NOT EXISTS "${LLAMA_CPP_DIR}/include/llama.h")
  message(FATAL_ERROR "llama.cpp source not found. Set -DLLAMA_CPP_DIR=/path/to/llama.cpp at commit f3f1a8f... or populate third_party/llama.cpp.")
endif()
```

Add llama.cpp with `add_subdirectory(... EXCLUDE_FROM_ALL)`. Disable examples/tests/server through cache options before adding the subdirectory where the pinned project exposes those options. Do not disable backends required by the selected preset incorrectly.

- [ ] **Step 4: Define final targets**

The final graph is:

```text
g9_core       json + contract
g9_lua        LuaAgent, links lua_static + g9_core
g9_tools      workspace + guardrails + patch + tools + process + sandbox, links g9_core
g9_runtime    llama_runtime + tool_protocol, links llama + g9_core
g9_agentlib   session + agent_loop + application + cli + terminal, links all above + g9_linenoise
g9-agent      main.cpp -> g9_agentlib
```

Tests link the narrowest target they require.

- [ ] **Step 5: Configure against locally provisioned dependencies**

```bash
cmake --preset cpu-release
```

Expected: configure completes only when Lua and llama.cpp local source trees are present; no network request occurs.

- [ ] **Step 6: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/third_party/linenoise agents/g9v3-3b-llamacpp-lua/CMakeLists.txt agents/g9v3-3b-llamacpp-lua/CMakePresets.json agents/g9v3-3b-llamacpp-lua/THIRD_PARTY.md
git commit -m "build: wire pinned local agent dependencies"
```

---

### Task 2: G9v3 tool schema rendering and function-call parser

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/tool_protocol.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/tool_protocol.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_tool_protocol.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/lua_agent.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/config/agent.lua`

**Interfaces:**
- Consumes: `ToolDefinition`, `JsonValue`, Lua prompt renderer.
- Produces: compact tool-definition JSON, `ParsedAssistantTurn`, `ParsedToolCall`, tool-response conversation encoding.

- [ ] **Step 1: Define parser API**

```cpp
struct ParsedToolCall {
    std::string name;
    JsonValue arguments{JsonValue::object{}};
    std::string raw_text;
};

struct ParsedAssistantTurn {
    std::string visible_text;
    std::vector<ParsedToolCall> tool_calls;
    bool malformed_tool_markup = false;
    std::string parse_error;
};

JsonValue tool_definitions_to_prompt_json(const std::vector<ToolDefinition>& tools);
ParsedAssistantTurn parse_g9_assistant_turn(std::string_view text,
                                             const ToolRegistry& registry);
std::string make_g9_tool_response(std::string_view tool_name,
                                  const ToolResult& result);
```

- [ ] **Step 2: Write failing parser tests from the model protocol**

Cover exact forms:

```xml
<function name="read_file"><param name="path">src/main.cpp</param></function>
```

and CDATA:

```xml
<function name="apply_patch"><param name="patch"><![CDATA[*** Begin Patch
*** Update File: x
@@
-a
+b
*** End Patch]]></param></function>
```

Tests also cover multiple sibling function calls, visible text before/after calls, XML entity decoding for `&amp; &lt; &gt; &quot; &apos;`, duplicate parameter names, unknown tool, missing required parameters, malformed nesting, incomplete closing tags, and unsupported XML declarations/entities.

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R tool_protocol --output-on-failure
```

Expected: fail before implementation.

- [ ] **Step 4: Implement narrow scanner, not XML library**

Parse only the G9 grammar:

```text
<function name="NAME"> PARAM* </function>
PARAM := <param name="NAME"> TEXT_OR_CDATA </param>
```

No recursive elements, namespaces, processing instructions, comments, DTD, entity declarations, or arbitrary XML are accepted. Bounds: function name <=128 bytes, param name <=128, total parsed tool markup <= configured generation output cap.

- [ ] **Step 5: Validate against ToolRegistry schemas**

Convert parameter strings to JSON argument fields. For schema properties typed integer/number/boolean, parse strict lexical forms; object/array parameters require valid JSON text. Reject missing required keys and unexpected keys when tool schema has `additionalProperties=false`.

- [ ] **Step 6: Extend Lua renderer for tools/history**

`config/agent.lua` renders compact tool schemas in the G9v3 `<tools>...</tools>` system-prompt convention and appends prior assistant function call text plus `<tool_response>...</tool_response>` results exactly as required by the model-specific template subset in the spec.

- [ ] **Step 7: Run tool protocol tests**

```bash
ctest --test-dir build/cpu-release -R tool_protocol --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/tool_protocol.* agents/g9v3-3b-llamacpp-lua/src/lua_agent.* agents/g9v3-3b-llamacpp-lua/config/agent.lua agents/g9v3-3b-llamacpp-lua/tests/test_tool_protocol.cpp
git commit -m "feat: implement G9v3 coding tool protocol"
```

---

### Task 3: Direct llama.cpp inference runtime

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/llama_runtime.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/llama_runtime.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_llama_runtime.cpp`

**Interfaces:**
- Consumes: pinned `llama.h`, `ModelConfig`, `GenerationConfig`.
- Produces: `LlamaRuntime`, `GenerationResult`, cancellation-aware local generation.

- [ ] **Step 1: Define runtime API**

```cpp
struct GenerationResult {
    std::string text;
    std::int64_t prompt_tokens = 0;
    std::int64_t completion_tokens = 0;
    bool hit_eog = false;
    bool cancelled = false;
};

class LlamaRuntime {
public:
    LlamaRuntime(std::filesystem::path model_path, const ModelConfig& config,
                 std::function<void(std::string_view)> log_sink);
    ~LlamaRuntime();
    LlamaRuntime(LlamaRuntime&&) noexcept;
    LlamaRuntime& operator=(LlamaRuntime&&) noexcept;
    LlamaRuntime(const LlamaRuntime&) = delete;
    LlamaRuntime& operator=(const LlamaRuntime&) = delete;

    GenerationResult generate(std::string_view rendered_prompt,
                              const GenerationConfig& config,
                              std::stop_token stop = {});
    [[nodiscard]] std::string model_description() const;
    [[nodiscard]] std::uint64_t model_parameter_count() const;
};
```

- [ ] **Step 2: Add compile-level API assertions/tests**

`test_llama_runtime.cpp` first covers helper functions that do not require a model: seed mapping, sampler settings validation, token-piece append behavior with resize retry, and context-size arithmetic. Build must prove the pinned function signatures compile.

- [ ] **Step 3: Implement llama/ggml logging redirection**

Route llama.cpp/ggml log callbacks to the supplied stderr-oriented sink. No library output may write to stdout.

- [ ] **Step 4: Implement RAII resource ownership**

Use unique_ptr-like custom deleters or small wrappers for:

```text
llama_model -> llama_model_free
llama_context -> llama_free
llama_sampler -> llama_sampler_free
```

Initialize backends via `ggml_backend_load_all()` once per process through a thread-safe static guard.

- [ ] **Step 5: Load local model and context**

Use:

```cpp
llama_model_params mp = llama_model_default_params();
mp.n_gpu_layers = config.gpu_layers;
llama_model_load_from_file(path.c_str(), mp);

llama_context_params cp = llama_context_default_params();
cp.n_ctx = config.context_size;
cp.n_batch = config.batch_size;
cp.n_threads = resolved_threads;
cp.n_threads_batch = resolved_threads;
llama_init_from_model(model, cp);
```

Resolve `threads=0` to a conservative hardware concurrency value >=1.

- [ ] **Step 6: Tokenize complete Lua-rendered prompt**

Call `llama_tokenize` with:

```text
add_special = false
parse_special = true
```

Use sizing call then allocation. Reject prompt if `prompt_tokens + max_tokens > context_size` with `ExitCode::context_failure` before decoding.

- [ ] **Step 7: Build sampler chain exactly from config**

At the pinned revision, construct:

```text
llama_sampler_init_penalties(llama_vocab_n_tokens(vocab), repeat_last_n, repeat_penalty, 0.0f, 0.0f)
llama_sampler_init_top_k(top_k)
llama_sampler_init_top_p(top_p, 1)
llama_sampler_init_min_p(min_p, 1)
llama_sampler_init_temp(temperature)
llama_sampler_init_dist(seed < 0 ? LLAMA_DEFAULT_SEED : uint32_t(seed))
```

Accept prompt/generated tokens into the sampler where the penalties implementation requires history via `llama_sampler_accept`.

- [ ] **Step 8: Implement decode/generate loop**

Evaluate prompt with `llama_batch_get_one` + `llama_decode`. Then sample token-by-token, stop on EOG, token cap, or stop token. Convert token to piece by first attempting a stack buffer, resizing when llama reports required negative/positive size semantics at the pinned API. Append exact bytes.

- [ ] **Step 9: Add optional model smoke CTest**

Register `g9_model_smoke` only when `G9_MODEL_PATH` is supplied and file exists. The test renders a tiny prompt, creates runtime, generates <=8 tokens, and checks `completion_tokens >= 1` or EOG with no process crash. Mark it with label `model` so default CI can exclude it.

- [ ] **Step 10: Run runtime tests**

```bash
ctest --test-dir build/cpu-release -R llama_runtime --output-on-failure
```

Expected: no-weight unit tests pass. If GGUF present:

```bash
ctest --test-dir build/cpu-release -L model --output-on-failure
```

Expected: model smoke passes.

- [ ] **Step 11: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/llama_runtime.* agents/g9v3-3b-llamacpp-lua/tests/test_llama_runtime.cpp agents/g9v3-3b-llamacpp-lua/CMakeLists.txt
git commit -m "feat: add local llama.cpp inference runtime"
```

---

### Task 4: Session state and coding-agent loop

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/session.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/session.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/agent_loop.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/agent_loop.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_agent_loop.cpp`

**Interfaces:**
- Consumes: `LuaAgent`, `LlamaRuntime`, `ToolRegistry`, `Guardrails`.
- Produces: `Session`, `AgentLoop`, `LoopEvent`, `TaskResult`, approval callback interface.

- [ ] **Step 1: Define session/event types**

```cpp
enum class MessageKind { system, user, assistant, tool };

struct ConversationItem {
    MessageKind kind;
    std::string content;
    std::optional<std::string> tool_name;
};

struct LoopEvent {
    enum class Type { generation_started, generation_finished, tool_requested,
                      approval_required, tool_finished, warning } type;
    std::string summary;
    JsonValue detail{JsonValue::object{}};
};

struct TaskResult {
    std::string output;
    Usage usage;
    std::uint32_t tool_steps = 0;
    std::size_t files_changed = 0;
};

class Session {
public:
    void add(ConversationItem item);
    void clear();
    [[nodiscard]] const std::vector<ConversationItem>& items() const noexcept;
};
```

- [ ] **Step 2: Define dependency-injected loop API**

```cpp
using ApprovalCallback = std::function<PolicyAction(const ActionDescriptor&)>;
using EventCallback = std::function<void(const LoopEvent&)>;

class AgentLoop {
public:
    AgentLoop(LuaAgent& policy,
              IGenerationRuntime& runtime,
              ToolRegistry& tools,
              Guardrails& guardrails,
              ApprovalCallback approval,
              EventCallback events);

    TaskResult run(Session& session,
                   const AgentRequest& request,
                   std::stop_token stop = {});
};
```

Introduce `IGenerationRuntime` with the same `generate` signature so tests use a scripted fake instead of a model.

- [ ] **Step 3: Write scripted red tests**

Fake runtime returns a sequence such as:

```text
1. <function name="read_file">...</function>
2. <function name="apply_patch">...</function>
3. final visible answer
```

Assert tool results are appended before subsequent generation, max steps enforced, multiple tool calls execute sequentially, malformed tool calls become recoverable tool-protocol failures where appropriate, denied calls return structured result to model, interactive `ask` invokes callback, and noninteractive callback denies unresolved asks without blocking.

- [ ] **Step 4: Verify red state**

```bash
ctest --test-dir build/cpu-release -R agent_loop --output-on-failure
```

Expected: fail before implementation.

- [ ] **Step 5: Implement loop state machine**

Per step:

```text
build current request/messages/history
obtain enabled tool schemas
Lua render complete prompt
runtime.generate
parse assistant turn
if no tool calls -> final
validate each call -> policy -> approval -> execute
append exact assistant raw tool-call content
append structured tool responses
repeat
```

Aggregate token usage across all model rounds. Count one `tool_step` per generation round that executes at least one tool call.

- [ ] **Step 6: Enforce context and loop limits explicitly**

Stop with a clear error if max steps is reached before final answer. Cap calls per model turn. Tool result payload is truncated to Lua-configured `max_tool_result_bytes` before rendering back to model, preserving a structured `truncated=true` marker.

- [ ] **Step 7: Run agent-loop suite**

```bash
ctest --test-dir build/cpu-release -R agent_loop --output-on-failure
```

Expected: all scripted tests pass without model weights.

- [ ] **Step 8: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/session.* agents/g9v3-3b-llamacpp-lua/src/agent_loop.* agents/g9v3-3b-llamacpp-lua/tests/test_agent_loop.cpp
git commit -m "feat: add local coding-agent tool loop"
```

---

### Task 5: CLI parser, terminal renderer, linenoise interaction, and approvals

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/cli.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/cli.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/terminal.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/terminal.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_cli.cpp`

**Interfaces:**
- Consumes: config/profile types and `LoopEvent`.
- Produces: parsed `CliOptions`, human terminal frontend, slash-command parser, approval UI.

- [ ] **Step 1: Define CLI options**

```cpp
enum class ColorMode { automatic, always, never };
enum class InvocationMode { chat, json_run, doctor, tools, version };

struct CliOptions {
    InvocationMode mode = InvocationMode::chat;
    std::filesystem::path workspace = ".";
    std::optional<std::filesystem::path> config;
    std::optional<std::filesystem::path> model;
    GuardrailProfile profile = GuardrailProfile::balanced;
    std::vector<ScopeRule> scopes;
    std::vector<std::string> denies;
    std::uint32_t max_steps = 32;
    std::optional<bool> thinking;
    ColorMode color = ColorMode::automatic;
    std::optional<std::filesystem::path> history_file;
    std::optional<std::filesystem::path> audit_log;
    SandboxMode command_sandbox = SandboxMode::strict;
    bool allow_unsandboxed_commands = false;
};

CliOptions parse_cli(int argc, char** argv);
```

- [ ] **Step 2: Write CLI parser tests**

Cover all spec options, invalid profile/scope syntax, missing values, repeated `--scope`, absolute/relative workspace, mutually incompatible flags, and `run --json` selection. Invalid usage maps to exit code 2.

- [ ] **Step 3: Implement terminal capabilities**

`Terminal` detects `isatty`, `NO_COLOR`, `TERM=dumb`, explicit color mode, and terminal width via ioctl with fallback 80. Methods:

```cpp
void header(const DoctorSummary&);
void tool_event(const LoopEvent&);
PolicyAction approval(const ActionDescriptor&);
void warning(std::string_view);
void final_answer(std::string_view);
void status(const SessionStatus&);
```

All ANSI styling lives here.

- [ ] **Step 4: Implement exact approval interaction**

Render exact action, risk, canonical targets/argv, summary/diff snippet, workspace, and reason. Read only:

```text
y -> allow once
s -> allow equivalent for session
n -> deny
```

Unknown/EOF defaults to deny. Session allow calls `Guardrails::allow_for_session(equivalence_key)` only after user chooses `s`.

- [ ] **Step 5: Integrate linenoise**

Interactive chat uses `linenoise()` with multiline enabled, slash-command completion, bracketed paste from vendored build, and history persistence only if `--history-file` was supplied. Do not call `linenoiseHistorySave` otherwise.

- [ ] **Step 6: Implement slash commands**

Parse locally before sending model input:

```text
/help
/status
/scope
/policy
/tools
/changes
/thinking [on|off]
/clear
/exit
```

`/thinking` updates only session-effective generation config; `/clear` clears Session but does not destroy LlamaRuntime.

- [ ] **Step 7: Test renderer without snapshot fragility**

Use a fake terminal sink and assert semantic content plus ANSI-disabled path. Tests verify approval defaults to deny on EOF, no ANSI when disabled, and machine/JSON mode never calls human renderer.

- [ ] **Step 8: Run CLI tests**

```bash
ctest --test-dir build/cpu-release -R cli --output-on-failure
```

Expected: all parser/terminal logic tests pass.

- [ ] **Step 9: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/cli.* agents/g9v3-3b-llamacpp-lua/src/terminal.* agents/g9v3-3b-llamacpp-lua/tests/test_cli.cpp agents/g9v3-3b-llamacpp-lua/third_party/linenoise
git commit -m "feat: add robust coding-agent terminal CLI"
```

---

### Task 6: Application composition, doctor, chat mode, and JSON contract mode

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/application.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/application.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/main.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/run.sh`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/test_cli.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/test_agent_loop.cpp`

**Interfaces:**
- Consumes: every subsystem.
- Produces: final executable behavior.

- [ ] **Step 1: Define application API**

```cpp
class Application {
public:
    explicit Application(CliOptions options);
    int run();
private:
    int run_chat();
    int run_json_contract();
    int run_doctor();
    int run_tools();
};
```

`main` catches `g9::Error`, `std::exception`, and unknown exceptions, maps exit codes, and never prints human text to stdout in JSON mode.

- [ ] **Step 2: Implement startup path resolution**

Resolve agent root from launcher-provided `--agent-root` hidden/internal option or executable location fallback. Canonicalize workspace once. Load Lua. Apply CLI overrides in precedence order. Construct Workspace, Guardrails, ProcessSandbox, ToolRegistry, LlamaRuntime, Session, AgentLoop.

- [ ] **Step 3: Implement `doctor`**

Doctor reports without network:

```text
agent root
config path/load status
model path/existence/size
llama.cpp compiled version/revision when available
Lua version
workspace canonical root
scope tree/denies
selected guardrail profile
command sandbox requested/detected capabilities
TTY/color capability
enabled tools
```

Doctor returns non-zero when required runtime prerequisites are absent; it must not load the full GGUF unless `--deep` is explicitly introduced later. v0.1 doctor checks file existence/readability only.

- [ ] **Step 4: Implement interactive chat mode**

Load model once before the first model task, display startup header, loop reading lines, handle slash commands, and call AgentLoop for ordinary user tasks. After each task, show final answer and keep Session/model for follow-up. Ctrl-C stop handling uses a process-wide signal flag/stop_source and obeys spec semantics.

- [ ] **Step 5: Implement contract mode**

`g9-agent run --json --workspace PATH` reads entire stdin with a hard request byte cap (1 MiB), parses one object, runs exactly one task, and serializes final contract response. Approval callback returns deny for unresolved `ask`; no terminal prompt. Tool/model diagnostics go stderr only.

- [ ] **Step 6: Implement `tools` and `version`**

`tools` lists enabled tool names, descriptions, and base risk classes human-readably. `version` prints `g9-agent 0.1.0` and pinned dependency identities.

- [ ] **Step 7: Update `run.sh`**

`run.sh` resolves agent root and binary path, then execs:

```sh
exec "$BIN" --agent-root "$ROOT" "$@"
```

It does not `cd` to repository root, download, modify environment broadly, or start network services.

- [ ] **Step 8: Test machine stdout isolation**

An integration test launches a fake-runtime build/application path or dependency-injected application with JSON input and verifies stdout parses as exactly one JSON document while all synthetic logs land on stderr.

- [ ] **Step 9: Run application tests**

```bash
ctest --test-dir build/cpu-release -R 'cli|agent_loop|contract' --output-on-failure
```

Expected: all pass.

- [ ] **Step 10: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/application.* agents/g9v3-3b-llamacpp-lua/src/main.cpp agents/g9v3-3b-llamacpp-lua/run.sh agents/g9v3-3b-llamacpp-lua/tests
git commit -m "feat: compose offline G9 coding agent application"
```

---

### Task 7: Complete documentation, smoke procedures, and final verification

**Files:**
- Modify: `agents/g9v3-3b-llamacpp-lua/README.md`
- Modify: `agents/g9v3-3b-llamacpp-lua/models/README.md`
- Modify: `agents/g9v3-3b-llamacpp-lua/third_party/README.md`
- Modify: `agents/g9v3-3b-llamacpp-lua/THIRD_PARTY.md`
- Modify: `agents/g9v3-3b-llamacpp-lua/config/agent.lua`

**Interfaces:**
- Consumes: completed implementation.
- Produces: reproducible operator/developer instructions.

- [ ] **Step 1: Write complete README usage path**

README sections must cover:

```text
architecture
requirements
connected provisioning only
copying the agent outside the sandbox repo
offline build
CPU preset
optional local GPU flags
model placement
interactive CLI
workspace/scope examples
guardrail profiles
approval meanings
command sandbox modes
coding tools
Lua configuration
thinking mode
JSON contract mode
doctor/tests
known v0.1 limits
security/offline interpretation
```

Provide runnable examples such as:

```bash
./build.sh cpu-release
./run.sh doctor --workspace /path/to/project
./run.sh --workspace /path/to/project --profile balanced
./run.sh run --json --workspace /path/to/project < request.json
```

- [ ] **Step 2: Document exact provisioning commands as optional manual examples**

Commands may show how a human obtains pinned llama.cpp/Lua/GGUF on a connected machine, but label them `manual provisioning` and keep them out of build/run scripts. Include SHA/revision checks where source ecosystem provides them.

- [ ] **Step 3: Document offline smoke sequence**

After provisioning and disconnecting network:

```bash
cmake --preset cpu-release
cmake --build --preset cpu-release
ctest --test-dir build/cpu-release --output-on-failure
./run.sh doctor --workspace "$PWD/smoke-workspace"
printf '%s\n' '{"prompt":"Read the project and summarize it."}' | ./run.sh run --json --workspace "$PWD/smoke-workspace"
```

If model test label configured:

```bash
ctest --test-dir build/cpu-release -L model --output-on-failure
```

- [ ] **Step 4: Run full non-model suite**

```bash
ctest --test-dir build/cpu-release --output-on-failure -LE model
```

Expected: zero failures.

- [ ] **Step 5: Run model smoke when GGUF exists**

```bash
ctest --test-dir build/cpu-release -L model --output-on-failure
```

Expected: zero failures. If GGUF is not provisioned in the current environment, record `model smoke not run: model artifact absent`; do not claim it passed.

- [ ] **Step 6: Run repository isolation validator**

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed`.

- [ ] **Step 7: Scan implementation for accidental networking/fetch behavior**

```bash
grep -RInE 'FetchContent|ExternalProject_Add|curl |wget |libcurl|http_client|git clone|system\(' agents/g9v3-3b-llamacpp-lua/src agents/g9v3-3b-llamacpp-lua/CMakeLists.txt agents/g9v3-3b-llamacpp-lua/*.sh
```

Expected: no prohibited runtime/build implementation. Any `http://`/`https://` in documentation/provenance is acceptable and excluded from this scan.

- [ ] **Step 8: Verify copy isolation practically**

Copy only the agent directory to a temporary unrelated location, provide dependency paths explicitly, configure/build there, and run non-model tests. No path should resolve back to `local-model-sandbox`.

Example:

```bash
tmp=$(mktemp -d)
cp -R agents/g9v3-3b-llamacpp-lua "$tmp/agent"
cmake -S "$tmp/agent" -B "$tmp/build" -DLUA_DIR="$LUA_SRC" -DLLAMA_CPP_DIR="$LLAMA_SRC" -DG9_BUILD_TESTS=ON
cmake --build "$tmp/build"
ctest --test-dir "$tmp/build" --output-on-failure -LE model
```

Expected: zero failures.

- [ ] **Step 9: Verify strict scope behavior manually/in integration test**

Create a workspace with `allowed/` and a sibling secret outside the root. Confirm file tools reject `../secret`; confirm a sandboxed helper cannot write outside scope on supported strict backend; confirm destructive action in balanced profile requires approval.

- [ ] **Step 10: Commit docs and final verification adjustments**

```bash
git add agents/g9v3-3b-llamacpp-lua
git commit -m "docs: complete G9 coding agent operator guide"
```

---

## Plan 3 completion gate

```text
[ ] pinned linenoise vendored with license
[ ] pinned llama.cpp source builds locally without fetch
[ ] G9 tool-call parser rejects malformed/unknown calls
[ ] prompt renderer supports tools, tool responses, thinking/non-thinking, exactly one BOS
[ ] llama.cpp model/context/sampler resources are RAII-managed
[ ] contract stdout is machine-clean
[ ] scripted agent-loop tests pass without model
[ ] model stays loaded across interactive task rounds
[ ] CLI supports startup header, tool cards, slash commands, cancellation, approvals
[ ] history remains opt-in
[ ] doctor reports sandbox/scope/model/tool status offline
[ ] full non-model CTest suite has zero failures
[ ] model smoke passes only if model was actually provisioned; otherwise status is explicitly unverified
[ ] repository isolation validation passes
[ ] copied agent builds/tests without repository siblings
[ ] no automatic network fetch/runtime path exists
```

After these gates, invoke `superpowers:verification-before-completion`, then `superpowers:finishing-a-development-branch` before merging or opening a PR.