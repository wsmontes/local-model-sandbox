# Shared evaluation

This directory contains repository-level material for comparing independent agents.

Evaluation tooling may discover and invoke agents from the outside. Agent runtime code must never import or depend on this directory.

- `benchmarks/`: reusable test definitions and scoring intent.
- `datasets/`: reusable test inputs or retrieval/generation instructions.
- `prompts/`: canonical prompt sets used across experiments.
- `schemas/`: versioned interchange formats.
- `harness/`: external launch and evaluation tooling.

The shared layer standardizes comparison, not implementation.
