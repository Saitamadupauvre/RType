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

Static analysis uses clang-tidy (`.clang-tidy`); CI treats every warning as an
error (`cmake --workflow --preset tidy`). `NOLINT` markers are comments, which
the code rules forbid: fix the code, or disable the check project-wide in
`.clang-tidy` and document why below.

| Disabled check | Reason |
|---|---|
| `clang-analyzer-optin.core.EnumCastOutOfRange` | Decoders turn untrusted bytes into enums and must reject out-of-range values; their tests have to cast such values (e.g. `static_cast<DisconnectReason>(9)`) to prove it. |
