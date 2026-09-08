# G9 Coding Agent Execution Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute the approved G9v3-3B local coding-agent design through three reviewable implementation plans in dependency order.

**Architecture:** The implementation is intentionally split into foundation, coding-tool security boundary, and runtime/product layers. Later plans may consume public interfaces from earlier plans but must not bypass them; in particular the agent loop can only execute tools through the policy-aware ToolRegistry.

**Tech Stack:** C++20, CMake 3.24+, Lua 5.4.9, llama.cpp pinned at `f3f1a8f2760f28325a5ec20c05b171e5b7c83a29`, vendored linenoise pinned at `a473823d74b93eab2ba83480df16ed37617493f2`.

**Spec:** `docs/superpowers/specs/2026-09-08-g9v3-3b-llamacpp-lua-design.md`

## Global Constraints

- Execute the plans in the order listed below.
- Every task uses TDD/red-green where the plan defines a testable behavior.
- Commit after every task's green verification gate.
- Do not claim model-smoke success unless the GGUF is actually present and the model test ran successfully.
- Do not introduce a root/shared runtime dependency.
- Do not replace direct `libllama` use with a CLI/server subprocess.
- Do not weaken workspace or network hard invariants to make a test pass.
- Automatic dependency/model network fetching remains prohibited.

---

## Execution order

1. `docs/superpowers/plans/2026-09-08-g9v3-core-lua-foundation.md`
2. `docs/superpowers/plans/2026-09-08-g9v3-coding-tools-guardrails.md`
3. `docs/superpowers/plans/2026-09-08-g9v3-runtime-agent-loop-cli.md`

Each plan has its own completion gate. Do not begin the next plan until the previous gate is green or an unavailable external artifact is explicitly recorded as unverified rather than passed.

---

## Self-review corrections and clarifications

These notes are normative and resolve two interface details found while cross-checking the three plans.

### Foundation test harness

Plan 1 Task 2 also creates:

```text
agents/g9v3-3b-llamacpp-lua/tests/test_support.hpp
```

The file provides the small local `CHECK`, `CHECK_EQ`, and `CHECK_THROWS` macros referenced by all test translation units. No external test framework is introduced.

Where Plan 1 shows a default JSON object argument, use an explicit `JsonValue` aggregate. The correct declaration is:

```cpp
JsonValue error_to_json(
    std::string message,
    JsonValue metadata = JsonValue{JsonValue::object{}});
```

This avoids relying on a nonexistent implicit conversion from `JsonValue::object` to `JsonValue`.

### Process runner before concrete sandbox backend

Plan 2 Task 5 must remain independently compilable before Task 6 creates the platform sandbox implementation. Therefore the low-level process runner uses an injectable child-setup callback:

```cpp
using ChildSetup = std::function<void()>;

ProcessResult run_process(
    const ProcessSpec& spec,
    ChildSetup child_setup = {},
    std::stop_token stop = {});
```

The child calls `child_setup()` after process-group setup and before `exec`.

Plan 2 Task 6 then implements `ProcessSandbox` and supplies:

```cpp
run_process(spec, [&sandbox] { sandbox.apply_in_child(); }, stop);
```

On macOS, where `sandbox-exec` is an argv wrapper rather than an in-child syscall policy, `ProcessSandbox` instead exposes a method that transforms `ProcessSpec.argv` before `run_process` while preserving direct argv execution. The resulting command still never uses a shell.

The public higher-level `run_command` tool never exposes `ChildSetup` to the model.

### llama.cpp pinned API cross-check

The pinned `llama_context_params` contains `n_threads` and `n_threads_batch`; Plan 3 may set both from the resolved thread count. The pinned API also exposes `llama_log_set`, which must be used to route llama/ggml logs away from stdout.

---

## Final acceptance sequence

After all three plans are implemented:

- [ ] Run `ctest --test-dir build/cpu-release --output-on-failure -LE model` and require zero failures.
- [ ] If the GGUF is present, run `ctest --test-dir build/cpu-release -L model --output-on-failure`; otherwise report it as not run.
- [ ] Run `python3 scripts/validate_repo.py` from repository root and require `OK: repository validation passed`.
- [ ] Copy only `agents/g9v3-3b-llamacpp-lua/` to a temporary unrelated path and build/test it with explicit local `LUA_DIR`/`LLAMA_CPP_DIR`.
- [ ] Run the implementation network/fetch scan from Plan 3 and inspect every match.
- [ ] Exercise a scope-escape test, a destructive-action approval test, and a child-sandbox containment test on a supported platform.
- [ ] Run `./run.sh doctor --workspace <test-workspace>` and verify the reported model, scope, policy, and sandbox state is truthful.
- [ ] Invoke `superpowers:verification-before-completion` before any completion claim.
- [ ] Invoke `superpowers:finishing-a-development-branch` before merge/PR integration.