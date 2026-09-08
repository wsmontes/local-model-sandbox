# Repository rules for coding agents

This repository hosts independent LLM-agent experiments. Preserve experiment portability above deduplication convenience.

## Non-negotiable isolation rule

Code inside `agents/<name>/` must not acquire runtime dependencies on files outside that agent directory.

Forbidden dependency directions include:

```text
agents/<name> -> ../evaluation
agents/<name> -> ../scripts
agents/<name> -> ../<other-agent>
agents/<name> -> root-level shared implementation libraries
```

Do not introduce a root `common/`, `shared/`, `lib/`, package workspace, or similar runtime library for agents to consume.

Small helper-code duplication between agents is acceptable and is preferable to coupling when portability is the goal.

## Agent requirements

Every real agent directory must:

- be independently understandable and runnable;
- own its dependency manifest or setup instructions;
- own its configuration examples and environment-variable documentation;
- keep entrypoints inside its own directory;
- avoid symlinks or relative runtime references that escape its directory;
- use a lowercase kebab-case directory name;
- keep secrets out of version control.

`agents/_template/` is copy-only scaffolding. Never import from it at runtime.

## Shared areas

`evaluation/`, `results/`, `docs/`, and `scripts/` belong to the repository laboratory. They may inspect or invoke agents from the outside, but agents must not depend on them.

Repository tooling should remain implementation-neutral unless a concrete repository-level need requires otherwise.

## Changes

When adding or changing an agent, prefer changes contained entirely inside that agent directory. If a repository-level contract changes, update the relevant documentation and schemas without forcing unrelated agents to adopt shared implementation code.
