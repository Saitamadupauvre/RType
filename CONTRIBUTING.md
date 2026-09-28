# Contributing to RType

Rules for code, tests and architecture are in [AGENTS.md](AGENTS.md).

## Workflow

1. Open (or pick) an issue, then work on a branch named after it.
2. Open a pull request that says `Closes #<issue>`; CI must pass before merging.
3. Pull requests target `dev`; `dev` is merged into `main` for releases.

## Build and test

Set `VCPKG_ROOT` first (see the README).

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

CI runs `cmake --workflow --preset ci`: run it locally before pushing.

## Style

Code is formatted with clang-format (`.clang-format`): run the `format` target
(`cmake --build --preset debug --target format`) before committing.
