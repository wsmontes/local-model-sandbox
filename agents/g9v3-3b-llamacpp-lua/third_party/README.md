# Local dependency directory

This directory is a provisioning target, not a source of shared monorepo dependencies.

For a copied standalone agent, you may place:

```text
third_party/
├── lua/          # contents of Lua 5.4.9 src/
└── llama.cpp/    # llama.cpp at f3f1a8f2760f28325a5ec20c05b171e5b7c83a29
```

These directories are ignored by Git because they are external source trees. CMake also accepts `-DLUA_DIR=...` and `-DLLAMA_CPP_DIR=...`, so keeping dependencies outside the agent directory is supported.

The build must never auto-clone, auto-fetch, or auto-download either dependency. See `../THIRD_PARTY.md` for the exact pins and redistribution notes.
