# G9 Coding Tools + Guardrails Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the coding-agent workspace boundary, hierarchical path policy, destructive-action guardrails, file/search/edit tools, structured patch engine, command policy, process execution, and child-process sandbox.

**Architecture:** All tool requests pass through `ToolRegistry -> Workspace -> Guardrails -> Executor`; model output never calls native functions directly. File tools use C++20 filesystem/streams and never escape the canonical workspace. Child processes execute by argv with filtered environment and a separate OS sandbox so terminal commands cannot bypass file-tool scope.

**Tech Stack:** C++20 standard library, POSIX process APIs, Linux Landlock syscalls, macOS `sandbox-exec` when available, CTest. No Boost, libgit2, Tree-sitter, ripgrep, shell command interpreter, or network client.

**Spec:** `docs/superpowers/specs/2026-09-08-g9v3-3b-llamacpp-lua-design.md`

## Global Constraints

- This plan modifies only `agents/g9v3-3b-llamacpp-lua/`.
- File tools may never resolve outside the canonical startup workspace root.
- Symlink traversal may never escape the workspace root.
- Deny rules always win over allows.
- Workspace-root expansion requires process restart; interactive approval cannot expand it.
- Tool requests with unknown names or invalid parameters are denied before execution.
- Destructive behavior is classified separately from normal write behavior.
- `run_command` executes argv directly; no `/bin/sh -c`, pipes, redirects, glob expansion, command substitution, or shell interpolation.
- Parent environment is not inherited wholesale.
- In `strict` command-sandbox mode, if the required OS sandbox cannot be installed, the command is not launched.
- The agent contains no network client implementation and child network access is denied when the platform sandbox can enforce it.
- Default guardrail profile is `balanced`.
- Default file scope is workspace root `rw`, `.git` is `none`, and secrets such as `.env`, PEM/key files are denied.

---

## File map

```text
src/workspace.hpp
src/workspace.cpp
src/guardrails.hpp
src/guardrails.cpp
src/patch_engine.hpp
src/patch_engine.cpp
src/tools.hpp
src/tools.cpp
src/tool_registry.hpp
src/tool_registry.cpp
src/process.hpp
src/process.cpp
src/process_sandbox.hpp
src/process_sandbox.cpp
tests/test_workspace.cpp
tests/test_guardrails.cpp
tests/test_patch_engine.cpp
tests/test_tools.cpp
tests/test_process.cpp
```

Later plans consume these interfaces but must not reach around them.

---

### Task 1: Workspace canonicalization, scope tree, and deny globs

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/workspace.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/workspace.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_workspace.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/CMakeLists.txt`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: standard library only.
- Produces: `Workspace`, `Access`, `ScopeRule`, `PathDecision`, `glob_match`.

- [ ] **Step 1: Define public types**

```cpp
namespace g9 {

enum class Access { none, read, read_write };

enum class Operation { read, write, remove, execute_cwd };

struct ScopeRule {
    std::filesystem::path relative_path;
    Access access;
};

struct PathDecision {
    bool allowed = false;
    std::filesystem::path canonical_path;
    Access effective_access = Access::none;
    std::string reason;
};

class Workspace {
public:
    Workspace(std::filesystem::path root,
              Access default_access,
              std::vector<ScopeRule> scopes,
              std::vector<std::string> deny_globs,
              bool follow_directory_symlinks = false);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] PathDecision resolve(std::filesystem::path relative,
                                       Operation operation,
                                       bool target_may_not_exist = false) const;
    [[nodiscard]] bool is_denied(std::filesystem::path relative) const;
};

bool glob_match(std::string_view pattern, std::string_view path);

} // namespace g9
```

- [ ] **Step 2: Write failing scope tests**

Use a temporary workspace and assert:

```text
. default rw -> src/main.cpp write allowed
.git none -> .git/config read denied
docs r -> docs/design.md read allowed, write denied
.env deny -> read denied even under rw root
../outside -> denied
absolute /tmp/outside -> denied
symlink file -> allowed only when resolved target remains inside workspace
symlink directory traversal -> not followed by directory walk by default
longest matching scope wins
```

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R workspace --output-on-failure
```

Expected: workspace test target fails before implementation.

- [ ] **Step 4: Implement portable glob matcher**

Support only documented patterns:

```text
*      zero or more non-/ characters
?      exactly one non-/ character
**     zero or more characters including /
```

Normalize paths to `/` for matching. Do not implement character classes or brace expansion. Matching is case-sensitive on all platforms for predictable policy semantics.

- [ ] **Step 5: Implement path resolution**

Algorithm must be exactly:

```text
1. reject empty/absolute paths unless path == "."
2. lexical normalize workspace_root / relative
3. verify lexical path is under workspace root
4. resolve existing path components using weakly_canonical
5. verify resolved existing prefix remains under workspace root
6. reject directory symlink traversal when configured false
7. convert resolved target back to workspace-relative normalized form
8. evaluate deny globs
9. choose longest matching scope ancestor
10. verify read/write permission for requested operation
```

For new files, canonicalize the nearest existing parent and append the remaining lexical components; never call canonical on a nonexistent final target and assume safety.

- [ ] **Step 6: Run workspace tests**

```bash
ctest --test-dir build/cpu-release -R workspace --output-on-failure
```

Expected: all path/scope/symlink/glob tests pass.

- [ ] **Step 7: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/workspace.* agents/g9v3-3b-llamacpp-lua/tests/test_workspace.cpp
git commit -m "feat: enforce hierarchical workspace scope"
```

---

### Task 2: Guardrail policy engine and approval semantics

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/guardrails.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/guardrails.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_guardrails.cpp`

**Interfaces:**
- Consumes: `PathDecision` from Task 1.
- Produces: `RiskClass`, `PolicyAction`, `GuardrailProfile`, `ActionDescriptor`, `GuardrailDecision`, `Guardrails`.

- [ ] **Step 1: Define policy API**

```cpp
enum class RiskClass {
    read,
    write,
    execute,
    filesystem_destructive,
    vcs_read,
    vcs_write,
    vcs_destructive,
};

enum class PolicyAction { allow, ask, deny };
enum class GuardrailProfile { review, balanced, autonomous_mode };

struct ActionDescriptor {
    std::string tool;
    RiskClass risk;
    std::vector<std::filesystem::path> targets;
    std::vector<std::string> command_argv;
    std::string equivalence_key;
    std::string summary;
};

struct GuardrailDecision {
    PolicyAction action;
    std::string reason;
};

class Guardrails {
public:
    explicit Guardrails(GuardrailProfile profile);
    [[nodiscard]] GuardrailDecision decide(const ActionDescriptor& action) const;
    void allow_for_session(std::string equivalence_key);
    [[nodiscard]] bool session_allows(std::string_view equivalence_key) const;
};
```

- [ ] **Step 2: Write failing profile tests**

Assert the spec matrix exactly:

```text
balanced: read allow, write allow, known execute allow, fs-destructive ask, vcs-read allow, vcs-write ask, vcs-destructive deny
autonomous: read/write allow, command-policy execute allow, fs-destructive ask, explicitly-enabled vcs-write allow, vcs-destructive deny
review: read allow, write ask, execute ask, fs-destructive deny, vcs-read allow, vcs-write ask, vcs-destructive deny
```

Also assert session approval can turn an `ask` into `allow` only for the exact equivalence key and never turns a hard `deny` into allow.

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R guardrails --output-on-failure
```

Expected: fail before implementation.

- [ ] **Step 4: Implement profile mapping and session grants**

Use explicit tables/switches; do not derive security decisions from human-readable strings. `Guardrails::decide` checks hard-deny class first, then session approvals, then profile action.

- [ ] **Step 5: Run guardrail tests**

```bash
ctest --test-dir build/cpu-release -R guardrails --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/guardrails.* agents/g9v3-3b-llamacpp-lua/tests/test_guardrails.cpp
git commit -m "feat: add configurable destructive-action guardrails"
```

---

### Task 3: Atomic structured patch engine

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/patch_engine.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/patch_engine.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_patch_engine.cpp`

**Interfaces:**
- Consumes: `Workspace` from Task 1.
- Produces: `PatchOperation`, `PatchPlan`, `PatchResult`, `parse_patch`, `validate_patch`, `apply_patch`.

- [ ] **Step 1: Define patch API**

```cpp
enum class PatchKind { add, update, remove };

struct PatchOperation {
    PatchKind kind;
    std::filesystem::path path;
    std::string original_text;
    std::string new_text;
};

struct PatchPlan {
    std::vector<PatchOperation> operations;
};

struct PatchFileStat {
    std::filesystem::path path;
    std::size_t added_lines = 0;
    std::size_t removed_lines = 0;
    PatchKind kind;
};

struct PatchResult {
    std::vector<PatchFileStat> files;
    bool rollback_attempted = false;
    bool rollback_complete = true;
};

PatchPlan parse_patch(std::string_view patch_text);
PatchPlan validate_patch(const Workspace& workspace, const PatchPlan& parsed);
PatchResult apply_patch(const Workspace& workspace, const PatchPlan& validated);
```

- [ ] **Step 2: Write failing parser/application tests**

Cover:

```text
add file
update single hunk
update multiple hunks
remove file
missing context -> failure, no writes
ambiguous repeated context -> failure, no writes
path outside workspace -> failure
write denied path -> failure
mixed patch where second file invalid -> first file remains untouched
newline-at-EOF preservation
CRLF input normalized only where explicitly changed; untouched files retain bytes outside edited spans
```

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R patch_engine --output-on-failure
```

Expected: failure before implementation.

- [ ] **Step 4: Implement strict patch parser**

Recognize only:

```text
*** Begin Patch
*** Add File: path
*** Update File: path
@@
-old
+new
 context
*** Delete File: path
*** End Patch
```

Reject duplicate operations for the same file, absolute paths, malformed markers, nested file blocks, and patch text after `*** End Patch` except whitespace.

- [ ] **Step 5: Implement preflight transformation in memory**

Read all existing target files first. For each update hunk, locate the exact context once. If zero or more than one match occurs, fail the entire plan. Build every final file image in memory before the first filesystem mutation.

- [ ] **Step 6: Implement atomic-ish commit and rollback**

For writes, create temporary sibling files, fsync/close where supported, then rename. Keep original bytes in memory until all commits finish. If a later commit fails, restore earlier originals best-effort and return rollback status. Deletions happen after all replacement temp files are prepared and preflight passed.

- [ ] **Step 7: Run patch suite**

```bash
ctest --test-dir build/cpu-release -R patch_engine --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/patch_engine.* agents/g9v3-3b-llamacpp-lua/tests/test_patch_engine.cpp
git commit -m "feat: add strict structured patch engine"
```

---

### Task 4: File discovery, search, read, write, replace, delete, and status tools

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/tools.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/tools.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/tool_registry.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/tool_registry.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_tools.cpp`

**Interfaces:**
- Consumes: `JsonValue`, `Workspace`, `Guardrails`, patch engine.
- Produces: `ToolDefinition`, `ToolCall`, `ToolResult`, `ToolRegistry` and the non-process coding tools.

- [ ] **Step 1: Define registry API**

```cpp
struct ToolDefinition {
    std::string name;
    std::string description;
    JsonValue input_schema;
    RiskClass base_risk;
};

struct ToolCall {
    std::string name;
    JsonValue arguments;
};

struct ToolResult {
    bool ok = false;
    JsonValue payload{JsonValue::object{}};
    std::string display_summary;
};

using ToolExecutor = std::function<ToolResult(const JsonValue&)>;

class ToolRegistry {
public:
    void add(ToolDefinition definition, ToolExecutor executor);
    [[nodiscard]] std::vector<ToolDefinition> definitions() const;
    [[nodiscard]] ToolResult execute(const ToolCall& call) const;
};
```

Unknown tools and schema/type violations return structured failure without invoking the executor.

- [ ] **Step 2: Write failing tool tests**

Create a temporary scoped workspace and test:

```text
list_files depth/result caps and no directory-symlink traversal
find_files glob include/exclude
search_text fixed and regex modes with line numbers and bounded context
read_file stable 1-based line numbers, range caps, binary rejection, truncation marker
write_file create succeeds, existing without overwrite fails, overwrite marked destructive upstream
replace_text exact expected_occurrences match succeeds; mismatch writes nothing
apply_patch delegates to Task 3 and returns per-file stats
delete_file refuses directories and removes exactly one regular file when allowed
workspace_status detects files this session created/modified/deleted without Git
all denied/scope-escaped paths fail before access
```

- [ ] **Step 3: Verify red state**

```bash
ctest --test-dir build/cpu-release -R tools --output-on-failure
```

Expected: fail before implementation.

- [ ] **Step 4: Implement bounded directory walking**

Use `std::filesystem::directory_iterator`/recursive traversal with explicit depth/result counters. Do not use `recursive_directory_iterator` defaults that follow symlinks implicitly. Sort returned paths lexicographically for deterministic model context.

- [ ] **Step 5: Implement search/read helpers**

Fixed search is a plain byte/string scan. Regex uses `std::regex` and catches `std::regex_error`. Detect binary files by NUL byte in an initial bounded sample. Results include workspace-relative path, 1-based line, and requested small context only.

- [ ] **Step 6: Implement safe write/replace/delete**

`write_file` to a new path uses temporary sibling + rename. Existing-file overwrite requires an explicit boolean argument and returns an `ActionDescriptor` classified `filesystem_destructive` before execution. `replace_text` counts exact byte occurrences before mutation and requires `expected_occurrences` to match. `delete_file` uses `symlink_status` and refuses directories.

- [ ] **Step 7: Implement session change tracker**

Before first mutation of a path, record existence + original bytes hash/size lazily. `workspace_status` compares touched files against that baseline and reports `created`, `modified`, or `deleted`. Use a small internal stable hash such as FNV-1a 64-bit; this is status tracking, not cryptographic integrity.

- [ ] **Step 8: Run tool suite**

```bash
ctest --test-dir build/cpu-release -R tools --output-on-failure
```

Expected: all file tool tests pass.

- [ ] **Step 9: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/tools.* agents/g9v3-3b-llamacpp-lua/src/tool_registry.* agents/g9v3-3b-llamacpp-lua/tests/test_tools.cpp
git commit -m "feat: add scoped coding file tools"
```

---

### Task 5: Command policy and direct process execution

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/process.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/process.cpp`
- Create: `agents/g9v3-3b-llamacpp-lua/tests/test_process.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tools.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tools.hpp`

**Interfaces:**
- Consumes: workspace cwd decision and guardrail types.
- Produces: `CommandPolicy`, `ProcessSpec`, `ProcessResult`, `run_process`, and `run_command` tool executor.

- [ ] **Step 1: Define process API**

```cpp
enum class CommandClass { known_safe, ask, denied, destructive };

struct ProcessSpec {
    std::vector<std::string> argv;
    std::filesystem::path cwd;
    std::chrono::milliseconds timeout{120000};
    std::size_t max_output_bytes = 131072;
    std::map<std::string, std::string> environment;
};

struct ProcessResult {
    int exit_code = -1;
    bool timed_out = false;
    bool cancelled = false;
    bool stdout_truncated = false;
    bool stderr_truncated = false;
    std::string stdout_text;
    std::string stderr_text;
    std::chrono::milliseconds duration{0};
};

class CommandPolicy {
public:
    [[nodiscard]] CommandClass classify(const std::vector<std::string>& argv) const;
    [[nodiscard]] RiskClass risk(const std::vector<std::string>& argv) const;
};

ProcessResult run_process(const ProcessSpec& spec,
                          const ProcessSandbox& sandbox,
                          std::stop_token stop = {});
```

- [ ] **Step 2: Write failing command-policy tests**

At minimum:

```text
cmake --build build -> known_safe execute
ctest --test-dir build -> known_safe execute
make -> known_safe execute
make clean -> destructive
git status/diff/show/log/grep/rev-parse -> vcs_read
git add/commit -> vcs_write
git reset --hard, git clean -> vcs_destructive denied
git push/pull/fetch/remote -> denied
a command token containing http:// or https:// -> denied in offline mode
curl/wget/ssh/scp -> denied
unknown executable -> ask
```

- [ ] **Step 3: Write failing process tests**

Use `/usr/bin/printf`, `/bin/pwd`, and a tiny test helper executable built by CMake. Verify:

```text
argv preserves spaces without shell parsing
"|" and "&&" are ordinary argv strings, not operators
cwd is respected
stdout/stderr captured separately
output cap sets truncation flags and keeps bounded head+tail
nonzero exit captured
 timeout terminates process group
stop_token cancellation terminates process group
```

- [ ] **Step 4: Implement command classifier by argv prefix/flags**

Do not classify solely by executable basename. Git and build-tool destructive subcommands/targets override the executable family. Reject empty argv, path names escaping allowed executable policy, and URL-shaped arguments for offline-denied commands.

- [ ] **Step 5: Implement POSIX spawn/fork execution without shell**

Use `fork` + `execve`/`execvp` or `posix_spawn` if process-group and sandbox hooks remain controllable. Child must call `setpgid(0,0)`. Parent captures pipes non-blockingly/polling, enforces timeout, sends SIGTERM to `-pid`, waits grace interval, then SIGKILL if needed.

- [ ] **Step 6: Implement filtered environment builder**

Allow only explicit keys such as `PATH`, controlled `HOME`, `TMPDIR`, compiler-related variables selected by config, and minimal locale. Do not forward secrets or arbitrary parent environment automatically.

- [ ] **Step 7: Run process tests without sandbox first**

```bash
ctest --test-dir build/cpu-release -R process --output-on-failure
```

Expected: direct process and command-policy tests pass with sandbox test doubles.

- [ ] **Step 8: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/process.* agents/g9v3-3b-llamacpp-lua/src/tools.* agents/g9v3-3b-llamacpp-lua/tests/test_process.cpp
git commit -m "feat: add direct argv command runner"
```

---

### Task 6: Linux Landlock and macOS child-process sandbox

**Files:**
- Create: `agents/g9v3-3b-llamacpp-lua/src/process_sandbox.hpp`
- Create: `agents/g9v3-3b-llamacpp-lua/src/process_sandbox.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/process.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/test_process.cpp`

**Interfaces:**
- Consumes: workspace root/scope information.
- Produces: `SandboxMode`, `SandboxCapabilities`, `ProcessSandbox`, platform implementations.

- [ ] **Step 1: Define sandbox API**

```cpp
enum class SandboxMode { strict, best_effort, off };

struct SandboxCapabilities {
    bool filesystem = false;
    bool network = false;
    std::string backend;
    std::string detail;
};

class ProcessSandbox {
public:
    static ProcessSandbox detect(SandboxMode mode,
                                 const Workspace& workspace,
                                 std::filesystem::path agent_root,
                                 std::filesystem::path temp_root);
    [[nodiscard]] const SandboxCapabilities& capabilities() const noexcept;
    void apply_in_child() const;
};
```

- [ ] **Step 2: Add sandbox detection tests**

Tests must not assume Landlock exists on every CI kernel or that `sandbox-exec` exists on every macOS host. Test deterministic policy behavior through injected/probed capability values:

```text
strict + required fs capability unavailable -> launch refused before exec
best_effort + unavailable -> action classified ask/unsandboxed, never silently allow
strict + capability available -> child apply hook required
```

- [ ] **Step 3: Implement Linux backend**

Under `__linux__`, use Landlock syscalls directly. Query ABI. Create ruleset handling supported filesystem rights. Add read/execute rules for required system/toolchain paths and agent runtime reads; add rights for workspace according to effective scope; add dedicated temp write access. Call `prctl(PR_SET_NO_NEW_PRIVS, 1, ...)` then `landlock_restrict_self` in child before exec.

If the available ABI supports network access rights, handle TCP bind/connect and grant neither. If not supported, report `network=false`; strict mode may still proceed only if deployment policy says filesystem-only sandbox meets requested capability. For this agent spec, `strict` requests both filesystem and network denial, so missing requested network restriction causes strict command execution refusal unless the machine is explicitly configured air-gapped outside the process and a future config flag defines that trust boundary. Do not silently claim network isolation.

- [ ] **Step 4: Implement macOS backend**

Under `__APPLE__`, detect executable `/usr/bin/sandbox-exec`. Generate a temporary profile that denies network, defaults deny, grants process execution/system reads required for the selected argv, agent runtime reads, scoped workspace reads/writes, and temp writes. Launch command as:

```text
/usr/bin/sandbox-exec -f PROFILE -- ORIGINAL_ARGV...
```

without a shell. If unavailable, strict refuses; best-effort returns an approval-required unsandboxed classification.

- [ ] **Step 5: Test real containment when supported**

Platform-conditional tests create a temporary workspace and a sibling forbidden file. A sandboxed helper must be able to read/write allowed workspace files and fail to write the forbidden sibling. Network test attempts localhost socket connect/bind only when the backend reports network enforcement; expected failure is sandbox denial, not dependency on external internet.

- [ ] **Step 6: Run process/sandbox suite**

```bash
ctest --test-dir build/cpu-release -R process --output-on-failure
```

Expected: all portable tests pass; supported platform containment tests pass; unsupported platform cases explicitly skip with capability reason rather than passing falsely.

- [ ] **Step 7: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src/process_sandbox.* agents/g9v3-3b-llamacpp-lua/src/process.* agents/g9v3-3b-llamacpp-lua/tests/test_process.cpp
git commit -m "feat: sandbox coding-agent child processes"
```

---

### Task 7: Wire policy-aware tool registry

**Files:**
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tool_registry.hpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tool_registry.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tools.hpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/src/tools.cpp`
- Modify: `agents/g9v3-3b-llamacpp-lua/tests/test_tools.cpp`

**Interfaces:**
- Consumes: all preceding tasks.
- Produces: one execution gateway that returns either `allowed result`, `approval required`, or `denied result` without side effects before approval.

- [ ] **Step 1: Extend tool execution result**

Add:

```cpp
enum class ToolDisposition { completed, approval_required, denied, failed };

struct ToolResult {
    ToolDisposition disposition = ToolDisposition::failed;
    bool ok = false;
    JsonValue payload{JsonValue::object{}};
    std::string display_summary;
    std::optional<ActionDescriptor> approval;
};
```

- [ ] **Step 2: Add preflight/execution split**

Each mutating/process tool must construct and validate its `ActionDescriptor` before any write/exec. ToolRegistry asks Guardrails. `ask` returns `approval_required` containing exact targets/argv/summary. The interactive layer later calls a separate `execute_approved(call, equivalence_key)` path only after the user grants approval.

- [ ] **Step 3: Test zero-side-effect approval path**

Tests must prove:

```text
review-profile write -> approval_required and target file unchanged
balanced delete -> approval_required and file still exists
vcs-destructive -> denied with no process launch
session-approved same equivalence key -> executes
session approval for one target does not approve different target
```

- [ ] **Step 4: Run all tools/policy/process tests**

```bash
ctest --test-dir build/cpu-release -R 'workspace|guardrails|patch_engine|tools|process' --output-on-failure
```

Expected: all pass.

- [ ] **Step 5: Run repository validator**

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed`.

- [ ] **Step 6: Commit**

```bash
git add agents/g9v3-3b-llamacpp-lua/src agents/g9v3-3b-llamacpp-lua/tests
git commit -m "feat: enforce policy-aware coding tools"
```

---

## Plan 2 completion gate

```text
[ ] workspace escape tests pass
[ ] symlink escape tests pass
[ ] hierarchical r/rw/none scope tests pass
[ ] deny globs win over allows
[ ] guardrail profile matrix is exact
[ ] destructive tools produce approval before side effects
[ ] patch engine is preflighted and rollback-aware
[ ] search/read/list outputs are bounded and deterministic
[ ] run_command never uses a shell
[ ] command classification examines argv, not only executable name
[ ] timeout/cancellation kill process groups
[ ] strict sandbox refuses to launch if required capabilities are unavailable
[ ] supported Landlock/Seatbelt containment tests pass or explicitly skip with reason
[ ] repository isolation validator passes
```

Do not implement model inference, G9 tool-call parsing, the agent loop, or terminal UI in this plan.