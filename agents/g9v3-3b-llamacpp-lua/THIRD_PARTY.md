# Third-party provisioning

This agent intentionally keeps external dependencies out of Git and does not download them automatically.

## Lua

Required baseline: **Lua 5.4.9**.

CMake expects `LUA_DIR` to point to the Lua `src` directory containing `lua.h`, `lauxlib.h`, `luaconf.h`, and the Lua C source files.

Example:

```bash
-DLUA_DIR=/opt/src/lua-5.4.9/src
```

For a self-contained copied agent, copy the contents of that `src` directory into:

```text
third_party/lua/
```

The build compiles the required Lua sources into a static library. The agent does not require a separately installed Lua interpreter at runtime.

Lua is distributed under the MIT license. Preserve the upstream license/copyright notice when provisioning or redistributing Lua sources/binaries.

## llama.cpp

Required revision:

```text
f3f1a8f2760f28325a5ec20c05b171e5b7c83a29
```

Either point CMake at a local checkout:

```bash
-DLLAMA_CPP_DIR=/opt/src/llama.cpp
```

or provision the checkout at:

```text
third_party/llama.cpp/
```

The expected directory contains the upstream `CMakeLists.txt` and `include/llama.h`.

The agent build disables upstream examples, tools, server, unified app, upstream tests, CURL integration, and GGML RPC. No Git fetch/clone is performed by this project.

`llama.cpp` is distributed under the MIT license. Preserve the upstream license/copyright notice when provisioning or redistributing its sources/binaries.

## Model

The configured model family is:

```text
ai9stars/G9v3-3B
```

For llama.cpp, provision a compatible GGUF ahead of time. The default Lua configuration expects:

```text
models/ai9stars_G9v3-3B-Q4_K_M.gguf
```

The model weights are deliberately excluded from Git. Review the model repository's current license/terms before copying or redistributing weights; this project does not assert a license for the model.

## Connected provisioning vs offline use

Provisioning the dependencies/model may require internet access, but that step is outside this project. Once the files are present locally, configure/build/runtime do not need a network connection.

For maximum assurance, perform build and execution inside a host/container with networking disabled.
