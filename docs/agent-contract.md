# Agent execution contract

This document defines the optional shared protocol used by repository-level evaluation tooling to invoke otherwise independent agents.

It is a protocol contract, not a software dependency. An agent implementing this contract still owns all of its runtime code and dependencies inside its own directory.

## Version 1

The preferred baseline transport is:

```text
one JSON object on stdin -> agent entrypoint -> one JSON object on stdout
```

This deliberately works across Python, JavaScript, shell wrappers, containers, local model CLIs, and cloud-backed agents.

## Input

Only `prompt` is required.

```json
{
  "prompt": "Explain recursion",
  "context": [],
  "options": {}
}
```

Fields:

- `prompt`: required non-empty string containing the primary request;
- `context`: optional array for prior messages, documents, or experiment-specific context;
- `options`: optional object for generation or execution options;
- additional fields are permitted for experiments that need them.

The schema is `evaluation/schemas/agent-input-v1.schema.json`.

## Success output

Only `output` is required.

```json
{
  "output": "Recursion is ...",
  "model": "model-name",
  "usage": {},
  "metadata": {}
}
```

Fields:

- `output`: required string containing the primary agent result;
- `model`: optional model identifier;
- `usage`: optional token/resource accounting;
- `metadata`: optional experiment-specific metadata;
- additional fields are permitted.

The schema is `evaluation/schemas/agent-output-v1.schema.json`.

## Failure behavior

An agent may signal process-level failure with a non-zero exit status. The harness must not assume every failure can emit valid JSON.

When JSON can be emitted before failure, the recommended shape is:

```json
{
  "error": "human-readable failure description",
  "metadata": {}
}
```

Typical failures include unavailable models, missing credentials, timeouts, invalid configuration, unsupported hardware, or provider errors.

## Metadata

Contract-compatible agents describe themselves with a local `agent.yaml`. The metadata tells repository tooling how to identify and launch the agent without requiring imports from the repository.

Baseline keys:

```yaml
schema_version: 1
name: example-agent
version: 0.1.0
execution_mode: local
runtime: custom
entrypoint: ./run.sh
contract_version: 1
model:
  provider: example
  name: example-model
environment: []
tags: []
```

`execution_mode` is one of `local`, `cloud`, or `hybrid`. Runtime, provider, and model names are intentionally open-ended.

The parsed metadata schema is `evaluation/schemas/agent-metadata-v1.schema.json`.

## Compatibility

Contract versions are explicit. A future breaking protocol change gets a new version instead of silently changing v1 semantics.

An experiment does not have to implement this contract unless it needs shared automated evaluation. Lack of support does not make it an invalid repository agent; it only means the shared harness needs a dedicated external adapter or cannot invoke it automatically.
