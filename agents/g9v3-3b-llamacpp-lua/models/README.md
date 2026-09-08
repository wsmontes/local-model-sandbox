# Model provisioning

This directory is intentionally kept free of committed model weights.

The agent targets the `ai9stars/G9v3-3B` family through `llama.cpp`, so a compatible **GGUF** must be supplied locally before runtime.

Default path used by `config/agent.lua`:

```text
models/ai9stars_G9v3-3B-Q4_K_M.gguf
```

You may instead pass an explicit model path:

```bash
./run.sh chat --workspace /path/to/project --model /absolute/path/to/model.gguf
```

or configure a model smoke test at build time:

```bash
./build.sh cpu-release \
  -DLUA_DIR=/path/to/lua-5.4.9/src \
  -DLLAMA_CPP_DIR=/path/to/llama.cpp \
  -DG9_MODEL_PATH=/absolute/path/to/model.gguf
```

## Rules

- Do not commit `.gguf` files to this repository.
- Do not add automatic model downloads to CMake, shell scripts, C++, or Lua.
- Provision and verify the chosen model before moving the machine into an offline environment.
- Review the model repository's current license/terms before redistribution.

The repository intentionally does not select or assert a license for third-party model weights.
