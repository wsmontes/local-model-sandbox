# Architecture

`local-model-sandbox` is a laboratory/orchestrator for independent LLM-agent experiments. It is not a framework that agents inherit from.

## Dependency direction

Allowed:

```text
evaluation/ ─────► agents/<agent-name>/
scripts/    ─────► agents/<agent-name>/
results/    ◄──── evaluation/
```

Forbidden:

```text
agents/* ──X──► evaluation/
agents/* ──X──► scripts/
agents/* ──X──► another agent
agents/* ──X──► root shared implementation code
```

The repository may know how to discover, launch, inspect, benchmark, and compare agents. An agent must not need the repository around it to run.

## Agent boundary

`agents/<agent-name>/` is the portability boundary. Source, dependency manifests, entrypoints, configuration examples, local tests, and agent-specific utility code belong inside that directory.

An agent may require external infrastructure such as Ollama, llama.cpp, Docker, CUDA, Metal, a cloud API, or another documented service. That is acceptable because the requirement is external to the repository, not a hidden dependency on sibling files.

## Shared laboratory

Shared assets are deliberately outside the agent boundary:

- `evaluation/benchmarks/` defines reusable tests and scoring intent;
- `evaluation/datasets/` stores or documents reusable inputs;
- `evaluation/prompts/` stores canonical prompt sets;
- `evaluation/schemas/` versions interchange formats;
- `evaluation/harness/` invokes agents from the outside;
- `results/` records evaluation outputs;
- `scripts/` contains repository-operator utilities.

These areas can evolve without changing agent internals as long as the execution contract remains compatible.

## Protocol, not library

Cross-agent comparability is provided through a small versioned input/output protocol and `agent.yaml` metadata. No common Python/Node package is required.

This keeps a Python Ollama agent, a Node cloud agent, a shell-wrapped local binary, and a hybrid agent equally valid citizens of the repository.

## Template

`agents/_template/` is copy-only scaffolding. Once copied, the new agent owns its copy. Existing agents must never import or reference `_template` at runtime.

## Portability test

The strongest architectural check is practical: copy one agent directory to a temporary location and follow only the instructions contained inside it. Repository validation can catch obvious path violations, but it cannot replace this smoke test.
