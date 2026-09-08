# Repository scripts

Utilities in this directory are for repository operators and shared evaluation workflows.

They may inspect or invoke agents from outside. Agents must never import `scripts/` or rely on these files at runtime.

## Repository validation

Run:

```bash
python3 scripts/validate_repo.py
```

The validator uses only the Python standard library. It checks real agent directory naming, required top-level metadata, entrypoint containment/existence, external symlink targets, and obvious relative runtime references that escape an agent directory.

It is a guardrail, not a formal proof of portability. A copy-out smoke test remains the strongest check.
