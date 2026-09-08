# local-model-sandbox

A laboratory for building and comparing independent LLM agents.

Each directory under `agents/` is a self-contained experiment. An agent may use a local model, a cloud model, a hybrid architecture, any programming language, and any runtime that makes sense for that experiment.

## Core rule

**An agent must keep working when its directory is copied out of this repository by itself.**

Repository-level tooling may discover, launch, test, and compare agents. Agents must not import or depend on repository-level implementation code, another agent, `evaluation/`, or `scripts/`.

## Repository structure

```text
agents/       Independent, portable agent implementations
evaluation/   Shared benchmarks, datasets, prompts, schemas, and harnesses
results/      Evaluation outputs and reproducibility metadata
docs/         Architecture, contracts, and experiment conventions
scripts/      Repository-operator tooling only
```

## Adding an agent

Start from the neutral scaffold if useful:

```bash
cp -R agents/_template agents/my-agent
```

Then make the copied directory independently installable/runnable, update its `agent.yaml`, and remove any template placeholders. Never make it depend on `_template` or another repository directory.

Agent directory names use lowercase kebab-case.

## Shared evaluation

The shared evaluation layer can invoke different agents through a minimal protocol rather than a shared software library. See [the agent contract](docs/agent-contract.md).

For the architectural rationale and dependency rules, see [architecture](docs/architecture.md). For recording experiments, see [experiments](docs/experiments.md).

## Status

This repository is intentionally framework-neutral. Individual experiments choose their own model runtime, language, dependencies, orchestration strategy, and deployment style.
