# Results

This directory stores outputs produced by shared evaluations, not agent implementation code.

Recommended result record:

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

Results should include enough configuration and version information to make comparisons interpretable and, where practical, reproducible.

Large generated artifacts, transient logs, caches, and bulky model outputs should not automatically be committed. Keep small meaningful result summaries when they are useful as experiment evidence.
