# Autonomous Agent Sandbox Architecture

Date: 2026-09-08
Status: Proposed and approved in chat; implementation pending spec review
Repository: `wsmontes/local-model-sandbox`

## 1. Purpose

`local-model-sandbox` is a laboratory for testing many LLM agents implemented with independent codebases. Agents may use local models, cloud models, hybrid approaches, different languages, different runtimes, and different orchestration strategies.

The repository must support shared evaluation and comparison while preserving one non-negotiable property: **every agent directory must remain fully portable and runnable when copied out of this repository by itself**.

## 2. Core architectural rule

The repository infrastructure may depend on agents, but agents must never depend on repository infrastructure.

Allowed direction:

```text
evaluation/ ─────► agents/<agent-name>/
results/     ◄──── evaluation/
scripts/     ─────► agents/<agent-name>/   # repository tooling only
```

Forbidden direction:

```text
agents/<agent-name>/ ──X──► evaluation/
agents/<agent-name>/ ──X──► scripts/
agents/<agent-name>/ ──X──► another agent
agents/<agent-name>/ ──X──► root-level shared libraries
```

This means there will be no mandatory root-level `common/`, `shared/`, `lib/`, workspace package, Python package, Node package, or other implementation dependency consumed by agents.

## 3. Target repository structure

```text
local-model-sandbox/
├── README.md
├── AGENTS.md
├── .gitignore
│
├── agents/
│   ├── README.md
│   ├── _template/
│   │   ├── README.md
│   │   ├── agent.yaml
│   │   ├── run.*
│   │   ├── src/
│   │   └── tests/
│   │
│   └── <agent-name>/
│       └── [fully self-contained implementation]
│
├── evaluation/
│   ├── README.md
│   ├── benchmarks/
│   ├── datasets/
│   ├── prompts/
│   ├── schemas/
│   └── harness/
│
├── results/
│   ├── README.md
│   └── ...
│
├── docs/
│   ├── architecture.md
│   ├── agent-contract.md
│   └── experiments.md
│
└── scripts/
    └── [repository-level utilities only]
```

Empty directories will be represented by README or placeholder files only where Git requires a tracked file.

## 4. Agent autonomy contract

Each directory under `agents/` represents an independent experiment or agent implementation.

An agent must:

- contain all source code required for its own execution;
- contain its own dependency manifest or installation instructions;
- contain its own configuration examples;
- contain its own tests when tests exist;
- document any external runtime requirement such as Ollama, llama.cpp, Docker, CUDA, Metal, Node.js, Python, or a cloud API;
- avoid imports, symlinks, package references, relative file references, or runtime assumptions that point outside the agent directory;
- remain understandable and usable after its directory is copied elsewhere.

An agent may duplicate small pieces of utility code that another agent also contains. Duplication is preferable to hidden coupling when portability is the goal.

## 5. `_template` semantics

`agents/_template/` is a scaffold, not a library.

Its contents may be copied to start a new agent, but existing agents must never import from or reference `_template` at runtime.

The template should remain language-neutral where practical. It may include placeholders such as `run.*` rather than forcing Python or Node on all implementations.

## 6. Shared evaluation area

`evaluation/` is intentionally shared because the repository needs a common laboratory for fair comparisons.

### `evaluation/benchmarks/`

Benchmark definitions and suites. A benchmark describes what is being tested and how success is measured.

### `evaluation/datasets/`

Reusable test inputs. Datasets may be versioned directly when small enough, or documented with retrieval/generation instructions when too large or legally unsuitable for direct inclusion.

### `evaluation/prompts/`

Canonical prompts or prompt sets used across multiple agents.

### `evaluation/schemas/`

Schemas for benchmark input, agent output, run metadata, and result records.

### `evaluation/harness/`

Code that invokes agents from the outside, runs benchmark cases, captures timing and metadata, validates output, and writes results.

The harness is allowed to know how to launch an agent. The agent is not allowed to know that the harness exists.

## 7. Common execution contract

Agents may expose a common execution interface so the shared harness can compare fundamentally different implementations without sharing code.

This is a **protocol contract**, not a software dependency.

The preferred baseline interface is:

```text
JSON input  -> agent entrypoint -> JSON output
```

The first implementation should support standard input / standard output where practical because this works across Python, JavaScript, shell wrappers, local model CLIs, containers, and cloud-backed agents.

Illustrative input:

```json
{
  "prompt": "Explain recursion",
  "context": [],
  "options": {}
}
```

Illustrative output:

```json
{
  "output": "...",
  "model": "qwen3",
  "usage": {},
  "metadata": {}
}
```

The exact versioned schema will be defined in `docs/agent-contract.md` and mirrored under `evaluation/schemas/` during implementation.

The contract should stay minimal. Agent-specific capabilities may be exposed through optional fields rather than forcing all implementations to support the same internal architecture.

## 8. Agent metadata

Each agent should include a small machine-readable `agent.yaml` describing how the repository harness can identify and launch it.

The metadata is descriptive and local to the agent. It should contain only values needed for discovery and execution, for example:

- agent name and version;
- implementation/runtime type;
- entrypoint;
- model/provider description;
- execution mode: local, cloud, or hybrid;
- environment variables required by name only, never secret values;
- supported contract version;
- optional tags or capabilities.

The schema must not require an agent to import any repository package.

## 9. Results

`results/` stores outputs produced by shared evaluations rather than implementation code.

Results should identify at least:

- agent;
- agent version or git commit when applicable;
- benchmark;
- timestamp;
- model/provider information;
- run configuration relevant to reproduction;
- raw or normalized score;
- latency and token/resource usage where available.

Large generated artifacts should not automatically be committed. The repository documentation will define what is tracked versus ignored.

## 10. Root-level documentation

### `README.md`

Explains the purpose of the repository, the isolation principle, top-level structure, and how to add or run an agent.

### `AGENTS.md`

Defines rules for humans and coding agents modifying the repository. The primary rule is that code inside one agent must not acquire dependencies on another directory in the repository.

### `docs/architecture.md`

Human-readable architectural explanation derived from this design.

### `docs/agent-contract.md`

Versioned execution and metadata contract.

### `docs/experiments.md`

Convention for recording experiment intent, hypotheses, variables, and interpretation without requiring every agent to use the same implementation.

## 11. Repository scripts

`scripts/` contains convenience tooling for repository operations such as:

- validating that agent metadata files are syntactically valid;
- checking for obvious cross-agent or root-level dependency violations;
- listing agents;
- launching an evaluation suite;
- collecting or summarizing results.

These scripts are tools for the repository operator. They are never part of an agent's runtime dependency graph.

## 12. Error handling and isolation failures

The shared harness should treat agent failures as experiment results, not as repository-wide failures whenever possible.

Examples include:

- missing model runtime;
- missing API credential;
- process timeout;
- invalid JSON output;
- non-zero process exit;
- unsupported contract version;
- unavailable hardware feature.

The harness should capture these failures with structured metadata so one broken experiment does not prevent other agents from being evaluated.

Isolation violations are different: they are architectural errors. Validation should fail when an agent contains an explicit runtime dependency on a path outside its own directory.

## 13. Testing strategy

Testing is split into two levels.

### Agent-local tests

Each agent owns its tests and dependency setup. Running those tests must not require the rest of the repository unless the test explicitly exercises the optional shared execution contract from outside the agent.

### Repository-level validation

Shared checks should verify:

1. expected metadata is present and parseable;
2. declared entrypoints exist;
3. contract-compatible agents can accept a minimal input and produce valid output;
4. benchmark definitions and schemas are valid;
5. no obvious forbidden cross-directory dependencies exist;
6. the harness records failure of one agent without aborting unrelated evaluations.

A portability smoke test should eventually copy an agent directory into a temporary location and verify that its documented setup does not rely on repository-relative files.

## 14. Naming and scope conventions

Agent directory names should be descriptive and stable, using lowercase kebab-case, for example:

```text
agents/qwen3-ollama-basic/
agents/qwen3-tool-agent/
agents/claude-cloud-agent/
agents/qwen-local-cloud-router/
```

The directory name identifies the experiment implementation, not necessarily only the model. Multiple agents may use the same model with different orchestration strategies.

## 15. Secrets and environment configuration

Secrets must never be committed.

Cloud-backed agents should include an `.env.example` or equivalent documenting required variable names. Each agent owns its own environment configuration conventions unless the execution contract requires otherwise.

The root `.gitignore` may provide repository-wide protection for common secret and generated-file patterns, but an agent must still be safe and understandable when copied outside the repository.

## 16. Design decisions intentionally deferred

The initial structure will not force:

- one programming language;
- one package manager;
- Docker;
- one local inference runtime;
- one cloud provider;
- one agent framework;
- one tool-calling framework;
- one vector database;
- one observability platform;
- one evaluation framework.

Those choices belong to individual experiments unless a later repository-level need justifies an optional adapter in the shared evaluation layer.

## 17. Initial implementation scope

The first implementation should create only the repository foundation:

1. root README, AGENTS rules, and gitignore;
2. `agents/` documentation and a neutral `_template`;
3. `evaluation/` documentation and initial schemas/placeholders;
4. `results/` documentation;
5. architecture and agent-contract documentation;
6. minimal repository validation tooling only if it can remain implementation-neutral.

It should not create fictional production agents or introduce a shared runtime library.

## 18. Success criteria

The architecture is successful when all of the following are true:

- a new agent can be added without changing a root dependency manifest;
- an agent directory can be copied elsewhere without breaking because of repository-relative code dependencies;
- local, cloud, and hybrid agents can coexist;
- shared datasets and benchmarks can evaluate multiple agents;
- comparison infrastructure can evolve without forcing changes to agent internals;
- one agent can use a completely different language, runtime, or framework from another;
- the repository remains understandable as the number of experiments grows.
