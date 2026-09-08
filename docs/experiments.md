# Experiment records

This file defines a lightweight convention for recording why an experiment was run and what changed. It is documentation only; agents do not depend on it at runtime.

Recommended record:

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

## Guidance

- `id` should be stable and descriptive enough to find the corresponding result set.
- `agent` identifies the directory used for the run.
- `hypothesis` states the expected effect before interpreting results.
- `variables.independent` lists deliberately changed factors such as model, temperature, tool strategy, or prompt policy.
- `variables.controlled` lists factors intentionally held constant for comparison.
- `benchmark` identifies the shared benchmark or test procedure.
- `notes` records observations, caveats, and interpretation after the run.

Not every exploratory run needs a formal record. Use this convention when reproducibility or comparison matters.
