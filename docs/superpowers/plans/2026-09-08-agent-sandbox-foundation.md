# Agent Sandbox Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a repository foundation where every agent is fully portable and self-contained, while shared evaluation tooling can discover, invoke, compare, and validate agents from outside their directories.

**Architecture:** Agent implementations live only under `agents/<agent-name>/` and must not import or reference repository-level code at runtime. Shared evaluation code, datasets, prompts, schemas, results, and repository utilities live outside agent directories and may invoke agents through a minimal JSON stdin/stdout protocol plus local `agent.yaml` metadata.

**Tech Stack:** Markdown, YAML, JSON Schema, Python 3 standard library for repository validation only; agent implementations remain language/runtime agnostic.

**Spec:** `docs/superpowers/specs/2026-09-08-agent-sandbox-architecture-design.md`

## Global Constraints

- Every agent directory must remain fully portable and runnable when copied out of this repository by itself.
- Agents must never depend on `evaluation/`, `scripts/`, another agent, or root-level shared libraries.
- No mandatory root-level `common/`, `shared/`, `lib/`, workspace package, Python package, Node package, or other implementation dependency may be consumed by agents.
- `agents/_template/` is a scaffold only and must never be imported by existing agents at runtime.
- Shared evaluation infrastructure may depend on agents; dependency direction is never reversed.
- Agent directory names use lowercase kebab-case.
- Secrets must never be committed.
- The repository must not force one programming language, package manager, inference runtime, cloud provider, agent framework, vector database, or observability platform.

---

### Task 1: Root Repository Contract and Navigation

**Files:**
- Modify: `README.md`
- Create: `AGENTS.md`
- Create: `.gitignore`
- Create: `docs/architecture.md`
- Create: `docs/experiments.md`

**Interfaces:**
- Consumes: approved architecture spec.
- Produces: human- and coding-agent-facing repository rules used by every later task.

- [ ] **Step 1: Replace the root README with the repository purpose and structure**

The README must state:

```text
local-model-sandbox is a laboratory for independent LLM agents.
Each directory under agents/ is a self-contained experiment.
Shared evaluation tooling may invoke agents, but agents may not depend on repository-level code.
```

It must include the target top-level structure, a short “Adding an agent” section, and links to `docs/architecture.md` and `docs/agent-contract.md`.

- [ ] **Step 2: Add `AGENTS.md` with non-negotiable isolation rules**

Include explicit rules forbidding:

```text
agents/<name> -> ../evaluation
agents/<name> -> ../scripts
agents/<name> -> ../<other-agent>
agents/<name> -> root shared implementation libraries
```

Also state that duplicating small helper code inside an agent is preferable to coupling agents through shared runtime code.

- [ ] **Step 3: Add a conservative root `.gitignore`**

Include at least:

```gitignore
.DS_Store
.env
.env.*
!.env.example
__pycache__/
*.py[cod]
.venv/
venv/
node_modules/
dist/
build/
coverage/
.pytest_cache/
.mypy_cache/
results/**/*.tmp
results/**/*.log
```

Do not ignore `agent.yaml`, benchmark definitions, schemas, small reproducible results, or documentation.

- [ ] **Step 4: Add `docs/architecture.md`**

Document the dependency direction:

```text
evaluation/ ─────► agents/<agent-name>/
scripts/    ─────► agents/<agent-name>/
results/    ◄──── evaluation/

agents/* ──X──► repository-level implementation code
```

Explain that the root repository is a lab/orchestrator, not a runtime framework agents inherit from.

- [ ] **Step 5: Add `docs/experiments.md`**

Define a lightweight experiment record with these fields:

```yaml
id: experiment-id
agent: agent-directory-name
hypothesis: what is expected to happen
variables:
  independent: []
  controlled: []
benchmark: benchmark-name
notes: free-form interpretation
```

The document must say this is a recording convention, not a runtime dependency.

- [ ] **Step 6: Verify documentation references resolve conceptually**

Check that every path named in the root README is either created in this task or in a later task in this plan.

- [ ] **Step 7: Commit**

```bash
git add README.md AGENTS.md .gitignore docs/architecture.md docs/experiments.md
git commit -m "docs: define autonomous agent repository contract"
```

---

### Task 2: Agent Area and Portable Template

**Files:**
- Create: `agents/README.md`
- Create: `agents/_template/README.md`
- Create: `agents/_template/agent.yaml`
- Create: `agents/_template/run.sh`
- Create: `agents/_template/src/.gitkeep`
- Create: `agents/_template/tests/.gitkeep`

**Interfaces:**
- Consumes: repository isolation rules from Task 1.
- Produces: copy-only scaffold for new self-contained agents and metadata shape consumed by repository tooling.

- [ ] **Step 1: Add `agents/README.md`**

Document that each direct child directory except names beginning with `_` is an independent agent experiment. Include the creation flow:

```bash
cp -R agents/_template agents/my-agent
```

Then require the copied agent to replace template metadata and implementation placeholders before evaluation.

- [ ] **Step 2: Add a language-neutral `agent.yaml` template**

Use this exact baseline shape:

```yaml
schema_version: 1
name: template-agent
version: 0.1.0
execution_mode: local
runtime: custom
entrypoint: ./run.sh
contract_version: 1
model:
  provider: unspecified
  name: unspecified
environment: []
tags: []
```

The README must explain that `environment` contains variable names only, never values.

- [ ] **Step 3: Add a portable `run.sh` placeholder**

Use a script that fails clearly instead of pretending to be a working agent:

```sh
#!/usr/bin/env sh
set -eu
printf '%s\n' '{"error":"template agent has no implementation"}'
exit 2
```

No repository-relative paths are allowed in this script.

- [ ] **Step 4: Add template README portability checklist**

The checklist must require that, after copying the directory elsewhere:

```text
- dependencies install from files/instructions inside the copied directory;
- entrypoint resolves inside the copied directory;
- no ../ repository paths are referenced at runtime;
- environment variables are documented locally;
- tests can run without importing repository code.
```

- [ ] **Step 5: Commit**

```bash
git add agents/
git commit -m "chore: add portable agent scaffold"
```

---

### Task 3: Versioned Agent Execution Contract

**Files:**
- Create: `docs/agent-contract.md`
- Create: `evaluation/README.md`
- Create: `evaluation/schemas/agent-input-v1.schema.json`
- Create: `evaluation/schemas/agent-output-v1.schema.json`
- Create: `evaluation/schemas/agent-metadata-v1.schema.json`
- Create: `evaluation/benchmarks/README.md`
- Create: `evaluation/datasets/README.md`
- Create: `evaluation/prompts/README.md`
- Create: `evaluation/harness/README.md`

**Interfaces:**
- Consumes: `agent.yaml` baseline from Task 2.
- Produces: protocol and schemas that external harnesses can use without sharing implementation code with agents.

- [ ] **Step 1: Define contract v1 in `docs/agent-contract.md`**

Required invocation model:

```text
one JSON object on stdin -> one JSON object on stdout
```

Baseline input:

```json
{
  "prompt": "Explain recursion",
  "context": [],
  "options": {}
}
```

Baseline success output:

```json
{
  "output": "Recursion is ...",
  "model": "model-name",
  "usage": {},
  "metadata": {}
}
```

The only required input field is `prompt`. The only required success-output field is `output`. Other fields are optional to avoid forcing uniform internals.

- [ ] **Step 2: Define structured failure behavior**

Document that process-level failure may use non-zero exit status. If an agent emits JSON before failing, the recommended shape is:

```json
{
  "error": "human-readable failure description",
  "metadata": {}
}
```

The repository harness must not assume every failure can produce valid JSON.

- [ ] **Step 3: Add input JSON Schema**

`agent-input-v1.schema.json` must be Draft 2020-12 compatible, require a non-empty string `prompt`, allow array `context`, object `options`, and permit additional properties for forward-compatible experiments.

- [ ] **Step 4: Add output JSON Schema**

`agent-output-v1.schema.json` must require a string `output` for success records and permit optional `model`, `usage`, `metadata`, and additional properties.

- [ ] **Step 5: Add metadata JSON Schema**

`agent-metadata-v1.schema.json` must represent the YAML metadata keys from Task 2 after YAML parsing. Require:

```text
schema_version
name
version
execution_mode
runtime
entrypoint
contract_version
model
```

Restrict `execution_mode` to `local`, `cloud`, or `hybrid`. Do not restrict runtime/provider names to a fixed vendor list.

- [ ] **Step 6: Add evaluation area READMEs**

Each README must explain its responsibility and reinforce that evaluation code may invoke agents but agents may not import anything from `evaluation/`.

- [ ] **Step 7: Validate schemas with Python standard library parse checks**

Run:

```bash
python3 - <<'PY'
import json
from pathlib import Path
for path in Path('evaluation/schemas').glob('*.json'):
    json.loads(path.read_text())
    print('OK', path)
PY
```

Expected: all schema files print `OK` and no exception is raised.

- [ ] **Step 8: Commit**

```bash
git add docs/agent-contract.md evaluation/
git commit -m "docs: define versioned agent execution contract"
```

---

### Task 4: Results Area and Repository Validation Tool

**Files:**
- Create: `results/README.md`
- Create: `scripts/README.md`
- Create: `scripts/validate_repo.py`
- Create: `scripts/tests/test_validate_repo.py`

**Interfaces:**
- Consumes: agent directory conventions, `agent.yaml`, metadata schema semantics, and isolation rules.
- Produces: repository-only validator executable as `python3 scripts/validate_repo.py`.

- [ ] **Step 1: Write failing tests for agent discovery and naming**

Use `unittest` and temporary directories. Cover:

```python
class ValidateRepoTests(unittest.TestCase):
    def test_discovers_non_private_agent_directories(self): ...
    def test_ignores_template_directory(self): ...
    def test_rejects_non_kebab_case_agent_name(self): ...
```

Expected discovery rule: direct child directories of `agents/` whose names do not begin with `_`.

- [ ] **Step 2: Run the tests and verify failure**

Run:

```bash
python3 -m unittest scripts.tests.test_validate_repo -v
```

Expected: FAIL because `scripts/validate_repo.py` does not exist yet.

- [ ] **Step 3: Implement discovery and naming validation**

Implement focused functions:

```python
def discover_agents(agents_root: Path) -> list[Path]:
    ...

def is_kebab_case(name: str) -> bool:
    ...
```

`is_kebab_case` must accept names matching:

```regex
^[a-z0-9]+(?:-[a-z0-9]+)*$
```

- [ ] **Step 4: Add failing tests for metadata presence and entrypoint containment**

Cover:

```python
def test_requires_agent_yaml(self): ...
def test_requires_declared_entrypoint_to_exist(self): ...
def test_rejects_entrypoint_outside_agent_directory(self): ...
```

Do not add a PyYAML dependency. For v1 repository validation, parse the small baseline metadata using a restricted line-oriented reader that extracts required top-level scalar keys only; full YAML/schema validation may be added later in the evaluation layer.

- [ ] **Step 5: Implement minimal metadata checks**

Implement:

```python
def read_top_level_yaml_scalars(path: Path) -> dict[str, str]:
    ...

def validate_agent_metadata(agent_dir: Path) -> list[str]:
    ...
```

Required scalar keys for this validator:

```text
schema_version
name
version
execution_mode
runtime
entrypoint
contract_version
```

Resolve `entrypoint` against `agent_dir` and reject any path whose resolved location escapes `agent_dir`.

- [ ] **Step 6: Add failing tests for obvious cross-directory references**

Create fixture files containing each forbidden runtime path form and assert validation errors:

```text
../evaluation
../scripts
../other-agent
../../evaluation
```

The test should inspect text-like implementation/config files while skipping `.git`, binary files, and the repository documentation file itself.

- [ ] **Step 7: Implement isolation scan**

Implement:

```python
def find_forbidden_references(agent_dir: Path) -> list[str]:
    ...
```

Scan common text file extensions plus extensionless scripts, and report file-relative path plus matched forbidden reference. This is a guardrail for obvious violations, not a proof of isolation.

- [ ] **Step 8: Add CLI aggregation test**

Cover a valid temporary repository and assert zero errors; add one broken agent and assert non-zero validation result.

Expose:

```python
def validate_repository(repo_root: Path) -> list[str]:
    ...

def main() -> int:
    ...
```

CLI output format:

```text
OK: repository validation passed
```

or one error per line prefixed with:

```text
ERROR: ...
```

- [ ] **Step 9: Run validator tests**

Run:

```bash
python3 -m unittest scripts.tests.test_validate_repo -v
```

Expected: PASS.

- [ ] **Step 10: Run validator against the real repository**

Run:

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed` because `_template` is excluded from real-agent discovery.

- [ ] **Step 11: Document scripts and results**

`scripts/README.md` must state that scripts are repository operator tooling and must never be imported by agents.

`results/README.md` must define a recommended result record containing:

```json
{
  "agent": "agent-name",
  "agent_version": "0.1.0",
  "benchmark": "benchmark-name",
  "timestamp": "RFC3339 timestamp",
  "model": {},
  "configuration": {},
  "score": {},
  "latency_ms": null,
  "usage": {},
  "status": "success"
}
```

and explain that large/generated artifacts should not automatically be committed.

- [ ] **Step 12: Commit**

```bash
git add results/ scripts/
git commit -m "feat: add repository isolation validator"
```

---

### Task 5: Foundation Verification and Review Preparation

**Files:**
- Modify only files found incorrect during verification.

**Interfaces:**
- Consumes: all prior tasks.
- Produces: reviewed branch ready for PR/merge decision.

- [ ] **Step 1: Verify the target tree**

Run:

```bash
find . -maxdepth 4 -type f | sort
```

Confirm the repository contains the documented root files, agent template, evaluation structure, schemas, results docs, validator, tests, spec, and plan.

- [ ] **Step 2: Run all repository-level tests**

Run:

```bash
python3 -m unittest discover -s scripts/tests -v
```

Expected: PASS.

- [ ] **Step 3: Run repository validation**

Run:

```bash
python3 scripts/validate_repo.py
```

Expected: `OK: repository validation passed`.

- [ ] **Step 4: Parse every JSON schema**

Run:

```bash
python3 - <<'PY'
import json
from pathlib import Path
for p in sorted(Path('evaluation/schemas').glob('*.json')):
    json.loads(p.read_text())
    print('OK', p)
PY
```

Expected: all files parse successfully.

- [ ] **Step 5: Search for accidental shared-runtime patterns in the template**

Run:

```bash
grep -RInE '(\.\./(evaluation|scripts|agents)|from +scripts|import +scripts|from +evaluation|import +evaluation)' agents/_template || true
```

Expected: no output.

- [ ] **Step 6: Compare branch with main**

Review the full diff and confirm:

```text
- no fictional production agent was added;
- no root runtime dependency manifest was added;
- no shared runtime library was added;
- all agent-facing rules preserve copy-out portability;
- repository tooling depends only on Python standard library.
```

- [ ] **Step 7: Commit verification fixes if any**

If verification required changes:

```bash
git add -A
git commit -m "fix: tighten sandbox foundation validation"
```

If no changes were required, do not create an empty commit.
