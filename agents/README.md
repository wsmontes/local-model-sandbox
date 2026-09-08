# Agents

Each direct child directory under `agents/` is an independent experiment, except directories whose names begin with `_`, which are repository scaffolding or support material.

## Create an agent

You may start from the neutral template:

```bash
cp -R agents/_template agents/my-agent
```

Then:

1. rename and describe the agent in `agent.yaml`;
2. replace the placeholder entrypoint with the real implementation;
3. add dependency manifests or installation instructions inside the agent directory;
4. document required environment variables locally;
5. add local tests where useful;
6. verify the directory works when copied outside this repository.

Real agent directory names use lowercase kebab-case.

## Isolation

An agent must not import, source, symlink, or otherwise depend at runtime on:

- `evaluation/`;
- `scripts/`;
- another agent directory;
- `agents/_template/`;
- any root-level shared runtime library.

External requirements such as Ollama, llama.cpp, Docker, CUDA, Metal, Python, Node.js, or a cloud API are fine when documented inside the agent.

Small utility-code duplication is acceptable when it preserves portability.
