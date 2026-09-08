# Agent template

This directory is copy-only scaffolding for a new agent. It is not a shared library and must never be referenced by an existing agent at runtime.

## Start

Copy the directory and rename it:

```bash
cp -R agents/_template agents/my-agent
```

Then replace all placeholder values and implementation files inside the copy.

## Portability checklist

Before considering the copied agent ready:

- dependencies install from files or instructions inside the copied directory;
- the declared entrypoint resolves inside the copied directory;
- no `../` repository paths are referenced at runtime;
- environment variables are documented locally;
- tests can run without importing repository code;
- no secrets are committed;
- copying the directory elsewhere does not break it because of repository-relative dependencies.

The repository's shared execution contract is optional for experiments that do not need automated comparison, but implementing it makes the agent compatible with shared evaluation tooling.
